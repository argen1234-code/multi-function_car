#ifndef BSP_BLUETOOTH_H
#define BSP_BLUETOOTH_H

#include "main.h"

#define BT_KEY_FORWARD       0x01U
#define BT_KEY_BACKWARD      0x02U
#define BT_KEY_LEFT          0x04U
#define BT_KEY_RIGHT         0x08U
#define BT_KEY_ROTATE_LEFT   0x10U
#define BT_KEY_ROTATE_RIGHT  0x20U

/* Motion direction from Bluetooth remote control */
typedef enum {
    BT_MOTION_STOP = 0,
    BT_MOTION_FORWARD,
    BT_MOTION_BACKWARD,
    BT_MOTION_LEFT,
    BT_MOTION_RIGHT,
    BT_MOTION_ROTATE_LEFT,
    BT_MOTION_ROTATE_RIGHT
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
uint8_t        BT_IsActive(void);
uint8_t        BT_IsOnline(void);
uint8_t        BT_GetKeyState(void);
BT_ModeReq_t   BT_GetAndClearModeReq(void);
uint8_t        BT_GetAndClearAckCount(void);

#endif
