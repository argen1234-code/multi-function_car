#include "bsp_usb.h"
#include "usbd_cdc_if.h"
#include <string.h>

#define CMD_VEL_FRAME_SIZE   12U
#define SCENE_FRAME_SIZE      4U
#define RX_FRAME_MAX_SIZE    CMD_VEL_FRAME_SIZE
#define TELEM_FRAME_SIZE     15U

static uint8_t   rx_buf[RX_FRAME_MAX_SIZE];
static uint8_t   rx_idx = 0;
static uint8_t   rx_expected_size = 0U;
static cmd_vel_t cmd_vel = {0};
static scene_cmd_t scene_cmd = {JETSON_SCENE_NONE, 0U, 0U};

static void USB_RxReset(void)
{
    rx_idx = 0U;
    rx_expected_size = 0U;
}

static void USB_RxStart(uint8_t header)
{
    rx_buf[0] = header;
    rx_idx = 1U;
    rx_expected_size = (header == 0xAAU) ? CMD_VEL_FRAME_SIZE : SCENE_FRAME_SIZE;
}

void USB_Init(void)
{
    USB_RxReset();
    memset(rx_buf, 0, sizeof(rx_buf));
    cmd_vel.mode = 0;
    cmd_vel.vx = 0.0f;
    cmd_vel.vz = 0.0f;
    cmd_vel.last_update_tick = 0U;
    cmd_vel.update_sequence = 0U;
    scene_cmd.scene = JETSON_SCENE_NONE;
    scene_cmd.last_update_tick = 0U;
    scene_cmd.update_sequence = 0U;
}

void USB_ProcessRxData(uint8_t *pBuf, uint16_t Size)
{
    if (pBuf == NULL || Size == 0U) return;

    for (uint16_t i = 0; i < Size; i++)
    {
        uint8_t byte = pBuf[i];

        /* Byte 0 selects frame type: AA=cmd_vel, BB=scene_cmd. */
        if (rx_idx == 0U)
        {
            if (byte == 0xAAU || byte == 0xBBU) USB_RxStart(byte);
            continue;
        }

        if (rx_idx == 1U)
        {
            if (byte != 0x55U)
            {
                USB_RxReset();
                if (byte == 0xAAU || byte == 0xBBU) USB_RxStart(byte);
                continue;
            }
        }

        rx_buf[rx_idx++] = byte;

        if (rx_idx == rx_expected_size)
        {
            uint8_t frame_type = rx_buf[0];
            uint8_t checksum = 0U;
            uint8_t j;

            USB_RxReset();

            if (frame_type == 0xAAU)
            {
                /* cmd_vel XOR: Byte2 ~ Byte10. */
                for (j = 2U; j < 11U; j++) checksum ^= rx_buf[j];
                if (checksum != rx_buf[11]) continue;

                cmd_vel.mode = rx_buf[2];
                memcpy(&cmd_vel.vx, &rx_buf[3], 4U);
                memcpy(&cmd_vel.vz, &rx_buf[7], 4U);
                cmd_vel.last_update_tick = HAL_GetTick();
                cmd_vel.update_sequence++;
            }
            else
            {
                /* scene_cmd XOR is exactly byte 2. Ignore unknown commands. */
                if (rx_buf[3] != rx_buf[2]) continue;
                if (rx_buf[2] != (uint8_t)JETSON_SCENE_INDOOR &&
                    rx_buf[2] != (uint8_t)JETSON_SCENE_OUTDOOR) continue;

                scene_cmd.scene = (JetsonScene_t)rx_buf[2];
                scene_cmd.last_update_tick = HAL_GetTick();
                scene_cmd.update_sequence++;
            }
        }
    }
}

scene_cmd_t USB_GetSceneCmd(void)
{
    return scene_cmd;
}

cmd_vel_t USB_GetCmdVel(void)
{
    return cmd_vel;
}

void USB_SendTelemetry(float heading_to_target_deg,
                       float current_lat,
                       float current_lon)
{
    uint8_t buf[TELEM_FRAME_SIZE];
    buf[0] = 0xAA;
    buf[1] = 0x55;
    memcpy(&buf[2],  &heading_to_target_deg, 4);
    memcpy(&buf[6],  &current_lat,           4);
    memcpy(&buf[10], &current_lon,           4);

    uint8_t checksum = 0;
    for (uint8_t i = 2; i < 14; i++) checksum ^= buf[i];
    buf[14] = checksum;

    CDC_Transmit_FS(buf, TELEM_FRAME_SIZE);
}
