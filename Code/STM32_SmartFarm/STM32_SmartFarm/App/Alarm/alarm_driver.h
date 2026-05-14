/**
 * @file    alarm_driver.h
 * @author  王国维
 * @date    2026-05-13
 * @brief   蜂鸣器 + LED报警驱动
 */

#ifndef ALARM_DRIVER_H
#define ALARM_DRIVER_H

#include "main.h"
#include <stdint.h>

typedef enum {
    ALARM_NONE = 0,
    ALARM_SOIL_DRY,        /* 土壤过干 */
    ALARM_SOIL_WET,        /* 土壤过湿 */
    ALARM_TEMP_HIGH,       /* 温度过高 */
    ALARM_TEMP_LOW,        /* 温度过低 */
    ALARM_WATER_LOW,       /* 低水位 */
    ALARM_SYSTEM           /* 系统告警 */
} AlarmType_t;

void Alarm_Init(void);
void Alarm_SetLED(uint8_t on);
void Alarm_Beep(uint16_t ms);
void Alarm_Trigger(AlarmType_t type);
void Alarm_Clear(void);
uint8_t Alarm_IsActive(void);

#endif
