#include "app_chassis_board.h"
#include "app_Navigation.h"
#include "app_remote_control.h"
#include "app_Voice_Recognition.h"
#include "usbd_cdc_if.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_uart.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_GPS.h"
#include "bsp_bluetooth.h"
#include "bsp_JY901S.h"
#include "bsp_road_classification.h"
#include "bsp_WonderEcho.h"
#include "usart.h"
#include <math.h>
#include <stdio.h>

/* ---- GPS waypoint count ---- */
#define CHASSIS_GPS_ROUTE_COUNT  2U

/* ---- Jetson timeout (ms); no valid frame within this period -> offline ---- */
#define JETSON_TIMEOUT_MS  500U
#define GPS_ONLINE_TIMEOUT_MS  3000U

/* ---- Power-on magnetometer calibration motion (ms / internal Wz units) ---- */
#define MAG_CALIB_WZ                 50.0f
#define MAG_CALIB_STILL_END_MS       2000U
#define MAG_CALIB_CW_END_MS         14000U
#define MAG_CALIB_PAUSE_END_MS      15000U
#define MAG_CALIB_CCW_END_MS        27000U

/* ---- Preset GPS cruise route ---- */
static GPS_Point_t chassis_gps_route[CHASSIS_GPS_ROUTE_COUNT] = {
    {26.449634, 106.650672},
 {26.449691, 106.650650}
		
		
};

/* Sole chassis instance; not directly accessible externally, only via pointer */
static chassis_move_t    chassis_move    = {0};
static volatile int gui_req_mode = -1;
static uint8_t chassis_manual_indoor_mode = 0U;
static volatile uint8_t chassis_init_done = 0U;
static char chassis_init_status[64] = "Init pending";

/*
 * 全局可见的 JY901S 调试镜像。volatile 用于保证即使打开优化，Keil Watch 读取的
 * 也是 RAM 中真实更新的内容。该镜像不被任何控制流程读取。
 */
volatile ChassisJY901SDebug_t g_chassis_jy901s_debug;

static uint8_t chassis_mode_available(chassis_move_t *chassis, CarMode_t mode);
static void chassis_mag_calibration_step(uint32_t elapsed_ms);

/*
 * 复制刚收到的 JY901S 完整数据帧，仅服务于在线调试。
 * update_sequence 前后各加一次，使 Watch 在实时刷新时可确认读取到的是完整快照。
 * 此函数不修改 source，不调用任何 PID/电机/模式函数，也不改动 chassis_move。
 */
static void chassis_update_jy901s_debug_snapshot(const JY901S_Data_t *source)
{
    uint8_t i;

    if (source == NULL)
    {
        return;
    }

    g_chassis_jy901s_debug.update_sequence++;

    for (i = 0U; i < 3U; i++)
    {
        g_chassis_jy901s_debug.acc[i] = source->acc[i];
        g_chassis_jy901s_debug.gyro[i] = source->gyro[i];
        g_chassis_jy901s_debug.angle[i] = source->angle[i];
        g_chassis_jy901s_debug.mag[i] = source->mag[i];
    }

    g_chassis_jy901s_debug.temperature = source->temperature;
    g_chassis_jy901s_debug.update_flag = source->update_flag;
    g_chassis_jy901s_debug.last_update_tick = source->last_update_tick;
    g_chassis_jy901s_debug.online = source->online;

    g_chassis_jy901s_debug.update_sequence++;
}

/*
 * Store an initialization status string for external monitoring.
 * Thread-safe single-writer; read via chassis_get_status_text().
 */
static void chassis_set_init_status(const char *status)
{
    if (status == NULL) return;

    snprintf(chassis_init_status, sizeof(chassis_init_status), "%s", status);
}

void chassis_get_status_text(char *buf, uint32_t size)
{
    if (buf == NULL || size == 0U) return;

    if (qmc5883_calibrating) {
        snprintf(buf, size, "Mag calib %us", (unsigned)qmc5883_calibration_remaining_s);
    } else if (!chassis_init_done) {
        snprintf(buf, size, "%s", chassis_init_status);
    } else {
        snprintf(buf, size, "Modules Ready");
    }
}

/*
 * Copy current PID parameters to all four motor PID controllers.
 * Called after any PID parameter change to keep motors in sync.
 */
static void chassis_apply_pid_params(chassis_move_t *chassis)
{
    uint8_t i;

    if (chassis == NULL) return;

    for (i = 0U; i < 4U; i++)
    {
        chassis->motor[i].speed_pid.Kp = chassis->pid_param.kp;
        chassis->motor[i].speed_pid.Ki = chassis->pid_param.ki;
        chassis->motor[i].speed_pid.Kd = chassis->pid_param.kd;
        chassis->motor[i].speed_pid.OutMax = chassis->pid_param.max_out;
        chassis->motor[i].speed_pid.max_iout = chassis->pid_param.max_iout;
    }
}



/*
 * Copy current chassis telemetry into the provided output structure.
 * Thread-safe snapshot; callable from any task or ISR-deferred context.
 */
void chassis_get_telemetry(ChassisTelemetry_t *out)
{
    uint8_t i;

    if (out == NULL) return;

    taskENTER_CRITICAL();
    out->mode = chassis_move.mode;
    out->gps_lat = chassis_move.date_to_usb.current_lat;
    out->gps_lon = chassis_move.date_to_usb.current_lon;
    out->gps_sats = chassis_move.date_to_usb.current_sats;
    out->gps_last_update_tick = chassis_move.date_to_usb.last_update_tick;
    out->gps_year = chassis_move.date_to_usb.current_year;
    out->gps_month = chassis_move.date_to_usb.current_month;
    out->gps_day = chassis_move.date_to_usb.current_day;
    out->gps_week = chassis_move.date_to_usb.current_week;
    out->gps_hour = chassis_move.date_to_usb.current_hour;
    out->gps_minute = chassis_move.date_to_usb.current_minute;
    out->gps_second = chassis_move.date_to_usb.current_second;
    out->gps_time_update_tick = chassis_move.date_to_usb.time_update_tick;
    out->ins_roll = chassis_move.imu.ins.euler.roll;
    out->ins_pitch = chassis_move.imu.ins.euler.pitch;
    out->ins_yaw = chassis_move.imu.ins.euler.yaw;
    out->ins_last_update_tick = chassis_move.imu.ins_last_update_tick;
    out->mag_yaw = chassis_move.imu.mag.yaw;
    out->mag_pitch = chassis_move.imu.mag.pitch;
    out->mag_roll = chassis_move.imu.mag.roll;
    out->mag_last_update_tick = chassis_move.imu.mag_last_update_tick;
    out->vx_set = chassis_move.Vx_set;
    out->vy_set = chassis_move.Vy_set;
    out->wz_set = chassis_move.Wz_set;
    for (i = 0U; i < 4U; i++)
    {
        out->motor_speed[i] = chassis_move.motor[i].speed;
        out->motor_speed_set[i] = chassis_move.motor[i].speed_set;
        out->motor_last_update_tick[i] = chassis_move.motor[i].last_update_tick;
    }
    out->qmc_calibrating = qmc5883_calibrating;
    out->qmc_calibration_remaining_s = qmc5883_calibration_remaining_s;
    taskEXIT_CRITICAL();
}

/*
 * Copy current chassis settings (gain, remote, PID) into the output structure.
 * Thread-safe snapshot; callable from any task or ISR-deferred context.
 */
void chassis_get_settings(ChassisSettings_t *out)
{
    if (out == NULL) return;

    taskENTER_CRITICAL();
    out->gain = chassis_move.gain;
    out->remote = chassis_move.remote;
    out->pid_param = chassis_move.pid_param;
    taskEXIT_CRITICAL();
}

/*
 * Set a single gain parameter for a specific control mode.
 * Takes effect immediately on the next chassis_set_control() call.
 */
void chassis_set_gain_param(ChassisGainParam_t param, float value)
{
    taskENTER_CRITICAL();
    switch (param)
    {
        case CHASSIS_GAIN_PARAM_GPS:    chassis_move.gain.gps = value;    break;
        case CHASSIS_GAIN_PARAM_INDOOR: chassis_move.gain.indoor = value; break;
        case CHASSIS_GAIN_PARAM_REMOTE: chassis_move.gain.remote = value; break;
        case CHASSIS_GAIN_PARAM_LINE:   chassis_move.gain.line = value;   break;
        case CHASSIS_GAIN_PARAM_VOICE:  chassis_move.gain.voice = value;  break;
        default: break;
    }
    taskEXIT_CRITICAL();
}

/*
 * Set a single remote control parameter.
 * Takes effect immediately on the next remote control update cycle.
 */
void chassis_set_remote_param(ChassisRemoteParam_t param, float value)
{
    taskENTER_CRITICAL();
    switch (param)
    {
        case CHASSIS_REMOTE_PARAM_BT_SPEED:        chassis_move.remote.bt_speed = value; break;
        case CHASSIS_REMOTE_PARAM_BT_WZ:           chassis_move.remote.bt_wz = value; break;
        case CHASSIS_REMOTE_PARAM_WECHAT_VX_SCALE: chassis_move.remote.wechat_vx_scale = value; break;
        case CHASSIS_REMOTE_PARAM_ROS_MAX_SPEED:   chassis_move.remote.ros_max_speed = value; break;
        default: break;
    }
    taskEXIT_CRITICAL();
}

/*
 * Set a single PID parameter and re-apply all PID params to all four motors.
 * Calling this while motors are running may cause a brief control transient.
 */
void chassis_set_pid_param(ChassisPidParam_t param, double value)
{
    taskENTER_CRITICAL();
    switch (param)
    {
        case CHASSIS_PID_PARAM_KP:      chassis_move.pid_param.kp = value; break;
        case CHASSIS_PID_PARAM_KI:      chassis_move.pid_param.ki = value; break;
        case CHASSIS_PID_PARAM_KD:      chassis_move.pid_param.kd = value; break;
        case CHASSIS_PID_PARAM_MAX_OUT: chassis_move.pid_param.max_out = value; break;
        default: break;
    }
    chassis_apply_pid_params(&chassis_move);
    taskEXIT_CRITICAL();
}

/*
 * Request a mode switch from the GUI / touch interface.
 * The request is processed in the next chassis_mode_change() cycle.
 * Invalid or unavailable mode requests are silently ignored there.
 */
void chassis_request_mode(CarMode_t mode)
{
    if (mode > CAR_MODE_VOICE) return;

    taskENTER_CRITICAL();
    gui_req_mode = (int)mode;
    taskEXIT_CRITICAL();
}


/* ============================================================
 *  Internal: Stop (zero all omnidirectional target speeds)
 * ============================================================ */
static void chassis_stop(chassis_move_t *chassis)
{
    chassis->Vx_set = 0.0f;
    chassis->Vy_set = 0.0f;
    chassis->Wz_set = 0.0f;
}

static void chassis_clear_motor_output(chassis_move_t *chassis)
{
    uint8_t i;

    if (chassis == NULL) return;

    for (i = 0U; i < 4U; i++)
    {
        chassis->motor[i].speed_set = 0;
        chassis->motor[i].speed_pid.Target = 0.0;
        chassis->motor[i].speed_pid.Out = 0.0;
        chassis->motor[i].speed_pid.Error0 = 0.0;
        chassis->motor[i].speed_pid.Error1 = 0.0;
        chassis->motor[i].speed_pid.ErrorInt = 0.0;
        chassis->motor[i].speed_pid.D_error = 0.0;
    }
}

/* ============================================================
 *  Internal: Start GPS cyclic cruise
 * ============================================================ */
static void chassis_start_gps_navigation(chassis_move_t *chassis)
{
    Navigation_Set_Route_Loop(&chassis->nav,
                              chassis_gps_route,
                              CHASSIS_GPS_ROUTE_COUNT);
}

static uint8_t chassis_is_jetson_online(chassis_move_t *chassis)
{
    if (chassis == NULL) return 0U;

    return (HAL_GetTick() - chassis->jetson_last_tick < JETSON_TIMEOUT_MS) ? 1U : 0U;
}

/*
 * Return 1 if the given year is a leap year, 0 otherwise.
 * Handles Gregorian rules: every 4th year, except centuries, but including every 400th.
 */
static uint8_t chassis_is_leap_year(uint16_t year)
{
    return ((year % 4U == 0U && year % 100U != 0U) || (year % 400U == 0U)) ? 1U : 0U;
}

/*
 * Return the number of days in a given month of a given year.
 * Handles February leap-year adjustment automatically.
 * Returns 31 for invalid month numbers.
 */
static uint8_t chassis_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if (month < 1U || month > 12U) return 31U;
    if (month == 2U && chassis_is_leap_year(year)) return 29U;

    return days[month - 1U];
}

/*
 * Return the day of the week (0 = Sunday .. 6 = Saturday) for a given date.
 * Uses a Zeller-like congruence formula. Returns 0 for invalid inputs.
 */
static uint8_t chassis_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t offset[12] = {0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U};
    uint16_t y = year;

    if (month < 1U || month > 12U || day < 1U) return 0U;
    if (month < 3U) y--;

    return (uint8_t)((y + y / 4U - y / 100U + y / 400U + offset[month - 1U] + day) % 7U);
}

/*
 * Convert UTC GPS time from the AGRIC sentence to local time (UTC+8) and store it
 * in chassis->date_to_usb. Skips stale data (age > GPS_ONLINE_TIMEOUT_MS) and
 * validates all time fields before writing.
 */
static void chassis_store_gps_time(chassis_move_t *chassis, PT_AGRIC agric, uint32_t now)
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;

    if (chassis == NULL || agric == NULL) return;
    if (agric->last_update_tick == 0U || agric->Month == 0U || agric->Day == 0U) return;
    if ((now - agric->last_update_tick) > GPS_ONLINE_TIMEOUT_MS) return;
    if (agric->Hour > 23U || agric->Minute > 59U || agric->Second > 59U) return;

    year = (agric->Year < 100U) ? (uint16_t)(2000U + agric->Year) : (uint16_t)agric->Year;
    month = agric->Month;
    day = agric->Day;
    hour = agric->Hour;

    if (month < 1U || month > 12U || day > chassis_days_in_month(year, month)) return;

    hour = (uint8_t)(hour + 8U);
    if (hour >= 24U)
    {
        hour = (uint8_t)(hour - 24U);
        day++;
        if (day > chassis_days_in_month(year, month))
        {
            day = 1U;
            month++;
            if (month > 12U)
            {
                month = 1U;
                year++;
            }
        }
    }

    chassis->date_to_usb.current_year = year;
    chassis->date_to_usb.current_month = month;
    chassis->date_to_usb.current_day = day;
    chassis->date_to_usb.current_week = chassis_weekday(year, month, day);
    chassis->date_to_usb.current_hour = hour;
    chassis->date_to_usb.current_minute = agric->Minute;
    chassis->date_to_usb.current_second = agric->Second;
    chassis->date_to_usb.time_update_tick = now;
}

static uint8_t chassis_is_gps_online(chassis_move_t *chassis)
{
    uint32_t tick;

    if (chassis == NULL) return 0U;

    tick = chassis->date_to_usb.last_update_tick;
    return (tick != 0U && (HAL_GetTick() - tick) <= GPS_ONLINE_TIMEOUT_MS) ? 1U : 0U;
}

static uint8_t chassis_is_bt_online(void)
{
    return (BT_IsOnline() || BT_IsActive()) ? 1U : 0U;
}

/*
 * Check whether a given operating mode is available based on current sensor and
 * communication link status. Returns 1 if available, 0 otherwise.
 * Used by mode arbitration to prevent switching into a mode whose prerequisites
 * are not met (e.g., GPS mode without GPS fix).
 */
static uint8_t chassis_mode_available(chassis_move_t *chassis, CarMode_t mode)
{
    uint8_t jetson_online;
    uint8_t jetson_mode;

    if (chassis == NULL) return 0U;

    jetson_online = chassis_is_jetson_online(chassis);
    jetson_mode = chassis->cmd_vel.mode;

    switch (mode)
    {
        case CAR_MODE_IDLE:
            return 1U;

        case CAR_MODE_GPS:
            return chassis_is_gps_online(chassis);

        case CAR_MODE_GPS_ROS:
            return (chassis_is_gps_online(chassis) &&
                    jetson_online &&
                    jetson_mode == JETSON_MODE_GPS) ? 1U : 0U;

        case CAR_MODE_REMOTE:
            return (jetson_online && jetson_mode == JETSON_MODE_REMOTE) ? 1U : 0U;

        case CAR_MODE_LINE:
            return (jetson_online && jetson_mode == JETSON_MODE_LINE) ? 1U : 0U;

        case CAR_MODE_INDOOR:
            return chassis_is_bt_online();

        case CAR_MODE_VOICE:
            return App_Voice_IsActive();

        default:
            return 0U;
    }
}



/* ============================================================
 *  Public: Switch vehicle operating mode.
 *  Retains navigation state only when switching between nav modes; resets otherwise.
 * ============================================================ */
static void Chassis_SetMode(chassis_move_t *chassis, CarMode_t mode)
{
    uint8_t was_nav_mode;
    uint8_t is_nav_mode;

    if (chassis == NULL) return;

    was_nav_mode = (chassis->mode == CAR_MODE_GPS || chassis->mode == CAR_MODE_GPS_ROS) ? 1U : 0U;
    is_nav_mode = (mode == CAR_MODE_GPS || mode == CAR_MODE_GPS_ROS) ? 1U : 0U;

    if (!was_nav_mode || !is_nav_mode)
    {
        Navigation_Stop(chassis);
    }
    chassis_stop(chassis);
    chassis->mode = mode;

    if (mode != CAR_MODE_VOICE)
    {
        App_Voice_Clear();
    }

    if (!was_nav_mode && is_nav_mode)
    {
        chassis_start_gps_navigation(chassis);
    }
}

/* ============================================================
 *  Sensor data refresh: Magnetometer + JY901S IMU + Encoders + USB + GPS
 *  Called each cycle before the control loop; records per-sensor update timestamps.
 * ============================================================ */
void chassis_feedback_update(chassis_move_t *chassis)
{
    uint32_t now;

    if (chassis == NULL) return;

    now = HAL_GetTick();

    /* Magnetometer -> chassis->imu.mag */
    QMC5883_GetAngles(&chassis->imu.mag);
    chassis->imu.mag_last_update_tick = now;

    {
        JY901S_Data_t jy901s_data;
        if (JY901S_GetData(&jy901s_data))
        {
            chassis->imu.jy901s = jy901s_data;
            /*
             * 将“已经被底盘采用”的同一帧复制到全局调试镜像，便于 Keil 观察。
             * 这行只增加 RAM 镜像，不会影响后续姿态、运动学、PID 或电机输出。
             */
            chassis_update_jy901s_debug_snapshot(&jy901s_data);
            chassis->imu.ins.euler.roll = jy901s_data.angle[0];
            chassis->imu.ins.euler.pitch = jy901s_data.angle[1];
            chassis->imu.ins.euler.yaw = jy901s_data.angle[2];
            chassis->imu.ins_last_update_tick = jy901s_data.last_update_tick;
        }
    }

    /* Encoders -> chassis->motor[i].speed */
    for (uint8_t i = 0; i < 4; i++)
    {
        chassis->motor[i].speed = Encoder_Rpm_Get(i);
        chassis->motor[i].last_update_tick = now;
    }

    /* USB Jetson 12-byte frame -> chassis->cmd_vel */
    if (usb_rx_flag)
    {
        USB_ProcessRxData(UserRxBufferFS, (uint16_t)usb_rx_len);
        usb_rx_flag = 0;
    }
    chassis->cmd_vel = USB_GetCmdVel();
    if (chassis->cmd_vel.mode != 0)
    {
        chassis->jetson_last_tick = HAL_GetTick();
    }

    /* GPS position -> chassis->date_to_usb (NMEA to decimal degrees) */
    {
        PT_GNGGA pGGA = GetGNGGA();
        if (pGGA->qf >= 1 && pGGA->lat >= 1.0f)
        {
            double lat_deg = floor(pGGA->lat / 100.0)
                           + (pGGA->lat - floor(pGGA->lat / 100.0) * 100.0) / 60.0;
            double lon_deg = floor(pGGA->lon / 100.0)
                           + (pGGA->lon - floor(pGGA->lon / 100.0) * 100.0) / 60.0;
            chassis->date_to_usb.current_lat = (float)lat_deg;
            chassis->date_to_usb.current_lon = (float)lon_deg;
            chassis->date_to_usb.current_sats = pGGA->sats;
            chassis->date_to_usb.last_update_tick = now;
        }
    }

    /* Target heading relative to vehicle heading [0, 360) */
    {
        PT_AGRIC pAGRIC = GetAGRIC();
        chassis_store_gps_time(chassis, pAGRIC, now);
    }

    {
        float tgt = chassis->nav.target_bearing;
        float yaw = chassis->imu.mag.yaw;
        float diff = tgt - yaw;
        while (diff < 0.0f)    diff += 360.0f;
        while (diff >= 360.0f) diff -= 360.0f;
        chassis->date_to_usb.heading_to_target_deg = diff;
    }
}

/* ============================================================
 *  Init: Encoders / Motors / Bluetooth / UART / IMU / Remote params / Gain / PID
 *  Called once from the FreeRTOS task.
 * ============================================================ */
static void chassis_init(chassis_move_t *chassis)
{
    double speed_pid_param[3];

    chassis_set_init_status("Init Encoder");
    Encoder_Init();
    chassis_set_init_status("Init Motor");
    Motor_Init();
    chassis_set_init_status("Init Bluetooth");
    BT_Init();
    chassis_set_init_status("Init USB");
    USB_Init();
    chassis_set_init_status("Init UART1");
    uart_init(&huart1, UART_DMA_ToIdle_RX);
    chassis_set_init_status("Init UART2");
    uart_init(&huart2, UART_DMA_ToIdle_RX);
    chassis_set_init_status("Init JY901S");
    JY901S_Init();

    /*
     * 路面识别只维护自己的 NanoEdge 缓冲和结果快照。即使模型初始化失败，
     * 底盘初始化与后续控制仍按原有路径继续，绝不因此改变 PID 或电机输出。
     */
    chassis_set_init_status("Init Road AI");
    (void)BSP_RoadClassification_Init();

    /* Remote control parameter defaults */
    chassis->remote.bt_speed         = BT_REMOTE_SPEED;
    chassis->remote.bt_wz            = BT_REMOTE_WZ;
    chassis->remote.wechat_vx_scale  = WECHAT_VX_SCALE;
    chassis->remote.wechat_vz_scale  = WECHAT_VZ_SCALE;
    chassis->remote.wechat_speed_gain = WECHAT_SPEED_GAIN;
    chassis->remote.wechat_max_speed = WECHAT_MAX_SPEED;
    chassis->remote.wechat_max_wz    = WECHAT_MAX_WZ;
    chassis->remote.ros_vx_scale     = ROS_LINE_VX_SCALE;
    chassis->remote.ros_vz_scale     = ROS_LINE_VZ_SCALE;
    chassis->remote.ros_max_speed    = ROS_LINE_MAX_SPEED;
    chassis->remote.ros_max_wz       = ROS_LINE_MAX_WZ;

    /* Per-mode control gain defaults */
    chassis->gain.gps    = CHASSIS_GAIN_GPS;
    chassis->gain.indoor = CHASSIS_GAIN_INDOOR;
    chassis->gain.remote = CHASSIS_GAIN_REMOTE;
    chassis->gain.line   = CHASSIS_GAIN_LINE;
    chassis->gain.voice  = CHASSIS_GAIN_VOICE;

    chassis->pid_param.kp = MOTOR_SPEED_PID_KP;
    chassis->pid_param.ki = MOTOR_SPEED_PID_KI;
    chassis->pid_param.kd = MOTOR_SPEED_PID_KD;
    chassis->pid_param.max_out = MOTOR_SPEED_PID_MAX_OUT;
    chassis->pid_param.max_iout = MOTOR_SPEED_PID_MAX_IOUT;
    speed_pid_param[0] = chassis->pid_param.kp;
    speed_pid_param[1] = chassis->pid_param.ki;
    speed_pid_param[2] = chassis->pid_param.kd;

    /* WonderEcho needs brief stabilization after power-on, then init PB0/PB1 software I2C.
       Init remains in the chassis task; no new FreeRTOS task, avoiding concurrent chassis speed writes. */
    chassis_set_init_status("Init Voice");
    osDelay(200);
    WonderEcho_I2C_Init();

    chassis_set_init_status("Init PID");
    for (uint8_t i = 0; i < 4; i++)
    {
        PID_init(&chassis->motor[i].speed_pid, PID_POSITION,
                 speed_pid_param,
                 chassis->pid_param.max_out,
                 chassis->pid_param.max_iout);
    }
    for (uint8_t i = 0; i < 4; i++)
    {
        chassis->motor[i].speed_set = 0;
    }
    chassis_set_init_status("Chassis Init OK");
}

/* ============================================================
 *  Step 4: Chassis core control loop.
 *    Vx/Vy/Wz (written by step 3) -> kinematics decomposition -> PID calculation.
 *    PID output is sent to motors uniformly by step 5 chassis_send_cmd.
 * ============================================================ */
void chassis_control_loop(chassis_move_t *chassis)
{
    if (chassis == NULL) return;

    /* 1. Omnidirectional kinematics: Vx/Vy/Wz -> 4 motor target speeds */
    chassis->motor[0].speed_set =  chassis->Vx_set + chassis->Vy_set + chassis->Wz_set;
    chassis->motor[1].speed_set =  chassis->Vx_set - chassis->Vy_set - chassis->Wz_set;
    chassis->motor[2].speed_set =  chassis->Vx_set - chassis->Vy_set + chassis->Wz_set;
    chassis->motor[3].speed_set =  chassis->Vx_set + chassis->Vy_set - chassis->Wz_set;

    /* 2. PID speed closed loop (result stored in motor[i].speed_pid.Out) */
    for (uint8_t i = 0; i < 4; i++)
    {
        PID_Calculate(&chassis->motor[i].speed_pid,
                      chassis->motor[i].speed,
                      chassis->motor[i].speed_set);
    }
}

/* ============================================================
 *  Power-on QMC5883 calibration motion step (called every 10 ms).
 *  Keeps the original magnetometer sampling/calculation unchanged while
 *  using the existing encoder feedback, kinematics and motor speed PID.
 * ============================================================ */
static void chassis_mag_calibration_step(uint32_t elapsed_ms)
{
    uint8_t i;
    uint32_t now;

    now = HAL_GetTick();

    for (i = 0U; i < 4U; i++)
    {
        chassis_move.motor[i].speed = Encoder_Rpm_Get(i);
        chassis_move.motor[i].last_update_tick = now;
    }

    chassis_move.Vx_set = 0.0f;
    chassis_move.Vy_set = 0.0f;
    chassis_move.Wz_set = 0.0f;

    if (elapsed_ms >= MAG_CALIB_STILL_END_MS && elapsed_ms < MAG_CALIB_CW_END_MS)
    {
        chassis_move.Wz_set = -MAG_CALIB_WZ;
    }
    else if (elapsed_ms >= MAG_CALIB_PAUSE_END_MS && elapsed_ms < MAG_CALIB_CCW_END_MS)
    {
        chassis_move.Wz_set = MAG_CALIB_WZ;
    }

    chassis_control_loop(&chassis_move);

    for (i = 0U; i < 4U; i++)
    {
        Motor_SetPWM((int16_t)chassis_move.motor[i].speed_pid.Out, i);
    }
}

/* ============================================================
 *  Step 1: Chassis control mode arbitration.
 *  Priority: GUI request > Bluetooth > Manual hold > Voice > Jetson -> auto fallback to IDLE.
 * ============================================================ */
void chassis_mode_change(chassis_move_t *chassis)
{
    BT_ModeReq_t req;
    CarMode_t requested_mode;
    uint8_t jetson_online;
    uint8_t jetson_mode;

    if (chassis == NULL) return;

    if (gui_req_mode >= 0 && gui_req_mode <= 6) {
        requested_mode = (CarMode_t)gui_req_mode;
        if (chassis_mode_available(chassis, requested_mode))
        {
            chassis_manual_indoor_mode = (requested_mode == CAR_MODE_INDOOR) ? 1U : 0U;
            Chassis_SetMode(chassis, requested_mode);
        }
        else if (!chassis_mode_available(chassis, chassis->mode))
        {
            chassis_manual_indoor_mode = 0U;
            Chassis_SetMode(chassis, CAR_MODE_IDLE);
        }
        gui_req_mode = -1;
        return;
    }

    req = BT_GetAndClearModeReq();
    if (req != BT_MODE_REQ_NONE)
    {
    }

    switch (req)
    {
        case BT_MODE_REQ_GPS:
            chassis_manual_indoor_mode = 0U;
            if (chassis_mode_available(chassis, CAR_MODE_GPS))
            {
                Chassis_SetMode(chassis, CAR_MODE_GPS);
            }
            else if (!chassis_mode_available(chassis, chassis->mode))
            {
                Chassis_SetMode(chassis, CAR_MODE_IDLE);
            }
            return;

        case BT_MODE_REQ_INDOOR:
            if (chassis_mode_available(chassis, CAR_MODE_INDOOR))
            {
                chassis_manual_indoor_mode = 1U;
                Chassis_SetMode(chassis, CAR_MODE_INDOOR);
            }
            else if (!chassis_mode_available(chassis, chassis->mode))
            {
                chassis_manual_indoor_mode = 0U;
                Chassis_SetMode(chassis, CAR_MODE_IDLE);
            }
            return;

        default:
            break;
    }

    if (!chassis_mode_available(chassis, chassis->mode))
    {
        if (chassis->mode == CAR_MODE_GPS_ROS && chassis_mode_available(chassis, CAR_MODE_GPS))
        {
            Chassis_SetMode(chassis, CAR_MODE_GPS);
        }
        else if (chassis->mode != CAR_MODE_IDLE)
        {
            Chassis_SetMode(chassis, CAR_MODE_IDLE);
        }
        return;
    }

    if (chassis->mode == CAR_MODE_GPS && chassis_mode_available(chassis, CAR_MODE_GPS_ROS))
    {
        Chassis_SetMode(chassis, CAR_MODE_GPS_ROS);
        return;
    }

    if (BT_IsActive() && chassis_mode_available(chassis, CAR_MODE_INDOOR))
    {
        chassis_manual_indoor_mode = 1U;
        if (chassis->mode != CAR_MODE_INDOOR)
        {
            Chassis_SetMode(chassis, CAR_MODE_INDOOR);
        }
        return;
    }

    if (chassis_manual_indoor_mode)
    {
        if (chassis_mode_available(chassis, CAR_MODE_INDOOR))
        {
            if (chassis->mode != CAR_MODE_INDOOR)
            {
                Chassis_SetMode(chassis, CAR_MODE_INDOOR);
            }
        }
        else
        {
            chassis_manual_indoor_mode = 0U;
            if (chassis->mode != CAR_MODE_IDLE)
            {
                Chassis_SetMode(chassis, CAR_MODE_IDLE);
            }
        }
        return;
    }

    if (App_Voice_IsActive() && chassis_mode_available(chassis, CAR_MODE_VOICE))
    {
        if (chassis->mode != CAR_MODE_VOICE)
        {
            Chassis_SetMode(chassis, CAR_MODE_VOICE);
        }
        return;
    }

    jetson_online = chassis_is_jetson_online(chassis);
    jetson_mode = chassis->cmd_vel.mode;

    if (jetson_online)
    {
        if (jetson_mode == JETSON_MODE_REMOTE)
        {
            if (chassis_mode_available(chassis, CAR_MODE_REMOTE) && chassis->mode != CAR_MODE_REMOTE)
            {
                Chassis_SetMode(chassis, CAR_MODE_REMOTE);
            }
            return;
        }

        if (jetson_mode == JETSON_MODE_LINE)
        {
            if (chassis_mode_available(chassis, CAR_MODE_LINE) && chassis->mode != CAR_MODE_LINE)
            {
                Chassis_SetMode(chassis, CAR_MODE_LINE);
            }
            return;
        }

        if (jetson_mode == JETSON_MODE_GPS)
        {
            if (chassis_mode_available(chassis, CAR_MODE_GPS_ROS) && chassis->mode != CAR_MODE_GPS_ROS)
            {
                Chassis_SetMode(chassis, CAR_MODE_GPS_ROS);
            }
            return;
        }
    }

    if (chassis->mode == CAR_MODE_REMOTE || chassis->mode == CAR_MODE_LINE)
    {
        Chassis_SetMode(chassis, CAR_MODE_IDLE);
    }
}

/* ============================================================
 *  Internal: Apply per-mode control gain.
 *  Called at the end of chassis_set_control; scales Vx/Vy/Wz uniformly.
 * ============================================================ */
static void chassis_apply_gain(chassis_move_t *chassis)
{
    float gain = 1.0f;

    if (chassis == NULL) return;

    switch (chassis->mode)
    {
        case CAR_MODE_IDLE:
            gain = 1.0f;
            break;

        case CAR_MODE_GPS:    gain = chassis->gain.gps;    break;
        case CAR_MODE_GPS_ROS: gain = chassis->gain.gps;   break;
        case CAR_MODE_INDOOR: gain = chassis->gain.indoor; break;
        case CAR_MODE_REMOTE: gain = chassis->gain.remote; break;
        case CAR_MODE_LINE:   gain = chassis->gain.line;   break;
        case CAR_MODE_VOICE:  gain = chassis->gain.voice;  break;
        default: break;
    }

    chassis->Vx_set *= gain;
    chassis->Vy_set *= gain;
    chassis->Wz_set *= gain;
}

/* ============================================================
 *  Step 3: Set chassis control targets.
 *  Writes Vx/Vy/Wz according to current mode; applies mode gain at the end.
 * ============================================================ */
void chassis_set_control(chassis_move_t *chassis)
{
    uint8_t jetson_online;
    uint8_t jetson_mode;

    if (chassis == NULL) return;

    jetson_online = chassis_is_jetson_online(chassis);
    jetson_mode = chassis->cmd_vel.mode;

    switch (chassis->mode)
    {
        case CAR_MODE_IDLE:
            chassis_stop(chassis);
            break;

        case CAR_MODE_INDOOR:
            if (BT_IsActive())
            {
                Remote_Control_Update(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_REMOTE:
            if (jetson_online && jetson_mode == JETSON_MODE_REMOTE)
            {
                Remote_WeChat_Update(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_LINE:
            if (jetson_online && jetson_mode == JETSON_MODE_LINE)
            {
                Remote_ROS_Update(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_GPS:
            if (chassis_mode_available(chassis, CAR_MODE_GPS))
            {
                Navigation_Update_Loop(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_GPS_ROS:
            if (jetson_online && jetson_mode == JETSON_MODE_GPS && chassis_mode_available(chassis, CAR_MODE_GPS_ROS))
            {
                Navigation_Update_Loop_Fusion(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        case CAR_MODE_VOICE:
            if (App_Voice_IsActive())
            {
                App_Voice_ApplyControl(chassis);
            }
            else
            {
                chassis_stop(chassis);
            }
            break;

        default:
            chassis_stop(chassis);
            break;
    }

    chassis_apply_gain(chassis);
}

/* ============================================================
 *  Step 5: Send chassis control commands.
 *    IDLE mode: clear motors + send telemetry.
 *    Normal mode: PID output -> motor PWM + USB telemetry to Jetson.
 * ============================================================ */
static void chassis_send_bt_ack(void);

void chassis_send_cmd(chassis_move_t *chassis)
{
    if (chassis == NULL) return;

    chassis_send_bt_ack();

    if (chassis->mode == CAR_MODE_IDLE)
    {
        chassis_clear_motor_output(chassis);
        for (uint8_t i = 0; i < 4; i++)
        {
            Motor_SetPWM(0, i);
        }
        USB_SendTelemetry(chassis->date_to_usb.heading_to_target_deg,
                          chassis->date_to_usb.current_lat,
                          chassis->date_to_usb.current_lon);
        return;
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        Motor_SetPWM((int16_t)chassis->motor[i].speed_pid.Out, i);
    }

    USB_SendTelemetry(chassis->date_to_usb.heading_to_target_deg,
                      chassis->date_to_usb.current_lat,
                      chassis->date_to_usb.current_lon);
}

static void chassis_send_bt_ack(void)
{
    uint8_t ack_count = BT_GetAndClearAckCount();
    static const uint8_t ack_msg[] = {'o', 'k', '\r', '\n'};

    while (ack_count > 0U)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)ack_msg, sizeof(ack_msg), 10U);
        ack_count--;
    }
}


/* ============================================================
 *  FreeRTOS task entry
 * ============================================================ */
void chassis_task(void *pvParameters)
{
    uint32_t voice_tick = 0U;

    /* -- One-time initialization -- */ 
    chassis_init(&chassis_move);
    chassis_set_init_status("Init QMC5883");
	//QMC5883_SetCalibrationStepCallback(chassis_mag_calibration_step);
	//QMC5883_Init();
	//QMC5883_SetCalibrationStepCallback(NULL);
	Motor_SetAllPWM(0, 0, 0, 0);
	chassis_stop(&chassis_move);
	chassis_clear_motor_output(&chassis_move);
    chassis_set_init_status("Init GPS");
	GPS_Init();
    chassis_set_init_status("Modules Ready");
    chassis_init_done = 1U;

//		/* 路面识别直行测试：仅本次测试使用 */
//chassis_move.mode = CAR_MODE_INDOOR;  /* 只用于避开 IDLE 的强制停车；不执行蓝牙控制 */
//chassis_move.Vx_set = 100.0f;          /* 先从 30 开始测试 */
//chassis_move.Vy_set = 0.0f;           /* 不横移 */
//chassis_move.Wz_set = 0.0f;           /* 不旋转 */
    /* -- Default mode; chassis_mode_change auto-arbitrates based on availability -- */
    /* -- Main loop (100Hz) -- */
	
    while (1)
    {
        chassis_feedback_update(&chassis_move);   /* Sensors + USB data refresh */

        /*
         * 复用现有 10 ms 底盘任务，不创建新任务。BSP 内部会自动忽略重复的
         * JY901S 帧，仅在收集满 32 个新 IMU 帧后推理；它不写入 chassis_move。
         */
        BSP_RoadClassification_Process(&chassis_move.imu.jy901s);

        if (HAL_GetTick() - voice_tick >= 50U)
        {
            voice_tick = HAL_GetTick();
            App_Voice_Recognition_Update(&chassis_move);
        }
        
        chassis_mode_change(&chassis_move);       /* Mode switch */
		    chassis_set_control(&chassis_move);       /* Control targets (priority-adjusted) */
        chassis_control_loop(&chassis_move);      /* Kinematics + PID */
        chassis_send_cmd(&chassis_move);          /* Motor output + USB telemetry */
        osDelay(10);
    }
}
