/**
 * @file    pump_control.c
 * @author  王国维
 * @date    2026-05-13
 * @brief   水泵/继电器控制驱动 - PB1, 高电平=ON
 */

#include "pump_control.h"

static uint8_t pump_state = 0;

void Pump_Init(void)
{
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);
    pump_state = 0;
}

void Pump_On(void)
{
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_SET);
    pump_state = 1;
}

void Pump_Off(void)
{
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);
    pump_state = 0;
}

uint8_t Pump_GetState(void)
{
    return pump_state;
}
