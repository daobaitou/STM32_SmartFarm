/**
 * @file    sensor_mhz19b.h
 * @author  王国维
 * @date    2026-05-13
 * @brief   MH-Z19B CO2传感器驱动
 * @note    UART3 PB10(TX)/PB11(RX), 9600bps, 主动查询模式
 */

#ifndef SENSOR_MHZ19B_H
#define SENSOR_MHZ19B_H

#include "main.h"
#include <stdint.h>

typedef struct {
    uint16_t co2;        /* CO2浓度 (ppm) */
    int8_t   temp;       /* 传感器内置温度 (°C) */
    uint8_t  valid;      /* 数据有效标志 */
} MHZ19B_Data_t;

HAL_StatusTypeDef MHZ19B_Init(void);
HAL_StatusTypeDef MHZ19B_Read(MHZ19B_Data_t *data);

#endif
