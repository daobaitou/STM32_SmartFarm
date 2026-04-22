/**
 * @file    usart2_driver.c
 * @author  王国维
 * @date    2026-04-21
 * @brief   USART2底层驱动实现（ESP8266通信）
 * @note    环形缓冲区 + RXNE中断直接读取DR寄存器
 *          单生产者(ISR) / 单消费者(任务)，无需互斥
 */

#include "usart2_driver.h"

extern UART_HandleTypeDef huart2;

static volatile uint8_t  rx_buf[USART2_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

void USART2_Driver_Init(void)
{
    rx_head = 0;
    rx_tail = 0;

    /* 启用RXNE中断 */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);

    /* 启用溢出错误中断 */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_ERR);
}

HAL_StatusTypeDef USART2_SendData(const uint8_t *data, uint16_t len)
{
    return HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 1000);
}

uint16_t USART2_ReadByte(uint8_t *byte)
{
    if (rx_head == rx_tail)
        return 0;

    *byte = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % USART2_RX_BUF_SIZE;
    return 1;
}

uint16_t USART2_Available(void)
{
    uint16_t h = rx_head;
    uint16_t t = rx_tail;
    return (h >= t) ? (h - t) : (USART2_RX_BUF_SIZE - t + h);
}

void USART2_Flush(void)
{
    rx_tail = rx_head;
}

void USART2_IRQ_Callback(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
    {
        uint8_t ch = (uint8_t)(huart2.Instance->DR & 0xFF);
        uint16_t next = (rx_head + 1) % USART2_RX_BUF_SIZE;

        if (next != rx_tail)
        {
            rx_buf[rx_head] = ch;
            rx_head = next;
        }
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&huart2);
}
