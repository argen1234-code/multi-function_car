#include "app_chassis_board.h"
#include "app_Navigation.h"
#include "app_remote_control.h"
#include "app_Voice_Recognition.h"
#include "usbd_cdc_if.h"
#include "cmsis_os.h"
#include "bsp_uart.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_GPS.h"
#include "bsp_bluetooth.h"
#include "bsp_JY901S.h"
#include "bsp_WonderEcho.h"
#include "usart.h"
#include <math.h>

/* ---- GPS 航点数量 ---- */
#define CHASSIS_GPS_ROUTE_COUNT  2U

/* ---- Jetson 超时 (ms), 超过此时间无有效帧视为离线 ---- */
#define JETSON_TIMEOUT_MS  500U

/* ---- 预设 GPS 巡航路线 ---- */
static GPS_Point_t chassis_gps_route[CHASSIS_GPS_ROUTE_COUNT] = {
    {26.449634, 106.650672},
 {26.449691, 106.650650}
		
		
};

/* 全局唯一的底盘实例 (外部不可直接访问, 仅通过指针传递) */
static chassis_move_t    chassis_move    = {0};
volatile int gui_req_mode = -1;
volatile CarMode_t chassis_current_mode_debug = CAR_MODE_GPS;
volatile int chassis_last_bt_req_debug = BT_MODE_REQ_NONE;
static uint8_t chassis_manual_indoor_mode = 0U;

/* Keil Watch 调试: 直接输入 chassis_debug 即可展开结构体 */
chassis_move_t *const chassis_debug = &chassis_move;


/* ============================================================
 *  内部: 停车 (归零全向移动目标速度)
 * ============================================================ */
static void chassis_stop(chassis_move_t *chassis)
{
    chassis->Vx_set = 0.0f;
    chassis->Vy_set = 0.0f;
    chassis->Wz_set = 0.0f;
}

/* ============================================================
 *  内部: 启动 GPS 循环巡航
 * ============================================================ */
static void chassis_start_gps_navigation(chassis_move_t *chassis)
{
    Navigation_Set_Route_Loop(&chassis->nav,
                              chassis_gps_route,
                              CHASSIS_GPS_ROUTE_COUNT);
}

static uint8_t chassis_is_jetson_online(chassis_move_t *chassis)
{
    if (chassis == NULL) return 0U;

    return (HAL_GetTick() - chassis->jetson_last_tick < JETSON_TIMEOUT_MS) ? 1U : 0U;
}

/* ============================================================
 *  公开: 切换车辆工作模式
 * ============================================================ */
void Chassis_SetMode(chassis_move_t *chassis, CarMode_t mode)
{
    if (chassis == NULL) return;

    Navigation_Stop(chassis);
    chassis_stop(chassis);
    chassis->mode = mode;
    chassis_current_mode_debug = mode;

    if (mode != CAR_MODE_VOICE)
    {
        App_Voice_Clear();
    }

    if (mode == CAR_MODE_GPS)
    {
        chassis_start_gps_navigation(chassis);
    }
}

/* ============================================================
 *  传感器数据刷新: 磁力计 + 编码器 + USB
 *  每周期在控制循环之前调用
 * ============================================================ */
void chassis_feedback_update(chassis_move_t *chassis)
{
    if (chassis == NULL) return;

    /* 磁力计 → chassis->imu.mag */
    QMC5883_GetAngles(&chassis->imu.mag);

    {
        JY901S_Data_t jy901s_data;
        if (JY901S_GetData(&jy901s_data))
        {
            chassis->imu.jy901s = jy901s_data;
            chassis->imu.ins.euler.roll = jy901s_data.angle[0];
            chassis->imu.ins.euler.pitch = jy901s_data.angle[1];
            chassis->imu.ins.euler.yaw = jy901s_data.angle[2];
        }
    }

    /* 编码器 → chassis->motor[i].speed */
    for (uint8_t i = 0; i < 4; i++)
    {
        chassis->motor[i].speed = Encoder_Rpm_Get(i);
    }

    /* USB Jetson 12字节帧 → chassis->cmd_vel */
    if (usb_rx_flag)
    {
        USB_ProcessRxData(UserRxBufferFS, (uint16_t)usb_rx_len);
        usb_rx_flag = 0;
    }
    chassis->cmd_vel = USB_GetCmdVel();
    if (chassis->cmd_vel.mode != 0)
    {
        chassis->jetson_last_tick = HAL_GetTick();
    }

    /* GPS 定位 → chassis->date_to_usb (NMEA → 十进制) */
    {
        PT_GNGGA pGGA = GetGNGGA();
        if (pGGA->qf >= 1 && pGGA->lat >= 1.0f)
        {
            double lat_deg = floor(pGGA->lat / 100.0)
                           + (pGGA->lat - floor(pGGA->lat / 100.0) * 100.0) / 60.0;
            double lon_deg = floor(pGGA->lon / 100.0)
                           + (pGGA->lon - floor(pGGA->lon / 100.0) * 100.0) / 60.0;
            chassis->date_to_usb.current_lat = (float)lat_deg;
            chassis->date_to_usb.current_lon = (float)lon_deg;
        }
    }

    /* 目标相对于车头的方位角 [0, 360) */
    {
        float tgt = chassis->nav.target_bearing;
        float yaw = chassis->imu.mag.yaw;
        float diff = tgt - yaw;
        while (diff < 0.0f)    diff += 360.0f;
        while (diff >= 360.0f) diff -= 360.0f;
        chassis->date_to_usb.heading_to_target_deg = diff;
    }
}

/* ============================================================
 *  初始化: 编码器 / 电机 / 蓝牙 / UART / PID
 *  在 FreeRTOS 任务中调用一次
 * ============================================================ */
static void chassis_init(chassis_move_t *chassis)
{
    const static double speed_pid_param[3] =
    {
        MOTOR_SPEED_PID_KP,
        MOTOR_SPEED_PID_KI,
        MOTOR_SPEED_PID_KD
    };

    Encoder_Init();
    Motor_Init();
    BT_Init();
    USB_Init();
    uart_init(&huart1, UART_DMA_ToIdle_RX);
    uart_init(&huart2, UART_DMA_ToIdle_RX);
    JY901S_Init();

    /* 遥控控制量默认值 */
    chassis->remote.bt_speed         = BT_REMOTE_SPEED;
    chassis->remote.bt_wz            = BT_REMOTE_WZ;
    chassis->remote.wechat_vx_scale  = WECHAT_VX_SCALE;
    chassis->remote.wechat_vz_scale  = WECHAT_VZ_SCALE;
    chassis->remote.wechat_speed_gain = WECHAT_SPEED_GAIN;
    chassis->remote.wechat_max_speed = WECHAT_MAX_SPEED;
    chassis->remote.wechat_max_wz    = WECHAT_MAX_WZ;
    chassis->remote.ros_vx_scale     = ROS_LINE_VX_SCALE;
    chassis->remote.ros_vz_scale     = ROS_LINE_VZ_SCALE;
    chassis->remote.ros_max_speed    = ROS_LINE_MAX_SPEED;
    chassis->remote.ros_max_wz       = ROS_LINE_MAX_WZ;

    /* 各模式控制增益默认值 */
    chassis->gain.gps    = CHASSIS_GAIN_GPS;
    chassis->gain.indoor = CHASSIS_GAIN_INDOOR;
    chassis->gain.remote = CHASSIS_GAIN_REMOTE;
    chassis->gain.line   = CHASSIS_GAIN_LINE;
    chassis->gain.voice  = CHASSIS_GAIN_VOICE;

    /* WonderEcho 上电后需要短暂稳定时间，再初始化 PB0/PB1 软件 I2C。
       初始化仍放在底盘任务内完成，不新增 FreeRTOS 任务，避免多处同时写底盘速度。 */
    osDelay(200);
    WonderEcho_I2C_Init();

    for (uint8_t i = 0; i < 4; i++)
    {
        PID_init(&chassis->motor[i].speed_pid, PID_POSITION,
                 speed_pid_param,
                 MOTOR_SPEED_PID_MAX_OUT,
                 MOTOR_SPEED_PID_MAX_IOUT);
    }
    for (uint8_t i = 0; i < 4; i++)
    {
        chassis->motor[i].speed_set = 0;
    }
}

/* ============================================================
 *  步骤4: 底盘核心控制循环
 *    Vx/Vy/Wz (已由步骤3写入) → 运动学分解 → PID 计算
 *    PID 输出由步骤5 chassis_send_cmd 统一发送到电机
 * ============================================================ */
void chassis_control_loop(chassis_move_t *chassis)
{
    if (chassis == NULL) return;

    /* 1. 全向运动学分解: Vx/Vy/Wz → 4路电机目标转速 */
    chassis->motor[0].speed_set =  chassis->Vx_set + chassis->Vy_set + chassis->Wz_set;
    chassis->motor[1].speed_set =  chassis->Vx_set - chassis->Vy_set - chassis->Wz_set;
    chassis->motor[2].speed_set =  chassis->Vx_set - chassis->Vy_set + chassis->Wz_set;
    chassis->motor[3].speed_set =  chassis->Vx_set + chassis->Vy_set - chassis->Wz_set;

    /* 2. PID 速度闭环 (结果存入 motor[i].speed_pid.Out) */
    for (uint8_t i = 0; i < 4; i++)
    {
        PID_Calculate(&chassis->motor[i].speed_pid,
                      chassis->motor[i].speed,
                      chassis->motor[i].speed_set);
    }
}

/* ============================================================
 *  步骤1: 底盘控制模式切换
 *  蓝牙模式切换 (仅 GPS / 室内遥控)
 * ============================================================ */
void chassis_mode_change(chassis_move_t *chassis)
{
    BT_ModeReq_t req;
    CarMode_t requested_mode;
    uint8_t jetson_online;
    uint8_t jetson_mode;

    if (chassis == NULL) return;

    if (gui_req_mode >= 0 && gui_req_mode <= 4) {
        requested_mode = (CarMode_t)gui_req_mode;
        chassis_manual_indoor_mode = (requested_mode == CAR_MODE_INDOOR) ? 1U : 0U;
        Chassis_SetMode(chassis, requested_mode);
        gui_req_mode = -1;
        return;
    }

    req = BT_GetAndClearModeReq();
    if (req != BT_MODE_REQ_NONE)
    {
        chassis_last_bt_req_debug = req;
    }

    switch (req)
    {
        case BT_MODE_REQ_GPS:
            chassis_manual_indoor_mode = 0U;
            Chassis_SetMode(chassis, CAR_MODE_GPS);
            return;

        case BT_MODE_REQ_INDOOR:
            chassis_manual_indoor_mode = 1U;
            Chassis_SetMode(chassis, CAR_MODE_INDOOR);
            return;

        default:
            break;
    }

    if (BT_IsActive())
    {
        chassis_manual_indoor_mode = 1U;
        if (chassis->mode != CAR_MODE_INDOOR)
        {
            Chassis_SetMode(chassis, CAR_MODE_INDOOR);
        }
        return;
    }

    if (chassis_manual_indoor_mode)
    {
        if (chassis->mode != CAR_MODE_INDOOR)
        {
            Chassis_SetMode(chassis, CAR_MODE_INDOOR);
        }
        return;
    }

    if (App_Voice_IsActive())
    {
        if (chassis->mode != CAR_MODE_VOICE)
        {
            Chassis_SetMode(chassis, CAR_MODE_VOICE);
        }
        return;
    }

    jetson_online = chassis_is_jetson_online(chassis);
    jetson_mode = chassis->cmd_vel.mode;

    if (jetson_online)
    {
        if (jetson_mode == JETSON_MODE_REMOTE)
        {
            if (chassis->mode != CAR_MODE_REMOTE)
            {
                Chassis_SetMode(chassis, CAR_MODE_REMOTE);
            }
            return;
        }

        if (jetson_mode == JETSON_MODE_LINE)
        {
            if (chassis->mode != CAR_MODE_LINE)
            {
                Chassis_SetMode(chassis, CAR_MODE_LINE);
            }
            return;
        }

        if (jetson_mode == JETSON_MODE_GPS)
        {
            if (chassis->mode != CAR_MODE_GPS)
            {
                Chassis_SetMode(chassis, CAR_MODE_GPS);
            }
            return;
        }
    }

    if (chassis->mode == CAR_MODE_REMOTE || chassis->mode == CAR_MODE_LINE)
    {
        Chassis_SetMode(chassis, CAR_MODE_GPS);
    }
}

/* ============================================================
 *  内部: 各模式控制增益放大
 *  在 chassis_set_control 末尾调用，对 Vx/Vy/Wz 统一乘增益
 * ============================================================ */
static void chassis_apply_gain(chassis_move_t *chassis)
{
    float gain = 1.0f;

    if (chassis == NULL) return;

    switch (chassis->mode)
    {
        case CAR_MODE_GPS:    gain = chassis->gain.gps;    break;
        case CAR_MODE_INDOOR: gain = chassis->gain.indoor; break;
        case CAR_MODE_REMOTE: gain = chassis->gain.remote; break;
        case CAR_MODE_LINE:   gain = chassis->gain.line;   break;
        case CAR_MODE_VOICE:  gain = chassis->gain.voice;  break;
        default: break;
    }

    chassis->Vx_set *= gain;
    chassis->Vy_set *= gain;
    chassis->Wz_set *= gain;
}

/* ============================================================
 *  步骤3: 底盘控制量设置 (优先级: 蓝牙 > 微信 > ROS室内 > GPS)
 * ============================================================ */
void chassis_set_control(chassis_move_t *chassis)
{
    uint8_t jetson_online;
    uint8_t jetson_mode;

    if (chassis == NULL) return;

    jetson_online = chassis_is_jetson_online(chassis);
    jetson_mode = chassis->cmd_vel.mode;

    switch (chassis->mode)
    {
        case CAR_MODE_INDOOR:
            if (BT_IsActive())
            {
                Remote_Control_Update(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_REMOTE:
            if (jetson_online && jetson_mode == JETSON_MODE_REMOTE)
            {
                Remote_WeChat_Update(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_LINE:
            if (jetson_online && jetson_mode == JETSON_MODE_LINE)
            {
                Remote_ROS_Update(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_GPS:
            if (jetson_online && jetson_mode == JETSON_MODE_GPS)
            {
                Navigation_Update_Loop_Fusion(chassis);
            }
            else
            {
                Navigation_Update_Loop(chassis);
            }
            break;

        case CAR_MODE_VOICE:
            if (App_Voice_IsActive())
            {
                App_Voice_ApplyControl(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        default:
            chassis_stop(chassis);
            break;
    }

    chassis_apply_gain(chassis);
}

/* ============================================================
 *  步骤5: 底盘控制指令发送
 *    PID 输出 → 电机PWM
 *    遥测数据 → USB 回传 Jetson
 * ============================================================ */
static void chassis_send_bt_ack(void);

void chassis_send_cmd(chassis_move_t *chassis)
{
    if (chassis == NULL) return;

    chassis_send_bt_ack();

    for (uint8_t i = 0; i < 4; i++)
    {
        Motor_SetPWM((int16_t)chassis->motor[i].speed_pid.Out, i);
    }

    USB_SendTelemetry(chassis->date_to_usb.heading_to_target_deg,
                      chassis->date_to_usb.current_lat,
                      chassis->date_to_usb.current_lon);
}

static void chassis_send_bt_ack(void)
{
    uint8_t ack_count = BT_GetAndClearAckCount();
    static const uint8_t ack_msg[] = {'o', 'k', '\r', '\n'};

    while (ack_count > 0U)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)ack_msg, sizeof(ack_msg), 10U);
        ack_count--;
    }
}


/* ============================================================
 *  FreeRTOS 任务入口
 * ============================================================ */
void chassis_task(void *pvParameters)
{
    uint32_t voice_tick = 0U;

    /* -- 一次性初始化 -- */ 
    chassis_init(&chassis_move);
	QMC5883_Init();
	GPS_Init();

    /* -- 默认启动 GPS 循环巡航 -- */
    chassis_start_gps_navigation(&chassis_move);

    /* -- 主循环 (100Hz) -- */
    while (1)
    {
        chassis_feedback_update(&chassis_move);   /* 传感器 + USB 数据刷新 */
        if (HAL_GetTick() - voice_tick >= 50U)
        {
            voice_tick = HAL_GetTick();
            App_Voice_Recognition_Update(&chassis_move);
        }
        
        chassis_mode_change(&chassis_move);       /* 模式切换 */
		    chassis_set_control(&chassis_move);       /* 控制量设置 (优先级调度) */
        chassis_control_loop(&chassis_move);      /* 运动学 + PID */
        chassis_send_cmd(&chassis_move);          /* 电机输出 + USB遥测 */
        osDelay(10);
    }
}
