/**
 * @file    retarget.c
 * @brief   printf重定向到USART1（HAL库方式）
 */

#include "retarget.h"
#include "usart.h"
#include <stdio.h>

#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
