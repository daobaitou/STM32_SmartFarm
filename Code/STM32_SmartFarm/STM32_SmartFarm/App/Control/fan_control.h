/**
 * @file    fan_control.h
 * @author  王国维
 * @date    2026-05-14
 * @brief   风扇控制驱动 - PA4 GPIO + MOS驱动
 */

#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "main.h"
#include <stdint.h>

void Fan_Init(void);
void Fan_On(void);
void Fan_Off(void);
uint8_t Fan_GetState(void);

#endif
