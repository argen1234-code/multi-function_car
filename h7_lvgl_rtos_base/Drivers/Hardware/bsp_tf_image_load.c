/**
 * bsp_tf_image_load.c — Load LVGL images from TF card via FAT32
 */
#include "bsp_tf_image_load.h"
#include "bsp_TF_card.h"
#include "main.h"
#include "ui.h"
#include "ff.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static FATFS g_fs;

static const char *IMG_FILES[] = { "icon.bin", "bg.bin", "data.bin", "setbtn.bin", "lock.bin" };
static lv_img_dsc_t *IMG_DESC[] = { &ui_img_1301672413, &ui_img_1943878613, &ui_img_380527393, &ui_img_1565185032, &ui_img_1614311220 };


int32_t bsp_tf_image_load_all(void)
{
	FRESULT fr;
	FIL     fp;
	UINT    br;
	FSIZE_t fsz;
	uint8_t *buf;

	fr = f_mount(&g_fs, "0:", 1);
	if (fr != FR_OK) { printf("[IMG] mount fail\n"); return -1; }

	for (int i = 0; i < (int)(sizeof(IMG_FILES)/sizeof(IMG_FILES[0])); i++)
	{
		fr = f_open(&fp, IMG_FILES[i], FA_READ);
		if (fr != FR_OK) {
			printf("[IMG] %s open fail\n", IMG_FILES[i]);
			return -1;
		}

		fsz = f_size(&fp);
		buf = lv_mem_alloc((uint32_t)fsz);
		if (!buf) {
			printf("[IMG] %s alloc fail (%uB)\n", IMG_FILES[i], (unsigned)fsz);
			f_close(&fp);
			return -1;
		}

		fr = f_read(&fp, buf, (UINT)fsz, &br);
		f_close(&fp);
		if (fr != FR_OK || br != (UINT)fsz) {
			printf("[IMG] %s read fail fr=%d %u/%u\n",
			       IMG_FILES[i], (int)fr, (unsigned)br, (unsigned)fsz);
			lv_mem_free(buf);
			return -1;
		}

		SCB_InvalidateDCache_by_Addr((uint32_t *)buf, (int32_t)fsz);

		IMG_DESC[i]->data      = buf;
		IMG_DESC[i]->data_size = (uint32_t)fsz;
		printf("[IMG] %s %uB\n", IMG_FILES[i], (unsigned)fsz);
	}

	printf("[IMG] all OK\n");
	return 0;
}
