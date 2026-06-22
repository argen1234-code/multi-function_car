/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-07-20
 * @brief       LVGL lv_img(图片) 实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 CH32V307开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com/forum.php
 * 公司网址:www.alientek.com
 * 购买地址:zhengdianyuanzi.tmall.com
 *
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TIMER/btim.h"

#include "lvgl.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
#include "LVGL/GUI_APP/lv_mainstart.h"

#include "LVGL/GUI/lvgl/src/extra/libs/gif/lv_gif.h"
#include "./BSP/BEEP/beep.h"
#include "./BSP/DHT11/dht11.h"
int main(void)
{
    uint8_t t = 0;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);     /* 中断优先级分组2 */
    delay_init(144);                                    /* 延时初始化 */
    usart_init(115200);                                 /* 串口初始化为115200 */
    led_init();                                         /* 初始化LED */
    beep_init(); /* 初始化蜂鸣器 */
    lcd_init();                                         /* 初始化LCD */
    key_init();                                         /* 初始化按键 */
    btim_timx_int_init(10-1, 14400-1);                  /* 初始化定时器 */
//    while (dht11_init())    /* DHT11初始化 */
//        {
//            lcd_show_string(30, 110, 200, 16, 16, "DHT11 Error", RED);
//            delay_ms(200);
//            lcd_fill(30, 110, 239, 130 + 16, WHITE);
//            delay_ms(200);
//        }

    lv_init();                                          /* lvgl系统初始化 */
    lv_port_disp_init();                                /* lvgl显示接口初始化,放在lv_init()的后面 */
    lv_port_indev_init();                               /* lvgl输入接口初始化,放在lv_init()的后面 */

    lv_boot_anim_run();
    //lv_main_demo_run();
    while(1)
    {

//        if (t % 10 == 0)    /* 每100ms读取一次 */
//                {
//                    dht11_read_data(&temperature, &humidity);             /* 读取温湿度值 */
//                    lcd_show_num(30 + 40, 130, temperature, 2, 16, BLUE); /* 显示温度 */
//                    lcd_show_num(30 + 40, 150, humidity, 2, 16, BLUE);    /* 显示湿度 */
//                }
//
        lv_timer_handler();                             /* LVGL计时器 */
        t++;
        if(t == 250)
        {

            t = 0;
        }
        delay_ms(2);
    }
}

