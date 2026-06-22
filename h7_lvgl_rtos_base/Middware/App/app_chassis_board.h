#ifndef APP_CHASSIS_H
#define APP_CHASSIS_H

#include "main.h"
#include "pid.h"
#include "bsp_QMC5883.h"
#include "bsp_usb.h"
#include "app_Navigation.h"
#include "bsp_JY901S.h"

#define chassis_board_task 1

/* ---- 底盘电机速度PID参数 ---- */
#define MOTOR_SPEED_PID_KP       1.05f
#define MOTOR_SPEED_PID_KI       0.1f
#define MOTOR_SPEED_PID_KD       0.1f
//#define MOTOR_SPEED_PID_KP       0.0f
//#define MOTOR_SPEED_PID_KI       0.0f
//#define MOTOR_SPEED_PID_KD       0.0f



//#define MOTOR_SPEED_PID_KP       1.60f
//#define MOTOR_SPEED_PID_KI       0.15f
//#define MOTOR_SPEED_PID_KD       0.10f
//#define MOTOR_SPEED_PID_KP       1.22f
//#define MOTOR_SPEED_PID_KI       0.09f
//#define MOTOR_SPEED_PID_KD       0.08f
#define MOTOR_SPEED_PID_MAX_OUT  120.0f
#define MOTOR_SPEED_PID_MAX_IOUT 40.0f

/* ============================================================
 *  IMU 姿态数据结构
 * ============================================================ */

/* 四元数 (原始IMU输出) */
typedef struct {
    float q0, q1, q2, q3;
} Quaternion_t;

/* INS 解算欧拉角 */
typedef struct {
    float yaw;      /* 偏航角  0~360° */
    float pitch;    /* 俯仰角 */
    float roll;     /* 横滚角 */
} INS_Euler_t;

/* INS 完整数据: 原始四元数 + 解算后的欧拉角 */
typedef struct {
    Quaternion_t quaternion;   /* 原始4元数 */
    INS_Euler_t  euler;        /* 解算出的欧拉角 */
} INS_Data_t;

/* IMU 子系统: 磁力计 + INS 姿态 */
typedef struct {
    EulerAngles mag;           /* QMC5883 磁力计欧拉角 */
    INS_Data_t  ins;           /* IMU/INS 姿态数据 */
    JY901S_Data_t jy901s;      /* JY901S 九轴数据 */
} IMU_Data_t;

/* ============================================================
 *  底盘电机 + 速度PID
 * ============================================================ */

typedef struct {
    double speed;              /* 当前转速     (rpm, 编码器反馈) */
    double speed_set;          /* 目标转速     (rpm) */
    double angle;              /* 当前角度 */
    double angle_set;          /* 目标角度 */
    PID_t  speed_pid;          /* 速度环 PID 控制器 */
} chassis_motor_t;

/* ============================================================
 *  车辆工作模式
 * ============================================================ */

typedef enum {
    CAR_MODE_GPS = 0,          /* GPS 导航 (默认纯GPS, Jetson在线时融合ROS) */
    CAR_MODE_REMOTE,           /* 微信小程序遥控 (Jetson转发) */
    CAR_MODE_LINE,             /* 室内 ROS 自主导航 (Jetson mode=3) */
    CAR_MODE_INDOOR,           /* 蓝牙遥控 */
    CAR_MODE_VOICE             /* WonderEcho 语音识别控制 */
} CarMode_t;

/* ============================================================
 *  USB 回传数据 (STM32 → Jetson)
 * ============================================================ */

typedef struct {
    float heading_to_target_deg;   /* 目标相对车头方位: 0°=正前, 90°=右侧, 180°=后方, 270°=左侧, 顺时针为正 [0,360) */
    float current_lat;             /* 当前纬度 (十进制) */
    float current_lon;             /* 当前经度 (十进制) */
} date_to_usb_t;

/* ============================================================
 *  底盘全向移动总控制结构体
 *  (实例在 app_chassis_board.c, 外部通过指针传递)
 * ============================================================ */

typedef struct chassis_move_s {
    /* ---- IMU 传感器 ---- */
    IMU_Data_t          imu;       /* 磁力计 + INS 姿态 */

    /* ---- GPS 导航 ---- */
    Navigation_State_t  nav;       /* 导航控制器状态 */

    /* ---- 工作模式 ---- */
    CarMode_t           mode;      /* 当前模式 */

    /* ---- USB 下发的 Jetson 数据 ---- */
    cmd_vel_t           cmd_vel;        /* 最新帧解析结果 (mode + vx + vz) */
    uint32_t            jetson_last_tick; /* 最后一次收到有效帧的时间戳 */

    /* ---- USB 回传数据 ---- */
    date_to_usb_t       date_to_usb; /* STM32 → Jetson */

    /* ---- 全向移动目标速度 (运动学分解前的合速度) ---- */
    float Vx_set;                  /* X 轴目标速度  (纵向) */
    float Vy_set;                  /* Y 轴目标速度  (横向) */
    float Wz_set;                  /* Z 轴目标角速度 (旋转) */

    /* ---- 4 路电机 [FL:前左, FR:前右, RL:后左, RR:后右] ---- */
    chassis_motor_t     motor[4];

} chassis_move_t;

/* ============================================================
 *  控制量结构体 (云台 / 舵机 / 辅助控制等)
 * ============================================================ */

typedef struct {
    float aux1;          /* 预留控制量 1 */
    float aux2;          /* 预留控制量 2 */
    float aux3;          /* 预留控制量 3 */
} chassis_control_t;

/* ============================================================
 *  外部接口
 * ============================================================ */

extern void chassis_task(void *pvParameters);
extern void Chassis_SetMode(chassis_move_t *chassis, CarMode_t mode);

extern volatile int gui_req_mode;   /* GUI mode request (-1=none, 0..4=CarMode_t) */

/* 主循环 5 步骤 (定义于此, 便于外部模块替换实现) */
extern void chassis_mode_change(chassis_move_t *chassis);
extern void chassis_feedback_update(chassis_move_t *chassis);
extern void chassis_set_control(chassis_move_t *chassis);
extern void chassis_control_loop(chassis_move_t *chassis);
extern void chassis_send_cmd(chassis_move_t *chassis);

#endif
