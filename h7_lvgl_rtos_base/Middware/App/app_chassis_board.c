#include "app_chassis_board.h"
#include "app_Navigation.h"
#include "cmsis_os.h"
#include "bsp_uart.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_GPS.h"
#include "usart.h"

chassis_move_t chassis_move;

float Chassis_Vx_set = 0.0f;
float Chassis_Vy_set = 0.0f;
float Chassis_Wz_set = 0.0f;

volatile CarMode_t car_mode = CAR_MODE_GPS;

#define BT_REMOTE_SPEED          20.0f
#define BT_REMOTE_TIMEOUT_MS     300U
#define CHASSIS_GPS_ROUTE_COUNT  3U

static GPS_Point_t chassis_gps_route[CHASSIS_GPS_ROUTE_COUNT] = {
	{26.449591, 106.650651},
	{26.449698, 106.650615},
	{26.449820, 106.650896}
};

/* Bluetooth: F/B/L/R/S for move/stop, I for indoor, G for GPS. */
static volatile uint8_t bt_remote_cmd = 'S';
static volatile uint32_t bt_remote_last_tick = 0U;
static volatile uint8_t car_mode_request_pending = 0U;
static volatile CarMode_t car_mode_request = CAR_MODE_GPS;

static void chassis_stop(void)
{
	Chassis_Vx_set = 0.0f;
	Chassis_Vy_set = 0.0f;
	Chassis_Wz_set = 0.0f;
}

static void chassis_start_gps_navigation(void)
{
	Navigation_Set_Route_Loop(chassis_gps_route, CHASSIS_GPS_ROUTE_COUNT);
}

void Chassis_SetMode(CarMode_t mode)
{
	if (mode != CAR_MODE_GPS && mode != CAR_MODE_INDOOR)
	{
		return;
	}

	Navigation_Stop();
	chassis_stop();

	car_mode = mode;

	if (mode == CAR_MODE_GPS)
	{
		chassis_start_gps_navigation();
	}
}

void Chassis_Bluetooth_RxPro(uint8_t *pBuf, uint16_t Size)
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
				car_mode_request = CAR_MODE_GPS;
				car_mode_request_pending = 1U;
				break;

			case 'I':
			case 'i':
				car_mode_request = CAR_MODE_INDOOR;
				car_mode_request_pending = 1U;
				bt_remote_cmd = 'S';
				bt_remote_last_tick = HAL_GetTick();
				break;

			case 'F':
			case 'f':
			case 'B':
			case 'b':
			case 'L':
			case 'l':
			case 'R':
			case 'r':
			case 'S':
			case 's':
				bt_remote_cmd = pBuf[i];
				bt_remote_last_tick = HAL_GetTick();
				car_mode_request = CAR_MODE_INDOOR;
				car_mode_request_pending = 1U;
				break;

			default:
				break;
		}
	}
}

static void chassis_handle_mode_request(void)
{
	if (car_mode_request_pending)
	{
		CarMode_t request = car_mode_request;
		car_mode_request_pending = 0U;
		Chassis_SetMode(request);
	}
}

static void chassis_indoor_control_update(void)
{
	uint8_t cmd = bt_remote_cmd;

	if (bt_remote_last_tick == 0U ||
		HAL_GetTick() - bt_remote_last_tick > BT_REMOTE_TIMEOUT_MS)
	{
		cmd = 'S';
	}

	switch (cmd)
	{
		case 'F':
		case 'f':
			Chassis_Vx_set = BT_REMOTE_SPEED;
			Chassis_Vy_set = 0.0f;
			Chassis_Wz_set = 0.0f;
			break;

		case 'B':
		case 'b':
			Chassis_Vx_set = -BT_REMOTE_SPEED;
			Chassis_Vy_set = 0.0f;
			Chassis_Wz_set = 0.0f;
			break;

		case 'L':
		case 'l':
			Chassis_Vx_set = 0.0f;
			Chassis_Vy_set = BT_REMOTE_SPEED;
			Chassis_Wz_set = 0.0f;
			break;

		case 'R':
		case 'r':
			Chassis_Vx_set = 0.0f;
			Chassis_Vy_set = -BT_REMOTE_SPEED;
			Chassis_Wz_set = 0.0f;
			break;

		case 'S':
		case 's':
		default:
			chassis_stop();
			break;
	}
}

static void chassis_feedback_update(chassis_move_t *chassis_move_update)
{
	if (chassis_move_update == NULL)
	{
		return;
	}

	QMC5883_GetAngles(&chassis_move_update->qmc_debug_data);

	for (uint8_t i = 0; i < 4; i++)
	{
		chassis_move_update->chassis_motor_MG370[i].speed = Encoder_Rpm_Get(i);
	}
}

static void chassis_init(chassis_move_t *chassis_move_init)
{
	const static double chas_speed_pid_param[3] =
	{
		MOTOR_SPEED_PID_KP,
		MOTOR_SPEED_PID_KI,
		MOTOR_SPEED_PID_KD
	};

	Encoder_Init();
	Motor_Init();
//	uart_init(&huart1, UART_DMA_ToIdle_RX);
	uart_init(&huart2, UART_DMA_ToIdle_RX);

	for (uint8_t i = 0; i < 4; i++)
	{
		PID_init(&chassis_move_init->chas_speed_pid_MG370[i], PID_POSITION,
				 chas_speed_pid_param, MOTOR_SPEED_PID_MAX_OUT, MOTOR_SPEED_PID_MAX_IOUT);
	}
}

static void chassis_control_loop(chassis_move_t *chassis_move_control_loop)
{
	
	chassis_handle_mode_request();

	if (car_mode == CAR_MODE_GPS)
	{
		Navigation_Update_Loop();//导航更新循环
	}
	else
	{
		chassis_indoor_control_update();
	}
	chassis_move_control_loop->chassis_motor_MG370[0].speed_set = Chassis_Vx_set + Chassis_Vy_set + Chassis_Wz_set;
	chassis_move_control_loop->chassis_motor_MG370[1].speed_set = Chassis_Vx_set - Chassis_Vy_set - Chassis_Wz_set;
	chassis_move_control_loop->chassis_motor_MG370[2].speed_set = Chassis_Vx_set - Chassis_Vy_set + Chassis_Wz_set;
	chassis_move_control_loop->chassis_motor_MG370[3].speed_set = Chassis_Vx_set + Chassis_Vy_set - Chassis_Wz_set;

	for (uint8_t i = 0; i < 4; i++)
	{
		PID_Calculate(&chassis_move_control_loop->chas_speed_pid_MG370[i],
					  chassis_move_control_loop->chassis_motor_MG370[i].speed,
					  chassis_move_control_loop->chassis_motor_MG370[i].speed_set);

		Motor_SetPWM((int16_t)chassis_move_control_loop->chas_speed_pid_MG370[i].Out, i);
	}
}

void chassis_task(void *pvParameters)
{
	chassis_init(&chassis_move);
	QMC5883_Init();
	GPS_Init();
	//目标点写在这		
	chassis_start_gps_navigation();
	//Navigation_Set_Route();
	//Navigation_Set_Target(26.449591,106.650651);
	
		
		
	while (1)
	{
		chassis_feedback_update(&chassis_move);
		//Motor_SetAllPWM(20,20,20,20);
		//Chassis_Vx_set = 10;
//		Chassis_Vy_set = 0;
//		Chassis_Wz_set = 0;

		chassis_control_loop(&chassis_move);
		osDelay(10);  	
	}
}
