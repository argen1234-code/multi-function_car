#include "bsp_bluetooth.h"

#define BT_REMOTE_TIMEOUT_MS  300U

static volatile uint8_t      bt_cmd       = 'S';
static volatile uint32_t     bt_last_tick = 0U;
static volatile BT_ModeReq_t bt_mode_req  = BT_MODE_REQ_NONE;

void BT_Init(void)
{
    bt_cmd       = 'S';
    bt_last_tick = 0U;
    bt_mode_req  = BT_MODE_REQ_NONE;
}

/* Called from UART ISR / DMA callback to feed received data */
void BT_ProcessRxData(uint8_t *pBuf, uint16_t Size)
{
    if (pBuf == NULL || Size == 0U)
    {
        return;
    }

    for (uint16_t i = 0; i < Size; i++)
    {
        switch (pBuf[i])
        {
            case 'G':
            case 'g':
                bt_mode_req = BT_MODE_REQ_GPS;
                break;

            case 'O':
            case 'o':
                bt_mode_req = BT_MODE_REQ_GPS_ROS;
                break;

            case 'I':
            case 'i':
                bt_mode_req  = BT_MODE_REQ_INDOOR;
                bt_cmd       = 'S';
                bt_last_tick = HAL_GetTick();
                break;

            case 'F': case 'f':
            case 'B': case 'b':
            case 'L': case 'l':
            case 'R': case 'r':
            case 'S': case 's':
                bt_cmd       = pBuf[i];
                bt_last_tick = HAL_GetTick();
                bt_mode_req  = BT_MODE_REQ_INDOOR;
                break;

            default:
                break;
        }
    }
}

/* Returns current motion command, auto-fallback to STOP on timeout */
BT_Motion_t BT_GetMotion(void)
{
    uint8_t cmd = bt_cmd;

    if (bt_last_tick == 0U ||
        HAL_GetTick() - bt_last_tick > BT_REMOTE_TIMEOUT_MS)
    {
        cmd = 'S';
    }

    switch (cmd)
    {
        case 'F': case 'f': return BT_MOTION_FORWARD;
        case 'B': case 'b': return BT_MOTION_BACKWARD;
        case 'L': case 'l': return BT_MOTION_LEFT;
        case 'R': case 'r': return BT_MOTION_RIGHT;
        default:            return BT_MOTION_STOP;
    }
}

/* Returns and clears the pending mode request from BT */
BT_ModeReq_t BT_GetAndClearModeReq(void)
{
    BT_ModeReq_t req = bt_mode_req;
    bt_mode_req = BT_MODE_REQ_NONE;
    return req;
}
