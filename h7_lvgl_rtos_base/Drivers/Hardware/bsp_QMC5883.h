#ifndef __QMC5883_H
#define __QMC5883_H

#include "stm32h7xx_hal.h"
#include <math.h>

/* I2C 7-bit addr = 0x0D. HAL requires left-shift (8-bit addr), hence 0x1A */
#define QMC5883_ADDR 0x1A

typedef struct {
    float yaw;      /* Yaw angle (0~360 deg) */
    float pitch;    /* Pitch angle (-180~180 deg) */
    float roll;     /* Roll angle (-180~180 deg) */
    uint32_t last_update_tick;  /* Last update timestamp */
} EulerAngles;

typedef struct {
    float offset_x, offset_y, offset_z;  /* Hard-iron offset */
    float scale_x, scale_y, scale_z;      /* Soft-iron scale */
} CalibParams;

/* Initialize QMC5883: configure registers and run 30-second calibration. */
void QMC5883_Init(void);

/* Read raw 16-bit ADC counts for X/Y/Z axes via I2C. */
void QMC5883_ReadRawData(int16_t *x, int16_t *y, int16_t *z);

/* Compute yaw/pitch/roll from calibrated magnetometer readings. */
void QMC5883_GetAngles(EulerAngles *angles);

/* Run hard-iron calibration: rotate sensor through all axes for ~30 s. */
void Magnetometer_Calibration(void);

/* Read calibrated magnetometer data (offset removed, scale normalized). */
void QMC5883_Get_CalibrationData(float *hx, float *hy, float *hz);

extern volatile uint8_t qmc5883_calibrating;
extern volatile uint16_t qmc5883_calibration_remaining_s;

#endif
