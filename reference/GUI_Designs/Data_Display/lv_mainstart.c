/**
 ****************************************************************************************************
 * @file        lv_mainstart.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-07-20
 * @brief       LVGL应用程序
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 CH32V307开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */
 
#include "LVGL/GUI_APP/lv_mainstart.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdint.h>



/* 获取当前活动屏幕的宽高 */
#define scr_act_width()  lv_obj_get_width(lv_scr_act())
#define scr_act_height() lv_obj_get_height(lv_scr_act())
 lv_obj_t* tileview;
void tileview_load_tile_event_cb(lv_event_t* event);
void module_style_init(module_t* module, const void* icon_src);
//*********************************************ststus**************************************************************
/*Static Variable*/
lv_obj_t* status_bar;
 lv_obj_t* app_name_label;
 lv_obj_t* wifi_status_img;
 lv_obj_t* pc_status_img;     //上位机连接状态图标


/*Static Function*/
 void status_bar_hidden_anim(void* var, int32_t value);

/**
 * @brief
 * @param
*/
void status_bar_init(void)
{
    if (lv_obj_is_valid(status_bar))
    {
        printf("Error,Status Bar Exised!");
        return;
    }

    status_bar = lv_obj_create(lv_disp_get_layer_top(lv_disp_get_default()));
    //lv_obj_set_style_bg_color(status_bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(status_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(status_bar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(status_bar, LV_PCT(100), 28);

    app_name_label = lv_label_create(status_bar);
    lv_label_set_text(app_name_label, "Marjin");
    lv_obj_align(app_name_label, LV_ALIGN_LEFT_MID, 10, 0);


    lv_obj_t* right_container = lv_obj_create(status_bar);
    lv_obj_set_style_bg_opa(right_container, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_radius(right_container, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(right_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_container, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(right_container, 5, LV_PART_MAIN);
    lv_obj_clear_flag(right_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(right_container, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_flex_flow(right_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);

    wifi_status_img = lv_img_create(right_container);
    lv_img_set_src(wifi_status_img, &lv_img_wifi);
    lv_obj_set_style_img_recolor_opa(wifi_status_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(wifi_status_img, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);

    pc_status_img = lv_img_create(right_container);
    lv_img_set_src(pc_status_img, &lv_img_pc);
    lv_obj_set_style_img_recolor_opa(pc_status_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(pc_status_img, GLOBALE_COLOR, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(pc_status_img, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
}


/**
 * @brief
 * @param status_bar
 * @param enable
*/
void status_bar_hidden(bool enable)
{
    if (status_bar == NULL)
    {
        return;
    }
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, status_bar);
    lv_anim_set_time(&anim, 500);
    lv_anim_set_exec_cb(&anim, status_bar_hidden_anim);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);

    if (enable)
    {
        lv_anim_set_values(&anim, 0, -lv_obj_get_height(status_bar));
    }
    else
    {
        lv_anim_set_values(&anim, -lv_obj_get_height(status_bar), 0);
    }
    lv_anim_start(&anim);
}

void status_bar_set_name(const char* app_name)
{
    lv_label_set_text(app_name_label, app_name);
}

void status_bar_set_opa(lv_opa_t value)
{
    lv_obj_set_style_bg_opa(status_bar, value, LV_PART_MAIN);
}

void status_bar_set_wifi_status(connect_status_t status)
{
    if (status == CONNECT)
    {
        lv_obj_clear_flag(wifi_status_img, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(wifi_status_img, LV_OBJ_FLAG_HIDDEN);
    }
}

void status_bar_set_pc_status(connect_status_t status)
{
    if (status == CONNECT)
    {
        lv_obj_clear_flag(pc_status_img, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(pc_status_img, LV_OBJ_FLAG_HIDDEN);
    }
}


/**
 * @brief
 * @param var
 * @param value
*/
 void status_bar_hidden_anim(void* var, int32_t value)
{
    lv_obj_move_to(var, 0, value);
}



//*****************************************************APP_ABOUT***********************************************

 const char* MCU_Model = "ESP32-S3";

/*硬件信息*/
 lv_obj_t* cpu_model_label;
 lv_obj_t* cpu_temperature_label;
lv_obj_t* internal_heap_usage_label;
 lv_obj_t* external_heap_usage_label;
 lv_obj_t* sdcard_cap_label;
/*网络信息*/
 lv_obj_t* ipv4_addr_label;
//static lv_obj_t* ipv4_mask_label;
 lv_obj_t* ipv6_addr_label;
lv_obj_t* mac_address_label;
/*关于作者*/
//static lv_obj_t* github_label;
//static lv_obj_t* bilibili_label;
/*软件信息*/
//static lv_obj_t* espidf_version_label;
 lv_obj_t* startup_time_label;

 void app_about_update_data(lv_timer_t* timer);
lv_timer_t* app_about_update_data_timer;

lv_obj_t* about_list_add_item(lv_obj_t* parent, const char* name, const char* value);

lv_obj_t* app_about(lv_obj_t* parent)
{
    lv_obj_t* app_about_obj =  lv_obj_create(lv_scr_act());
    lv_obj_set_scroll_dir(app_about_obj, LV_DIR_VER);
    lv_obj_set_flex_flow(app_about_obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(app_about_obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(app_about_obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(app_about_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(app_about_obj, 30, LV_PART_MAIN);
    lv_obj_set_style_pad_row(app_about_obj, 0, LV_PART_MAIN);

    cpu_model_label = about_list_add_item(app_about_obj, "CPU型号", MCU_Model);
    cpu_temperature_label = about_list_add_item(app_about_obj, "CPU温度", "");
    internal_heap_usage_label = about_list_add_item(app_about_obj, "SRAM剩余", "");
    external_heap_usage_label = about_list_add_item(app_about_obj, "PSRAM剩余", "");
    sdcard_cap_label = about_list_add_item(app_about_obj, "SD卡剩余", "");
    ipv4_addr_label = about_list_add_item(app_about_obj, "IPv4地址", "");

    mac_address_label = about_list_add_item(app_about_obj, "MAC地址", "");
    startup_time_label = about_list_add_item(app_about_obj, "系统已运行时间", "");

    app_about_update_data_timer = lv_timer_create(app_about_update_data, 1000, NULL);
    return app_about_obj; // 添加返回语句
}


 lv_obj_t* about_list_add_item(lv_obj_t* parent, const char* name, const char* value)
{
    lv_obj_t* about_item = lv_btn_create(parent);
    lv_obj_remove_style_all(about_item);
    lv_obj_set_style_border_side(about_item, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(about_item, 1, LV_PART_MAIN);
    //lv_obj_set_style_bg_color(about_item, lv_color_black(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(about_item, LV_OPA_20, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(about_item, LV_OPA_50, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_pad_left(about_item, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(about_item, 10, LV_PART_MAIN);
    lv_obj_set_size(about_item, LV_HOR_RES, 40);

    lv_obj_t* item_name = lv_label_create(about_item);
    lv_label_set_text(item_name, name);
    lv_obj_set_style_text_font(item_name, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_align(item_name, LV_ALIGN_LEFT_MID);

    lv_obj_t* item_value = lv_label_create(about_item);
    lv_label_set_text(item_value, value);
    lv_label_set_long_mode(item_value, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(item_value, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(item_value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_align(item_value, LV_ALIGN_RIGHT_MID);

    return about_item;
}


 void app_about_update_data(lv_timer_t *timer)
{
    lv_label_set_text_fmt(lv_obj_get_child(cpu_temperature_label, 1), "%d℃", (int)lv_rand(0, 100));
    lv_label_set_text_fmt(lv_obj_get_child(internal_heap_usage_label, 1), "25%%\n2.1MB/8MB");
    lv_label_set_text_fmt(lv_obj_get_child(external_heap_usage_label, 1), "25%%\n2.1MB/8MB");
    lv_label_set_text_fmt(lv_obj_get_child(sdcard_cap_label, 1), "%.1f/%.1f", 3.1, 59.2);
    lv_label_set_text_fmt(lv_obj_get_child(ipv4_addr_label, 1), "%d:%d:%d:%d", 192, 168, 6, 110);

    if (lv_obj_is_valid(ipv6_addr_label))
    {
        lv_label_set_text_fmt(lv_obj_get_child(ipv6_addr_label, 1), "2048:1234:1234:1234:1234:1234:af0e:000d");
    }

    lv_label_set_text_fmt(lv_obj_get_child(mac_address_label, 1), "%s", "Not Know");
}

void app_about_release_resource_cb(lv_event_t* event)
{
    LV_LOG_WARN("ABOUT释放资源");
    lv_timer_del(app_about_update_data_timer);
}


//***************************************************APP_FILE_M***********************************************

 lv_obj_t* file_list;

 char current_path[LV_FS_MAX_PATH_LENGTH] = { 0 };

 void app_manager_update_file_list(char* path);
 void btn_event_cb(lv_event_t* event);

lv_obj_t* app_file_manager(lv_obj_t* parent)
{
    file_list = lv_list_create(lv_scr_act());
    lv_obj_set_scroll_dir(file_list, LV_DIR_VER);
    lv_obj_set_size(file_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(file_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(file_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(file_list, 30, LV_PART_MAIN);
    lv_obj_set_style_pad_row(file_list, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(file_list, &lv_font_montserrat_16, LV_PART_MAIN);

    lv_fs_get_letters(current_path);
    app_manager_update_file_list(current_path);

    return file_list;
}

 void app_manager_update_file_list(char* path)
{
    lv_fs_dir_t rddir_p;
    if (lv_fs_dir_open(&rddir_p, path) == LV_FS_RES_OK)
    {
        char fn[256];
        memset(fn, 0, 256);
        lv_obj_clean(file_list);

        if (strcmp(path, "//") != 0)
        {
            lv_obj_t* back_btn = lv_list_add_btn(file_list, NULL, /*"../上一级"*/"../Last");
            lv_obj_set_size(back_btn, LV_HOR_RES, 40);
            lv_obj_add_event_cb(back_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
        }

        while (lv_fs_dir_read(&rddir_p, fn) == LV_FS_RES_OK && strlen(fn) != 0)
        {
            file_type_t* file_type = (file_type_t*)lv_mem_alloc(sizeof(file_type_t));
            lv_obj_t* btn;

            if (fn[0] == '/')
            {
                *file_type = DIR;
                btn = lv_list_add_btn(file_list, LV_SYMBOL_DIRECTORY, fn + 1);
            }
            else if (strcmp(lv_fs_get_ext(fn), "png") == 0)
            {
                *file_type = PNG;
                btn = lv_list_add_btn(file_list, LV_SYMBOL_IMAGE, fn);
            }
            else
            {
                *file_type = UNKNOW;
                btn = lv_list_add_btn(file_list, LV_SYMBOL_FILE, fn);
            }

            lv_obj_set_user_data(btn, file_type);
            //lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
            //lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
            //lv_obj_set_style_bg_color(btn, lv_color_black(), LV_PART_MAIN | LV_STATE_FOCUSED);
            //lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_PART_MAIN | LV_STATE_FOCUSED);
            //lv_obj_set_style_bg_opa(btn, LV_OPA_50, LV_PART_MAIN | LV_STATE_PRESSED);
            //lv_obj_set_style_pad_left(btn, 10, LV_PART_MAIN);
            //lv_obj_set_style_pad_right(btn, 10, LV_PART_MAIN);
            lv_obj_set_size(btn, LV_HOR_RES, 40);
            lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_DELETE, NULL);
        }
        lv_fs_dir_close(&rddir_p);
    }
}

 void btn_event_cb(lv_event_t* event)
{
    file_type_t* file_type;

    switch (event->code)
    {
    case LV_EVENT_CLICKED:
        file_type = (file_type_t*)lv_obj_get_user_data(event->current_target);

        if (file_type == NULL)
        {
            if (lv_obj_get_index(lv_event_get_current_target(event)) == 0)
            {
                lv_fs_up(current_path);
                strcat(current_path, "/");
                app_manager_update_file_list(current_path);
            }
            return;
        }

        if (*file_type == DIR)
        {
          const char* dir_name = lv_list_get_btn_text(file_list, lv_event_get_current_target(event));

            strcat(current_path, dir_name);
            strcat(current_path, "/");

            app_manager_update_file_list(current_path);
        }
        else
        {

        }
        break;
    case LV_EVENT_DELETE:
        file_type = (file_type_t *)lv_obj_get_user_data(event->current_target);
        if (file_type != NULL)
        {
            lv_mem_free(file_type);
        }
        LV_LOG_WARN("DEL");
        break;
    default:
        break;
    }
}


//***************************************************APP_MUSIC***********************************************

lv_obj_t* app_music(void)
{
    lv_obj_t* container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(container, 30, LV_PART_MAIN);
    lv_obj_set_style_pad_row(container, 0, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(container);
    lv_label_set_text(label, "雨一直下");
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_width(label, LV_HOR_RES - 100);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -20);
    return container; // 添加返回语句
}




//************************************************MOUDLE_L********************************************************
/**
 * @brief Chart drawing event
 * @param event
*/
 void chart_draw_event_cb_L(lv_event_t* event)
{
    lv_obj_t* obj = lv_event_get_target(event);

    /*在线条绘制之前添加褪色的区域*/
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(event);
    if (dsc->part == LV_PART_ITEMS)
    {
        if (!dsc->p1 || !dsc->p2)
        {
            return;
        }

        /*添加一个线条蒙版，保持线条下方的区域*/
        lv_draw_mask_line_param_t line_mask_param;
        lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y, LV_DRAW_MASK_LINE_SIDE_BOTTOM);
        int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

        /*添加淡出效果:透明的底部覆盖顶部*/
        lv_coord_t h = lv_obj_get_height(obj);
        lv_draw_mask_fade_param_t fade_mask_param;
        lv_draw_mask_fade_init(&fade_mask_param, &obj->coords, LV_OPA_COVER, obj->coords.y1 + h / 8, LV_OPA_TRANSP, obj->coords.y2);
        int16_t fade_mask_id = lv_draw_mask_add(&fade_mask_param, NULL);

        /*绘制一个受蒙版影响的矩形*/
        lv_draw_rect_dsc_t draw_rect_dsc;
        lv_draw_rect_dsc_init(&draw_rect_dsc);
        draw_rect_dsc.bg_opa = LV_OPA_20;
        draw_rect_dsc.bg_color = dsc->line_dsc->color;

//        lv_area_t area;
//        area.x1 = dsc->p1->x;
//        area.x2 = dsc->p2->x - 1;
//        area.y1 = LV_MIN(dsc->p1->y, dsc->p2->y);
//        area.y2 = obj->coords.y2;
        //lv_draw_rect(, dsc->rect_dsc, &area);

        /*删除蒙版*/
        lv_draw_mask_free_param(&line_mask_param);
        lv_draw_mask_free_param(&fade_mask_param);
        lv_draw_mask_remove_id(line_mask_id);
        lv_draw_mask_remove_id(fade_mask_id);
    }
}


/**
 * @brief Initializing a module
 * @param module The module to be initialized
 * @param icon_src Icon of the module
*/
 void module_style_init_L(module_t* module, const void* icon_src)
{
    lv_obj_remove_style_all(module->container);
    lv_obj_set_style_radius(module->container, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(module->container, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_bg_color(module->container, lv_color_hex(0x151515), LV_PART_MAIN);
    lv_obj_set_size(module->container, MODULE_WIDTH, MODULE_HEIGHT );

    /*Icon*/
    lv_obj_set_size(module->icon, 28, 28);
    lv_obj_align(module->icon, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_update_layout(module->icon);
    if (icon_src != NULL)
    {
        lv_img_set_src(module->icon, icon_src);
    }

    /*Temperature Bar*/
    lv_obj_remove_style_all(module->temp_bar);
    lv_obj_set_style_radius(module->temp_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(module->temp_bar, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(module->temp_bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(module->temp_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(module->temp_bar, lv_color_hex(0xFF6B6B), LV_PART_INDICATOR);
    lv_obj_set_size(module->temp_bar, MODULE_WIDTH - (lv_obj_get_width(module->icon) + 14), 18);
    lv_obj_align_to(module->temp_bar, module->icon, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    lv_bar_set_range(module->temp_bar, 0, 100);
    lv_bar_set_value(module->temp_bar, 50, LV_ANIM_ON);

    /*Temperature Label*/
    lv_obj_remove_style_all(module->temp_label);
    lv_obj_set_style_text_color(module->temp_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(module->temp_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(module->temp_label);
    lv_label_set_text(module->temp_label, "00");

    /*Utilize Label*/
    lv_obj_remove_style_all(module->utilize_label);
    lv_obj_set_style_text_color(module->utilize_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(module->utilize_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(module->utilize_label, LV_ALIGN_CENTER, 0, 8);

    /*Utilize Chart*/
    lv_obj_move_background(module->utilize_chart);
    lv_obj_set_style_bg_opa(module->utilize_chart, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_line_width(module->utilize_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(module->utilize_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(module->utilize_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_size(module->utilize_chart, 0, LV_PART_INDICATOR); //在数据上不显示点
    lv_obj_set_style_opa(module->utilize_chart, LV_OPA_80, LV_PART_ITEMS);
    lv_obj_set_size(module->utilize_chart, MODULE_WIDTH, LV_PCT(40));
    lv_obj_set_align(module->utilize_chart, LV_ALIGN_BOTTOM_LEFT);
    lv_chart_set_type(module->utilize_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(module->utilize_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(module->utilize_chart, 60);
    lv_chart_set_update_mode(module->utilize_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(module->utilize_chart, 10, 10);
    lv_obj_add_event_cb(module->utilize_chart, chart_draw_event_cb_L, LV_EVENT_DRAW_PART_BEGIN, NULL);
}


/**
 * @brief
 * @param parent
 * @param icon_src
 * @return
*/
module_t* module_create_L(lv_obj_t* parent, const void* icon_src)
{
    module_t* _module = (module_t*)malloc(sizeof(module_t));

    if (_module != NULL)
    {
        _module->container = lv_obj_create(parent);

        _module->icon = lv_img_create(_module->container);

        _module->temp_bar = lv_bar_create(_module->container);

        _module->temp_label = lv_label_create(_module->temp_bar);

        _module->utilize_label = lv_label_create(_module->container);

        _module->utilize_chart = lv_chart_create(_module->container);

        _module->utilize_series = lv_chart_add_series(_module->utilize_chart, lv_palette_main(LV_PALETTE_RED)/*线条颜色*/, LV_CHART_AXIS_PRIMARY_Y);

        module_style_init_L(_module, icon_src);
    }

    return _module;
}


/**
 * @brief 设置模块的图标
 * @param module 需要设置的模块
 * @param icon_src 图标的路径
*/
void module_set_icon_L(module_t* module, const void* icon_src)
{
    if (module != NULL)
    {
        lv_img_set_src(module->icon, icon_src);
    }
}


/**
 * @brief 设置模块中温度的值
 * @param module 需要设置的模块
 * @param value 温度
*/
void module_set_temp_value_L(module_t* module, int32_t value)
{
    lv_bar_set_value(module->temp_bar, value, LV_ANIM_ON);
    lv_label_set_text_fmt(module->temp_label, "%02d", value);
}


/**
 * @brief 设置模块中资源利用率的值
 * @param module 需要设置的模块
 * @param value 利用率
*/
void module_set_utilize_value_L(module_t* module, int32_t value)
{
    lv_label_set_text_fmt(module->temp_label, "%02d", value);
}


/**
 * @brief 设置模块中图表折线的颜色
 * @param module 需要设置的模块
 * @param color 颜色
*/
void module_set_chart_line_color_L(module_t* module, lv_color_t color)
{
    lv_chart_set_series_color(module->utilize_chart, module->utilize_series, color);
}
//****************************************************app****************************************
/*Global Variable*/
lv_obj_t* running_application;

/*Static Variable*/
 lv_obj_t* container;

/*Static Function*/
 void application_reg(application_info_t* application_info);
 void application_button_event_cb(lv_event_t* event);


/**
 * @brief
 * @param tile
*/
void application_tile_init(lv_obj_t* tile)
{
    container = lv_obj_create(tile);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_snap_x(container, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_style_bg_opa(container, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(container, 30, LV_PART_MAIN);   //图标之间的间隙
    lv_obj_center(container);

    static application_info_t app1 =
    {
        .name = "APP_ABOUT",
        .img_src = &lv_img_about,
        .entry_point = app_about,
        .release_resource_cb = app_about_release_resource_cb
    };
    application_reg(&app1);

    static application_info_t app2 =
    {
        .name = "FILE_MANAGER",
        .entry_point = app_file_manager,
        .img_src = &lv_img_system,
        .release_resource_cb = NULL
    };
    application_reg(&app2);

    static application_info_t app3 =
    {
        .name = "APP_ABOUT",
        .entry_point = app_about,
        .release_resource_cb = app_about_release_resource_cb
    };
    application_reg(&app3);

    static application_info_t app4 =
    {
        .name = "APP_ABOUT",
        .entry_point = app_about,
        .release_resource_cb = app_about_release_resource_cb
    };
    application_reg(&app4);

    static application_info_t app5 =
    {
        .name = "APP_ABOUT",
        .entry_point = app_about,
        .release_resource_cb = app_about_release_resource_cb
    };
    application_reg(&app5);

    static application_info_t app6 =
    {
        .name = "APP_ABOUT",
        .entry_point = app_about,
        .release_resource_cb = app_about_release_resource_cb
    };
    application_reg(&app6);

    static application_info_t app7 =
    {
        .name = "APP_ABOUT",
        .entry_point = app_about,
        .release_resource_cb = app_about_release_resource_cb
    };
    application_reg(&app7);

    static application_info_t app8 =
    {
        .name = "APP_ABOUT",
        .entry_point = app_about,
        .release_resource_cb = app_about_release_resource_cb
    };
    application_reg(&app8);

    uint32_t mid_btn_index = (lv_obj_get_child_cnt(container) - 1) / 2;
    for (uint32_t i = 0; i < mid_btn_index; i++)
    {
        lv_obj_move_to_index(lv_obj_get_child(container, -1), 0);
    }
    /*当按钮数为偶数时，确保按钮居中*/
    lv_obj_scroll_to_view(lv_obj_get_child(container, mid_btn_index), LV_ANIM_OFF);
}

 void application_reg(application_info_t* application_info)
{
    lv_obj_t* btn = lv_btn_create(container);
    lv_obj_set_size(btn, 90, 90);
    lv_obj_add_event_cb(btn, application_button_event_cb, LV_EVENT_FOCUSED, application_info->name);
    lv_obj_add_event_cb(btn, application_button_event_cb, LV_EVENT_CLICKED, application_info);
    lv_obj_add_event_cb(btn, application_button_event_cb, LV_EVENT_SIZE_CHANGED, NULL);

    //lv_obj_set_style_radius(btn, 20, LV_PART_MAIN);
    //lv_obj_set_style_shadow_width(btn, 40, LV_PART_MAIN);
    //lv_obj_set_style_shadow_spread(btn, 5, LV_PART_MAIN);
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_bg_opa(btn, LV_OPA_0, LV_PART_MAIN);


    if (application_info->img_src != NULL)
    {
        lv_obj_t* img = lv_img_create(btn);
        lv_img_set_src(img, application_info->img_src);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    }
}


/**
 * @brief 处理按钮事件的回调函数
 * @param event
*/
 void application_button_event_cb(lv_event_t* event)
{
    lv_obj_t* current_btn = lv_event_get_current_target(event);
    if (event->code == LV_EVENT_FOCUSED)
    {
        uint32_t current_btn_index = lv_obj_get_index(current_btn);
        uint32_t mid_btn_index = (lv_obj_get_child_cnt(container) - 1) / 2;

        if (current_btn_index > mid_btn_index)
        {
            lv_obj_scroll_to_view(lv_obj_get_child(container, mid_btn_index - 1), LV_ANIM_OFF);
            lv_obj_scroll_to_view(lv_obj_get_child(container, mid_btn_index), LV_ANIM_ON);
            lv_obj_move_to_index(lv_obj_get_child(container, 0), -1);
        }
        else if (current_btn_index < mid_btn_index)
        {
            lv_obj_scroll_to_view(lv_obj_get_child(container, mid_btn_index + 1), LV_ANIM_OFF);
            lv_obj_scroll_to_view(lv_obj_get_child(container, mid_btn_index), LV_ANIM_ON);
            lv_obj_move_to_index(lv_obj_get_child(container, -1), 0);
        }
        for (uint8_t i = 0; i < 3; i++)
        {
            lv_obj_set_size(lv_obj_get_child(container, mid_btn_index - i), 90 - i * 10, 90 - i * 10);
            lv_obj_set_size(lv_obj_get_child(container, mid_btn_index + i), 90 - i * 10, 90 - i * 10);
            lv_obj_set_style_bg_opa(lv_obj_get_child(container, mid_btn_index - i), 255 - 50 * i, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(lv_obj_get_child(container, mid_btn_index + i), 255 - 50 * i, LV_PART_MAIN);
        }

        status_bar_set_name((char*)lv_event_get_user_data(event));
    }
    else if (event->code == LV_EVENT_SIZE_CHANGED)
    {
        /*缩放图标*/
        lv_obj_t* img = lv_obj_get_child(current_btn, 0);

        if (lv_obj_is_valid(img))
        {
            lv_img_set_zoom(img, (uint16_t)(lv_obj_get_width(current_btn) * 0.7 / 64 * LV_IMG_ZOOM_NONE));
        }
    }
    else if (event->code == LV_EVENT_CLICKED)
    {
        /*获取正准备打开APP的信息*/
        application_info_t* application_info = lv_event_get_user_data(event);
        lv_obj_t* (*app_entry_point)(lv_obj_t * parent) = application_info->entry_point;

        /*创建一个基本对象，用于承载APP的控件*/
        lv_obj_t* base_obj = lv_obj_create(lv_scr_act());
        lv_obj_set_size(base_obj, LV_PCT(100), LV_PCT(100));
        lv_obj_set_scrollbar_mode(base_obj, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(base_obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_top(base_obj, 30, LV_PART_MAIN);
        lv_obj_set_style_pad_row(base_obj, 0, LV_PART_MAIN);

        status_bar_set_opa(LV_OPA_COVER);
        /*打开APP并将基本对象赋值给当前正在运行的程序，返回按钮触发时将删除该对象*/
        running_application = app_entry_point(base_obj);
        /*添加一个删除事件到基本对象，当APP退出时调用该回调函数释放定时器、数据结构等资源*/
        lv_obj_add_event_cb(base_obj, application_info->release_resource_cb, LV_EVENT_DELETE, NULL);
        running_application = base_obj;
    }
    else if (event->code == LV_EVENT_SCROLL)
    {
        LV_LOG_ERROR("H");
    }
}






//*****************************************************perform**************************************

module_t* cpu_module;
module_t* mem_module;
module_t* gpu_module;

/**
 * @brief Chart drawing event
 * @param event
*/
 void chart_draw_event_cb(lv_event_t* event)
{
    lv_obj_t* obj = lv_event_get_target(event);

    /*在线条绘制之前添加褪色的区域*/
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(event);
    if (dsc->part == LV_PART_ITEMS)
    {
        if (!dsc->p1 || !dsc->p2)
        {
            return;
        }

        /*添加一个线条蒙版，保持线条下方的区域*/
        lv_draw_mask_line_param_t line_mask_param;
        lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y, LV_DRAW_MASK_LINE_SIDE_BOTTOM);
        int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

        /*添加淡出效果:透明的底部覆盖顶部*/
        lv_coord_t h = lv_obj_get_height(obj);
        lv_draw_mask_fade_param_t fade_mask_param;
        lv_draw_mask_fade_init(&fade_mask_param, &obj->coords, LV_OPA_COVER, obj->coords.y1 + h / 8, LV_OPA_TRANSP, obj->coords.y2);
        int16_t fade_mask_id = lv_draw_mask_add(&fade_mask_param, NULL);

        /*绘制一个受蒙版影响的矩形*/
        lv_draw_rect_dsc_t draw_rect_dsc;
        lv_draw_rect_dsc_init(&draw_rect_dsc);
        draw_rect_dsc.bg_opa = LV_OPA_20;
        draw_rect_dsc.bg_color = dsc->line_dsc->color;

        lv_area_t area;
        area.x1 = dsc->p1->x;
        area.x2 = dsc->p2->x - 1;
        area.y1 = LV_MIN(dsc->p1->y, dsc->p2->y);
        area.y2 = obj->coords.y2;
        lv_draw_rect(dsc->draw_ctx, dsc->rect_dsc, &area);

        /*删除蒙版*/
        lv_draw_mask_free_param(&line_mask_param);
        lv_draw_mask_free_param(&fade_mask_param);
        lv_draw_mask_remove_id(line_mask_id);
        lv_draw_mask_remove_id(fade_mask_id);
    }
}


/**
 * @brief Initializing a module
 * @param module The module to be initialized
 * @param icon_src Icon of the module
*/
 void module_style_init(module_t* module, const void* icon_src)
{
    lv_obj_remove_style_all(module->module);
    lv_obj_set_style_radius(module->module, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(module->module, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_bg_color(module->module, lv_color_hex(0x151515), LV_PART_MAIN);
    lv_obj_set_size(module->module, MODULE_WIDTH, MODULE_HEIGHT);
    lv_obj_clear_flag(module->module, LV_OBJ_FLAG_SCROLLABLE);

    /*Icon*/
    lv_obj_set_size(module->icon1, 28, 28);
    lv_obj_align(module->icon1, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_update_layout(module->icon1);
    if (icon_src != NULL)
    {
        lv_img_set_src(module->icon1, icon_src);
    }

    /*Hardware Name*/
    lv_obj_align_to(module->hardware_name, module->icon1, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_text_color(module->hardware_name, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(module->hardware_name, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_width(module->hardware_name, MODULE_WIDTH - 28);
    lv_label_set_long_mode(module->hardware_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(module->hardware_name, "NVIDA RTX 3080Ti");

    /*Utilize Label*/
    lv_obj_remove_style_all(module->usage_label);
    lv_obj_set_style_text_color(module->usage_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(module->usage_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_label_set_text(module->usage_label, "40%");
    lv_obj_align(module->usage_label, LV_ALIGN_CENTER, 0, 8);

    /*Utilize Chart*/
    lv_obj_move_background(module->usage_chart);
    lv_obj_set_style_bg_opa(module->usage_chart, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_line_width(module->usage_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(module->usage_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(module->usage_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_size(module->usage_chart, 0, LV_PART_INDICATOR);     //在折线上不显示点
    lv_obj_set_style_opa(module->usage_chart, LV_OPA_80, LV_PART_ITEMS);
    lv_obj_set_size(module->usage_chart, MODULE_WIDTH, LV_PCT(40));
    lv_obj_align(module->usage_chart, LV_ALIGN_BOTTOM_LEFT, 0, -5);
    lv_chart_set_type(module->usage_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(module->usage_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(module->usage_chart, 30);
    lv_chart_set_update_mode(module->usage_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(module->usage_chart, 10, 10);
    lv_obj_add_event_cb(module->usage_chart, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
}


/**
 * @brief
 * @param parent
 * @param icon_src
 * @return
*/
 module_t* module_create(lv_obj_t* parent, const void* icon_src)
{
    module_t* _module = (module_t*)malloc(sizeof(module_t));

    if (_module != NULL)
    {
        _module->module = lv_obj_create(parent);
        _module->icon1 = lv_img_create(_module->module);
        _module->hardware_name = lv_label_create(_module->module);
        _module->usage_label = lv_label_create(_module->module);
        _module->usage_chart = lv_chart_create(_module->module);
        _module->usage_series = lv_chart_add_series(_module->usage_chart, lv_palette_main(LV_PALETTE_RED)/*线条颜色*/, LV_CHART_AXIS_PRIMARY_Y);
        module_style_init(_module, icon_src);
    }

    return _module;
}


void module_set_icon(module_t* module, const void* icon_src)
{
    if (module != NULL)
    {
        lv_img_set_src(module->icon1, icon_src);
    }
}


void module_set_usage_value(module_t* module, int32_t value)
{
    lv_label_set_text_fmt(module->usage_label, "%02d%%", value);
}


void module_set_chart_line_color(module_t* module, lv_color_t color)
{
    lv_chart_set_series_color(module->usage_chart, module->usage_series, color);
}

 void chart_add_value(lv_timer_t* timer)
{
    uint32_t cpu_usage = lv_rand(1, 50);
    uint32_t mem_usage = lv_rand(1, 50);
    uint32_t gpu_usage = lv_rand(1, 50);

    module_set_usage_value(cpu_module, cpu_usage);
    lv_chart_set_next_value(cpu_module->usage_chart, cpu_module->usage_series, cpu_usage);
    module_set_usage_value(mem_module, mem_usage);
    lv_chart_set_next_value(mem_module->usage_chart, mem_module->usage_series, mem_usage);
    module_set_usage_value(gpu_module, gpu_usage);
    lv_chart_set_next_value(gpu_module->usage_chart, gpu_module->usage_series, gpu_usage);
}

void performance_tile_init(lv_obj_t* tile)
{
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(tile, MTM_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tile, MTM_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_column(tile, MTM_GAP, LV_PART_MAIN);

    cpu_module = module_create(tile, NULL);
    mem_module = module_create(tile, NULL);
    gpu_module = module_create(tile, NULL);
    //module_create(tile, NULL);

    lv_timer_create(chart_add_value, 1000, NULL);
}

//***************************************************************************************************





/**
 * @brief  图片部件实例
 * @param  无
 * @return 无
 */
 void lv_example_img(void)
{
   // lv_disp_t* display = lv_disp_get_default();

        //lv_theme_t* th = lv_theme_default_init
        //(
        //    display,
        //    lv_color_hex(0xFFFFFF),
        //    lv_color_hex(0x000000),
        //    false,
        //    &lv_font_montserrat_16
        //);

        //lv_disp_set_theme(display, th);

    status_bar_init();


        /*桌面TileView创建，实现Android桌面左右滑动的效果*/
        tileview = lv_tileview_create(lv_scr_act());
        lv_obj_add_event_cb(tileview, tileview_load_tile_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
        //lv_obj_set_style_bg_img_src(tileview, &lv_img_bg, LV_PART_MAIN);

        lv_obj_t* performance_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
        performance_tile_init(performance_tile);



        lv_obj_t* application_tile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT);
        application_tile_init(application_tile);

        /*设置主页面为时间所在页*/
       lv_obj_set_tile(tileview, application_tile, LV_ANIM_OFF);
    }


    void tileview_load_tile_event_cb(lv_event_t* event)
    {
        static bool first_trigger = false;
        if (lv_obj_get_index(lv_tileview_get_tile_act(tileview)) == 0 && first_trigger == false)
        {
            LV_LOG_ERROR("Hello,Performance");
            status_bar_hidden(true);
            first_trigger = true;
        }
        else if (lv_obj_get_index(lv_tileview_get_tile_act(tileview)) == 1)
        {
            LV_LOG_ERROR("Hello,Home");
            status_bar_set_name("Marjin");
            if (first_trigger == true)
            {
                status_bar_hidden(false);
            }
            first_trigger = false;
        }
        else if (lv_obj_get_index(lv_tileview_get_tile_act(tileview)) == 2 && first_trigger == false)
        {
            LV_LOG_ERROR("Hello,Application");
            //first_trigger = true;
        }
    }






/**
 * @brief  LVGL演示
 * @param  无
 * @return 无
 */
void lv_mainstart(void)
{
    lv_example_img();
}

