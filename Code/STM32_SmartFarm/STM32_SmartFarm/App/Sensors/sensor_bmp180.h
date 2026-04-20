/**
 * @file    sensor_bmp180.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   BMP180大气压强传感器驱动
 */

#ifndef SENSOR_BMP180_H
#define SENSOR_BMP180_H

#include "main.h"

typedef struct {
    float temperature;  /* 温度 (°C) */
    float pressure;     /* 大气压强 (hPa) */
    uint8_t valid;
} BMP180_Data_t;

HAL_StatusTypeDef BMP180_Init(void);
HAL_StatusTypeDef BMP180_Read(BMP180_Data_t *data);

#endif
