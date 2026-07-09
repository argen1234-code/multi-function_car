#include "bsp_bluetooth.h"
#include <string.h>

#define BT_TOKEN_MAX_LEN      16U
#define BT_FRAME_MAX_LEN      64U
#define BT_ONLINE_TIMEOUT_MS  3000U

static volatile uint8_t      bt_cmd       = 'S';
static volatile uint8_t      bt_key_state = 0U;
static volatile BT_ModeReq_t bt_mode_req  = BT_MODE_REQ_NONE;
static volatile uint8_t      bt_ack_count = 0U;
static volatile uint32_t     bt_last_rx_tick = 0U;
static char                  bt_frame_buf[BT_FRAME_MAX_LEN];
static uint8_t               bt_frame_len = 0U;
static uint8_t               bt_frame_active = 0U;
static uint8_t               bt_frame_saw_cr = 0U;

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
    if (bt_ack_count < 255U)
    {
        bt_ack_count++;
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

static uint8_t bt_process_token(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return 0U;
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
    bt_frame_reset();
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
            bt_frame_reset();
        }
    }
}

/* Returns 1 if BT remote is actively sending motion commands */
uint8_t BT_IsActive(void)
{
    return (bt_key_state != 0U) ? 1U : 0U;
}

uint8_t BT_IsOnline(void)
{
    uint32_t tick = bt_last_rx_tick;

    if (BT_IsActive())
    {
        return 1U;
    }

    return (tick != 0U && (HAL_GetTick() - tick) <= BT_ONLINE_TIMEOUT_MS) ? 1U : 0U;
}

uint8_t BT_GetKeyState(void)
{
    return bt_key_state;
}

/* Returns current motion command, auto-fallback to STOP on timeout */
BT_Motion_t BT_GetMotion(void)
{
    uint8_t cmd = bt_cmd;

    if (!BT_IsActive())
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

/* Returns and clears the pending mode request from BT */
BT_ModeReq_t BT_GetAndClearModeReq(void)
{
    BT_ModeReq_t req = bt_mode_req;
    bt_mode_req = BT_MODE_REQ_NONE;
    return req;
}

uint8_t BT_GetAndClearAckCount(void)
{
    uint8_t count;

    __disable_irq();
    count = bt_ack_count;
    bt_ack_count = 0U;
    __enable_irq();

    return count;
}
