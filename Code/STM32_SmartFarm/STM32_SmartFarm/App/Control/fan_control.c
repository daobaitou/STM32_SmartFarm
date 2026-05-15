/**
 * @file    fan_control.c
 * @author  王国维
 * @date    2026-05-14
 * @brief   风扇控制驱动 - PA4, 高电平=ON
 */

#include "fan_control.h"
#include <stdio.h>

static uint8_t fan_state = 0;

void Fan_Init(void)
{
    HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin, GPIO_PIN_RESET);
    fan_state = 0;
    printf("Fan Init OK (PA4)\r\n");
}

void Fan_On(void)
{
    HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin, GPIO_PIN_SET);
    fan_state = 1;
}

void Fan_Off(void)
{
    HAL_GPIO_WritePin(FAN_GPIO_Port, FAN_Pin, GPIO_PIN_RESET);
    fan_state = 0;
}

uint8_t Fan_GetState(void)
{
    return fan_state;
}
