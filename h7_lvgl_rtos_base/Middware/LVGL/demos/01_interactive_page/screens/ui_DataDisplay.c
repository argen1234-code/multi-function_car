/**
 * ui_DataDisplay.c — Car status & sensor data dashboard
 *
 * Paginated display: GPS, Speed, IMU, Mode.
 * Click screen → back to Screenmain.
 * Use dd_set_*() APIs to update values from other tasks.
 */
#include "../ui.h"
#include "../ui_helpers.h"
#include "ui_DataDetail.h"
#include "app_chassis_board.h"
#include <string.h>
#include <stdio.h>

#define PANEL_W   360
#define PANEL_H   110

lv_obj_t *ui_DataDisplay = NULL;

/* Shared data values (also used by ui_DataDetail) */
float  g_dd_lat   = 0, g_dd_lon = 0;
int    g_dd_sats  = 0;
float  g_dd_speed = 0;
float  g_dd_roll  = 0, g_dd_pitch = 0, g_dd_yaw = 0;
char   g_dd_mode[32] = "GPS Nav";
static const char *g_road_conditions[] = {
	"asphalt",
	"indoor",
	"outdoor_cement",
	"outdoor_marble"
};
static int g_dd_road_index = 0;

static lv_obj_t *lb_gps = NULL, *lb_speed = NULL, *lb_imu = NULL, *lb_mode = NULL;
static lv_obj_t *lb_road = NULL;


void dd_set_gps(float lat, float lon, int s)   { g_dd_lat=lat; g_dd_lon=lon; g_dd_sats=s; }
void dd_set_speed(float v)                      { g_dd_speed=v; }
void dd_set_imu(float r, float p, float y)     { g_dd_roll=r; g_dd_pitch=p; g_dd_yaw=y; }
void dd_set_mode(const char *s)                { strncpy(g_dd_mode,s,sizeof(g_dd_mode)-1); g_dd_mode[sizeof(g_dd_mode)-1] = '\0'; }

static const char *mode_name_from_chassis(CarMode_t mode)
{
	switch (mode) {
	case CAR_MODE_GPS:    return "GPS Nav";
	case CAR_MODE_REMOTE: return "WeChat";
	case CAR_MODE_LINE:   return "ROS Indoor";
	case CAR_MODE_INDOOR: return "Bluetooth";
	case CAR_MODE_VOICE:  return "Voice";
	default:              return "--";
	}
}

void dd_set_road_condition(int index)
{
	int count = (int)(sizeof(g_road_conditions) / sizeof(g_road_conditions[0]));
	if (index < 0 || index >= count) return;
	g_dd_road_index = index;
}

const char *dd_get_road_condition(void)
{
	return g_road_conditions[g_dd_road_index];
}


void ui_event_DataDisplay(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED
	    || lv_event_get_code(e) == LV_EVENT_GESTURE
	    || lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
		_ui_screen_change(&ui_Screenmain, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 200,
		                  &ui_Screenmain_screen_init);
	}
}


/* ---- Refresh the value labels ---- */
static void refresh_values(void)
{
	char buf[64];
	if (lb_gps)   { snprintf(buf,sizeof(buf),
	                  "Lat: %.5f\nLon: %.5f\nSat: %d", g_dd_lat,g_dd_lon,g_dd_sats);
	                  lv_label_set_text(lb_gps, buf); }
	if (lb_speed) { snprintf(buf,sizeof(buf),"%.2f m/s",g_dd_speed); lv_label_set_text(lb_speed,buf); }
	if (lb_imu)   { snprintf(buf,sizeof(buf),
	                  "R=%.1f P=%.1f Y=%.1f", g_dd_roll,g_dd_pitch,g_dd_yaw);
	                  lv_label_set_text(lb_imu, buf); }
	if (lb_mode)  { lv_label_set_text(lb_mode, mode_name_from_chassis(chassis_current_mode_debug)); }
	if (lb_road)  { snprintf(buf,sizeof(buf),"Road: %s", dd_get_road_condition()); lv_label_set_text(lb_road, buf); }
}

static void panel_click(lv_event_t *e)
{
	lv_obj_t *pnl = lv_event_get_target(e);
	int tab = (int)(uintptr_t)lv_obj_get_user_data(pnl);
	dd_open_detail(tab);
}

static void road_change_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

	int count = (int)(sizeof(g_road_conditions) / sizeof(g_road_conditions[0]));
	g_dd_road_index = (g_dd_road_index + 1) % count;
	refresh_values();
}


/* ---- Helper: create a data panel ---- */
static lv_obj_t *add_panel(lv_obj_t *parent, const char *icon, const char *title,
                           lv_obj_t **val_out, lv_coord_t x, lv_coord_t y)
{
	lv_obj_t *pnl = lv_obj_create(parent);
	lv_obj_set_size(pnl, PANEL_W, PANEL_H);
	lv_obj_set_pos(pnl, x, y);
	lv_obj_set_style_radius(pnl, 12, 0);
	lv_obj_set_style_bg_color(pnl, lv_color_hex(0x2A2A3E), 0);
	lv_obj_clear_flag(pnl, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t *ic = lv_label_create(pnl);
	lv_label_set_text(ic, icon);
	lv_obj_set_style_text_font(ic, &lv_font_montserrat_14, 0);
	lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 10, 5);

	lv_obj_t *tl = lv_label_create(pnl);
	lv_label_set_text(tl, title);
	lv_obj_set_style_text_color(tl, lv_color_hex(0x888888), 0);
	lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
	lv_obj_align_to(tl, ic, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

	lv_obj_t *vl = lv_label_create(pnl);
	lv_label_set_text(vl, "--");
	lv_obj_set_style_text_color(vl, lv_color_hex(0x00FF88), 0);
	lv_obj_set_style_text_font(vl, &lv_font_montserrat_14, 0);
	lv_obj_align(vl, LV_ALIGN_LEFT_MID, 20, 15);

	if (val_out) *val_out = vl;
	return pnl;
}

static void add_road_control(lv_obj_t *parent, lv_coord_t y)
{
	lv_obj_t *box = lv_obj_create(parent);
	lv_obj_set_size(box, 460, 94);
	lv_obj_align(box, LV_ALIGN_TOP_MID, 0, y);
	lv_obj_set_style_radius(box, 12, 0);
	lv_obj_set_style_bg_color(box, lv_color_hex(0x2A2A3E), 0);
	lv_obj_set_style_border_width(box, 0, 0);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

	lb_road = lv_label_create(box);
	lv_obj_set_size(lb_road, 440, 40);
	lv_label_set_text(lb_road, "Road: asphalt");
	lv_obj_set_style_text_color(lb_road, lv_color_hex(0x00FF88), 0);
	lv_obj_set_style_text_font(lb_road, &ui_font_Font2, 0);
	lv_obj_set_style_text_align(lb_road, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(lb_road, LV_LABEL_LONG_CLIP);
	lv_obj_align(lb_road, LV_ALIGN_TOP_MID, 0, 4);

	lv_obj_t *btn = lv_btn_create(box);
	lv_obj_set_size(btn, 120, 24);
	lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -6);
	lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
	lv_obj_set_style_radius(btn, 8, 0);
	lv_obj_add_event_cb(btn, road_change_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t *btn_label = lv_label_create(btn);
	lv_label_set_text(btn_label, "");
	lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
	lv_obj_center(btn_label);
}


/* ---- Screen init ---- */
static void dd_refresh_timer(lv_timer_t *t)
{
	if (lv_scr_act() == ui_DataDisplay) refresh_values();
}


void ui_DataDisplay_screen_init(void)
{
	if (ui_DataDisplay) return;

	ui_DataDisplay = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(ui_DataDisplay, lv_color_hex(0x1A1A2E), 0);
	lv_obj_set_style_bg_opa(ui_DataDisplay, 255, 0);

	/* Title */
	lv_obj_t *t = lv_label_create(ui_DataDisplay);
	lv_label_set_text(t, "Car Dashboard");
	lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), 0);
	lv_obj_set_style_text_font(t, &ui_font_FontTitle, 0);
	lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 10);

	lv_coord_t x0 = (1024 - PANEL_W * 2 - 20) / 2;   /* center the 2-column grid */
	lv_coord_t y0 = 100;

	lv_obj_t *p0 = add_panel(ui_DataDisplay, LV_SYMBOL_GPS,   "GPS",      &lb_gps,   x0,             y0);
	lv_obj_t *p1 = add_panel(ui_DataDisplay, LV_SYMBOL_WIFI,  "Speed",    &lb_speed, x0+PANEL_W+20,  y0);
	lv_obj_t *p2 = add_panel(ui_DataDisplay, LV_SYMBOL_SETTINGS, "IMU",   &lb_imu,   x0,             y0+PANEL_H+15);
	lv_obj_t *p3 = add_panel(ui_DataDisplay, LV_SYMBOL_HOME,  "Mode",     &lb_mode,  x0+PANEL_W+20,  y0+PANEL_H+15);

	add_road_control(ui_DataDisplay, y0 + PANEL_H * 2 + 55);

	lv_obj_set_user_data(p0, (void *)0); lv_obj_add_event_cb(p0, panel_click, LV_EVENT_CLICKED, NULL);
	lv_obj_set_user_data(p1, (void *)1); lv_obj_add_event_cb(p1, panel_click, LV_EVENT_CLICKED, NULL);
	lv_obj_set_user_data(p2, (void *)2); lv_obj_add_event_cb(p2, panel_click, LV_EVENT_CLICKED, NULL);
	lv_obj_set_user_data(p3, (void *)3); lv_obj_add_event_cb(p3, panel_click, LV_EVENT_CLICKED, NULL);

	refresh_values();
	lv_obj_add_event_cb(ui_DataDisplay, touch_reset_idle, LV_EVENT_PRESSED, NULL);

	lv_obj_add_event_cb(ui_DataDisplay, ui_event_DataDisplay, LV_EVENT_ALL, NULL);

	/* Periodic refresh: update labels from globals when screen is visible */
	lv_timer_create(dd_refresh_timer, 500, NULL);
}


void ui_DataDisplay_screen_destroy(void)
{
	if (ui_DataDisplay) { lv_obj_del(ui_DataDisplay); ui_DataDisplay = NULL; }
	lb_gps = lb_speed = lb_imu = lb_mode = NULL;
	lb_road = NULL;
}
