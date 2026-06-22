/**
 * ui_DataDetail.c — Detail views for GPS/Speed/IMU/Mode panels
 *
 * Uses tabview: 0=GPS, 1=Speed, 2=IMU, 3=Mode.
 * Mode tab has a roller to switch car mode.
 */
#include "../ui.h"
#include "../ui_helpers.h"
#include "app_chassis_board.h"
#include <string.h>
#include <stdio.h>

lv_obj_t *ui_DataDetail = NULL;
static lv_obj_t *g_tv = NULL;

extern float  g_dd_lat, g_dd_lon;
extern int    g_dd_sats;
extern float  g_dd_speed;
extern float  g_dd_roll, g_dd_pitch, g_dd_yaw;

/* Detail labels for live update */
static lv_obj_t *dl_gps   = NULL;
static lv_obj_t *dl_speed = NULL;
static lv_obj_t *dl_imu   = NULL;
static lv_obj_t *dl_mode  = NULL;


/* ---- External data values (shared with ui_DataDisplay) ---- */
extern float  g_dd_lat, g_dd_lon;
extern int    g_dd_sats;
extern float  g_dd_speed;
extern float  g_dd_roll, g_dd_pitch, g_dd_yaw;
extern char   g_dd_mode[32];
extern volatile int gui_req_mode;


static const char *g_mode_names[] = {"GPS Nav","WeChat","ROS Indoor","Bluetooth","Voice"};

static void mode_roller_cb(lv_event_t *e)
{
	int sel = (int)lv_roller_get_selected(lv_event_get_target(e));
	extern volatile int gui_req_mode;
	gui_req_mode = sel;
	if (sel >= 0 && sel < 5) {
		dd_set_mode(g_mode_names[sel]);
		lv_label_set_text(dl_mode, g_mode_names[sel]);
	}
}

static void detail_back_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED || lv_event_get_code(e) == LV_EVENT_LONG_PRESSED || lv_event_get_code(e) == LV_EVENT_GESTURE)
		lv_scr_load_anim(ui_DataDisplay, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 200, false);
}


void ui_DataDetail_screen_init(void)
{
	if (ui_DataDetail) return;

	ui_DataDetail = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(ui_DataDetail, lv_color_hex(0x1A1A2E), 0);

	g_tv = lv_tabview_create(ui_DataDetail, LV_DIR_TOP, 30);
	lv_obj_set_size(g_tv, 930, 560);
	lv_obj_align(g_tv, LV_ALIGN_TOP_LEFT, 5, 5);

	/* ---- Tab 0: GPS ---- */
	lv_obj_t *t0 = lv_tabview_add_tab(g_tv, "GPS");
	dl_gps = lv_label_create(t0);
	lv_label_set_text(dl_gps, "Lat: --\nLon: --\nSat: --");
	lv_obj_set_style_text_color(dl_gps, lv_color_hex(0x00FF88), 0);
	lv_obj_set_style_text_font(dl_gps, &lv_font_montserrat_14, 0);
	lv_obj_center(dl_gps);

	/* ---- Tab 1: Speed ---- */
	lv_obj_t *t1 = lv_tabview_add_tab(g_tv, "Speed");
	dl_speed = lv_label_create(t1);
	lv_label_set_text(dl_speed, "-- m/s");
	lv_obj_set_style_text_color(dl_speed, lv_color_hex(0x00FF88), 0);
	lv_obj_set_style_text_font(dl_speed, &ui_font_FontTitle, 0);
	lv_obj_center(dl_speed);

	/* ---- Tab 2: IMU ---- */
	lv_obj_t *t2 = lv_tabview_add_tab(g_tv, "IMU");
	dl_imu = lv_label_create(t2);
	lv_label_set_text(dl_imu, "Roll:  --\nPitch: --\nYaw:   --\n\nMagX:  --\nMagY:  --\nMagZ:  --");
	lv_obj_set_style_text_color(dl_imu, lv_color_hex(0x00FF88), 0);
	lv_obj_set_style_text_font(dl_imu, &lv_font_montserrat_14, 0);
	lv_obj_center(dl_imu);

	/* ---- Tab 3: Mode switcher ---- */
	lv_obj_t *t3 = lv_tabview_add_tab(g_tv, "Mode");
	lv_obj_t *roller = lv_roller_create(t3);
	lv_roller_set_options(roller,
		"GPS Nav\n"
		"WeChat Remote\n"
		"ROS Indoor\n"
		"Bluetooth\n"
		"Voice",
		LV_ROLLER_MODE_INFINITE);
	lv_obj_set_style_text_font(roller, &lv_font_montserrat_14, 0);
	lv_obj_center(roller);

	lv_obj_add_event_cb(roller, mode_roller_cb, LV_EVENT_VALUE_CHANGED, NULL);

	dl_mode = lv_label_create(t3);
	lv_label_set_text(dl_mode, "GPS Nav");
	lv_obj_set_style_text_color(dl_mode, lv_color_hex(0x00FF88), 0);
	lv_obj_set_style_text_font(dl_mode, &lv_font_montserrat_14, 0);
	lv_obj_align(dl_mode, LV_ALIGN_BOTTOM_MID, 0, -20);

	/* Back button — right edge, vertically centered */
	lv_obj_add_event_cb(ui_DataDetail, touch_reset_idle, LV_EVENT_PRESSED, NULL);
	lv_obj_t *back = lv_btn_create(ui_DataDetail);
	lv_obj_set_size(back, 70, 36);
	lv_obj_align(back, LV_ALIGN_RIGHT_MID, -5, 0);
	lv_obj_t *bl = lv_label_create(back);
	lv_label_set_text(bl, "Back");
	lv_obj_center(bl);
	lv_obj_add_event_cb(back, detail_back_cb, LV_EVENT_CLICKED, NULL);
}


void dd_open_detail(int tab)
{
	ui_DataDetail_screen_init();
	if (g_tv) lv_tabview_set_act(g_tv, tab, LV_ANIM_ON);
	lv_scr_load_anim(ui_DataDetail, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 200, false);

	char buf[128];
	if (dl_gps)   { snprintf(buf,sizeof(buf),"Lat: %.5f\nLon: %.5f\nSat: %d", g_dd_lat,g_dd_lon,g_dd_sats); lv_label_set_text(dl_gps,buf); }
	if (dl_speed) { snprintf(buf,sizeof(buf),"%.2f m/s", g_dd_speed); lv_label_set_text(dl_speed,buf); }
	if (dl_imu)   { snprintf(buf,sizeof(buf),"Roll: %.1f\nPitch: %.1f\nYaw: %.1f\n\nMagX: --\nMagY: --\nMagZ: --", g_dd_roll,g_dd_pitch,g_dd_yaw); lv_label_set_text(dl_imu,buf); }
}


void ui_DataDetail_screen_destroy(void)
{
	if (ui_DataDetail) { lv_obj_del(ui_DataDetail); ui_DataDetail = NULL; }
	g_tv = NULL;
	dl_gps = dl_speed = dl_imu = NULL;
}
