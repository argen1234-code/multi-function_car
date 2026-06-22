/**
 ****************************************************************************************************
 * @file        lv_mainstart.h
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
 
#ifndef __LV_MAINSTART_H
#define __LV_MAINSTART_H
#include "lvgl.h"
#include <malloc.h>
#include <time.h>
#include "lvgl/lvgl.h"
//*****************************************************APP_ABOUT***********************************************
lv_obj_t* app_about(lv_obj_t* parent);
void app_about_release_resource_cb(lv_event_t* event);

//***************************************************APP_FILE_M***********************************************
typedef enum
{
    DIR = 0,
    UNKNOW,
    PNG,
    JPG,
    MP3,
    BIN,
}file_type_t;

lv_obj_t* app_file_manager(lv_obj_t* parent);

//***************************************************APP_MUSIC***********************************************

lv_obj_t* app_music(void);
//*************************************moudle****************************************************************
/*组件与组件之间的间隔*/
#define MTM_GAP             6
/*组件宽度*/
#define MODULE_WIDTH        (LV_HOR_RES - MTM_GAP * 3) / 2
/*组件高度*/
#define MODULE_HEIGHT       (LV_VER_RES - MTM_GAP * 3) / 2

typedef struct _module_t
    {
        lv_obj_t*           container;        //监视模块承载Icon等组件的容器
        lv_obj_t*           icon;             //资源监视模块的图标
        lv_obj_t*           temp_bar;         //显示温度的数值条
        lv_obj_t*           temp_label;       //显示温度的标签
        lv_obj_t*           utilize_label;    //显示资源利用率的标签
        lv_obj_t*           utilize_chart;    //显示资源利用率的图表
        lv_chart_series_t*  utilize_series;
        lv_obj_t* module;
        lv_obj_t* icon1;
        lv_obj_t* hardware_name;
        lv_obj_t* usage_label;    //A label that displays resource utilization
        lv_obj_t* usage_chart;    //A graph showing resource utilization
        lv_chart_series_t* usage_series;
    }module_t;


    module_t* module_create_L(lv_obj_t* parent, const void* icon_src);

    void module_set_icon_L(module_t* module, const void* icon_src);

    void module_set_temp_value_L(module_t* module, int32_t value);

    void module_set_utilize_value_L(module_t* module, int32_t value);

    void module_set_chart_line_color_L(module_t* module, lv_color_t color);




//*************************************status*****************************************************************
typedef enum
{
    DISCONNECT = 0,
    CONNECT,
}connect_status_t;

void status_bar_init(void);
void status_bar_hidden(bool enable);
void status_bar_set_name(const char* app_name);
void status_bar_set_opa(lv_opa_t value);
void status_bar_set_wifi_status(connect_status_t status);
void status_bar_set_pc_status(connect_status_t status);



//****************************************app**************************************************

typedef struct _application_info_t
{
    char* name;

    const void* img_src; // 修改为const void*

    lv_obj_t* (*entry_point)(lv_obj_t* parent);

    void (*release_resource_cb)(lv_event_t* event);

}application_info_t;

void application_tile_init(lv_obj_t* tile);




//***********************************************perform*********************************************************

/*The interval between components and components*/

//
//typedef struct _module_t
//{
//    lv_obj_t* module;
//    lv_obj_t* icon;
//    lv_obj_t* hardware_name;
//    lv_obj_t* usage_label;    //A label that displays resource utilization
//    lv_obj_t* usage_chart;    //A graph showing resource utilization
//    lv_chart_series_t* usage_series;
//}module_t;


void module_set_icon(module_t* module, const void* icon_src);
void module_set_usage_value(module_t* module, int32_t value);
void module_set_chart_line_color(module_t* module, lv_color_t color);

void performance_tile_init(lv_obj_t* tile);



//****************************************asset********************************************************************
/*字体*/
//LV_FONT_DECLARE(lv_font_jetbrainsmono_16);
//LV_FONT_DECLARE(lv_font_jetbrainsmono_60);
//LV_FONT_DECLARE(lv_font_montserrat_16);

/*图片*/
LV_IMG_DECLARE(lv_img_wifi);
LV_IMG_DECLARE(lv_img_pc);
LV_IMG_DECLARE(lv_img_bg);
LV_IMG_DECLARE(lv_img_about);
LV_IMG_DECLARE(lv_img_system);

//#define DARK_STYLE 1

#ifdef DARK_STYLE
#define GLOBALE_COLOR lv_color_white()
#else
#define GLOBALE_COLOR lv_color_black()
#endif

//#define GLOBALE_FONT &lv_font_montserrat_16
//#define FONT &lv_font_oplusans_60

//***********************************************************************************************************************8
void lv_mainstart(void);

#endif
