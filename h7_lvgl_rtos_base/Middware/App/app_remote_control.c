#include "app_remote_control.h"
#include "bsp_bluetooth.h"

#define BT_REMOTE_SPEED  20.0f

/* 微信遥控 cmd_vel 缩放 (m/s / rad/s → 内部 RPM) */
#define WECHAT_VX_SCALE  100.0f
#define WECHAT_VZ_SCALE   30.0f
#define WECHAT_MAX_SPEED  50.0f
#define WECHAT_MAX_WZ     25.0f

/* ============================================================
 *  蓝牙遥控 (最高优先级)
 *  方向键 → Vx_set / Wz_set
 * ============================================================ */
void Remote_Control_Update(chassis_move_t *chassis)
{
    switch (BT_GetMotion())
    {
        case BT_MOTION_FORWARD:
            chassis->Vx_set =  BT_REMOTE_SPEED;
            chassis->Vy_set =  0.0f;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_BACKWARD:
            chassis->Vx_set = -BT_REMOTE_SPEED;
            chassis->Vy_set =  0.0f;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_LEFT:
            chassis->Vx_set =  0.0f;
            chassis->Vy_set =  BT_REMOTE_SPEED;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_RIGHT:
            chassis->Vx_set =  0.0f;
            chassis->Vy_set = -BT_REMOTE_SPEED;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_STOP:
        default:
            chassis->Vx_set = 0.0f;
            chassis->Vy_set = 0.0f;
            chassis->Wz_set = 0.0f;
            break;
    }
}

/* ============================================================
 *  微信小程序遥控 (第二优先级)
 *  Jetson 转发来的 cmd_vel, mode=2
 *  vx → 前向速度, vz → 旋转速度
 * ============================================================ */
void Remote_WeChat_Update(chassis_move_t *chassis)
{
    float out_vx = chassis->cmd_vel.vx * WECHAT_VX_SCALE;
    float out_wz = chassis->cmd_vel.vz * WECHAT_VZ_SCALE;

    if (out_vx >  WECHAT_MAX_SPEED) out_vx =  WECHAT_MAX_SPEED;
    if (out_vx < -WECHAT_MAX_SPEED) out_vx = -WECHAT_MAX_SPEED;
    if (out_wz >  WECHAT_MAX_WZ)    out_wz =  WECHAT_MAX_WZ;
    if (out_wz < -WECHAT_MAX_WZ)    out_wz = -WECHAT_MAX_WZ;

    chassis->Vx_set = out_vx;
    chassis->Vy_set = 0.0f;
    chassis->Wz_set = out_wz;
}

/* ============================================================
 *  室内 ROS 自主导航 (Jetson mode=3)
 *  纯 ROS cmd_vel 控制, 无 GPS 参与
 *  vx → 前向速度, vz → 旋转速度
 * ============================================================ */
#define ROS_LINE_VX_SCALE  100.0f
#define ROS_LINE_VZ_SCALE   30.0f
#define ROS_LINE_MAX_SPEED  50.0f
#define ROS_LINE_MAX_WZ     25.0f

void Remote_ROS_Update(chassis_move_t *chassis)
{
    float out_vx = chassis->cmd_vel.vx * ROS_LINE_VX_SCALE;
    float out_wz = chassis->cmd_vel.vz * ROS_LINE_VZ_SCALE;

    if (out_vx >  ROS_LINE_MAX_SPEED) out_vx =  ROS_LINE_MAX_SPEED;
    if (out_vx < -ROS_LINE_MAX_SPEED) out_vx = -ROS_LINE_MAX_SPEED;
    if (out_wz >  ROS_LINE_MAX_WZ)    out_wz =  ROS_LINE_MAX_WZ;
    if (out_wz < -ROS_LINE_MAX_WZ)    out_wz = -ROS_LINE_MAX_WZ;

    chassis->Vx_set = out_vx;
    chassis->Vy_set = 0.0f;
    chassis->Wz_set = out_wz;
}
