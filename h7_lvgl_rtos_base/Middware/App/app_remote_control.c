#include "app_remote_control.h"
#include "bsp_bluetooth.h"

#define BT_REMOTE_SPEED  20.0f

void Remote_Control_Update(chassis_move_t *chassis)
{
    switch (BT_GetMotion())
    {
        case BT_MOTION_FORWARD:
            chassis->Vx_set =  BT_REMOTE_SPEED;
            chassis->Vy_set =  0.0f;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_BACKWARD:
            chassis->Vx_set = -BT_REMOTE_SPEED;
            chassis->Vy_set =  0.0f;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_LEFT:
            chassis->Vx_set =  0.0f;
            chassis->Vy_set =  BT_REMOTE_SPEED;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_RIGHT:
            chassis->Vx_set =  0.0f;
            chassis->Vy_set = -BT_REMOTE_SPEED;
            chassis->Wz_set =  0.0f;
            break;

        case BT_MOTION_STOP:
        default:
            chassis->Vx_set = 0.0f;
            chassis->Vy_set = 0.0f;
            chassis->Wz_set = 0.0f;
            break;
    }
}
