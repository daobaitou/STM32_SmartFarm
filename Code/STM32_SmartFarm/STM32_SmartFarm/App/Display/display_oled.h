/**
 * @file    display_oled.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   0.96寸OLED显示驱动 (SSD1306, I2C, 128x64)
 */

#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include "main.h"

HAL_StatusTypeDef OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);

/* 基础绘制 */
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawChar(uint8_t x, uint8_t y, char ch, uint8_t size);
void OLED_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_Fill(uint8_t data);

/* 常用尺寸 */
#define FONT_SMALL  1   /* 6x8 */
#define FONT_MEDIUM 2   /* 8x16 */

#endif
