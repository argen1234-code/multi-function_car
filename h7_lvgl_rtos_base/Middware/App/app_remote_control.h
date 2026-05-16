#ifndef APP_REMOTE_CONTROL_H
#define APP_REMOTE_CONTROL_H

#include "app_chassis_board.h"

void Remote_Control_Update(chassis_move_t *chassis);
void Remote_WeChat_Update(chassis_move_t *chassis);
void Remote_ROS_Update(chassis_move_t *chassis);

#endif
