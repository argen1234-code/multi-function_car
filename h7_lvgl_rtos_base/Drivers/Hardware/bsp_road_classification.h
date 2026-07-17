#ifndef BSP_ROAD_CLASSIFICATION_H
#define BSP_ROAD_CLASSIFICATION_H

#include "bsp_JY901S.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NanoEdge 导出模型的固定输入维度：连续 32 帧、每帧 6 个 IMU 通道。 */
#define BSP_ROAD_CLASSIFICATION_WINDOW_SAMPLES   32U
#define BSP_ROAD_CLASSIFICATION_AXIS_COUNT       6U
#define BSP_ROAD_CLASSIFICATION_CLASS_COUNT      4U
#define BSP_ROAD_CLASSIFICATION_CLASS_NAME_LENGTH 20U

/*
 * 训练数据约每 108~110 ms 产生一帧。超过此间隔通常说明 IMU 串口数据中断，
 * 旧窗口不再是连续时序信号，必须重新积累，不能拿不连续数据推理。
 */
#define BSP_ROAD_CLASSIFICATION_MAX_SAMPLE_GAP_MS 250U

/* 导出模型的类别 ID，顺序与 NanoEdgeAI.h 中模型内部类别严格一致。 */
typedef enum
{
    BSP_ROAD_CLASS_OUTDOOR_MARBLE = 0,
    BSP_ROAD_CLASS_OUTDOOR_CEMENT = 1,
    BSP_ROAD_CLASS_INDOOR         = 2,
    BSP_ROAD_CLASS_ASPHALT        = 3,
    BSP_ROAD_CLASS_UNKNOWN        = -1
} BSP_RoadClassificationClass_t;

/*
 * BSP 对上层公开的运行状态。NanoEdge 的 enum 不直接暴露到工程其他模块，
 * 从而将导出库的 ABI 约束限制在本 BSP 的实现文件内。
 */
typedef enum
{
    BSP_ROAD_CLASSIFICATION_UNINITIALIZED = 0,
    BSP_ROAD_CLASSIFICATION_WARMUP,
    BSP_ROAD_CLASSIFICATION_READY,
    BSP_ROAD_CLASSIFICATION_ERROR
} BSP_RoadClassificationState_t;

/*
 * 只读识别结果快照。
 * 此结构不包含任何 PID、速度或底盘模式字段；道路识别在本次移植中仅提供感知结果。
 */
typedef struct
{
    BSP_RoadClassificationState_t state;
    BSP_RoadClassificationClass_t class_id;
    float probabilities[BSP_ROAD_CLASSIFICATION_CLASS_COUNT];
    uint32_t last_classification_tick;
    uint32_t last_imu_tick;
    uint8_t collected_samples;
    uint8_t has_result;
    int32_t last_neai_status;
} BSP_RoadClassificationResult_t;

/*
 * Keil 在线调试专用的全局快照。
 *
 * 变量 g_bsp_road_classification_debug 会由 BSP 在每次状态变化、新 IMU 帧到达
 * 或推理完成时自动刷新。它只镜像感知层数据，绝不被底盘控制逻辑读取，因此即使
 * 调试时观察它，也不会改变 PID、速度、模式或电机 PWM。
 *
 * 成员刻意使用固定宽度整数而非 enum，避免 NanoEdge BSP 文件独立的枚举大小配置
 * 影响其他原有模块或 Keil Watch 对结构体布局的解析。
 */
typedef struct BSP_RoadClassificationDebug_s
{
    /* 偶数表示一次完整快照；若暂停时为奇数，单步/继续一次后再观察即可。 */
    uint32_t update_sequence;

    uint8_t model_initialized;  /* 1：NanoEdge 初始化和模型维度校验均成功。 */
    uint8_t state;              /* 0 未初始化，1 预热，2 READY，3 错误。 */
    int8_t class_id;            /* 0 大理石，1 水泥，2 室内，3 沥青，-1 未知。 */
    uint8_t collected_samples;  /* 已收集的新 IMU 帧数；满 32 帧即执行一次推理。 */
    uint8_t has_result;         /* 1：当前快照包含一次有效分类结果；预热/错误时为 0。 */
    uint8_t reserved[3];        /* 预留并保持 32 位字段对齐，调试时无需关注。 */

    int32_t last_neai_status;   /* NanoEdge 最近返回码；0 通常代表 NEAI_OK。 */
    uint32_t last_imu_tick;     /* 最近一帧进入模型窗口的 HAL tick（ms）。 */
    uint32_t last_classification_tick; /* 最近一次推理完成的 HAL tick（ms）。 */

    /* 最新写入模型的原始一帧，顺序和单位固定为 Ax/Ay/Az(g)、Gx/Gy/Gz(deg/s)。 */
    float latest_imu_sample[BSP_ROAD_CLASSIFICATION_AXIS_COUNT];

    /* 下标 0~3 依次是 outdoor_marble、outdoor_cement、indoor、asphalt。 */
    float probabilities[BSP_ROAD_CLASSIFICATION_CLASS_COUNT];

    /* 当前类别的英文标签；未知或预热/错误状态时为 "unknown"。 */
    char class_name[BSP_ROAD_CLASSIFICATION_CLASS_NAME_LENGTH];
} BSP_RoadClassificationDebug_t;

/*
 * 可直接加入 Keil Watch 的调试变量：g_bsp_road_classification_debug。
 * 声明为 volatile 以保证变量在 Release/优化构建中仍会保留并真实反映 RAM 内容；
 * 业务代码不得向它写入，也不应以它作为控制依据。
 */
extern volatile BSP_RoadClassificationDebug_t g_bsp_road_classification_debug;

/* 初始化 NanoEdge 分类库，并校验运行时导出维度是否与 BSP 约定一致。 */
uint8_t BSP_RoadClassification_Init(void);

/*
 * 由现有 chassis_task 每个控制周期调用。
 * 函数只读取 IMU 数据和更新自身缓存，绝不修改电机、PID、速度设定或运行模式。
 */
void BSP_RoadClassification_Process(const JY901S_Data_t *imu_data);

/* 原子复制当前状态，供后续调试或界面模块按需读取。 */
void BSP_RoadClassification_GetResult(BSP_RoadClassificationResult_t *result);

/* 将类别 ID 转换为与 NanoEdge 导出模型一致的稳定英文标签。 */
const char *BSP_RoadClassification_GetClassName(BSP_RoadClassificationClass_t class_id);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ROAD_CLASSIFICATION_H */
