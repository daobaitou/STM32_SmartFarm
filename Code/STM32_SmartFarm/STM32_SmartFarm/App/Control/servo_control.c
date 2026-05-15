/**
 * @file    servo_control.c
 * @author  王国维
 * @date    2026-05-14
 * @brief   SG90舵机驱动 - TIM3_CH2 PWM (PA7), 50Hz
 * @note    角度0-180°, 脉宽0.5ms-2.5ms
 *          TIM3: 72MHz/72=1MHz, Period=20000(20ms=50Hz)
 *          CCR: 0°=500, 90°=1500, 180°=2500
 */

#include "servo_control.h"
#include "stm32f1xx_hal_tim.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim3;

#define SERVO_CHANNEL  TIM_CHANNEL_2
#define ANGLE_MIN      0
#define ANGLE_MAX      180
#define PULSE_MIN      500    /* 0.5ms  → 0° */
#define PULSE_MAX      2500   /* 2.5ms  → 180° */

#define WINDOW_OPEN_ANGLE   90
#define WINDOW_CLOSE_ANGLE  0

static uint8_t current_angle = 0;

static uint16_t angle_to_pulse(uint8_t angle)
{
    if (angle > ANGLE_MAX) angle = ANGLE_MAX;
    return (uint16_t)(PULSE_MIN + (uint32_t)(PULSE_MAX - PULSE_MIN) * angle / ANGLE_MAX);
}

void Servo_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA7 → TIM3_CH2 复用推挽 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* TIM3 PWM: 50Hz (20ms) */
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 72 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 20000 - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = angle_to_pulse(WINDOW_CLOSE_ANGLE);
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, SERVO_CHANNEL);

    HAL_TIM_PWM_Start(&htim3, SERVO_CHANNEL);

    current_angle = WINDOW_CLOSE_ANGLE;
    printf("Servo Init OK (PA7, TIM3_CH2, 50Hz)\r\n");
}

void Servo_SetAngle(uint8_t angle)
{
    if (angle > ANGLE_MAX) angle = ANGLE_MAX;
    __HAL_TIM_SET_COMPARE(&htim3, SERVO_CHANNEL, angle_to_pulse(angle));
    current_angle = angle;
}

void Servo_Open(void)
{
    Servo_SetAngle(WINDOW_OPEN_ANGLE);
    printf("[Servo] Window OPEN (%d°)\r\n", WINDOW_OPEN_ANGLE);
}

void Servo_Close(void)
{
    Servo_SetAngle(WINDOW_CLOSE_ANGLE);
    printf("[Servo] Window CLOSE (%d°)\r\n", WINDOW_CLOSE_ANGLE);
}

uint8_t Servo_GetAngle(void)
{
    return current_angle;
}
