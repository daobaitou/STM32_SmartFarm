/**
 * @file    sensor_ds18b20.h
 * @author  王国维
 * @date    2026-04-17
 * @brief   DS18B20土壤温度传感器驱动
 */

#ifndef SENSOR_DS18B20_H
#define SENSOR_DS18B20_H

#include "main.h"

typedef struct {
    float temperature;
    uint8_t valid;
} DS18B20_Data_t;

HAL_StatusTypeDef DS18B20_Init(void);
HAL_StatusTypeDef DS18B20_Read(DS18B20_Data_t *data);

#endif
