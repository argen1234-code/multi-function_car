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

/* ---- GPS 航点数量 ---- */
#define CHASSIS_GPS_ROUTE_COUNT  3U

/* ---- 预设 GPS 巡航路线 ---- */
static GPS_Point_t chassis_gps_route[CHASSIS_GPS_ROUTE_COUNT] = {
    {26.449591, 106.650651},
    {26.449698, 106.650615},
    {26.449820, 106.650896}
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
    if (mode != CAR_MODE_GPS && mode != CAR_MODE_INDOOR) return;

    Navigation_Stop(chassis);
    chassis_stop(chassis);
    chassis->mode = mode;

    if (mode == CAR_MODE_GPS)
    {
        chassis_start_gps_navigation(chassis);
    }
}

/* ============================================================
 *  传感器数据刷新: 磁力计 + 编码器
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

    /* USB /cmd_vel → chassis->cmd_vel */
    if (usb_rx_flag)
    {
        USB_ProcessRxData(UserRxBufferFS, (uint16_t)usb_rx_len);
        usb_rx_flag = 0;
    }
    chassis->cmd_vel = USB_GetCmdVel();
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
 *  步骤1: 底盘控制模式切换 / 数据过渡
 *  处理蓝牙下发的模式请求, 执行 GPS↔室内 模式切换
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

    /* TODO: 模式切换时的控制量平滑过渡 / 数据缓存等 */
}

/* ============================================================
 *  步骤3: 底盘控制量设置
 *  根据当前模式写入 Vx/Vy/Wz 目标值
 *    GPS模式  → Navigation_Update_Loop 导航解算
 *    室内模式 → Remote_Control_Update 蓝牙遥控
 * ============================================================ */
void chassis_set_control(chassis_move_t *chassis)
{
    if (chassis->mode == CAR_MODE_GPS)
    {
        Navigation_Update_Loop(chassis);     /* GPS 导航  写入 Vx/Vy/Wz */
    }
    else
    {
        Remote_Control_Update(chassis);      /* 蓝牙遥控  写入 Vx/Vy/Wz */
    }
}

/* ============================================================
 *  步骤5: 底盘控制指令发送
 *    PID 输出 → 电机PWM / 舵机 / CAN 等执行器
 * ============================================================ */
void chassis_send_cmd(chassis_move_t *chassis)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        Motor_SetPWM((int16_t)chassis->motor[i].speed_pid.Out, i);
    }

    /* TODO: 舵机 / CAN / 遥测数据上报 */
}

/* ============================================================
 *  FreeRTOS 任务入口
 * ============================================================ */
void chassis_task(void *pvParameters)
{
    /* -- 一次性初始化 -- */
    chassis_init(&chassis_move);
//    QMC5883_Init();
    GPS_Init();

    /* -- 默认启动 GPS 循环巡航 -- */
    chassis_start_gps_navigation(&chassis_move);

    /* -- 主循环 (100Hz) -- */
    while (1)
    {
        chassis_mode_change(&chassis_move);        /* 底盘控制模式切换 / 数据过渡 */
        chassis_feedback_update(&chassis_move);   /* 传感器数据刷新 */
        chassis_set_control(&chassis_move);      /* 底盘控制量设置 */
        chassis_control_loop(&chassis_move);      /* 底盘核心控制循环 */
        chassis_send_cmd(&chassis_move);                        /* 底盘控制指令发送 */
        osDelay(10);
    }
}
