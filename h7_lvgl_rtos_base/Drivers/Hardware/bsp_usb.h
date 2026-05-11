#ifndef BSP_USB_H
#define BSP_USB_H

#include "main.h"

/* Jetson 下发的 /cmd_vel 速度指令 */ //范围绝对值都小于1
typedef struct {
    float vx;      /* 线速度 (m/s), 正值前进 */
    float vz;      /* 角速度 (rad/s), 正值左转 (Z轴向上) */
} cmd_vel_t;

void       USB_Init(void);
void       USB_ProcessRxData(uint8_t *pBuf, uint16_t Size);
cmd_vel_t  USB_GetCmdVel(void);

#endif
