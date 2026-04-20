/**
 * @file    sensor_yfs201.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   YF-S201水流传感器驱动 (EXTI脉冲计数, PB0)
 */

#ifndef SENSOR_YFS201_H
#define SENSOR_YFS201_H

#include "main.h"

typedef struct {
    float flow_rate;        /* 流速 (L/min) */
    float total_volume;     /* 累计流量 (L) */
    uint32_t pulse_count;   /* 累计脉冲数 */
    uint8_t valid;
} YFS201_Data_t;

HAL_StatusTypeDef YFS201_Init(void);
HAL_StatusTypeDef YFS201_Read(YFS201_Data_t *data);
void YFS201_ResetVolume(void);

/* EXTI中断中调用, 供stm32f1xx_it.c使用 */
void YFS201_PulseHandler(void);

#endif
