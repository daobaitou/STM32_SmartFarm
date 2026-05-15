/**
 * @file    tim.h
 * @brief   TIM4 HAL时间基准 + TIM3 PWM舵机
 */

#ifndef TIM_H
#define TIM_H

#include "main.h"
#include "stm32f1xx_hal_tim.h"

extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim3;

void MX_TIM4_Init(void);

#endif
