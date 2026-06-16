#include "app_Voice_Recognition.h"
#include "bsp_WonderEcho.h"

#define VOICE_SPEED_DEFAULT     20.0f
#define VOICE_SPEED_MIN         10.0f
#define VOICE_SPEED_MAX         50.0f
#define VOICE_SPEED_STEP        10.0f
#define VOICE_TURN_SPEED        20.0f
#define VOICE_FEEDBACK_CMD_TYPE 0x00U

static float voice_speed = VOICE_SPEED_DEFAULT;
static uint8_t last_feedback_id = 0U;

volatile Voice_Debug_t voice_debug = {0};

static uint8_t App_Voice_IsMovingForwardOrBackward(chassis_move_t *chassis)
{
    if (chassis->mode != CAR_MODE_VOICE)
    {
        return 0U;
    }

    if ((chassis->Vy_set != 0.0f) || (chassis->Wz_set != 0.0f))
    {
        return 0U;
    }

    if ((chassis->Vx_set > 0.0f) || (chassis->Vx_set < 0.0f))
    {
        return 1U;
    }

    return 0U;
}

static void App_Voice_LimitSpeed(void)
{
    if (voice_speed > VOICE_SPEED_MAX)
    {
        voice_speed = VOICE_SPEED_MAX;
    }
    else if (voice_speed < VOICE_SPEED_MIN)
    {
        voice_speed = VOICE_SPEED_MIN;
    }
}

static void App_Voice_SpeakFeedback(uint8_t voice_id)
{
    if (voice_id == 0U)
    {
        last_feedback_id = 0U;
        voice_debug.last_feedback_id = 0U;
        return;
    }

    if (voice_id != last_feedback_id)
    {
        WonderEcho_Speak(VOICE_FEEDBACK_CMD_TYPE, voice_id);
        last_feedback_id = voice_id;
        voice_debug.last_feedback_id = voice_id;
        voice_debug.feedback_count++;
    }
}

static void App_Voice_SaveDebug(chassis_move_t *chassis, uint8_t voice_id, uint8_t handled)
{
    voice_debug.last_voice_id = voice_id;
    voice_debug.voice_speed = voice_speed;
    voice_debug.last_handled_valid = handled;

    if (chassis != NULL)
    {
        voice_debug.last_mode = chassis->mode;
        voice_debug.last_vx_set = chassis->Vx_set;
        voice_debug.last_vy_set = chassis->Vy_set;
        voice_debug.last_wz_set = chassis->Wz_set;
    }

    if (handled)
    {
        voice_debug.handled_count++;
        voice_debug.last_handled_id = voice_id;
        voice_debug.last_handled_tick = HAL_GetTick();
    }
}

void App_Voice_Recognition_Update(chassis_move_t *chassis)
{
    uint8_t voice_id;
    uint8_t update_line_speed = 0U;
    uint8_t handled = 0U;

    if (chassis == NULL)
    {
        return;
    }

    voice_debug.update_count++;
    voice_debug.last_update_tick = HAL_GetTick();

    voice_id = WonderEcho_GetResult();
    if (voice_id == 0U)
    {
        voice_debug.zero_count++;
        App_Voice_SpeakFeedback(0U);
    }

    switch (voice_id)
    {
        case WONDERECHO_CMD_FORWARD:
            chassis->mode = CAR_MODE_VOICE;
            chassis->Vx_set = voice_speed;
            chassis->Vy_set = 0.0f;
            chassis->Wz_set = 0.0f;
            App_Voice_SpeakFeedback(voice_id);
            handled = 1U;
            break;

        case WONDERECHO_CMD_BACKWARD:
            chassis->mode = CAR_MODE_VOICE;
            chassis->Vx_set = -voice_speed;
            chassis->Vy_set = 0.0f;
            chassis->Wz_set = 0.0f;
            App_Voice_SpeakFeedback(voice_id);
            handled = 1U;
            break;

        case WONDERECHO_CMD_TURN_LEFT:
            chassis->mode = CAR_MODE_VOICE;
            chassis->Vx_set = 0.0f;
            chassis->Vy_set = 0.0f;
            chassis->Wz_set = -VOICE_TURN_SPEED;
            App_Voice_SpeakFeedback(voice_id);
            handled = 1U;
            break;

        case WONDERECHO_CMD_TURN_RIGHT:
            chassis->mode = CAR_MODE_VOICE;
            chassis->Vx_set = 0.0f;
            chassis->Vy_set = 0.0f;
            chassis->Wz_set = VOICE_TURN_SPEED;
            App_Voice_SpeakFeedback(voice_id);
            handled = 1U;
            break;

        case WONDERECHO_CMD_STOP:
            chassis->mode = CAR_MODE_VOICE;
            chassis->Vx_set = 0.0f;
            chassis->Vy_set = 0.0f;
            chassis->Wz_set = 0.0f;
            App_Voice_SpeakFeedback(voice_id);
            handled = 1U;
            break;

        case WONDERECHO_CMD_SPEED_UP:
            update_line_speed = App_Voice_IsMovingForwardOrBackward(chassis);
            voice_speed += VOICE_SPEED_STEP;
            App_Voice_LimitSpeed();
            if (update_line_speed)
            {
                chassis->Vx_set = (chassis->Vx_set > 0.0f) ? voice_speed : -voice_speed;
            }
            App_Voice_SpeakFeedback(voice_id);
            handled = 1U;
            break;

        case WONDERECHO_CMD_SPEED_DOWN:
            update_line_speed = App_Voice_IsMovingForwardOrBackward(chassis);
            voice_speed -= VOICE_SPEED_STEP;
            App_Voice_LimitSpeed();
            if (update_line_speed)
            {
                chassis->Vx_set = (chassis->Vx_set > 0.0f) ? voice_speed : -voice_speed;
            }
            App_Voice_SpeakFeedback(voice_id);
            handled = 1U;
            break;

        default:
            if (voice_id != 0U)
            {
                voice_debug.unknown_count++;
            }
            break;
    }

    App_Voice_SaveDebug(chassis, voice_id, handled);
}
