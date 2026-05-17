#include "app_chassis_board.h"
#include "app_Navigation.h"
#include "app_remote_control.h"
#include "usbd_cdc_if.h"
#include "cmsis_os.h"
#include "bsp_uart.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_GPS.h"
#include "bsp_bluetooth.h"
#include "usart.h"
#include <math.h>

/* ---- GPS 航点数量 ---- */
#define CHASSIS_GPS_ROUTE_COUNT  2U

/* ---- Jetson 超时 (ms), 超过此时间无有效帧视为离线 ---- */
#define JETSON_TIMEOUT_MS  500U

/* ---- 预设 GPS 巡航路线 ---- */
static GPS_Point_t chassis_gps_route[CHASSIS_GPS_ROUTE_COUNT] = {
    {26.44970054683, 106.6505648425},
 {26.4496634045, 106.650618892}
		
		
		
};

/* 全局唯一的底盘实例 (外部不可直接访问, 仅通过指针传递) */
static chassis_move_t    chassis_move    = {0};

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

/* ============================================================
 *  公开: 切换车辆工作模式
 * ============================================================ */
void Chassis_SetMode(chassis_move_t *chassis, CarMode_t mode)
{
    if (chassis == NULL) return;

    Navigation_Stop(chassis);
    chassis_stop(chassis);
    chassis->mode = mode;

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
    BT_ModeReq_t req = BT_GetAndClearModeReq();

    switch (req)
    {
        case BT_MODE_REQ_GPS:
            Chassis_SetMode(chassis, CAR_MODE_GPS);
            break;

        case BT_MODE_REQ_INDOOR:
            Chassis_SetMode(chassis, CAR_MODE_INDOOR);
            break;

        default:
            break;
    }
}

/* ============================================================
 *  步骤3: 底盘控制量设置 (优先级: 蓝牙 > 微信 > ROS室内 > GPS)
 * ============================================================ */
void chassis_set_control(chassis_move_t *chassis)
{
    uint8_t jetson_online =
        (HAL_GetTick() - chassis->jetson_last_tick < JETSON_TIMEOUT_MS);
    uint8_t jetson_mode = chassis->cmd_vel.mode;

    /* 1. 蓝牙遥控 — 最高优先级 */
    if (BT_IsActive())
    {
        chassis->mode = CAR_MODE_INDOOR;
        Remote_Control_Update(chassis);
    }
    /* 2. 微信小程序遥控 (Jetson mode=2) */
    else if (jetson_online && jetson_mode == JETSON_MODE_REMOTE)
    {
        chassis->mode = CAR_MODE_REMOTE;
        Remote_WeChat_Update(chassis);
    }
    /* 3. 室内 ROS 自主导航 (Jetson mode=3, 纯cmd_vel) */
    else if (jetson_online && jetson_mode == JETSON_MODE_LINE)
    {
        chassis->mode = CAR_MODE_LINE;
        Remote_ROS_Update(chassis);
    }
    /* 4. GPS + ROS 融合导航 (Jetson mode=1) */
    else if (jetson_online && jetson_mode == JETSON_MODE_GPS)
    {
        chassis->mode = CAR_MODE_GPS;
        Navigation_Update_Loop_Fusion(chassis);
    }
    /* 5. 默认: 纯 GPS 导航 */
    else
    {
        chassis->mode = CAR_MODE_GPS;
        Navigation_Update_Loop(chassis);
    }

   
}

/* ============================================================
 *  步骤5: 底盘控制指令发送
 *    PID 输出 → 电机PWM
 *    遥测数据 → USB 回传 Jetson
 * ============================================================ */
void chassis_send_cmd(chassis_move_t *chassis)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        Motor_SetPWM((int16_t)chassis->motor[i].speed_pid.Out, i);
    }

    USB_SendTelemetry(chassis->date_to_usb.heading_to_target_deg,
                      chassis->date_to_usb.current_lat,
                      chassis->date_to_usb.current_lon);
}


/* ============================================================
 *  FreeRTOS 任务入口
 * ============================================================ */
void chassis_task(void *pvParameters)
{
    /* -- 一次性初始化 -- */
    chassis_init(&chassis_move);
	  QMC5883_Init();
	  GPS_Init();

    /* -- 默认启动 GPS 循环巡航 -- */
    chassis_start_gps_navigation(&chassis_move);

    /* -- 主循环 (100Hz) -- */
    while (1)
    {
        chassis_mode_change(&chassis_move);       /* 蓝牙模式切换请求 */
        chassis_feedback_update(&chassis_move);   /* 传感器 + USB 数据刷新 */
        chassis_set_control(&chassis_move);       /* 控制量设置 (优先级调度) */
        chassis_control_loop(&chassis_move);      /* 运动学 + PID */
        chassis_send_cmd(&chassis_move);          /* 电机输出 + USB遥测 */
        osDelay(10);
    }
}
