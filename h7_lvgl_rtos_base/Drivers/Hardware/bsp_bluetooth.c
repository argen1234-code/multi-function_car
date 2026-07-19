#include "bsp_bluetooth.h"
#include <string.h>

#define BT_TOKEN_MAX_LEN      16U
#define BT_FRAME_MAX_LEN      64U
#define BT_ONLINE_TIMEOUT_MS  3000U

volatile BT_Debug_t g_bt_debug;

static volatile uint8_t      bt_cmd       = 'S';
static volatile uint8_t      bt_key_state = 0U;
static volatile BT_ModeReq_t bt_mode_req  = BT_MODE_REQ_NONE;
static volatile uint8_t      bt_ack_count = 0U;
static volatile uint32_t     bt_last_rx_tick = 0U;
static volatile BT_RoadDisplay_t bt_road_display = BT_ROAD_DISPLAY_ASPHALT;
static char                  bt_frame_buf[BT_FRAME_MAX_LEN];
static uint8_t               bt_frame_len = 0U;
static uint8_t               bt_frame_active = 0U;
static uint8_t               bt_frame_saw_cr = 0U;

static BT_Motion_t bt_debug_motion_from_state(void)
{
    uint8_t cmd = bt_cmd;

    if (bt_key_state == 0U)
    {
        cmd = 'S';
    }

    switch (cmd)
    {
        case 'F': case 'f': return BT_MOTION_FORWARD;
        case 'B': case 'b': return BT_MOTION_BACKWARD;
        case 'L': case 'l': return BT_MOTION_LEFT;
        case 'R': case 'r': return BT_MOTION_RIGHT;
        case 'Q': case 'q': return BT_MOTION_ROTATE_LEFT;
        case 'E': case 'e': return BT_MOTION_ROTATE_RIGHT;
        default:            return BT_MOTION_STOP;
    }
}

static void bt_debug_reset(void)
{
    uint16_t i;

    g_bt_debug.update_sequence = 0U;
    g_bt_debug.rx_event_count = 0U;
    g_bt_debug.rx_byte_count = 0U;
    g_bt_debug.complete_frame_count = 0U;
    g_bt_debug.accepted_token_count = 0U;
    g_bt_debug.ack_queued_count = 0U;
    g_bt_debug.frame_overflow_count = 0U;
    g_bt_debug.format_error_count = 0U;
    g_bt_debug.last_rx_event_tick = 0U;
    g_bt_debug.last_valid_frame_tick = 0U;
    g_bt_debug.last_rx_size = 0U;
    g_bt_debug.last_rx_copied_len = 0U;
    g_bt_debug.frame_active = 0U;
    g_bt_debug.frame_saw_cr = 0U;
    g_bt_debug.frame_len = 0U;
    g_bt_debug.reserved0 = 0U;
    g_bt_debug.last_payload_len = 0U;
    g_bt_debug.cmd = 'S';
    g_bt_debug.key_state = 0U;
    g_bt_debug.active = 0U;
    g_bt_debug.online = 0U;
    g_bt_debug.motion = BT_MOTION_STOP;
    g_bt_debug.pending_mode_req = BT_MODE_REQ_NONE;
    g_bt_debug.last_mode_req = BT_MODE_REQ_NONE;
    g_bt_debug.ack_pending = 0U;
    g_bt_debug.last_ack_count = 0U;

    for (i = 0U; i < BT_DEBUG_RAW_MAX_LEN; i++)
    {
        g_bt_debug.raw_data[i] = 0U;
    }

    for (i = 0U; i < BT_DEBUG_PAYLOAD_MAX_LEN; i++)
    {
        g_bt_debug.last_payload[i] = '\0';
    }
}

static void bt_debug_sync_state(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t active = (bt_key_state != 0U) ? 1U : 0U;

    g_bt_debug.update_sequence++;
    g_bt_debug.frame_active = bt_frame_active;
    g_bt_debug.frame_saw_cr = bt_frame_saw_cr;
    g_bt_debug.frame_len = bt_frame_len;
    g_bt_debug.cmd = bt_cmd;
    g_bt_debug.key_state = bt_key_state;
    g_bt_debug.active = active;
    g_bt_debug.online = (active ||
                         (bt_last_rx_tick != 0U &&
                          (now - bt_last_rx_tick) <= BT_ONLINE_TIMEOUT_MS)) ? 1U : 0U;
    g_bt_debug.motion = bt_debug_motion_from_state();
    g_bt_debug.pending_mode_req = bt_mode_req;
    if (bt_mode_req != BT_MODE_REQ_NONE)
    {
        g_bt_debug.last_mode_req = bt_mode_req;
    }
    g_bt_debug.ack_pending = bt_ack_count;
    g_bt_debug.update_sequence++;
}

static void bt_debug_capture_rx(const uint8_t *pBuf, uint16_t Size)
{
    uint16_t i;
    uint16_t copy_len = Size;

    if (copy_len > BT_DEBUG_RAW_MAX_LEN)
    {
        copy_len = BT_DEBUG_RAW_MAX_LEN;
    }

    g_bt_debug.update_sequence++;
    g_bt_debug.rx_event_count++;
    g_bt_debug.rx_byte_count += Size;
    g_bt_debug.last_rx_event_tick = HAL_GetTick();
    g_bt_debug.last_rx_size = Size;
    g_bt_debug.last_rx_copied_len = copy_len;

    for (i = 0U; i < copy_len; i++)
    {
        g_bt_debug.raw_data[i] = pBuf[i];
    }

    g_bt_debug.update_sequence++;
}

static void bt_debug_capture_complete_frame(void)
{
    uint8_t i;

    g_bt_debug.update_sequence++;
    g_bt_debug.complete_frame_count++;
    g_bt_debug.last_valid_frame_tick = bt_last_rx_tick;
    g_bt_debug.last_payload_len = bt_frame_len;

    for (i = 0U; i < bt_frame_len; i++)
    {
        g_bt_debug.last_payload[i] = bt_frame_buf[i];
    }
    g_bt_debug.last_payload[bt_frame_len] = '\0';
    g_bt_debug.update_sequence++;
}

static char bt_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static uint8_t bt_is_token_char(char c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_') ? 1U : 0U;
}

static void bt_clear_motion(void)
{
    bt_cmd = 'S';
    bt_key_state = 0U;
}

static void bt_queue_ack(void)
{
    g_bt_debug.accepted_token_count++;

    if (bt_ack_count < 255U)
    {
        bt_ack_count++;
        g_bt_debug.ack_queued_count++;
    }
}

static void bt_toggle_motion(uint8_t key_bit, uint8_t cmd)
{
    bt_key_state ^= key_bit;
    bt_cmd = (bt_key_state == 0U) ? 'S' : cmd;
    bt_mode_req = BT_MODE_REQ_INDOOR;
}

static uint8_t bt_is_legacy_cmd_char(char ch)
{
    switch (ch)
    {
        case 'f':
        case 'b':
        case 'l':
        case 'r':
        case 'q':
        case 'e':
        case 's':
        case 'i':
        case 'g':
            return 1U;

        default:
            return 0U;
    }
}

static uint8_t bt_process_legacy_cmd_char(char ch)
{
    switch (ch)
    {
        case 'g':
            bt_mode_req = BT_MODE_REQ_GPS;
            bt_clear_motion();
            return 1U;

        case 'i':
            bt_clear_motion();
            bt_mode_req = BT_MODE_REQ_INDOOR;
            return 1U;

        case 'f':
            bt_toggle_motion(BT_KEY_FORWARD, 'F');
            return 1U;

        case 'b':
            bt_toggle_motion(BT_KEY_BACKWARD, 'B');
            return 1U;

        case 'l':
            bt_toggle_motion(BT_KEY_LEFT, 'L');
            return 1U;

        case 'r':
            bt_toggle_motion(BT_KEY_RIGHT, 'R');
            return 1U;

        case 'q':
            bt_toggle_motion(BT_KEY_ROTATE_LEFT, 'Q');
            return 1U;

        case 'e':
            bt_toggle_motion(BT_KEY_ROTATE_RIGHT, 'E');
            return 1U;

        case 's':
            bt_clear_motion();
            bt_mode_req = BT_MODE_REQ_INDOOR;
            return 1U;

        default:
            break;
    }

    return 0U;
}

static uint8_t bt_process_road_token(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return 0U;
    }

    if (strcmp(token, "road_asphalt") == 0)
    {
        BT_SetRoadDisplay(BT_ROAD_DISPLAY_ASPHALT);
        return 1U;
    }

    if (strcmp(token, "road_indoor") == 0)
    {
        BT_SetRoadDisplay(BT_ROAD_DISPLAY_INDOOR);
        return 1U;
    }

    if (strcmp(token, "road_cement") == 0)
    {
        BT_SetRoadDisplay(BT_ROAD_DISPLAY_OUTDOOR_CEMENT);
        return 1U;
    }

    if (strcmp(token, "road_marble") == 0)
    {
        BT_SetRoadDisplay(BT_ROAD_DISPLAY_OUTDOOR_MARBLE);
        return 1U;
    }

    return 0U;
}

static uint8_t bt_process_token(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return 0U;
    }

    if (bt_process_road_token(token))
    {
        return 1U;
    }

    if (strcmp(token, "g") == 0 || strcmp(token, "gps") == 0)
    {
        bt_mode_req = BT_MODE_REQ_GPS;
        bt_clear_motion();
        return 1U;
    }

    if (strcmp(token, "i") == 0 ||
        strcmp(token, "indoor") == 0 ||
        strcmp(token, "bt") == 0 ||
        strcmp(token, "bluetooth") == 0)
    {
        bt_clear_motion();
        bt_mode_req = BT_MODE_REQ_INDOOR;
        return 1U;
    }

    if (strcmp(token, "f") == 0 || strcmp(token, "forward") == 0)
    {
        bt_toggle_motion(BT_KEY_FORWARD, 'F');
        return 1U;
    }

    if (strcmp(token, "b") == 0 ||
        strcmp(token, "back") == 0 ||
        strcmp(token, "backward") == 0)
    {
        bt_toggle_motion(BT_KEY_BACKWARD, 'B');
        return 1U;
    }

    if (strcmp(token, "l") == 0 || strcmp(token, "left") == 0)
    {
        bt_toggle_motion(BT_KEY_LEFT, 'L');
        return 1U;
    }

    if (strcmp(token, "r") == 0 || strcmp(token, "right") == 0)
    {
        bt_toggle_motion(BT_KEY_RIGHT, 'R');
        return 1U;
    }

    if (strcmp(token, "s") == 0 || strcmp(token, "stop") == 0)
    {
        bt_clear_motion();
        bt_mode_req = BT_MODE_REQ_INDOOR;
        return 1U;
    }

    if (strcmp(token, "q") == 0 ||
        strcmp(token, "ccw") == 0 ||
        strcmp(token, "turn_left") == 0 ||
        strcmp(token, "rotate_left") == 0)
    {
        bt_toggle_motion(BT_KEY_ROTATE_LEFT, 'Q');
        return 1U;
    }

    if (strcmp(token, "e") == 0 ||
        strcmp(token, "cw") == 0 ||
        strcmp(token, "turn_right") == 0 ||
        strcmp(token, "rotate_right") == 0)
    {
        bt_toggle_motion(BT_KEY_ROTATE_RIGHT, 'E');
        return 1U;
    }

    {
        uint8_t i = 0U;
        uint8_t handled = 0U;

        while (token[i] != '\0')
        {
            if (!bt_is_legacy_cmd_char(token[i]))
            {
                return 0U;
            }
            i++;
        }

        for (i = 0U; token[i] != '\0'; i++)
        {
            handled |= bt_process_legacy_cmd_char(token[i]);
        }

        return handled;
    }
}

static void bt_process_payload(const char *payload)
{
    char token[BT_TOKEN_MAX_LEN];
    uint8_t token_len = 0U;
    uint16_t i = 0U;

    if (payload == NULL)
    {
        return;
    }

    while (payload[i] != '\0')
    {
        char ch = payload[i++];

        if (bt_is_token_char(ch))
        {
            if (token_len < (BT_TOKEN_MAX_LEN - 1U))
            {
                token[token_len++] = bt_to_lower(ch);
            }
        }
        else
        {
            token[token_len] = '\0';
            if (bt_process_token(token))
            {
                bt_queue_ack();
            }
            token_len = 0U;
        }
    }

    token[token_len] = '\0';
    if (bt_process_token(token))
    {
        bt_queue_ack();
    }
}

static void bt_frame_reset(void)
{
    bt_frame_len = 0U;
    bt_frame_active = 0U;
    bt_frame_saw_cr = 0U;
}

void BT_Init(void)
{
    bt_cmd       = 'S';
    bt_key_state = 0U;
    bt_mode_req  = BT_MODE_REQ_NONE;
    bt_ack_count = 0U;
    bt_last_rx_tick = 0U;
    bt_road_display = BT_ROAD_DISPLAY_ASPHALT;
    bt_frame_reset();
    bt_debug_reset();
    bt_debug_sync_state();
}

/* Called from UART ISR / DMA callback to feed received data */
void BT_ProcessRxData(uint8_t *pBuf, uint16_t Size)
{
    if (pBuf == NULL || Size == 0U)
    {
        return;
    }

    bt_debug_capture_rx(pBuf, Size);

    for (uint16_t i = 0; i < Size; i++)
    {
        char ch = (char)pBuf[i];

        if (!bt_frame_active)
        {
            if (ch == '@')
            {
                bt_frame_active = 1U;
                bt_frame_len = 0U;
                bt_frame_saw_cr = 0U;
            }
            continue;
        }

        if (ch == '@')
        {
            bt_frame_active = 1U;
            bt_frame_len = 0U;
            bt_frame_saw_cr = 0U;
            continue;
        }

        if (bt_frame_saw_cr)
        {
            if (ch == '\n')
            {
                bt_frame_buf[bt_frame_len] = '\0';
                bt_last_rx_tick = HAL_GetTick();
                bt_debug_capture_complete_frame();
                bt_process_payload(bt_frame_buf);
                bt_frame_reset();
            }
            else if (ch == '@')
            {
                bt_frame_active = 1U;
                bt_frame_len = 0U;
                bt_frame_saw_cr = 0U;
            }
            else
            {
                g_bt_debug.format_error_count++;
                bt_frame_reset();
            }
            continue;
        }

        if (ch == '\r')
        {
            bt_frame_saw_cr = 1U;
            continue;
        }

        if (bt_frame_len < (BT_FRAME_MAX_LEN - 1U))
        {
            bt_frame_buf[bt_frame_len++] = ch;
        }
        else
        {
            g_bt_debug.frame_overflow_count++;
            bt_frame_reset();
        }
    }

    bt_debug_sync_state();
}

/*
 * The phone application may end a road_* command with a UART idle gap instead
 * of transmitting CR/LF. Accept only the four display commands in this path;
 * motion and mode commands keep the original @payload\r\n protocol unchanged.
 */
void BT_ProcessRxIdle(void)
{
    char token[BT_TOKEN_MAX_LEN];
    uint8_t i;

    if (!bt_frame_active || bt_frame_saw_cr || bt_frame_len == 0U ||
        bt_frame_len >= BT_TOKEN_MAX_LEN)
    {
        return;
    }

    for (i = 0U; i < bt_frame_len; i++)
    {
        if (!bt_is_token_char(bt_frame_buf[i]))
        {
            return;
        }
        token[i] = bt_to_lower(bt_frame_buf[i]);
    }
    token[bt_frame_len] = '\0';

    if (!bt_process_road_token(token))
    {
        return;
    }

    bt_frame_buf[bt_frame_len] = '\0';
    bt_last_rx_tick = HAL_GetTick();
    bt_debug_capture_complete_frame();
    bt_queue_ack();
    bt_frame_reset();
    bt_debug_sync_state();
}

/* Returns 1 if BT remote is actively sending motion commands */
uint8_t BT_IsActive(void)
{
    uint8_t active = (bt_key_state != 0U) ? 1U : 0U;

    g_bt_debug.active = active;
    return active;
}

uint8_t BT_IsOnline(void)
{
    uint32_t tick = bt_last_rx_tick;
    uint8_t online;

    if (BT_IsActive())
    {
        online = 1U;
    }
    else
    {
        online = (tick != 0U && (HAL_GetTick() - tick) <= BT_ONLINE_TIMEOUT_MS) ? 1U : 0U;
    }

    g_bt_debug.online = online;
    return online;
}

uint8_t BT_GetKeyState(void)
{
    g_bt_debug.key_state = bt_key_state;
    return bt_key_state;
}

BT_RoadDisplay_t BT_GetRoadDisplay(void)
{
    return bt_road_display;
}

void BT_SetRoadDisplay(BT_RoadDisplay_t road)
{
    if (road < BT_ROAD_DISPLAY_COUNT)
    {
        bt_road_display = road;
    }
}

/* Returns current motion command, auto-fallback to STOP on timeout */
BT_Motion_t BT_GetMotion(void)
{
    uint8_t cmd = bt_cmd;
    BT_Motion_t motion;

    if (!BT_IsActive())
    {
        cmd = 'S';
    }

    switch (cmd)
    {
        case 'F': case 'f': motion = BT_MOTION_FORWARD; break;
        case 'B': case 'b': motion = BT_MOTION_BACKWARD; break;
        case 'L': case 'l': motion = BT_MOTION_LEFT; break;
        case 'R': case 'r': motion = BT_MOTION_RIGHT; break;
        case 'Q': case 'q': motion = BT_MOTION_ROTATE_LEFT; break;
        case 'E': case 'e': motion = BT_MOTION_ROTATE_RIGHT; break;
        default:            motion = BT_MOTION_STOP; break;
    }

    g_bt_debug.motion = motion;
    return motion;
}

/* Returns and clears the pending mode request from BT */
BT_ModeReq_t BT_GetAndClearModeReq(void)
{
    BT_ModeReq_t req = bt_mode_req;
    bt_mode_req = BT_MODE_REQ_NONE;
    if (req != BT_MODE_REQ_NONE)
    {
        g_bt_debug.last_mode_req = req;
    }
    bt_debug_sync_state();
    return req;
}

uint8_t BT_GetAndClearAckCount(void)
{
    uint8_t count;

    __disable_irq();
    count = bt_ack_count;
    bt_ack_count = 0U;
    __enable_irq();

    g_bt_debug.last_ack_count = count;
    bt_debug_sync_state();
    return count;
}
