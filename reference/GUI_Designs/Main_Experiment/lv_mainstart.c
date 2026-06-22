/**
 ****************************************************************************************************
 * @file        lv_mainstart.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-03-23
 * @brief       LVGL综合 实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 探索者 F407开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */
 
#include "LVGL/GUI_APP/lv_mainstart.h"
#include "./FATFS/exfuns/exfuns.h"
#include "./MALLOC/malloc.h"
#include "./BSP/LCD/lcd.h"
#include "./SYSTEM/usart/usart.h"
#include "LVGL/GUI_APP/lv_qr.h"
#include "LVGL/GUI_APP/lv_draw.h"
#include "LVGL/GUI_APP/lv_file.h"
#include "LVGL/GUI_APP/lv_shelf.h"
#include "LVGL/GUI_APP/lv_setting.h"
#include "LVGL/GUI_APP/lv_calculator.h"
#include "LVGL/GUI_APP/lv_meter.h"
#include "LVGL/GUI_APP/lv_scale.h"
#include "LVGL/GUI_APP/lv_scale.h"
#include "lvgl.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "./TEXT/text.h"
#include "lvgl/lvgl.h"
lv_m_general lv_general_dev;

typedef struct
{
    char* app_text_English;
    char* app_text_Chinese;
    uint16_t app_witch;
    uint16_t app_hietch;
}app_image_info;


/* 图片库存放在磁盘中的路径 */
char *const IMAGE_GBK_PATH[10] =
{
    "0:/PICTURE/LVGLBIN/calculator.bin",
    "0:/PICTURE/LVGLBIN/File.bin",
    "0:/PICTURE/LVGLBIN/lv_system.bin",
    "0:/PICTURE/LVGLBIN/Setting.bin",
    "0:/PICTURE/LVGLBIN/Test.bin",
    "0:/PICTURE/LVGLBIN/Timer.bin",
    "0:/PICTURE/LVGLBIN/lv_qr.bin",
    "0:/PICTURE/LVGLBIN/lv_draw.bin",
  	"0:/PICTURE/LVGLBIN/label.bin",
	  "0:/PICTURE/BMP/au.bmp",
};

#define IMAGE_GBK_NUM (int)(sizeof(IMAGE_GBK_PATH)/sizeof(IMAGE_GBK_PATH[0]))
    
static const app_image_info app_image[] =
{
    {" "," ",NULL},
    {"Calculator","计算器",146,140},
    {"File","文件管理器",146,140},
    {"System","进制",146,140},
    {"Setting","设置",146,140},
    {"Test","测试",146,140},
    {"Timer","时钟",304,140},
    {"Qrcode","二维码",146,140},
    {"Draw","绘画",304,140},
};

/* 获取路径的个数 */
#define image_mun (int)(sizeof(app_image)/sizeof(app_image[0]))
/* 设置一个app数组 */
lv_obj_t *lv_app_t[image_mun];
/* 设置一个app名字数组 */
lv_obj_t *lv_app_name[image_mun];
/* 设置一个app图片数组 */
lv_obj_t* lv_app_img[image_mun];
/* app就绪表 */
unsigned int  app_readly_list[32];
/* app触发位 */
int lv_trigger_bit = 0;



//****************************************************************************************************************************************************
// 字体声明
LV_FONT_DECLARE(Font12);
LV_FONT_DECLARE(Font13);
// 新增：全局动画完成计数器
static int hide_anim_count = 0;
// 全局对象指针
static lv_obj_t *text_obj;
static lv_obj_t *text_label;
static lv_obj_t *text_obj1;
static lv_obj_t *text_label1;

// 文本常量
static const char *full_text = "碳友智算";
static const char *full_text1 = "tan  you  zhi  suan";
static size_t text_len = 0;
static size_t text_len1 = 0;

// 第一动画回调函数声明
static void text_anim_a_cb(void *a, int32_t v);
static void text_anim_b_cb(void *a, int32_t v);
static void text_anim_a_end(lv_anim_t *anim1);
static void text_anim_b_end(lv_anim_t *anim2);
static void lv_typing_effect(lv_timer_t *task);
//第二动画函数
static lv_obj_t * author_obj;   //界面父对象
static lv_obj_t * label_img;
static lv_obj_t * author_photo_img;
static lv_obj_t * label_obj;

static void display_anim(void *var);
static void bounce_anim(void *var);
static void left_move_anim(void *var);
static void obj_hide_anim(void *var);
static void lv_boot_anim2_author(void);


void _ttywrch1(int ch) {
    // 空实现
}

// 辅助函数
size_t my_strnlen(const char *str, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && str[len] != '\0') {
        len++;
    }
    return len;
}

char *strndup(const char *s, size_t n) {
    size_t len = my_strnlen(s, n);
    char *dup = (char *)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

// 第一段动画：打字效果及渐变消失
static void text_anim_a_cb(void *a, int32_t v) {
    lv_obj_set_style_text_opa(a, v, LV_PART_MAIN);
}

static void text_anim_b_cb(void *a, int32_t v) {
    lv_obj_set_style_text_opa(a, v, LV_PART_MAIN);
}

static void text_anim_a_end(lv_anim_t *anim1) {
    lv_anim_del(text_label, NULL); // 删除动画
}

static void text_anim_b_end(lv_anim_t *anim2) {
    // 1. 删除动画
    lv_anim_del(text_label, NULL);
    lv_anim_del(text_label1, NULL);
    
    // 2. 删除对象（含子对象）
    lv_obj_del(text_obj);
    lv_obj_del(text_obj1);
    
    // 3. 释放定时器
    static lv_timer_t *typing_timer = NULL;
    if (typing_timer) {
        lv_timer_del(typing_timer);
        typing_timer = NULL;
    }
    
    // 4. 清空全局指针
    text_label = NULL;
    text_label1 = NULL;
    text_obj = NULL;
    text_obj1 = NULL;
    
    // 5. 启动第二段动画
    lv_boot_anim2_author();
}

// 打字效果函数
    static void lv_typing_effect(lv_timer_t *task) {
    static char *prev_text = NULL;  // 用于存储之前分配的内存地址

    if (text_len < strlen(full_text)) {
        if (prev_text) {
            free(prev_text);  // 释放之前分配的内存
        }
        text_len++;
        prev_text = strndup(full_text, text_len);
        lv_label_set_text(text_label, prev_text);
    } else {
        if (text_len1 < strlen(full_text1)) {
            if (prev_text) {
                free(prev_text);  // 释放之前分配的内存
            }
            text_len1++;
            prev_text = strndup(full_text1, text_len1);
            lv_label_set_text(text_label1, prev_text);
        } else {
            if (prev_text) {
                free(prev_text);  // 释放之前分配的内存
            }
            lv_timer_del(task); // 打字效果完成后删除定时器

            lv_anim_t text_label_a;
            lv_anim_init(&text_label_a);
            lv_anim_set_var(&text_label_a, text_label);
            lv_anim_set_values(&text_label_a, 255, 0);
            lv_anim_set_exec_cb(&text_label_a, text_anim_a_cb);
            lv_anim_set_time(&text_label_a, 2000); // 动画时间
            lv_anim_set_ready_cb(&text_label_a, text_anim_a_end); // 设置动画结束回调函数
            lv_anim_start(&text_label_a);

            lv_anim_t text_label_b;
            lv_anim_init(&text_label_b);
            lv_anim_set_var(&text_label_b, text_label1);
            lv_anim_set_values(&text_label_b, 255, 0);
            lv_anim_set_exec_cb(&text_label_b, text_anim_b_cb);
            lv_anim_set_time(&text_label_b, 2000); // 动画时间
            lv_anim_set_ready_cb(&text_label_b, text_anim_b_end); // 设置动画结束回调函数
            lv_anim_start(&text_label_b);
        }
    }
}

// 创建打字效果界面
void create_typing_effect(lv_obj_t *parent) {
    text_obj = lv_obj_create(parent);
    lv_obj_set_size(text_obj, 400, 100);
    lv_obj_align(text_obj, LV_ALIGN_CENTER, 0, -100);
    lv_obj_set_style_bg_opa(text_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_opa(text_obj, 0, LV_PART_MAIN);

    text_obj1 = lv_obj_create(parent);
    lv_obj_set_size(text_obj1, 700, 100);
    lv_obj_align(text_obj1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(text_obj1, 0, LV_PART_MAIN);
    lv_obj_set_style_border_opa(text_obj1, 0, LV_PART_MAIN);
    
    text_label = lv_label_create(text_obj);
    lv_obj_set_style_text_font(text_label, &Font12, LV_PART_MAIN);
    lv_label_set_text(text_label, " ");
    lv_obj_set_style_text_color(text_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(text_label, LV_ALIGN_CENTER, 0, 0);

    text_label1 = lv_label_create(text_obj1);
    lv_obj_set_style_text_font(text_label1, &Font13, LV_PART_MAIN);
    lv_label_set_text(text_label1, " ");
    lv_obj_set_style_text_color(text_label1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(text_label1, LV_ALIGN_CENTER, 0, 0);

    text_len1 = 0;
    text_len = 0;
    // 创建定时器，每隔 150 毫秒调用一次 typing_effect 函数
    lv_timer_create(lv_typing_effect, 100, NULL);
}

// 下移动画回调函数
// 功能：在动画过程中，根据传入的值 v 更新对象 var 的 Y 坐标
static void labelimg_anim_y_cb(void * var, int32_t v)
{
    lv_obj_set_y(var, v);
}

// label 动画结束后的回调函数
// 功能：删除 label_img 对象上与 label_anim_y_cb 相关的动画效果
// 接着启动作者照片的显示动画和标签的左移展开动画
static void label_ovenr_anim_end(lv_anim_t  *var)
{
    lv_anim_del(label_img, labelimg_anim_y_cb);  
    display_anim(author_photo_img);
    left_move_anim(label_obj);
	  
}

// 弹跳动画函数
// 功能：创建并启动一个弹跳动画，让传入的对象 var 产生弹跳效果
// 动画从对象当前的 Y 坐标移动到 Y 坐标为 100 的位置，动画时长 1500 毫秒
// 动画轨迹为撞墙效果，动画结束后调用 labelimg_ovenr_anim_end 回调函数
static void bounce_anim(void *var)
{
    lv_anim_t labelimg_a;
    lv_anim_init(&labelimg_a);
    lv_anim_set_var(&labelimg_a, var);
    lv_anim_set_values(&labelimg_a, lv_obj_get_y(var), 100);
    lv_anim_set_time(&labelimg_a, 1500);
    lv_anim_set_exec_cb(&labelimg_a, labelimg_anim_y_cb);
    lv_anim_set_path_cb(&labelimg_a, lv_anim_path_bounce); 
    lv_anim_set_ready_cb(&labelimg_a, label_ovenr_anim_end);
    lv_anim_start(&labelimg_a);
}

// 照片逐渐显示动画回调函数
// 功能：在动画过程中，根据传入的值 v 更新对象 var 的图片透明度
static void author_photo_anim_disp_cb(void * var, int32_t v)
{
    lv_obj_set_style_img_opa(var, v, 0);
}

// 照片显示动画结束后的回调函数
// 功能：删除 author_photo_img 对象上与 author_photo_anim_disp_cb 相关的动画效果
// 接着启动 label_img、author_photo_img 和 label_obj 的隐藏动画
static void author_photo_anim_end(lv_anim_t *var)
{
	
	  
    lv_anim_del(author_photo_img, author_photo_anim_disp_cb);
    
}

// 隐藏对象开始显示动画函数
// 功能：创建并启动一个显示动画，让传入的对象 var 从透明度为 0 逐渐显示到透明度为 255
// 动画时长 2550 毫秒，动画结束后调用 author_photo_anim_end 回调函数
static void display_anim(void *var)
{
    lv_anim_t author_photo_a;
    lv_anim_init(&author_photo_a);
    lv_anim_set_var(&author_photo_a, var);
    lv_anim_set_values(&author_photo_a, 0, 255);
    lv_anim_set_time(&author_photo_a, 2250);
    lv_anim_set_exec_cb(&author_photo_a, author_photo_anim_disp_cb);
    lv_anim_set_ready_cb(&author_photo_a, author_photo_anim_end);
    lv_anim_start(&author_photo_a);
}

// 对象向左展开动画回调函数
// 功能：在动画过程中，根据传入的值 v 更新对象 var 的宽度
static void label_anim_y_cb(void * var, int32_t v)
{
    lv_obj_set_width(var, v);
}

// 左移展开动画结束后的回调函数
// 功能：删除 label_obj 对象上与 label_anim_y_cb 相关的动画效果
static void label_obj_anim_end(lv_anim_t *var)
{   // 初始化计数器
    hide_anim_count = 3;
    
	  obj_hide_anim(label_img);
    obj_hide_anim(author_photo_img);
    obj_hide_anim(label_obj);
	lv_anim_del(label_obj, label_anim_y_cb);
}

// 左移展开动画函数
// 功能：创建并启动一个左移展开动画，让传入的对象 var 从宽度为 0 逐渐展开到宽度为 235
// 动画时长 2350 毫秒，动画结束后调用 label_obj_anim_end 回调函数
static void left_move_anim(void *var)
{
    lv_anim_t label_obj_a;
    lv_anim_init(&label_obj_a);
    lv_anim_set_var(&label_obj_a, var);
    lv_anim_set_values(&label_obj_a, 0, 235);
    lv_anim_set_time(&label_obj_a, 2050);
    lv_anim_set_exec_cb(&label_obj_a, label_anim_y_cb);
    lv_anim_set_ready_cb(&label_obj_a, label_obj_anim_end);
    lv_anim_start(&label_obj_a);
}

// 对象进行隐藏动画回调函数
// 功能：在动画过程中，根据传入的值 v 更新对象 var 的透明度
static void obj_hide_anim_cb(void * var, int32_t v)
{
    lv_obj_set_style_opa(var, v, 0);
}

// 所有对象动画结束后的回调函数
// 功能：删除 label_img 和 author_photo_img 对象上的所有动画效果
// 删除 author_obj 对象，然后进入登入界面
static void obj_anim_end(lv_anim_t  *var)
{
	  hide_anim_count--;
	
	if(hide_anim_count == 0) {
    // 1. 删除动画（确保动画已结束）
   
    lv_anim_del(author_photo_img, NULL);    
    lv_anim_del(label_img, NULL);
		lv_anim_del(label_obj, NULL);
		author_photo_img = NULL;
	  label_img = NULL;
		label_obj = NULL;		
    // 删除父对象（会自动删除子对象）
        if(author_obj) {
					  author_obj = NULL;
            lv_obj_del(author_obj); // 使用 lv_obj_clean 代替 lv_obj_del
					  
           
        }
    
    // 3. 清空全局指针
    
    
		//登录		
				vTaskDelay(500);
	  lv_messagebox_create();
		lv_login_window_demo();
	}
}  

// 对象隐藏动画函数
// 功能：创建并启动一个隐藏动画，让传入的对象 var 从透明度为 255 逐渐隐藏到透明度为 0
// 动画时长 7650 毫秒，动画结束后调用 obj_anim_end 回调函数
static void obj_hide_anim(void *var)
{
    lv_anim_t obj_a;
    lv_anim_init(&obj_a);
    lv_anim_set_var(&obj_a, var);
    lv_anim_set_values(&obj_a, 255, 0);
    lv_anim_set_time(&obj_a, 8350);
    lv_anim_set_exec_cb(&obj_a, obj_hide_anim_cb);
    lv_anim_set_ready_cb(&obj_a, obj_anim_end);
    lv_anim_start(&obj_a);
}   

static void lv_boot_anim2_author(void)
{
    author_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(author_obj,600,480);
    lv_obj_set_style_bg_opa(author_obj,0,0);
    lv_obj_set_style_border_opa(author_obj,0,0);
    lv_obj_center(author_obj);

    label_img = lv_img_create(author_obj);
    lv_img_set_src(label_img,IMAGE_GBK_PATH[8]);
	// 设置图片的重新着色颜色为绿色
    lv_obj_set_style_img_recolor(label_img, lv_color_hex(0x00FF7F), LV_PART_MAIN);
	// 设置重新着色的透明度为255（完全不透明）
    lv_obj_set_style_img_recolor_opa(label_img, 255, LV_PART_MAIN);
    lv_obj_align(label_img,LV_ALIGN_TOP_MID,0,0);
    bounce_anim(label_img);

    author_photo_img = lv_img_create(author_obj);
    lv_img_set_src(author_photo_img,IMAGE_GBK_PATH[9]);
    lv_obj_align(author_photo_img,LV_ALIGN_CENTER,-120,50);
    lv_obj_set_style_img_opa(author_photo_img,0,0);

    label_obj = lv_obj_create(author_obj);
    lv_obj_set_size(label_obj,0,110);
    lv_obj_set_style_bg_opa(label_obj,0,0);
    lv_obj_set_style_border_opa(label_obj,0,0);
    lv_obj_align(label_obj,LV_ALIGN_RIGHT_MID,-100,50);

    lv_obj_t * name_label = lv_label_create(label_obj);
    lv_label_set_text(name_label,"团队");
    lv_obj_set_style_text_font(name_label,&Font13,0);
    lv_obj_set_style_text_color(name_label,lv_color_hex(0xFFD700),0);
    lv_obj_align(name_label,LV_ALIGN_TOP_MID,0,0);

    lv_obj_t * name_label1 = lv_label_create(label_obj);
    lv_label_set_text(name_label1,"愿逐月华流照君");
    lv_obj_set_style_text_font(name_label1,&Font13,0);
    lv_obj_set_style_text_color(name_label1,lv_color_hex(0xFFD700),0);
    lv_obj_align(name_label1,LV_ALIGN_BOTTOM_MID,-5,0);
    lv_obj_set_scrollbar_mode(label_obj, LV_SCROLLBAR_MODE_OFF);
}
// 第一段动画启动函数
void lv_boot_anim(void) {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);
    create_typing_effect(lv_scr_act());
}

// 启动开机动画
void lv_boot_anim_run(void) {
    lv_boot_anim();
}





//****************************************************************************************************************************************************

//**************************************************************登陆界面******************************************************************************
#define FONTAWESOME_SYMBOL_USER  "\xef\x80\x87"   /*f007*/
#define FONTAWESOME_SYMBOL_KEY   "\xef\x82\x84"   /*f084*/


LV_FONT_DECLARE(user_fontawesome);  //图标库
LV_FONT_DECLARE(user_login_name_12);
static void background_set_color(lv_color_t color);    //更改背景颜色

static lv_obj_t * window_obj;  //登陆窗口容器

static lv_obj_t * user_name_input;        //用户名文本框
static lv_obj_t * user_name_keyboard;     //用户名键盘
static lv_obj_t * password_input;         //密码文本框
static lv_obj_t * password_keyboard;      //密码键盘
lv_obj_t * voltage_label1;
static  lv_obj_t* msgbox ;





//--------------------------------回调函数区域-----------------------------------------------------
/*用户文本框点击事件回调*/
static void textarea_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED)  // 当文本区域获得焦点时显示键盘
    {
        lv_obj_clear_flag(user_name_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    else if(code == LV_EVENT_DEFOCUSED)// 当文本区域失去焦点时隐藏键盘
    {
        lv_obj_add_flag(user_name_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}
/*密码文本框点击事件回调*/
static void password_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED)  // 当文本区域获得焦点时显示键盘
    {
        lv_obj_clear_flag(password_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    else if(code == LV_EVENT_DEFOCUSED)// 当文本区域失去焦点时隐藏键盘
    {
        lv_obj_add_flag(password_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

/*键盘按钮√的事件回调*/
static void keyboard_event_cb(lv_event_t * e)
{
     lv_event_code_t code = lv_event_get_code(e);
     if(code == LV_EVENT_READY) //当按下键盘的√按键 触发隐藏键盘
     {
        lv_obj_add_flag(user_name_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(password_keyboard, LV_OBJ_FLAG_HIDDEN);
     }
}


// 修改后的回调函数
static void close_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_obj_add_flag(msgbox, LV_OBJ_FLAG_HIDDEN);  // 隐藏消息框

        // 新增关键代码：停止事件传播，阻止默认删除行为
        lv_event_stop_processing(e); // 或 lv_event_stop_bubbling(e)
    }
}




/*登陆事件回调函数*/
static void login_btn_event_cb(lv_event_t*e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        const char *user_txt = lv_textarea_get_text(user_name_input);
        const char *password_txt = lv_textarea_get_text(password_input);
//        printf("%s:%s\n",user_txt,password_txt);
        if((strcmp(user_txt,"tyzs")==0)&&(strcmp(password_txt,"123456")==0))
        {
//             printf("login success!\n");
                        lv_obj_del(user_name_input);
                        lv_obj_del(password_input);
                        lv_obj_del(user_name_keyboard);
                        lv_obj_del(password_keyboard);
                        lv_obj_del(msgbox);
                        lv_obj_del(window_obj);           //删除登入窗口

             background_set_color(lv_color_hex(0x000000));
                        //系统运行界面
             

        }
        else
        {

            // 登录失败


                        // 清空输入框
                        lv_textarea_set_text(user_name_input, "");
                        lv_textarea_set_text(password_input, "");

                        // 移除输入框的异常状态
                        lv_obj_clear_state(user_name_input, LV_STATE_DISABLED);
                        lv_obj_clear_state(password_input, LV_STATE_DISABLED);

                        // 让光标重新聚焦到用户名输入框
                        lv_obj_add_state(user_name_input, LV_STATE_FOCUSED);

                        // 显示消息框提示密码错误
                        lv_obj_clear_flag(msgbox, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_align(msgbox, LV_ALIGN_CENTER, 0, -100);

                        lv_obj_add_flag(msgbox, LV_OBJ_FLAG_HIDDEN);

        }

    }
}

/*改变背景颜色*/
static void background_set_color(lv_color_t color)
{
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr,color, 0);  // 0表示使用默认样式
}

void lv_messagebox_create(void)
{
    static const char  *tips_txt={"  密码错误，请重新输入!"};

    msgbox = lv_msgbox_create(lv_scr_act(),NULL,tips_txt,NULL,true );
    lv_obj_set_style_text_font(msgbox,&user_login_name_12,0);
    lv_obj_set_size(msgbox,300,150);
    lv_obj_set_style_bg_opa(msgbox,200,0);
    lv_obj_align(msgbox,LV_ALIGN_CENTER,0,50);

    lv_obj_set_style_pad_top(msgbox,15,LV_STATE_DEFAULT);  //顶部填充
    lv_obj_t *close_btnt = lv_msgbox_get_close_btn(msgbox); //获取到关闭图标
    lv_obj_set_style_text_color(close_btnt,lv_color_hex(0x696969),LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(close_btnt,&lv_font_montserrat_14,0);
    lv_obj_set_style_pad_top(close_btnt,0,LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(close_btnt,0,LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(close_btnt,0,LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(close_btnt,0,LV_STATE_DEFAULT);

    if (close_btnt != NULL) {
            // 为关闭按钮添加点击事件回调
            lv_obj_add_event_cb(close_btnt, close_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }

    lv_obj_t* msgbox_content = lv_msgbox_get_content(msgbox); //获取到主体
//    lv_obj_set_style_text_color(msgbox_content,lv_color_hex(0xFF0000),LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(msgbox_content,lv_color_hex(0x808080),LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(msgbox_content,10,LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(msgbox_content,5,0);
    lv_obj_set_style_text_line_space(msgbox_content,10,0);

    lv_obj_add_flag(msgbox,LV_OBJ_FLAG_HIDDEN);                 //默认隐藏

}


void lv_login_window_demo(void)
{




    background_set_color(lv_color_hex(0x1E90FF));

    /*创建一个容器*/
    window_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(window_obj,lv_obj_get_width(lv_scr_act())/2,lv_obj_get_height(lv_scr_act())/2);
    lv_obj_set_style_bg_opa(window_obj,100,0);
    lv_obj_set_style_border_opa(window_obj,0,0);
    lv_obj_center(window_obj);


    /*创建一个父类 占位对象1，优化布局用*/
    lv_obj_t * name_obj = lv_obj_create(window_obj);
    lv_obj_set_size(name_obj, 250, 44);
    lv_obj_align(name_obj, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_scrollbar_mode(name_obj, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动功能

    /* 创建用户名输入框 */
    user_name_input = lv_textarea_create(window_obj);
    lv_textarea_set_placeholder_text(user_name_input, "name"); //提示文本
    lv_obj_set_size(user_name_input, 210, 44);
    lv_textarea_set_one_line(user_name_input, true);                    //限制为1行高度
    lv_textarea_set_max_length(user_name_input,5);                      //限制长度为5个字符
    lv_obj_set_scrollbar_mode(user_name_input, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动功能
    lv_obj_set_style_border_opa(user_name_input,0,0);                   //边框透明为0
    lv_obj_align(user_name_input, LV_ALIGN_TOP_MID, 20, 20);
    lv_obj_add_event_cb(user_name_input, textarea_event_cb, LV_EVENT_ALL, NULL); //用户名输入框回调
    /*创建用户名键盘*/
    user_name_keyboard =lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(user_name_keyboard,600,200);
    lv_obj_set_style_radius(user_name_keyboard,20,0);
    lv_keyboard_set_textarea(user_name_keyboard,user_name_input);
    lv_obj_add_event_cb(user_name_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_flag(user_name_keyboard,LV_OBJ_FLAG_HIDDEN);//默认隐藏
    /*用户图标对象框*/
    lv_obj_t *user_name_obj = lv_obj_create(window_obj);
    lv_obj_set_size(user_name_obj,44,44);
    lv_obj_set_style_bg_color(user_name_obj,lv_color_hex(0x696969),0);
    lv_obj_align_to(user_name_obj,user_name_input,LV_ALIGN_OUT_LEFT_MID,0,0);
    lv_obj_set_scrollbar_mode(user_name_obj, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动功能
    /*用户图标对象*/
    lv_obj_t * user_name_label = lv_label_create(window_obj);
    lv_obj_set_style_text_font(user_name_label,&user_fontawesome,0);
    lv_label_set_text(user_name_label, FONTAWESOME_SYMBOL_USER);
    lv_obj_set_style_text_color(user_name_label,lv_color_hex(0xffffff),0);
    lv_obj_align_to(user_name_label,user_name_obj,LV_ALIGN_CENTER,0,0);
//--------------------------------------------------------------------------
    /*创建一个父类 占位对象2，优化布局用*/
    lv_obj_t * key_obj = lv_obj_create(window_obj);
    lv_obj_set_size(key_obj, 250, 44);
    lv_obj_align_to(key_obj,name_obj, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_set_scrollbar_mode(key_obj, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动功能
    /* 创建密码输入框 */
    password_input = lv_textarea_create(window_obj);
    lv_textarea_set_placeholder_text(password_input,"password");//提示文本
    lv_obj_set_scrollbar_mode(password_input, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动功能
    lv_textarea_set_password_mode(password_input, true);  //密码模式
    lv_textarea_set_accepted_chars(password_input, "0123456789"); //字符白名单
    lv_obj_set_size(password_input, 210, 44);
    lv_textarea_set_one_line(password_input, true);                    //限制为1行高度
    lv_obj_set_style_border_opa(password_input,0,0);
    lv_obj_align_to(password_input,key_obj, LV_ALIGN_CENTER,19, 0);
    lv_obj_add_event_cb(password_input, password_event_cb, LV_EVENT_ALL, NULL); //用户名输入框回调
    /*创建密码键盘*/
    password_keyboard =lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(password_keyboard,600,200);
    lv_obj_set_style_radius(password_keyboard,20,0);
    lv_keyboard_set_textarea(password_keyboard,password_input);
    lv_keyboard_set_mode(password_keyboard,LV_KEYBOARD_MODE_NUMBER); //数字键盘模式
    lv_obj_add_event_cb(password_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_flag(password_keyboard,LV_OBJ_FLAG_HIDDEN);//默认隐藏
    /*密码图标框*/
    lv_obj_t *password_obj = lv_obj_create(window_obj);
    lv_obj_set_size(password_obj,44,44);
    lv_obj_set_style_bg_color(password_obj,lv_color_hex(0x696969),0);
    lv_obj_align_to(password_obj,password_input,LV_ALIGN_OUT_LEFT_MID,0,0);
    lv_obj_set_scrollbar_mode(password_obj, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动功能
    /*密码图标*/
    lv_obj_t * password_label = lv_label_create(window_obj);
    lv_obj_set_style_text_font(password_label,&user_fontawesome,0);
    lv_label_set_text(password_label, FONTAWESOME_SYMBOL_KEY);
    lv_obj_set_style_text_color(password_label,lv_color_hex(0xffffff),0);
    lv_obj_align_to(password_label,password_obj,LV_ALIGN_CENTER,0,0);
//--------------------------------------------------------------------------
    /* 创建登录按钮 */
    lv_obj_t * login_btn = lv_btn_create(window_obj);
    lv_obj_set_size(login_btn, 255, 44);
    lv_obj_align_to(login_btn,key_obj, LV_ALIGN_OUT_BOTTOM_MID, -3, 20);
     /*创建按钮标签 */
   lv_obj_t * login_label = lv_label_create(login_btn);
    lv_label_set_text(login_label, "登录");
    lv_obj_set_style_text_font(login_label,&user_login_name_12,0);

    lv_obj_center(login_label);
    /* 设置按钮事件 */
    lv_obj_add_event_cb(login_btn, login_btn_event_cb, LV_EVENT_CLICKED, NULL);
//-------------------------------------------------------------------------------

}

//***************************************************首页***************************************************************************************************
/**
  * @brief  返回按键
  * @param  无
  * @retval 无
  */
void lv_general_win_create(void)
{
    #define TOP_OFFSET  -10
    lv_obj_t* back_btn = lv_label_create(lv_general_dev.parent);
    lv_label_set_text(back_btn, BACK_BTN_TITLE);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(back_btn, NULL, LV_ALIGN_TOP_LEFT, 15, TOP_OFFSET);
    lv_obj_set_style_text_color(back_btn, lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(back_btn, lv_general_dev.lv_back_event, LV_EVENT_ALL, NULL);
}

/**
  * @brief  计算前导置零
  * @param  无
  * @retval 无
  */
int lv_clz(unsigned int  app_readly_list[])
{
    int bit = 0;

    for (int i = 0; i < 32; i++)
    {
        if (app_readly_list[i] == 1)
        {
            break;
        }

        bit ++ ;
    }

    return bit;
}

/**
  * @brief  APP按键回调函数
  * @param  obj  :对象
  * @param  event:事件
  * @retval 无
  */
static void lv_imgbtn_control_event_handler(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t * obj = lv_event_get_target(event);
    lv_obj_t *lv_app_parent = lv_obj_get_parent(obj);

    if (code == LV_EVENT_CLICKED)
    {
        for (int i = 0;i < image_mun;i ++)
        {
            if (obj == lv_app_t[i])
            {
                app_readly_list[i] = 1 ;                                       /* app就绪表位置1 */
            }
        }

        lv_trigger_bit = ((unsigned int)lv_clz((app_readly_list)));            /* 计算前导指令 */
        app_readly_list[lv_trigger_bit] = 0;                                   /* 该位清零就绪表 */
        lv_obj_del(lv_app_parent);                                             /* 界面切换使用删除方法 */
        lv_app_parent = NULL;                                                  /* 主界面的容器设置为空 */

        switch(lv_trigger_bit)                                                 /* 根据该位做相应的函数 */
        {
            case 1:
              lv_calculator_demo();                                            /* 计算器(完成) */
              break;
            case 2:
              lv_file_demo();                                                  /* 文件管理系统(完成) */
              break;
            case 3:
              lv_scale_demo();                                                 /* 进制转换系统(完成) */
              break;
            case 4:
              lv_setting_demo();                                               /* 设置系统(完成) */
              break;
            case 5:
              lv_shelf_demo();                                                 /* 板载测试系统(完成) */
              break;
            case 6:
              lv_meter_demo();                                                 /* 时钟(未完成) */
              break;
            case 7:
              lv_qr_windowm();                                                 /* 二维码制作(完成) */
              break;
            case 8:
              lv_draw_demo();                                                  /* 绘画(完成) */
              break;
        }
    }
}

/**
  * @brief  APP显示
  * @param  parent:父类对象
  * @retval 无
  */
void lv_mid_cont_add_app(lv_obj_t *parent)
{
    int line_feed_num = 0;
    int lv_index = 0;
    lv_app_t[lv_index] = NULL;
    lv_index ++;
    int i = 0;
    int n = 1;

    lv_app_t[lv_index] = lv_obj_create(parent);
    lv_obj_set_pos(lv_app_t[lv_index],-7,20);
    lv_obj_set_size(lv_app_t[lv_index], app_image[lv_index].app_witch, app_image[lv_index].app_hietch);
    lv_obj_set_style_bg_color(lv_app_t[lv_index], lv_color_make(26, 57, 137), LV_STATE_DEFAULT);
    lv_obj_clear_flag(lv_app_t[lv_index], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(lv_app_t[lv_index],0,LV_PART_MAIN);
    lv_obj_add_event_cb(lv_app_t[lv_index], lv_imgbtn_control_event_handler, LV_EVENT_ALL, NULL);

    lv_app_name[lv_index] = lv_label_create(lv_app_t[lv_index]);
    lv_obj_set_style_text_color(lv_app_name[lv_index],lv_color_make(255,255,255), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lv_app_name[lv_index],&lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_label_set_text(lv_app_name[lv_index], app_image[lv_index].app_text_English);
    lv_obj_align(lv_app_name[lv_index],LV_ALIGN_BOTTOM_LEFT,-10, 10);

    lv_app_img[lv_index] = lv_img_create(lv_app_t[lv_index]);
    lv_img_set_src(lv_app_img[lv_index], IMAGE_GBK_PATH[lv_index - 1]);
    lv_obj_center(lv_app_img[lv_index]);
    lv_obj_set_style_img_recolor_opa(lv_app_img[lv_index], 255, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(lv_app_img[lv_index], lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
    unsigned int lv_width_x = lv_obj_get_width(lv_app_t[1]) + 20;
    lv_index ++;
    

    for (lv_index = 2 ; lv_index < image_mun ; lv_index ++)
    {
        lv_app_t[lv_index] = lv_obj_create(parent);
        lv_obj_set_size(lv_app_t[lv_index], app_image[lv_index].app_witch, app_image[lv_index].app_hietch);
        lv_obj_set_style_bg_color(lv_app_t[lv_index], lv_color_make(26, 57, 137), LV_STATE_DEFAULT);
        lv_obj_clear_flag(lv_app_t[lv_index], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(lv_app_t[lv_index], 0, LV_PART_MAIN);
        lv_obj_update_layout(lv_app_t[lv_index]);
        lv_width_x = lv_width_x + lv_obj_get_width(lv_app_t[lv_index]) + 10;

        if (lv_width_x < lv_obj_get_width(lv_scr_act()))
        {
            lv_obj_align_to(lv_app_t[lv_index], lv_app_t[lv_index - 1], LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        }
        else
        {
            line_feed_num++;

            if (line_feed_num >= 2)
            {
                i = 10 * n;
                n ++;
            }
            else
            {
                i = 0;
            }
            
            lv_obj_set_pos(lv_app_t[lv_index], -7, (lv_obj_get_height(lv_app_t[lv_index]) + 20) * line_feed_num + 10 - i );
            lv_width_x = lv_obj_get_width(lv_app_t[lv_index]) + 10;
        }
        
        lv_app_name[lv_index] = lv_label_create(lv_app_t[lv_index]);
        lv_obj_set_style_text_color(lv_app_name[lv_index], lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lv_app_name[lv_index], &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_label_set_text(lv_app_name[lv_index], app_image[lv_index].app_text_English);
        lv_obj_align(lv_app_name[lv_index], LV_ALIGN_BOTTOM_LEFT, -10, 10);

        lv_app_img[lv_index] = lv_img_create(lv_app_t[lv_index]);
        lv_img_set_src(lv_app_img[lv_index], IMAGE_GBK_PATH[lv_index - 1]);
        lv_obj_center(lv_app_img[lv_index]);
        lv_obj_set_style_img_recolor_opa(lv_app_img[lv_index], 255, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(lv_app_img[lv_index], lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
        lv_obj_add_event_cb(lv_app_t[lv_index], lv_imgbtn_control_event_handler, LV_EVENT_ALL, NULL);
    }
}

void lv_app_icon(lv_obj_t *praten)
{
    /* 左上角图标 */
    lv_obj_t* lv_letf_acon = lv_label_create(praten);
    lv_label_set_text(lv_letf_acon, LV_SYMBOL_WIFI " " LV_SYMBOL_AUDIO);
    lv_obj_align(lv_letf_acon,LV_ALIGN_TOP_LEFT,-5,-10);
    lv_obj_set_style_text_color(lv_letf_acon, lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
    /* 中间的时间 */
    lv_obj_t* lv_timer = lv_label_create(praten);
    lv_label_set_text(lv_timer, "2022/1/20");
    lv_obj_align(lv_timer, LV_ALIGN_TOP_MID, 0, -10);
    lv_obj_set_style_text_color(lv_timer, lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
    /* 右上角图标 */
    lv_obj_t* lv_right_acon = lv_label_create(praten);
    lv_label_set_text(lv_right_acon, LV_SYMBOL_BATTERY_3 " " LV_SYMBOL_USB);
    lv_obj_align(lv_right_acon, LV_ALIGN_TOP_RIGHT, 5, -10);
    lv_obj_set_style_text_color(lv_right_acon, lv_color_make(255, 255, 255), LV_STATE_DEFAULT);

}

/**
  * @brief  主界面
  * @param  无
  * @retval 无
  */
void lv_main_window(void)
{
    lv_obj_t *lv_main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(lv_main_cont, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
    lv_obj_set_style_bg_color(lv_main_cont, lv_color_make(1, 27, 54), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lv_main_cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(lv_main_cont,LV_OBJ_FLAG_SCROLLABLE);
    lv_mid_cont_add_app(lv_main_cont);
    lv_app_icon(lv_main_cont);
}

/**
  * @brief  LVGL 入口
  * @param  无
  * @retval 无
  */
void lv_mainstart(void)
{ 
  	//lv_boot_anim_run();
   
		lv_login_window_demo();
	lv_messagebox_create();
}
