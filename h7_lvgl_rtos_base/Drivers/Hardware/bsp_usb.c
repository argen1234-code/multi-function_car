#include "bsp_usb.h"
#include <string.h>

#define CMD_VEL_FRAME_SIZE  11U

static uint8_t   rx_buf[CMD_VEL_FRAME_SIZE];
static uint8_t   rx_idx = 0;
static cmd_vel_t cmd_vel = {0.0f, 0.0f};

void USB_Init(void)
{
    rx_idx = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    cmd_vel.vx = 0.0f;
    cmd_vel.vz = 0.0f;
}

void USB_ProcessRxData(uint8_t *pBuf, uint16_t Size)
{
    if (pBuf == NULL || Size == 0U) return;

    for (uint16_t i = 0; i < Size; i++)
    {
        uint8_t byte = pBuf[i];

        /* 帧头同步 */
        if (rx_idx == 0)
        {
            if (byte != 0xAA) continue;
        }
        else if (rx_idx == 1)
        {
            if (byte != 0x55) { rx_idx = 0; continue; }
        }

        rx_buf[rx_idx++] = byte;

        if (rx_idx == CMD_VEL_FRAME_SIZE)
        {
            rx_idx = 0;

            /* XOR 校验: Byte2 ~ Byte9 */
            uint8_t checksum = 0;
            for (uint8_t j = 2; j < 10; j++)
            {
                checksum ^= rx_buf[j];
            }
            if (checksum != rx_buf[10]) continue;

            /* 解析 (STM32 小端, 无需字节序转换) */
            memcpy(&cmd_vel.vx, &rx_buf[2], 4);
            memcpy(&cmd_vel.vz, &rx_buf[6], 4);
        }
    }
}

cmd_vel_t USB_GetCmdVel(void)
{
    return cmd_vel;
}
