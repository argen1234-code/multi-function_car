#include "bsp_usb.h"
#include "usbd_cdc_if.h"
#include <string.h>

#define CMD_VEL_FRAME_SIZE   12U
#define TELEM_FRAME_SIZE      7U

static uint8_t   rx_buf[CMD_VEL_FRAME_SIZE];
static uint8_t   rx_idx = 0;
static cmd_vel_t cmd_vel = {0, 0.0f, 0.0f};

void USB_Init(void)
{
    rx_idx = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    cmd_vel.mode = 0;
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

            /* XOR 校验: Byte2 ~ Byte10 (9 bytes) */
            uint8_t checksum = 0;
            for (uint8_t j = 2; j < 11; j++)
            {
                checksum ^= rx_buf[j];
            }
            if (checksum != rx_buf[11]) continue;

            /* 解析: mode(1B) + vx(4B LE) + vz(4B LE) */
            cmd_vel.mode = rx_buf[2];
            memcpy(&cmd_vel.vx, &rx_buf[3], 4);
            memcpy(&cmd_vel.vz, &rx_buf[7], 4);
        }
    }
}

cmd_vel_t USB_GetCmdVel(void)
{
    return cmd_vel;
}

void USB_SendTelemetry(float heading_to_target_deg)
{
    uint8_t buf[TELEM_FRAME_SIZE];
    buf[0] = 0xAA;
    buf[1] = 0x55;
    memcpy(&buf[2], &heading_to_target_deg, 4);

    uint8_t checksum = 0;
    for (uint8_t i = 2; i < 6; i++) checksum ^= buf[i];
    buf[6] = checksum;

    CDC_Transmit_FS(buf, TELEM_FRAME_SIZE);
}
