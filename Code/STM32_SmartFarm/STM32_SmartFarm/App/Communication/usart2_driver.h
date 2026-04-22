/**
 * @file    usart2_driver.h
 * @author  王国维
 * @date    2026-04-21
 * @brief   USART2底层驱动（ESP8266通信）
 * @note    环形缓冲区 + RX中断接收，用于AT命令通信
 */

#ifndef USART2_DRIVER_H
#define USART2_DRIVER_H

#include "main.h"
#include <stdint.h>

#define USART2_RX_BUF_SIZE  512

void USART2_Driver_Init(void);
HAL_StatusTypeDef USART2_SendData(const uint8_t *data, uint16_t len);
uint16_t USART2_ReadByte(uint8_t *byte);
uint16_t USART2_Available(void);
void USART2_Flush(void);
void USART2_IRQ_Callback(void);

#endif
