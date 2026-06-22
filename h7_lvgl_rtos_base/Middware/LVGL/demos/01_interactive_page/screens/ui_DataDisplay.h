#ifndef UI_DATADISPLAY_H
#define UI_DATADISPLAY_H

#include "lvgl.h"

extern lv_obj_t *ui_DataDisplay;

void ui_DataDisplay_screen_init(void);
void ui_DataDisplay_screen_destroy(void);
void ui_event_DataDisplay(lv_event_t *e);

/* APIs to update displayed values at runtime (call from any task) */
void dd_set_gps(float lat, float lon, int sats);
void dd_set_speed(float speed);
void dd_set_imu(float roll, float pitch, float yaw);
void dd_set_mode(const char *mode_name);

#endif
