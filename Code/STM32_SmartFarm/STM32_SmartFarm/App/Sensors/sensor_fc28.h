/**
 * @file    sensor_fc28.h
 * @author  王国维
 * @date    2026-04-19
 * @brief   FC28土壤湿度传感器驱动
 */

#ifndef SENSOR_FC28_H
#define SENSOR_FC28_H

#include "main.h"

typedef struct {
    uint16_t adc_value;   /* ADC原始值 0~4095 */
    uint8_t moisture;     /* 湿度百分比 0~100% */
    uint8_t valid;
} FC28_Data_t;

HAL_StatusTypeDef FC28_Init(void);
HAL_StatusTypeDef FC28_Read(FC28_Data_t *data);

#endif
