/**
 * @file    alarm_driver.c
 * @author  王国维
 * @date    2026-05-13
 * @brief   蜂鸣器 + LED报警驱动 - PB8(蜂鸣器), PB9(LED)
 */

#include "alarm_driver.h"

static uint8_t alarm_active = 0;
static AlarmType_t alarm_type = ALARM_NONE;

void Alarm_Init(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_RESET);
    alarm_active = 0;
    alarm_type = ALARM_NONE;
}

void Alarm_SetLED(uint8_t on)
{
    HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Alarm_Beep(uint16_t ms)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(ms);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void Alarm_Trigger(AlarmType_t type)
{
    alarm_active = 1;
    alarm_type = type;
    Alarm_SetLED(1);

    switch (type) {
    case ALARM_SOIL_DRY:
    case ALARM_WATER_LOW:
    case ALARM_SYSTEM:
        /* 长鸣 */
        Alarm_Beep(500);
        break;
    case ALARM_TEMP_HIGH:
    case ALARM_TEMP_LOW:
    case ALARM_SOIL_WET:
        /* 短促3声 */
        for (int i = 0; i < 3; i++) {
            Alarm_Beep(150);
            HAL_Delay(100);
        }
        break;
    default:
        break;
    }
}

void Alarm_Clear(void)
{
    /* 短鸣确认清除 */
    if (alarm_active) {
        Alarm_Beep(50);
    }
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    Alarm_SetLED(0);
    alarm_active = 0;
    alarm_type = ALARM_NONE;
}

uint8_t Alarm_IsActive(void)
{
    return alarm_active;
}
