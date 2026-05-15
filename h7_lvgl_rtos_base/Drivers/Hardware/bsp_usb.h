#ifndef BSP_USB_H
#define BSP_USB_H

#include "main.h"

/* Jetson → STM32 帧模式 */
typedef enum {
    JETSON_MODE_GPS    = 1,   /* GPS + ROS 融合导航 */
    JETSON_MODE_REMOTE = 2,   /* 微信小程序遥控 */
    JETSON_MODE_LINE   = 3    /* 巡线 (暂未实现) */
} JetsonMode_t;

/* Jetson 下发的 12 字节帧解析结果 */
typedef struct {
    uint8_t mode;    /* 1=GPS, 2=REMOTE, 3=LINE */
    float   vx;      /* 线速度 (m/s), 正值前进 */
    float   vz;      /* 角速度 (rad/s), 正值左转 (Z轴向上) */
} cmd_vel_t;

void       USB_Init(void);
void       USB_ProcessRxData(uint8_t *pBuf, uint16_t Size);
cmd_vel_t  USB_GetCmdVel(void);
void       USB_SendTelemetry(float heading_to_target_deg);

#endif
