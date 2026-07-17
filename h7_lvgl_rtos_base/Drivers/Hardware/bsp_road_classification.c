#include "bsp_road_classification.h"

/*
 * 仅本实现文件接触 NanoEdge 原始 API。
 * 上层模块只使用 bsp_road_classification.h 中的自有类型，避免把库的 enum ABI
 * 和内部符号扩散到已验证的底盘、通信及界面代码。
 */
#include "NanoEdgeAI/NanoEdgeAI.h"

#include <string.h>

/*
 * NanoEdge 输入缓冲的布局必须是“时间优先”：
 * [第 0 帧 Ax, Ay, Az, Gx, Gy, Gz, 第 1 帧 Ax, ...]。
 * 原始训练 CSV 的列顺序正是 ACC_X_g、ACC_Y_g、ACC_Z_g、
 * GYRO_X_dps、GYRO_Y_dps、GYRO_Z_dps；不能改成六个轴各自连续存放。
 */
static float s_input_signal[NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER];

static BSP_RoadClassificationResult_t s_result;
static uint32_t s_last_collected_imu_tick;
static uint8_t s_collected_samples;
static uint8_t s_model_initialized;
static float s_latest_imu_sample[BSP_ROAD_CLASSIFICATION_AXIS_COUNT];

/*
 * 为 Keil Watch 保留的外部可见调试镜像。该变量从不参与任何控制计算，只在本
 * 文件的极短临界区内由 s_result 和最新 IMU 输入复制而来。
 */
volatile BSP_RoadClassificationDebug_t g_bsp_road_classification_debug;

/*
 * 结果读取者可能是其他 FreeRTOS 任务。推理过程不关中断，只有复制/发布结果
 * 的极短临界区关闭中断，避免读取到概率数组写入一半的快照。
 */
static uint32_t BSP_RoadClassification_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void BSP_RoadClassification_ExitCritical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/*
 * 在写入 s_result 的同一个临界区内刷新 Keil 调试镜像，保证观察者不会看到
 * "类别已更新但概率仍是上一帧" 这种长期不一致的状态。update_sequence 先变奇、
 * 结束后变偶：调试器恰好在写入过程中暂停时，可用该字段判断是否需要继续一次。
 */
static void BSP_RoadClassification_UpdateDebugSnapshot(void)
{
    const char *class_name;
    uint32_t i;

    g_bsp_road_classification_debug.update_sequence++;
    g_bsp_road_classification_debug.model_initialized = s_model_initialized;
    g_bsp_road_classification_debug.state = (uint8_t)s_result.state;
    g_bsp_road_classification_debug.class_id = (int8_t)s_result.class_id;
    g_bsp_road_classification_debug.collected_samples = s_result.collected_samples;
    g_bsp_road_classification_debug.has_result = s_result.has_result;
    g_bsp_road_classification_debug.last_neai_status = s_result.last_neai_status;
    g_bsp_road_classification_debug.last_imu_tick = s_result.last_imu_tick;
    g_bsp_road_classification_debug.last_classification_tick = s_result.last_classification_tick;

    for (i = 0U; i < BSP_ROAD_CLASSIFICATION_AXIS_COUNT; i++)
    {
        g_bsp_road_classification_debug.latest_imu_sample[i] = s_latest_imu_sample[i];
    }

    for (i = 0U; i < BSP_ROAD_CLASSIFICATION_CLASS_COUNT; i++)
    {
        g_bsp_road_classification_debug.probabilities[i] = s_result.probabilities[i];
    }

    class_name = BSP_RoadClassification_GetClassName(s_result.class_id);
    for (i = 0U; i < BSP_ROAD_CLASSIFICATION_CLASS_NAME_LENGTH; i++)
    {
        g_bsp_road_classification_debug.class_name[i] = class_name[i];
        if (class_name[i] == '\0')
        {
            break;
        }
    }
    /* 类别名较短，但仍明确补零，避免调试器显示上一次较长字符串的尾部残留。 */
    for (; i < BSP_ROAD_CLASSIFICATION_CLASS_NAME_LENGTH; i++)
    {
        g_bsp_road_classification_debug.class_name[i] = '\0';
    }

    g_bsp_road_classification_debug.update_sequence++;
}

static void BSP_RoadClassification_PublishWarmup(void)
{
    uint32_t primask = BSP_RoadClassification_EnterCritical();

    if (s_model_initialized != 0U)
    {
        s_result.state = BSP_ROAD_CLASSIFICATION_WARMUP;
    }
    s_result.class_id = BSP_ROAD_CLASS_UNKNOWN;
    s_result.probabilities[0] = 0.0f;
    s_result.probabilities[1] = 0.0f;
    s_result.probabilities[2] = 0.0f;
    s_result.probabilities[3] = 0.0f;
    s_result.collected_samples = s_collected_samples;
    s_result.has_result = 0U;

    BSP_RoadClassification_UpdateDebugSnapshot();

    BSP_RoadClassification_ExitCritical(primask);
}

/*
 * 采样间断后不保留半窗数据。重置只影响 AI 自己的输入缓存，不会反向影响
 * JY901S、底盘控制、PID 积分项或电机 PWM。
 */
static void BSP_RoadClassification_ResetWindow(void)
{
    s_collected_samples = 0U;
    BSP_RoadClassification_PublishWarmup();
}

static void BSP_RoadClassification_PublishError(int32_t neai_status)
{
    uint32_t primask = BSP_RoadClassification_EnterCritical();

    s_result.state = BSP_ROAD_CLASSIFICATION_ERROR;
    s_result.class_id = BSP_ROAD_CLASS_UNKNOWN;
    s_result.probabilities[0] = 0.0f;
    s_result.probabilities[1] = 0.0f;
    s_result.probabilities[2] = 0.0f;
    s_result.probabilities[3] = 0.0f;
    s_result.last_neai_status = neai_status;
    s_result.collected_samples = s_collected_samples;
    s_result.has_result = 0U;

    BSP_RoadClassification_UpdateDebugSnapshot();

    BSP_RoadClassification_ExitCritical(primask);
}

static void BSP_RoadClassification_PublishResult(int class_id,
                                                  const float *probabilities,
                                                  uint32_t classification_tick)
{
    uint32_t primask = BSP_RoadClassification_EnterCritical();

    s_result.state = BSP_ROAD_CLASSIFICATION_READY;
    s_result.class_id = (BSP_RoadClassificationClass_t)class_id;
    s_result.probabilities[0] = probabilities[0];
    s_result.probabilities[1] = probabilities[1];
    s_result.probabilities[2] = probabilities[2];
    s_result.probabilities[3] = probabilities[3];
    s_result.last_classification_tick = classification_tick;
    s_result.last_neai_status = (int32_t)NEAI_OK;
    s_result.collected_samples = 0U;
    s_result.has_result = 1U;

    BSP_RoadClassification_UpdateDebugSnapshot();

    BSP_RoadClassification_ExitCritical(primask);
}

uint8_t BSP_RoadClassification_Init(void)
{
    enum neai_state neai_state;

    memset(s_input_signal, 0, sizeof(s_input_signal));
    memset(&s_result, 0, sizeof(s_result));
    memset(s_latest_imu_sample, 0, sizeof(s_latest_imu_sample));
    s_result.state = BSP_ROAD_CLASSIFICATION_UNINITIALIZED;
    s_result.class_id = BSP_ROAD_CLASS_UNKNOWN;
    s_result.last_neai_status = (int32_t)NEAI_NOT_INITIALIZED;
    s_last_collected_imu_tick = 0U;
    s_collected_samples = 0U;
    s_model_initialized = 0U;

    /* 即使 NanoEdge 尚未初始化，Keil 也能立即看到清晰的“未初始化”状态。 */
    BSP_RoadClassification_UpdateDebugSnapshot();

    /* 加载 NanoEdge 预训练模型。失败时仅记录错误，调用者仍可继续运行底盘任务。 */
    neai_state = neai_classification_init();
    if (neai_state != NEAI_OK)
    {
        BSP_RoadClassification_PublishError((int32_t)neai_state);
        return 0U;
    }

    /*
     * 二次校验可防止日后误替换为其它 NanoEdge 导出包后发生缓冲区越界或误判。
     * 当前模型应固定为 32 样本、6 轴、4 类。
     */
    if ((neai_get_input_signal_size() != (int)BSP_ROAD_CLASSIFICATION_WINDOW_SAMPLES) ||
        (neai_get_axis_number() != (int)BSP_ROAD_CLASSIFICATION_AXIS_COUNT) ||
        (neai_get_number_of_classes() != (int)BSP_ROAD_CLASSIFICATION_CLASS_COUNT))
    {
        BSP_RoadClassification_PublishError((int32_t)NEAI_INVALID_PARAM);
        return 0U;
    }

    s_model_initialized = 1U;
    BSP_RoadClassification_PublishWarmup();
    return 1U;
}

void BSP_RoadClassification_Process(const JY901S_Data_t *imu_data)
{
    uint32_t now;
    uint32_t input_offset;
    float probabilities[NEAI_NUMBER_OF_CLASSES];
    int class_id = -1;
    enum neai_state neai_state;

    if ((s_model_initialized == 0U) || (imu_data == NULL))
    {
        return;
    }

    now = HAL_GetTick();

    /*
     * 调度频率为 10 ms，IMU 实际帧率约 9~10 Hz。若串口掉线或长期未更新，
     * 立即丢弃半窗并回到预热状态，避免跨越长空档拼出一个伪时序窗口。
     */
    if ((imu_data->online == 0U) || (imu_data->last_update_tick == 0U) ||
        ((uint32_t)(now - imu_data->last_update_tick) > BSP_ROAD_CLASSIFICATION_MAX_SAMPLE_GAP_MS))
    {
        if (s_collected_samples != 0U || s_result.state == BSP_ROAD_CLASSIFICATION_READY)
        {
            BSP_RoadClassification_ResetWindow();
        }
        return;
    }

    /* 同一 IMU 帧会被 100 Hz 底盘循环多次看到；重复帧绝不能重复塞入模型。 */
    if (imu_data->last_update_tick == s_last_collected_imu_tick)
    {
        return;
    }

    /* 两个新帧之间出现异常间隔时，先丢弃旧窗口，再把当前新帧作为第一帧。 */
    if ((s_last_collected_imu_tick != 0U) &&
        ((uint32_t)(imu_data->last_update_tick - s_last_collected_imu_tick) >
         BSP_ROAD_CLASSIFICATION_MAX_SAMPLE_GAP_MS))
    {
        BSP_RoadClassification_ResetWindow();
    }

    s_last_collected_imu_tick = imu_data->last_update_tick;
    input_offset = (uint32_t)s_collected_samples * BSP_ROAD_CLASSIFICATION_AXIS_COUNT;

    /*
     * 与原始训练 CSV 完全一致的单位和轴序：acc 已是 g，gyro 已是 deg/s。
     * 此处没有坐标变换、归一化或滤波，NanoEdge 导出库会执行训练时固化的特征提取。
     */
    s_input_signal[input_offset + 0U] = imu_data->acc[0];
    s_input_signal[input_offset + 1U] = imu_data->acc[1];
    s_input_signal[input_offset + 2U] = imu_data->acc[2];
    s_input_signal[input_offset + 3U] = imu_data->gyro[0];
    s_input_signal[input_offset + 4U] = imu_data->gyro[1];
    s_input_signal[input_offset + 5U] = imu_data->gyro[2];

    /* 单独保存最新一帧，供 Keil 验证实际进入 NanoEdge 的轴序和物理单位。 */
    s_latest_imu_sample[0] = imu_data->acc[0];
    s_latest_imu_sample[1] = imu_data->acc[1];
    s_latest_imu_sample[2] = imu_data->acc[2];
    s_latest_imu_sample[3] = imu_data->gyro[0];
    s_latest_imu_sample[4] = imu_data->gyro[1];
    s_latest_imu_sample[5] = imu_data->gyro[2];

    s_collected_samples++;
    {
        uint32_t primask = BSP_RoadClassification_EnterCritical();
        s_result.last_imu_tick = imu_data->last_update_tick;
        s_result.collected_samples = s_collected_samples;
        BSP_RoadClassification_UpdateDebugSnapshot();
        BSP_RoadClassification_ExitCritical(primask);
    }

    if (s_collected_samples < BSP_ROAD_CLASSIFICATION_WINDOW_SAMPLES)
    {
        return;
    }

    /*
     * 推理仅在已有 chassis_task 内同步执行。其输出只写入本 BSP 的结果快照；
     * 不调用任何电机/PID/模式切换函数，因此不会改变原有验证通过的底盘行为。
     */
    neai_state = neai_classification(s_input_signal, probabilities, &class_id);
    s_collected_samples = 0U;

    if ((neai_state == NEAI_OK) && (class_id >= 0) &&
        (class_id < (int)BSP_ROAD_CLASSIFICATION_CLASS_COUNT))
    {
        BSP_RoadClassification_PublishResult(class_id, probabilities, now);
    }
    else
    {
        /* 返回 NEAI_OK 却给出越界类别同样属于库输出异常，不能当作有效识别结果。 */
        BSP_RoadClassification_PublishError((int32_t)((neai_state == NEAI_OK) ?
                                                      NEAI_INVALID_PARAM : neai_state));
    }
}

void BSP_RoadClassification_GetResult(BSP_RoadClassificationResult_t *result)
{
    uint32_t primask;

    if (result == NULL)
    {
        return;
    }

    primask = BSP_RoadClassification_EnterCritical();
    *result = s_result;
    BSP_RoadClassification_ExitCritical(primask);
}

const char *BSP_RoadClassification_GetClassName(BSP_RoadClassificationClass_t class_id)
{
    switch (class_id)
    {
        case BSP_ROAD_CLASS_OUTDOOR_MARBLE:
            return "outdoor_marble";
        case BSP_ROAD_CLASS_OUTDOOR_CEMENT:
            return "outdoor_cement";
        case BSP_ROAD_CLASS_INDOOR:
            return "indoor";
        case BSP_ROAD_CLASS_ASPHALT:
            return "asphalt";
        default:
            return "unknown";
    }
}
