/**
 * @file    sensor_dht22.h
 * @author  王国维
 * @date    2026-04-17
 * @brief   DHT22温湿度传感器驱动
 */

#ifndef SENSOR_DHT22_H
#define SENSOR_DHT22_H

#include "main.h"

typedef struct {
    float temperature;
    float humidity;
    uint8_t valid;
} DHT22_Data_t;

HAL_StatusTypeDef DHT22_Init(void);
HAL_StatusTypeDef DHT22_Read(DHT22_Data_t *data);

#endif
