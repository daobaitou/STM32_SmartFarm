/**
 * @file    pump_control.h
 * @author  王国维
 * @date    2026-05-13
 * @brief   水泵/继电器控制驱动
 */

#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

#include "main.h"
#include <stdint.h>

void Pump_Init(void);
void Pump_On(void);
void Pump_Off(void);
uint8_t Pump_GetState(void);

#endif
