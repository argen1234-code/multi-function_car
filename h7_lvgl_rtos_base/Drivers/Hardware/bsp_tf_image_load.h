#ifndef BSP_TF_IMAGE_LOAD_H
#define BSP_TF_IMAGE_LOAD_H

#include <stdint.h>

/* Load LVGL image pixel data from TF card (FAT32 filesystem).
 * Image files on TF card:
 *   "icon.bin"  — 64x64 RGBA raw pixels  (16 KB)
 *   "bg.bin"    — 1024x600 RGBA raw pixels (2.4 MB)
 *
 * Allocates SDRAM via lv_mem_alloc(), patches lv_img_dsc_t descriptors.
 * Returns 0 on success, -1 on failure.
 */
int32_t bsp_tf_image_load_all(void);

#endif /* BSP_TF_IMAGE_LOAD_H */
