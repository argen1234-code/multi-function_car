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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "LVGL/GUI_APP/lv_mainstart.h"
#include "LVGL/GUI/lvgl/src/extra/libs/gif/lv_gif.h"
#include "./BSP/BEEP/beep.h"
#include "./BSP/LED/led.h"
#include "./BSP/DHT11/dht11.h"

//*******************************************************************anim***************************************************************
//-------------------------------------------------------
//-------------------------------------------------------
// 字体声明
LV_FONT_DECLARE(system_name_20);


// 全局对象指针
static lv_obj_t *text_obj;
static lv_obj_t *text_label;
static lv_obj_t *text_obj1;
static lv_obj_t *text_label1;

// 文本常量
static const char *full_text = "碳友智算";
static const char *full_text1 = "LVGL";
static size_t text_len = 0;
static size_t text_len1 = 0;

// 动画回调函数声明
static void text_anim_a_cb(void *a, int32_t v);
static void text_anim_b_cb(void *a, int32_t v);
static void text_anim_a_end(lv_anim_t *anim1);
static void text_anim_b_end(lv_anim_t *anim2);
static void lv_typing_effect(lv_timer_t *task);

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
    lv_anim_del(text_label1, NULL); // 删除动画
    lv_obj_del(text_obj);
    lv_obj_del(text_obj1);

    // 去掉第二个动画调用，直接进入登入界面
    lv_main_demo_run();
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
    lv_obj_set_style_text_font(text_label, &system_name_20, LV_PART_MAIN);
    lv_label_set_text(text_label, " ");
    lv_obj_set_style_text_color(text_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(text_label, LV_ALIGN_CENTER, 0, 0);

    text_label1 = lv_label_create(text_obj1);
    lv_obj_set_style_text_font(text_label1, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_label_set_text(text_label1, " ");
    lv_obj_set_style_text_color(text_label1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(text_label1, LV_ALIGN_CENTER, 0, 0);

    text_len1 = 0;
    text_len = 0;
    // 创建定时器，每隔 200 毫秒调用一次 typing_effect 函数
    lv_timer_create(lv_typing_effect, 200, NULL);
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
//*********************************************************************main*************************************************************
#define FONTAWESOME_SYMBOL_USER  "\xef\x80\x87"   /*f007*/
#define FONTAWESOME_SYMBOL_KEY   "\xef\x82\x84"   /*f084*/

LV_FONT_DECLARE(user_fontawesome);  //图标库
LV_FONT_DECLARE(temp_symbol_14);
LV_FONT_DECLARE(user_login_name_12);

LV_IMG_DECLARE(led_symbol);
LV_IMG_DECLARE(beep_symbol);
LV_IMG_DECLARE(mygif);

static void background_set_color(lv_color_t color);    //更改背景颜色
void lv_run_interface_demo(void);  //运行界面
static void user_timer1(void);
static void user_timer2(void);
static lv_obj_t * window_obj;  //登陆窗口容器
static void user_timer3(void);
static lv_obj_t * user_name_input;        //用户名文本框
static lv_obj_t * user_name_keyboard;     //用户名键盘
static lv_obj_t * password_input;         //密码文本框
static lv_obj_t * password_keyboard;      //密码键盘

lv_obj_t * temp_label;
lv_obj_t * humi_label ;
static lv_obj_t *led1 ;
lv_obj_t * voltage_label1;
static  lv_obj_t* msgbox ;
static lv_timer_t *my_timer1;
static lv_timer_t *my_timer2;
static lv_timer_t *my_timer3;
lv_anim_t beep_anim;
lv_anim_t gif_anim;
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
             lv_run_interface_demo();           //系统运行界面
             user_timer1();
             user_timer3();

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



static void led_switch_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
 //   lv_obj_t * targer = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED)
    {
        lv_led_toggle(led1);
        LED0_TOGGLE();
    }
}


static void beep_switch_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * targer = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED)
    {
        if(lv_obj_has_state(targer,LV_STATE_CHECKED))
        {
            lv_anim_set_values(&beep_anim, 0, 900);                        //设置动画的起始值和结束值(90度旋转)
            lv_anim_set_time(&beep_anim, 1000);                            //动画的时间
            lv_anim_start(&beep_anim);                                     //开始动画
            user_timer2();

        }
        else
        {

            lv_anim_set_values(&beep_anim, 900, 0);                        //设置动画的起始值和结束值(90度旋转)
            lv_anim_set_time(&beep_anim, 1000);                            //动画的时间
            lv_anim_start(&beep_anim);                                     //开始动画
            BEEP(0);

        }
    }
}


static void set_beep_angle_anim(void *a,int32_t v)
{
    lv_img_set_angle(a,v);
}


static void gif_size_anim(void *a,int32_t v)
{
    lv_obj_set_size(a,v,40);
}



static void other_switch_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * targer = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED)
    {
        if(lv_obj_has_state(targer,LV_STATE_CHECKED))
        {
                lv_anim_set_values(&gif_anim, 0, 40);                        //设置动画的起始值和结束值(90度旋转)
                lv_anim_set_time(&gif_anim, 1000);                            //动画的时间
                lv_anim_start(&gif_anim);
        }
        else
        {
                lv_anim_set_values(&gif_anim, 40, 0);                        //设置动画的起始值和结束值(90度旋转)
                lv_anim_set_time(&gif_anim, 1000);                            //动画的时间
                lv_anim_start(&gif_anim);
        }
    }
}





//--------------------------------主函数区域-----------------------------------------------------
/*改变背景颜色*/
static void background_set_color(lv_color_t color)
{
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr,color, 0);  // 0表示使用默认样式
}

static void lv_messagebox_create(void)
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
    lv_obj_set_style_text_font(close_btnt,&lv_font_montserrat_16,0);
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


void lv_run_interface_demo(void)
{

    lv_obj_t *tileview = lv_tileview_create(lv_scr_act());
    lv_obj_t *tile1 = lv_tileview_add_tile(tileview,0,0,LV_DIR_RIGHT);
   // lv_obj_t *tile2 = lv_tileview_add_tile(tileview,1,0,LV_DIR_LEFT);
    lv_obj_set_tile(tileview,tile1,LV_ANIM_OFF);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tileview,lv_color_hex(0xF0FFFF),0);

    lv_obj_update_layout(tileview);
    /*一级父类容器 */
    lv_obj_t *run_obj = lv_obj_create(tile1);
    lv_obj_set_size(run_obj, 700, 410);
    lv_obj_set_style_bg_opa(run_obj,0,0);
    lv_obj_center(run_obj);

//--------------第二排  名字--------------------------
    lv_obj_t *system_obj =lv_obj_create(run_obj);
    lv_obj_set_size(system_obj, 600, 80);
    lv_obj_align(system_obj,LV_ALIGN_TOP_MID,0,0);


    lv_obj_t * system_name_label = lv_label_create(system_obj);
    lv_obj_set_style_text_font(system_name_label,&system_name_20,0);
    lv_label_set_text(system_name_label,"碳友智算");
    lv_obj_align(system_name_label,LV_ALIGN_CENTER,0,0);
    // 控制 灯,蜂鸣器    显示 :温度

//--------------第二排  显示类--------------------------

//------------------------温度-----------------------------------
    lv_obj_t *temp_obj3 =lv_obj_create(run_obj);
    lv_obj_set_size(temp_obj3, 150, 120);
    lv_obj_align_to(temp_obj3,system_obj,LV_ALIGN_OUT_BOTTOM_LEFT,0,20);

    lv_obj_t * temp_name_label = lv_label_create(temp_obj3);
    lv_label_set_text(temp_name_label,"温度");
    lv_obj_set_style_text_font(temp_name_label,&temp_symbol_14,0);
    lv_obj_align(temp_name_label,LV_ALIGN_TOP_MID,0,-10);

    temp_label = lv_label_create(temp_obj3);
    lv_label_set_text(temp_label,"00");
    lv_obj_set_style_text_font(temp_label,&lv_font_montserrat_16,0);
    lv_obj_align(temp_label,LV_ALIGN_CENTER,-5,10);

    lv_obj_t * temp_symbol_label = lv_label_create(temp_obj3);
    lv_label_set_text(temp_symbol_label,"°");
    lv_obj_set_style_text_font(temp_symbol_label,&temp_symbol_14,0);
    lv_obj_align_to(temp_symbol_label,temp_label,LV_ALIGN_OUT_RIGHT_MID,0,5);

    lv_obj_t * temp_symbol_label2 = lv_label_create(temp_obj3);
    lv_label_set_text(temp_symbol_label2,"C");
    lv_obj_set_style_text_font(temp_symbol_label2,&temp_symbol_14,0);
    lv_obj_align_to(temp_symbol_label2,temp_symbol_label,LV_ALIGN_OUT_RIGHT_MID,-10,0);

//------------------------湿度-----------------------------------
    lv_obj_t *humi_obj =lv_obj_create(run_obj);
    lv_obj_set_size(humi_obj, 150, 120);
    lv_obj_align_to(humi_obj,system_obj,LV_ALIGN_OUT_BOTTOM_MID,0,20);

    lv_obj_t * humi_name_label = lv_label_create(humi_obj);
    lv_label_set_text(humi_name_label,"湿度");
    lv_obj_set_style_text_font(humi_name_label,&temp_symbol_14,0);
    lv_obj_align(humi_name_label,LV_ALIGN_TOP_MID,0,-10);

    humi_label = lv_label_create(humi_obj);
    lv_label_set_text(humi_label,"00");
    lv_obj_set_style_text_font(humi_label,&lv_font_montserrat_16,0);
    lv_obj_align(humi_label,LV_ALIGN_CENTER,-5,10);

    lv_obj_t * humi_symbol_label = lv_label_create(humi_obj);
    lv_label_set_text(humi_symbol_label,"%");
    lv_obj_set_style_text_font(humi_symbol_label,&lv_font_montserrat_16,0);
    lv_obj_align_to(humi_symbol_label,humi_label,LV_ALIGN_OUT_RIGHT_MID,5,5);



    lv_obj_t *voltage_obj =lv_obj_create(run_obj);
    lv_obj_set_size(voltage_obj, 150, 120);
    lv_obj_align_to(voltage_obj,system_obj,LV_ALIGN_OUT_BOTTOM_RIGHT,0,20);

    lv_obj_t * voltage_label = lv_label_create(voltage_obj);
    lv_label_set_text(voltage_label,"电压");
    lv_obj_set_style_text_font(voltage_label,&temp_symbol_14,0);
    lv_obj_align(voltage_label,LV_ALIGN_TOP_MID,0,-10);


    voltage_label1 = lv_label_create(voltage_obj);
    lv_label_set_text(voltage_label1,"0");
    lv_obj_set_style_text_font(voltage_label1,&lv_font_montserrat_16,0);
    lv_obj_align(voltage_label1,LV_ALIGN_LEFT_MID,0,10);

//    lv_obj_t * voltage_label2 = lv_label_create(voltage_obj);
//    lv_label_set_text(voltage_label2,".");
//    lv_obj_set_style_text_font(voltage_label2,&lv_font_montserrat_16,0);
//    lv_obj_align_to(voltage_label2,voltage_label1,LV_ALIGN_OUT_RIGHT_MID,0,0);

//    voltage_label3 = lv_label_create(voltage_obj);
//    lv_label_set_text(voltage_label3,"00");
//    lv_obj_set_style_text_font(voltage_label3,&lv_font_montserrat_16,0);
//    lv_obj_align_to(voltage_label3,voltage_label2,LV_ALIGN_OUT_RIGHT_MID,0,0);

    lv_obj_t * voltage_label4 = lv_label_create(voltage_obj);
    lv_label_set_text(voltage_label4,"V");
    lv_obj_set_style_text_font(voltage_label4,&lv_font_montserrat_16,0);
    lv_obj_align_to(voltage_label4,voltage_label1,LV_ALIGN_OUT_RIGHT_MID,60,10);



//--------------第三排  控制类--------------------------
    //灯 蜂鸣器  颜色块(或者动画)切换

    //灯控制系统
    lv_obj_t *led_obj1 =lv_obj_create(run_obj);
    lv_obj_set_size(led_obj1, 150, 120);
    lv_obj_align_to(led_obj1,temp_obj3,LV_ALIGN_OUT_BOTTOM_LEFT,0,20);

    led1 = lv_led_create(led_obj1);
    lv_obj_set_size(led1,23,23);
    lv_obj_set_style_bg_color(led1,lv_color_hex(0xFFD700),0);
    lv_led_set_color(led1,lv_color_hex(0xFF4500));
    lv_led_set_brightness(led1,255);
    lv_obj_align(led1,LV_ALIGN_TOP_MID,-1,12);
    lv_led_off(led1);

    lv_obj_t *led_img = lv_img_create(led_obj1);
    lv_img_set_src(led_img,&led_symbol);
    lv_obj_align(led_img,LV_ALIGN_TOP_MID,0,-10);

    lv_obj_t* led_switch = lv_switch_create(led_obj1);
    lv_obj_set_size(led_switch,60,25);
    lv_obj_align(led_switch,LV_ALIGN_BOTTOM_MID,-1,0);
    lv_obj_add_event_cb(led_switch,led_switch_event_cb,LV_EVENT_VALUE_CHANGED,NULL);


  //---------------------------------------------------

    //蜂鸣器

    lv_obj_t *beep_obj1 =lv_obj_create(run_obj);
    lv_obj_set_size(beep_obj1, 150, 120);
    lv_obj_align_to(beep_obj1,humi_obj,LV_ALIGN_OUT_BOTTOM_MID,0,20);


    lv_obj_t *beep_img = lv_img_create(beep_obj1);
    lv_img_set_src(beep_img,&beep_symbol);
    lv_obj_align(beep_img,LV_ALIGN_TOP_MID,0,-10);

    lv_obj_t* beep_switch = lv_switch_create(beep_obj1);
    lv_obj_set_size(beep_switch,60,25);
    lv_obj_align(beep_switch,LV_ALIGN_BOTTOM_MID,-1,0);

    lv_obj_update_layout(beep_img);
    lv_img_set_pivot(beep_img,20,30);
    lv_obj_add_event_cb(beep_switch,beep_switch_event_cb,LV_EVENT_VALUE_CHANGED,NULL);

    //lv_anim_t beep_anim;                                     //定义一个动画类型
    lv_anim_init(&beep_anim);                                      //动画初始化
    lv_anim_set_var(&beep_anim, beep_img);                         //需要动画的对象
    lv_anim_set_exec_cb(&beep_anim, set_beep_angle_anim);          //设置一个动画函数
    //lv_anim_set_values(&beep_anim, 0, 900);                        //设置动画的起始值和结束值(90度旋转)
    //lv_anim_set_time(&beep_anim, 1000);                            //动画的时间
    //lv_anim_start(&beep_anim);                                     //开始动画


    lv_obj_t *other_obj =lv_obj_create(run_obj);
    lv_obj_set_size(other_obj, 150, 120);
    lv_obj_align_to(other_obj,voltage_obj,LV_ALIGN_OUT_BOTTOM_RIGHT,0,20);


    lv_obj_t *gif1 = lv_gif_create(other_obj);
    lv_gif_set_src(gif1,&mygif);
    lv_obj_align(gif1,LV_ALIGN_TOP_MID,0,-5);
    lv_obj_set_size(gif1,1,1);

//    lv_anim_t gif_anim;
    lv_anim_init(&gif_anim);                                            //动画初始化
    lv_anim_set_var(&gif_anim, gif1);                                   //需要动画的对象
    lv_anim_set_exec_cb(&gif_anim, gif_size_anim);                      //设置一个动画函数
//    lv_anim_set_values(&gif_anim, 0, 40);                             //设置动画的起始值和结束值(90度旋转)
//    lv_anim_set_time(&gif_anim, 1000);                                //动画的时间
//    lv_anim_start(&gif_anim);                                         //开始动画

    lv_obj_t* other_switch = lv_switch_create(other_obj);
    lv_obj_set_size(other_switch,60,25);
    lv_obj_align(other_switch,LV_ALIGN_BOTTOM_MID,-1,0);
    lv_obj_add_event_cb(other_switch,other_switch_event_cb,LV_EVENT_VALUE_CHANGED,NULL);


}

#include <stdio.h>
    uint8_t temperature[2]={0};
    uint8_t humidity[2]={0};
    float temp =0;

static void my_time_cb1(lv_timer_t *timer)
{
//    dht11_read_data(temperature,humidity);
//    char buf[20];
//
//    sprintf(buf,"%d",temperature[0]);
//    lv_label_set_text(temp_label,buf);
//
//    lv_label_set_text_fmt(humi_label,"%d",humidity[0]);
//



}


static void  my_time_cb2(lv_timer_t *timer)
{

    BEEP(1);
    lv_timer_del(my_timer2);    //销毁定时器2

}

static void  my_time_cb3(lv_timer_t *timer)
{
//    char buf1[20];
//    temp = (float)ADC1->DR;
//    temp = temp/4095*3.3;
//    sprintf(buf1,"%.2f",temp);
//    lv_label_set_text(voltage_label1,buf1);

}
//---------------------------------------------LVGL定时器区域---------------------------------------------------------

//用户定时器1
static void user_timer1(void)
{
    my_timer1 = lv_timer_create(my_time_cb1,500,NULL); //创建定时器  //不使用时进行销毁
    lv_timer_set_repeat_count(my_timer1, -1); //无限循环
}


//用户定时器2
static void user_timer2(void)
{
    my_timer2 = lv_timer_create(my_time_cb2,1500,NULL); //创建定时器  //不使用时进行销毁
    lv_timer_set_repeat_count(my_timer2, 1);  //只进行一次
}

//用户定时器3
static void user_timer3(void)
{
    my_timer3 = lv_timer_create(my_time_cb3,1000,NULL); //创建定时器  //不使用时进行销毁
    lv_timer_set_repeat_count(my_timer3, -1);  //无限循环
}
//---------------------------------------------------------------------------------------------------------------------


 void  lv_main_demo_run(void)
 {

     /*密码错误提示窗口*/
    lv_messagebox_create();
    lv_login_window_demo();

 }
