#ifndef BSP_BLUETOOTH_H
#define BSP_BLUETOOTH_H

#include "main.h"

/* Motion direction from Bluetooth remote control */
typedef enum {
    BT_MOTION_STOP = 0,
    BT_MOTION_FORWARD,
    BT_MOTION_BACKWARD,
    BT_MOTION_LEFT,
    BT_MOTION_RIGHT
} BT_Motion_t;

/* Mode request received over Bluetooth */
typedef enum {
    BT_MODE_REQ_NONE = 0,
    BT_MODE_REQ_GPS,
    BT_MODE_REQ_INDOOR
} BT_ModeReq_t;

void           BT_Init(void);
void           BT_ProcessRxData(uint8_t *pBuf, uint16_t Size);
BT_Motion_t    BT_GetMotion(void);
BT_ModeReq_t   BT_GetAndClearModeReq(void);

#endif
