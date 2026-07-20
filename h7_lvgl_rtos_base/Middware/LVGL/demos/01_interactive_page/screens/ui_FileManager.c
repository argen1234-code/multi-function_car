/**
 * ui_FileManager.c --- TF card file browser with folder navigation
 *
 * Uses LVGL FATFS driver (LV_USE_FS_FATFS, drive 'S').
 * Click folder -> enter, ".." -> go up, file -> show info.
 * Screen touch -> back to Screenmain.
 */
#include "../ui.h"
#include "../ui_helpers.h"
#include <string.h>
#include <stdio.h>

lv_obj_t *ui_FileManager = NULL;
static lv_obj_t *g_list  = NULL;
static lv_obj_t *g_title = NULL;
static char g_path[128] = "S:/";


/* ---- forward ---- */
static void refresh_file_list(void);
static void item_click_cb(lv_event_t *e);


void ui_event_FileManager(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED || lv_event_get_code(e) == LV_EVENT_LONG_PRESSED || lv_event_get_code(e) == LV_EVENT_GESTURE) {
		_ui_screen_change(&ui_Screenmain, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 200,
		                  &ui_Screenmain_screen_init);
	}
}


/* ---- item click: folder -> enter, ".." -> go up ---- */
static void item_click_cb(lv_event_t *e)
{
	lv_obj_t *btn = lv_event_get_target(e);
	const char *name = lv_list_get_btn_text(g_list, btn);
	if (!name) return;

	/* ".." -> go up */
	if (strncmp(name, "..", 2) == 0) {
		char *slash = strrchr(g_path, '/');
		if (slash && slash > g_path + 2) {   /* not "S:" */
			*slash = '\0';
			if (*(slash - 1) == ':') strcat(g_path, "/");  /* keep S:/ */
		}
		refresh_file_list();
	lv_obj_add_event_cb(ui_FileManager, touch_reset_idle, LV_EVENT_PRESSED, NULL);
		return;
	}

	/* Build new path and try to open as dir */
	char new_path[128];
	snprintf(new_path, sizeof(new_path), "%s%s/", g_path, name);
	lv_fs_dir_t dd;
	if (lv_fs_dir_open(&dd, new_path) == LV_FS_RES_OK) {
		lv_fs_dir_close(&dd);
		snprintf(g_path, sizeof(g_path), "%s%s/", g_path, name);
		refresh_file_list();
	lv_obj_add_event_cb(ui_FileManager, touch_reset_idle, LV_EVENT_PRESSED, NULL);
		return;
	}

	/* Not a directory --- just show info (name kept in button) */
}


/* ---- Refresh the file list from g_path ---- */
static void refresh_file_list(void)
{
	lv_fs_dir_t  dd;
	lv_fs_res_t  res;
	char         fn[64];

	if (g_list) lv_obj_del(g_list);

	/* Update title */
	if (g_title) lv_label_set_text(g_title, g_path);

	g_list = lv_list_create(ui_FileManager);
	lv_obj_set_size(g_list, 800, 440);
	lv_obj_set_align(g_list, LV_ALIGN_CENTER);
	lv_obj_set_y(g_list, 30);
	lv_obj_set_style_radius(g_list, 10, 0);
	lv_obj_set_style_bg_color(g_list, lv_color_hex(UI_COLOR_CARD), 0);
	ui_apply_raised_panel(g_list);

	/* ".." to go up (shown unless at root S:/) */
	if (strcmp(g_path, "S:/") != 0) {
		lv_obj_t *up = lv_list_add_btn(g_list, NULL, "..");
		lv_obj_set_style_border_side(up, LV_BORDER_SIDE_BOTTOM, 0);
		lv_obj_set_style_border_color(up, lv_color_hex(UI_COLOR_CYAN_SOFT), 0);
		lv_obj_set_style_border_width(up, 1, 0);
		lv_obj_add_event_cb(up, item_click_cb, LV_EVENT_CLICKED, NULL);
	}

	res = lv_fs_dir_open(&dd, g_path);
	if (res != LV_FS_RES_OK) {
		lv_list_add_text(g_list, "  [ERR] Cannot open directory");
		return;
	}

	while (1) {
		res = lv_fs_dir_read(&dd, fn);
		if (res != LV_FS_RES_OK || fn[0] == '\0') break;

		/* Determine if directory */
		char sub[128];
		int is_dir = 0;
		snprintf(sub, sizeof(sub), "%s%s/", g_path, fn);
		lv_fs_dir_t tmp;
		if (lv_fs_dir_open(&tmp, sub) == LV_FS_RES_OK) {
			lv_fs_dir_close(&tmp);
			is_dir = 1;
		}

		/* Build display text */
		char label[128];
		uint32_t sz = 0;
		if (is_dir) {
			snprintf(label, sizeof(label), LV_SYMBOL_DIRECTORY "  %s", fn);
		} else {
			lv_fs_file_t fp;
			char fpath[128];
			snprintf(fpath, sizeof(fpath), "%s%s", g_path, fn);
			if (lv_fs_open(&fp, fpath, LV_FS_MODE_RD) == LV_FS_RES_OK) {
				lv_fs_seek(&fp, 0, LV_FS_SEEK_END);
				lv_fs_tell(&fp, &sz);
				lv_fs_close(&fp);
			}
			if (sz >= 1048576)
				snprintf(label, sizeof(label), LV_SYMBOL_FILE "  %s  (%u MB)", fn, (unsigned)(sz/1048576));
			else if (sz >= 1024)
				snprintf(label, sizeof(label), LV_SYMBOL_FILE "  %s  (%u KB)", fn, (unsigned)(sz/1024));
			else
				snprintf(label, sizeof(label), LV_SYMBOL_FILE "  %s  (%u B)", fn, (unsigned)sz);
		}

		lv_obj_t *btn = lv_list_add_btn(g_list, NULL, label);
		lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
		lv_obj_set_style_border_color(btn, lv_color_hex(UI_COLOR_CYAN_SOFT), 0);
		lv_obj_set_style_border_width(btn, 1, 0);
		lv_obj_add_event_cb(btn, item_click_cb, LV_EVENT_CLICKED, NULL);
	}

	lv_fs_dir_close(&dd);
}


/* ---- Screen init ---- */
void ui_FileManager_screen_init(void)
{
	if (ui_FileManager) return;

	ui_FileManager = lv_obj_create(NULL);
	lv_obj_clear_flag(ui_FileManager, LV_OBJ_FLAG_SCROLLABLE);
	ui_apply_gradient_background(ui_FileManager, 0xF8FBFF, 0xD7E6F2, LV_GRAD_DIR_VER);

	g_title = lv_label_create(ui_FileManager);
	lv_label_set_text(g_title, "S:/");
	lv_obj_set_style_text_color(g_title, lv_color_hex(UI_COLOR_BLUE_DARK), LV_PART_MAIN);
	lv_obj_set_style_text_font(g_title, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_align(g_title, LV_ALIGN_TOP_MID);
	lv_obj_set_y(g_title, 5);

	lv_obj_t *hint = lv_label_create(ui_FileManager);
	lv_label_set_text(hint, "\xE6\xBB\x91\xE5\x8A\xA8\xE6\xB5\x8F\xE8\xA7\x88 | \xE7\x82\xB9\xE5\x87\xBB\xE5\xB1\x8F\xE5\xB9\x95\xE8\xBF\x94\xE5\x9B\x9E");
	lv_obj_set_style_text_color(hint, lv_color_hex(UI_COLOR_TEXT_MUTED), LV_PART_MAIN);
	lv_obj_set_style_text_font(hint, &ui_font_CN14, LV_PART_MAIN);
	lv_obj_set_align(hint, LV_ALIGN_BOTTOM_MID);
	lv_obj_set_y(hint, -10);

	strcpy(g_path, "S:/");
	refresh_file_list();
	lv_obj_add_event_cb(ui_FileManager, touch_reset_idle, LV_EVENT_PRESSED, NULL);

	lv_obj_add_event_cb(ui_FileManager, ui_event_FileManager, LV_EVENT_CLICKED, NULL);
}


void ui_FileManager_screen_destroy(void)
{
	if (ui_FileManager) {
		lv_obj_del(ui_FileManager);
		ui_FileManager = NULL;
		g_list = NULL;
		g_title = NULL;
	}
}
