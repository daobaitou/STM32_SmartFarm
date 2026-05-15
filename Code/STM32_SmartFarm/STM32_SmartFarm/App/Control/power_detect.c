/**
 * @file    power_detect.c
 * @author  王国维
 * @date    2026-05-15
 * @brief   断电检测模块实现 - LM393比较器检测市电状态
 * @note    LM393通过分压网络检测12V适配器，输出到PA5
 *          市电正常: PA5=LOW, 断电: PA5=HIGH(上拉)
 *          双沿中断：上升沿=断电，下降沿=恢复
 */

#include "power_detect.h"

static volatile PowerSource_t current_source = POWER_SOURCE_MAIN;

void PowerDetect_Init(void)
{
    /* GPIO初始化在gpio.c中完成 */
    /* 启动时读取PA5当前电平确定初始状态 */
    if (HAL_GPIO_ReadPin(POWER_DETECT_GPIO_Port, POWER_DETECT_Pin) == GPIO_PIN_SET)
        current_source = POWER_SOURCE_BACKUP;
    else
        current_source = POWER_SOURCE_MAIN;
}

void PowerDetect_SetBackup(void)
{
    current_source = POWER_SOURCE_BACKUP;
}

void PowerDetect_SetMain(void)
{
    current_source = POWER_SOURCE_MAIN;
}

PowerSource_t PowerDetect_GetSource(void)
{
    return current_source;
}

uint8_t PowerDetect_IsBackup(void)
{
    return (current_source == POWER_SOURCE_BACKUP);
}
