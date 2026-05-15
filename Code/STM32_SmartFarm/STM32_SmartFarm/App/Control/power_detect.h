/**
 * @file    power_detect.h
 * @author  王国维
 * @date    2026-05-15
 * @brief   断电检测模块 - LM393比较器检测市电状态
 * @note    PA5双沿中断，上升沿=断电，下降沿=恢复
 */

#ifndef POWER_DETECT_H
#define POWER_DETECT_H

#include "main.h"

typedef enum {
    POWER_SOURCE_MAIN,      /* 市电 */
    POWER_SOURCE_BACKUP     /* 备用电池 */
} PowerSource_t;

void PowerDetect_Init(void);
void PowerDetect_SetBackup(void);
void PowerDetect_SetMain(void);
PowerSource_t PowerDetect_GetSource(void);
uint8_t PowerDetect_IsBackup(void);

#endif
