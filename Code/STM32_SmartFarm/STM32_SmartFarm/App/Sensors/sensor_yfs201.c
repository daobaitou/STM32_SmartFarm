/**
 * @file    sensor_yfs201.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   YF-S201水流传感器驱动 (EXTI脉冲计数, PB0)
 * @note    Flow(L/min) = frequency(Hz) / 7.5, 1L = 450 pulses
 */

#include "sensor_yfs201.h"

#define FLOW_COEFFICIENT  7.5f
#define PULSES_PER_LITER  450.0f

static volatile uint32_t pulse_total = 0;
static uint32_t last_count = 0;
static uint32_t last_tick = 0;

void YFS201_PulseHandler(void)
{
    pulse_total++;
}

HAL_StatusTypeDef YFS201_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PB0配置为EXTI下降沿触发 */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 使能EXTI0中断 */
    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    pulse_total = 0;
    last_count = 0;
    last_tick = HAL_GetTick();

    return HAL_OK;
}

HAL_StatusTypeDef YFS201_Read(YFS201_Data_t *data)
{
    uint32_t now = HAL_GetTick();
    uint32_t dt_ms = now - last_tick;

    if (dt_ms < 100)
    {
        data->valid = 0;
        return HAL_ERROR;
    }

    uint32_t current = pulse_total;
    uint32_t delta = current - last_count;
    last_count = current;
    last_tick = now;

    /* 流速 = 脉冲频率 / 7.5 */
    float freq = (float)delta * 1000.0f / (float)dt_ms;
    data->flow_rate = freq / FLOW_COEFFICIENT;
    data->total_volume = (float)current / PULSES_PER_LITER;
    data->pulse_count = current;
    data->valid = 1;

    return HAL_OK;
}

void YFS201_ResetVolume(void)
{
    pulse_total = 0;
    last_count = 0;
}
