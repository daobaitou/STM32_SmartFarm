/**
 * @file    servo_control.h
 * @author  王国维
 * @date    2026-05-14
 * @brief   SG90舵机驱动 - TIM3_CH2 PWM, PA7, 50Hz
 */

#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "main.h"
#include <stdint.h>

void Servo_Init(void);
void Servo_SetAngle(uint8_t angle);
void Servo_Open(void);
void Servo_Close(void);
uint8_t Servo_GetAngle(void);

#endif
