/**
 * @file    sensor_bh1750.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   BH1750光照传感器驱动
 */

#ifndef SENSOR_BH1750_H
#define SENSOR_BH1750_H

#include "main.h"

typedef struct {
    float light;    /* 光照强度 (lx) */
    uint8_t valid;
} BH1750_Data_t;

HAL_StatusTypeDef BH1750_Init(void);
HAL_StatusTypeDef BH1750_Read(BH1750_Data_t *data);

#endif
