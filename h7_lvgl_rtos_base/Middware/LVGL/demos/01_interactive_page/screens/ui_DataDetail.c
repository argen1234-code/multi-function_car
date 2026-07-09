/**
 * ui_DataDetail.c --- Detail views for GPS/Speed/IMU/Mode panels
 *
 * Uses tabview: 0=GPS, 1=Speed, 2=IMU, 3=Mode.
 * Mode tab has a dial to switch car mode.
 */
#include "../ui.h"
#include "../ui_helpers.h"
#include "app_chassis_board.h"
#include <stdio.h>

#define COMPASS_CX          150
#define COMPASS_CY          145
#define COMPASS_RING_SIZE   230
#define COMPASS_LABEL_R     94
#define COMPASS_TRIGO_SCALE 32767L
#define SENSOR_TIMEOUT_MS   1500U
#define GPS_TIMEOUT_MS      3000U
#define WHEEL_TARGET_ACTIVE_RPM   10.0f
#define WHEEL_FEEDBACK_ACTIVE_RPM 3.0f
#define WHEEL_ERROR_RPM           35.0f
#define MODE_DIAL_COUNT           7U

lv_obj_t *ui_DataDetail = NULL;
static lv_obj_t *g_tv = NULL;

/* Detail labels for live update */
static lv_obj_t *dl_gps   = NULL;
static lv_obj_t *dl_speed = NULL;
static lv_obj_t *dl_imu   = NULL;
static lv_obj_t *dl_mode  = NULL;
static lv_obj_t *compass_heading = NULL;
static lv_obj_t *compass_cardinal[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *compass_marker = NULL;
static lv_point_t compass_marker_points[2];
static lv_obj_t *mode_dial_buttons[MODE_DIAL_COUNT] = {NULL};
static lv_obj_t *mode_needle = NULL;
static lv_point_t mode_needle_points[2];
static lv_timer_t *detail_refresh_timer = NULL;


static const char * const mode_names[MODE_DIAL_COUNT] = {"No Power","GPS Nav","GPS+ROS","WeChat","ROS Indoor","Bluetooth","Voice"};
static int mode_dial_indices[MODE_DIAL_COUNT] = {0, 1, 2, 3, 4, 5, 6};
static ChassisTelemetry_t detail_data;

static void mode_dial_refresh(void);

static void detail_update_data(void)
{
	chassis_get_telemetry(&detail_data);
}

static uint8_t sensor_tick_online(uint32_t tick, uint32_t timeout)
{
	uint32_t now = HAL_GetTick();

	return (tick != 0U && (now - tick) <= timeout) ? 1U : 0U;
}

static uint8_t motor_speed_online(void)
{
	uint8_t i;

	for (i = 0U; i < 4U; i++) {
		if (sensor_tick_online(detail_data.motor_last_update_tick[i], SENSOR_TIMEOUT_MS)) {
			return 1U;
		}
	}

	return 0U;
}

static float ui_absf(float value)
{
	return (value < 0.0f) ? -value : value;
}

static uint8_t wheel_online(uint8_t index)
{
	if (index >= 4U) return 0U;

	return sensor_tick_online(detail_data.motor_last_update_tick[index], SENSOR_TIMEOUT_MS);
}

static const char *wheel_status_text(uint8_t index)
{
	float target;
	float feedback;
	float err;

	if (!wheel_online(index)) return "OFFLINE";

	target = (float)detail_data.motor_speed_set[index];
	feedback = (float)detail_data.motor_speed[index];

	if (ui_absf(target) <= WHEEL_TARGET_ACTIVE_RPM) return "IDLE";
	if (ui_absf(feedback) < WHEEL_FEEDBACK_ACTIVE_RPM) return "STUCK";
	if ((target > 0.0f && feedback < -WHEEL_FEEDBACK_ACTIVE_RPM) ||
	    (target < 0.0f && feedback > WHEEL_FEEDBACK_ACTIVE_RPM)) return "REVERSE";

	err = ui_absf(target - feedback);
	return (err > WHEEL_ERROR_RPM) ? "ERROR" : "OK";
}

static void speed_detail_refresh(void)
{
	char buf[320];

	if (dl_speed == NULL) return;

	if (!motor_speed_online()) {
		lv_label_set_text(dl_speed, "Wheel encoder: OFFLINE");
		return;
	}

	snprintf(buf, sizeof(buf),
	         "Target: Vx %.1f  Vy %.1f  Wz %.1f\n\n"
	         "FL set %.1f  enc %.1f  %s\n"
	         "FR set %.1f  enc %.1f  %s\n"
	         "RL set %.1f  enc %.1f  %s\n"
	         "RR set %.1f  enc %.1f  %s",
	         detail_data.vx_set,
	         detail_data.vy_set,
	         detail_data.wz_set,
	         (float)detail_data.motor_speed_set[0],
	         (float)detail_data.motor_speed[0],
	         wheel_status_text(0U),
	         (float)detail_data.motor_speed_set[1],
	         (float)detail_data.motor_speed[1],
	         wheel_status_text(1U),
	         (float)detail_data.motor_speed_set[2],
	         (float)detail_data.motor_speed[2],
	         wheel_status_text(2U),
	         (float)detail_data.motor_speed_set[3],
	         (float)detail_data.motor_speed[3],
	         wheel_status_text(3U));
	lv_label_set_text(dl_speed, buf);
}

static int16_t compass_norm_angle(float angle)
{
	int32_t deg = (int32_t)(angle + 0.5f);

	while (deg < 0) deg += 360;
	while (deg >= 360) deg -= 360;

	return (int16_t)deg;
}

static lv_coord_t compass_sin(int16_t deg, lv_coord_t radius)
{
	return (lv_coord_t)(((int32_t)lv_trigo_sin(deg) * radius) / COMPASS_TRIGO_SCALE);
}

static lv_coord_t compass_cos(int16_t deg, lv_coord_t radius)
{
	return (lv_coord_t)(((int32_t)lv_trigo_cos(deg) * radius) / COMPASS_TRIGO_SCALE);
}

static void compass_center_obj(lv_obj_t *obj, lv_coord_t x, lv_coord_t y)
{
	if (obj == NULL) return;

	lv_obj_update_layout(obj);
	lv_obj_set_pos(obj, x - lv_obj_get_width(obj) / 2, y - lv_obj_get_height(obj) / 2);
}

static void compass_place_cardinal(lv_obj_t *label, int16_t base_deg, float heading)
{
	int16_t deg;
	lv_coord_t x;
	lv_coord_t y;

	if (label == NULL) return;

	deg = compass_norm_angle((float)base_deg - heading);
	x = COMPASS_CX + compass_sin(deg, COMPASS_LABEL_R);
	y = COMPASS_CY - compass_cos(deg, COMPASS_LABEL_R);
	compass_center_obj(label, x, y);
}

static void compass_refresh(void)
{
	char buf[160];
	float qmc_yaw = 0.0f;
	float qmc_pitch = 0.0f;
	float qmc_roll = 0.0f;
	uint8_t qmc_online = 0U;
	uint8_t ins_online = 0U;

	qmc_online = sensor_tick_online(detail_data.mag_last_update_tick, SENSOR_TIMEOUT_MS);
	ins_online = sensor_tick_online(detail_data.ins_last_update_tick, SENSOR_TIMEOUT_MS);
	qmc_yaw = detail_data.mag_yaw;
	qmc_pitch = detail_data.mag_pitch;
	qmc_roll = detail_data.mag_roll;

	if (compass_heading) {
		if (qmc_online) {
			snprintf(buf, sizeof(buf), "%.1f deg", qmc_yaw);
		} else {
			snprintf(buf, sizeof(buf), "OFFLINE");
		}
		lv_label_set_text(compass_heading, buf);
	}

	if (qmc_online) {
		compass_place_cardinal(compass_cardinal[0], 0, qmc_yaw);
		compass_place_cardinal(compass_cardinal[1], 90, qmc_yaw);
		compass_place_cardinal(compass_cardinal[2], 180, qmc_yaw);
		compass_place_cardinal(compass_cardinal[3], 270, qmc_yaw);
	}

	if (dl_imu) {
		if (ins_online && qmc_online) {
			snprintf(buf, sizeof(buf),
			         "INS R: %.1f\nINS P: %.1f\nINS Y: %.1f\n\nQMC Yaw: %.1f\nQMC Pitch: %.1f\nQMC Roll: %.1f",
			         detail_data.ins_roll,
			         detail_data.ins_pitch,
			         detail_data.ins_yaw,
			         qmc_yaw, qmc_pitch, qmc_roll);
		} else if (ins_online) {
			snprintf(buf, sizeof(buf),
			         "INS R: %.1f\nINS P: %.1f\nINS Y: %.1f\n\nQMC: OFFLINE",
			         detail_data.ins_roll,
			         detail_data.ins_pitch,
			         detail_data.ins_yaw);
		} else if (qmc_online) {
			snprintf(buf, sizeof(buf),
			         "INS: OFFLINE\n\nQMC Yaw: %.1f\nQMC Pitch: %.1f\nQMC Roll: %.1f",
			         qmc_yaw, qmc_pitch, qmc_roll);
		} else {
			snprintf(buf, sizeof(buf), "INS: OFFLINE\n\nQMC: OFFLINE");
		}
		lv_label_set_text(dl_imu, buf);
	}
}

static lv_obj_t *compass_label_create(lv_obj_t *parent, const char *text, lv_color_t color)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, color, 0);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

	return label;
}

static void compass_create(lv_obj_t *parent)
{
	lv_obj_t *box;
	lv_obj_t *ring;

	box = lv_obj_create(parent);
	lv_obj_set_size(box, 310, 330);
	lv_obj_align(box, LV_ALIGN_LEFT_MID, 40, 0);
	lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(box, 0, 0);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

	ring = lv_obj_create(box);
	lv_obj_set_size(ring, COMPASS_RING_SIZE, COMPASS_RING_SIZE);
	lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(ring, lv_color_hex(UI_COLOR_CYAN_SOFT), 0);
	lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(ring, 3, 0);
	lv_obj_set_style_border_color(ring, lv_color_hex(UI_COLOR_CYAN), 0);
	lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(ring, LV_ALIGN_TOP_LEFT, COMPASS_CX - COMPASS_RING_SIZE / 2, COMPASS_CY - COMPASS_RING_SIZE / 2);

	compass_marker = lv_line_create(box);
	compass_marker_points[0].x = COMPASS_CX;
	compass_marker_points[0].y = COMPASS_CY;
	compass_marker_points[1].x = COMPASS_CX;
	compass_marker_points[1].y = COMPASS_CY - 86;
	lv_line_set_points(compass_marker, compass_marker_points, 2);
	lv_obj_set_style_line_width(compass_marker, 4, 0);
	lv_obj_set_style_line_color(compass_marker, lv_color_hex(UI_COLOR_ORANGE), 0);
	lv_obj_set_style_line_rounded(compass_marker, true, 0);

	compass_cardinal[0] = compass_label_create(box, "N", lv_color_hex(UI_COLOR_ORANGE));
	compass_cardinal[1] = compass_label_create(box, "E", lv_color_hex(UI_COLOR_TEXT));
	compass_cardinal[2] = compass_label_create(box, "S", lv_color_hex(UI_COLOR_TEXT));
	compass_cardinal[3] = compass_label_create(box, "W", lv_color_hex(UI_COLOR_TEXT));

	compass_heading = compass_label_create(box, "--.- deg", lv_color_hex(UI_COLOR_CYAN_DARK));
	lv_obj_set_style_text_font(compass_heading, &ui_font_Font2, 0);
	compass_center_obj(compass_heading, COMPASS_CX, COMPASS_CY + 138);
	compass_refresh();
}

static void detail_refresh_timer_cb(lv_timer_t *t)
{
	char buf[128];

	(void)t;

	if (lv_scr_act() == ui_DataDetail) {
		detail_update_data();
		if (dl_gps) {
			if (sensor_tick_online(detail_data.gps_last_update_tick, GPS_TIMEOUT_MS)) {
				snprintf(buf,sizeof(buf),"Lat: %.5f\nLon: %.5f", detail_data.gps_lat, detail_data.gps_lon);
			} else {
				snprintf(buf,sizeof(buf),"OFFLINE");
			}
			lv_label_set_text(dl_gps,buf);
		}
		if (dl_speed) {
			speed_detail_refresh();
		}
		compass_refresh();
		mode_dial_refresh();
	}
}

static int mode_dial_current_index(void)
{
	int mode = (int)detail_data.mode;

	if (mode < 0 || mode >= (int)MODE_DIAL_COUNT) return 0;

	return mode;
}

static void mode_dial_refresh_to(int active)
{
	uint8_t i;
	static const lv_coord_t needle_x[MODE_DIAL_COUNT] = {0, 77, 105, 68, -68, -105, -77};
	static const lv_coord_t needle_y[MODE_DIAL_COUNT] = {-98, -61, 0, 80, 80, 0, -61};

	if (active < 0 || active >= (int)MODE_DIAL_COUNT) active = 0;

	if (dl_mode) {
		lv_label_set_text(dl_mode, mode_names[active]);
	}

	if (mode_needle) {
		mode_needle_points[0].x = 280;
		mode_needle_points[0].y = 210;
		mode_needle_points[1].x = 280 + needle_x[active];
		mode_needle_points[1].y = 210 + needle_y[active];
		lv_line_set_points(mode_needle, mode_needle_points, 2);
	}

	for (i = 0U; i < MODE_DIAL_COUNT; i++) {
		if (mode_dial_buttons[i] == NULL) continue;

		if ((int)i == active) {
			lv_obj_set_style_bg_color(mode_dial_buttons[i], lv_color_hex(UI_COLOR_CYAN), 0);
			lv_obj_set_style_bg_opa(mode_dial_buttons[i], LV_OPA_COVER, 0);
			lv_obj_set_style_border_color(mode_dial_buttons[i], lv_color_hex(UI_COLOR_CYAN_DARK), 0);
			lv_obj_set_style_text_color(mode_dial_buttons[i], lv_color_hex(UI_COLOR_BG), 0);
		} else {
			lv_obj_set_style_bg_color(mode_dial_buttons[i], lv_color_hex(UI_COLOR_CARD), 0);
			lv_obj_set_style_bg_opa(mode_dial_buttons[i], LV_OPA_COVER, 0);
			lv_obj_set_style_border_color(mode_dial_buttons[i], lv_color_hex(UI_COLOR_CYAN_SOFT), 0);
			lv_obj_set_style_text_color(mode_dial_buttons[i], lv_color_hex(UI_COLOR_TEXT), 0);
		}
	}
}

static void mode_dial_refresh(void)
{
	mode_dial_refresh_to(mode_dial_current_index());
}

static void mode_dial_cb(lv_event_t *e)
{
	int *sel_ptr = (int *)lv_event_get_user_data(e);
	int sel;

	if (sel_ptr == NULL) return;

	sel = *sel_ptr;
	chassis_request_mode((CarMode_t)sel);
	if (sel >= 0 && sel < (int)MODE_DIAL_COUNT) {
		mode_dial_refresh_to(sel);
	}
}

static lv_obj_t *mode_dial_button_create(lv_obj_t *parent, int index, lv_coord_t x, lv_coord_t y)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label;

	lv_obj_set_size(btn, 128, 38);
	lv_obj_align(btn, LV_ALIGN_CENTER, x, y);
	lv_obj_set_style_radius(btn, 19, 0);
	lv_obj_set_style_border_width(btn, 2, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_add_event_cb(btn, mode_dial_cb, LV_EVENT_CLICKED, &mode_dial_indices[index]);

	label = lv_label_create(btn);
	lv_label_set_text(label, mode_names[index]);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_center(label);

	mode_dial_buttons[index] = btn;
	return btn;
}

static void mode_dial_create(lv_obj_t *parent)
{
	lv_obj_t *box;
	lv_obj_t *ring;
	lv_obj_t *center;

	lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

	box = lv_obj_create(parent);
	lv_obj_set_size(box, 560, 420);
	lv_obj_center(box);
	lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(box, 0, 0);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

	ring = lv_obj_create(box);
	lv_obj_set_size(ring, 300, 300);
	lv_obj_center(ring);
	lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(ring, lv_color_hex(UI_COLOR_CYAN_SOFT), 0);
	lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(ring, 4, 0);
	lv_obj_set_style_border_color(ring, lv_color_hex(UI_COLOR_CYAN), 0);
	lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

	mode_needle = lv_line_create(box);
	lv_obj_set_style_line_width(mode_needle, 5, 0);
	lv_obj_set_style_line_color(mode_needle, lv_color_hex(UI_COLOR_ORANGE), 0);
	lv_obj_set_style_line_rounded(mode_needle, true, 0);

	mode_dial_button_create(box, 0, 0, -158);
	mode_dial_button_create(box, 1, 130, -102);
	mode_dial_button_create(box, 2, 172, 0);
	mode_dial_button_create(box, 3, 108, 130);
	mode_dial_button_create(box, 4, -108, 130);
	mode_dial_button_create(box, 5, -172, 0);
	mode_dial_button_create(box, 6, -130, -102);

	center = lv_obj_create(box);
	lv_obj_set_size(center, 170, 74);
	lv_obj_center(center);
	lv_obj_set_style_radius(center, 16, 0);
	lv_obj_set_style_bg_color(center, lv_color_hex(UI_COLOR_BLUE_DARK), 0);
	lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(center, 2, 0);
	lv_obj_set_style_border_color(center, lv_color_hex(UI_COLOR_CYAN), 0);
	lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);

	dl_mode = lv_label_create(center);
	lv_label_set_text(dl_mode, "No Power");
	lv_obj_set_style_text_color(dl_mode, lv_color_hex(UI_COLOR_BG), 0);
	lv_obj_set_style_text_font(dl_mode, &lv_font_montserrat_14, 0);
	lv_obj_center(dl_mode);

	mode_dial_refresh();
}

static void detail_back_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED || lv_event_get_code(e) == LV_EVENT_LONG_PRESSED || lv_event_get_code(e) == LV_EVENT_GESTURE)
		_ui_screen_load(ui_DataDisplay, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 200);
}


void ui_DataDetail_screen_init(void)
{
	if (ui_DataDetail) return;

	ui_DataDetail = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(ui_DataDetail, lv_color_hex(UI_COLOR_BG), 0);

	g_tv = lv_tabview_create(ui_DataDetail, LV_DIR_TOP, 30);
	lv_obj_set_size(g_tv, 930, 560);
	lv_obj_align(g_tv, LV_ALIGN_TOP_LEFT, 5, 5);

	/* ---- Tab 0: GPS ---- */
	lv_obj_t *t0 = lv_tabview_add_tab(g_tv, "GPS");
	dl_gps = lv_label_create(t0);
	lv_label_set_text(dl_gps, "Lat: --\nLon: --\nSat: --");
	lv_obj_set_style_text_color(dl_gps, lv_color_hex(UI_COLOR_CYAN_DARK), 0);
	lv_obj_set_style_text_font(dl_gps, &lv_font_montserrat_14, 0);
	lv_obj_center(dl_gps);

	/* ---- Tab 1: Speed ---- */
	lv_obj_t *t1 = lv_tabview_add_tab(g_tv, "Wheel");
	dl_speed = lv_label_create(t1);
	lv_label_set_text(dl_speed, "Wheel encoder: --");
	lv_obj_set_style_text_color(dl_speed, lv_color_hex(UI_COLOR_CYAN_DARK), 0);
	lv_obj_set_style_text_font(dl_speed, &lv_font_montserrat_14, 0);
	lv_obj_center(dl_speed);

	/* ---- Tab 2: IMU ---- */
	lv_obj_t *t2 = lv_tabview_add_tab(g_tv, "IMU");
	lv_obj_clear_flag(t2, LV_OBJ_FLAG_SCROLLABLE);
	detail_update_data();
	compass_create(t2);

	dl_imu = lv_label_create(t2);
	lv_label_set_text(dl_imu, "INS R: --\nINS P: --\nINS Y: --\n\nQMC Yaw: --\nQMC Pitch: --\nQMC Roll: --");
	lv_obj_set_style_text_color(dl_imu, lv_color_hex(UI_COLOR_CYAN_DARK), 0);
	lv_obj_set_style_text_font(dl_imu, &lv_font_montserrat_14, 0);
	lv_obj_align(dl_imu, LV_ALIGN_RIGHT_MID, -95, 0);

	/* ---- Tab 3: Mode switcher ---- */
	lv_obj_t *t3 = lv_tabview_add_tab(g_tv, "Mode");
	mode_dial_create(t3);

	/* Back button --- right edge, vertically centered */
	lv_obj_add_event_cb(ui_DataDetail, touch_reset_idle, LV_EVENT_PRESSED, NULL);
	lv_obj_t *back = lv_btn_create(ui_DataDetail);
	lv_obj_set_size(back, 70, 36);
	lv_obj_align(back, LV_ALIGN_RIGHT_MID, -5, 0);
	lv_obj_t *bl = lv_label_create(back);
	lv_label_set_text(bl, "Back");
	lv_obj_center(bl);
	lv_obj_add_event_cb(back, detail_back_cb, LV_EVENT_CLICKED, NULL);

	if (detail_refresh_timer == NULL) {
		detail_refresh_timer = lv_timer_create(detail_refresh_timer_cb, 200, NULL);
	}
}


void dd_open_detail(int tab)
{
	ui_DataDetail_screen_init();
	if (g_tv) lv_tabview_set_act(g_tv, tab, LV_ANIM_ON);
	_ui_screen_load(ui_DataDetail, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 200);

	char buf[128];
	detail_update_data();
	if (dl_gps)   {
		if (sensor_tick_online(detail_data.gps_last_update_tick, GPS_TIMEOUT_MS)) {
			snprintf(buf,sizeof(buf),"Lat: %.5f\nLon: %.5f", detail_data.gps_lat, detail_data.gps_lon);
		} else {
			snprintf(buf,sizeof(buf),"OFFLINE");
		}
		lv_label_set_text(dl_gps,buf);
	}
	if (dl_speed) {
		speed_detail_refresh();
	}
	compass_refresh();
	mode_dial_refresh();
}


void ui_DataDetail_screen_destroy(void)
{
	if (detail_refresh_timer) {
		lv_timer_del(detail_refresh_timer);
		detail_refresh_timer = NULL;
	}

	if (ui_DataDetail) { lv_obj_del(ui_DataDetail); ui_DataDetail = NULL; }
	g_tv = NULL;
	dl_gps = dl_speed = dl_imu = NULL;
	dl_mode = NULL;
	mode_dial_buttons[0] = NULL;
	mode_dial_buttons[1] = NULL;
	mode_dial_buttons[2] = NULL;
	mode_dial_buttons[3] = NULL;
	mode_dial_buttons[4] = NULL;
	mode_dial_buttons[5] = NULL;
	mode_dial_buttons[6] = NULL;
	mode_needle = NULL;
	compass_heading = NULL;
	compass_marker = NULL;
	compass_cardinal[0] = NULL;
	compass_cardinal[1] = NULL;
	compass_cardinal[2] = NULL;
	compass_cardinal[3] = NULL;
}
