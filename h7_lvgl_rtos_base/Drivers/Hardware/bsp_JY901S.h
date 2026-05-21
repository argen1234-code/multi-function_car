#ifndef BSP_JY901S_H
#define BSP_JY901S_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JY901S_UPDATE_ACC    0x01U
#define JY901S_UPDATE_GYRO   0x02U
#define JY901S_UPDATE_ANGLE  0x04U
#define JY901S_UPDATE_MAG    0x08U
#define JY901S_UPDATE_TEMP   0x10U

typedef struct
{
    float acc[3];       /* g */
    float gyro[3];      /* deg/s */
    float angle[3];     /* roll, pitch, yaw, deg */
    int16_t mag[3];     /* raw magnetic field */
    float temperature;  /* deg C */
    uint32_t update_flag;
    uint32_t last_update_tick;
    uint8_t online;
} JY901S_Data_t;

void JY901S_Init(void);
void JY901S_RxPro_HAL(uint8_t *pBuf, uint16_t Size);
uint8_t JY901S_GetData(JY901S_Data_t *pData);
uint32_t JY901S_GetUpdateFlag(void);
void JY901S_ClearUpdateFlag(uint32_t flag);

#ifdef __cplusplus
}
#endif

#endif
