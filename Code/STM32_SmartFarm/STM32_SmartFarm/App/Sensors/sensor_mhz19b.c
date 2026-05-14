/**
 * @file    sensor_mhz19b.c
 * @author  王国维
 * @date    2026-05-13
 * @brief   MH-Z19B CO2传感器驱动 - UART3, 9600bps
 * @note    查询命令 0xFF 0x01 0x86 0x00 0x00 0x00 0x00 0x00 0x79
 *          响应  0xFF 0x86 [CO2_H] [CO2_L] [T] [S] [U] [L] [CHK]
 */

#include "sensor_mhz19b.h"
#include "usart.h"
#include <stdio.h>

extern UART_HandleTypeDef huart3;

static const uint8_t cmd_read[9] = {
    0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79
};

static uint16_t last_co2 = 0;
static uint8_t warmup_done = 0;
static uint32_t warmup_start = 0;

HAL_StatusTypeDef MHZ19B_Init(void)
{
    /* 清空USART3缓冲区 */
    uint8_t dummy;
    while (HAL_UART_Receive(&huart3, &dummy, 1, 10) == HAL_OK);
    warmup_start = HAL_GetTick();
    warmup_done = 0;
    printf("MHZ19B Init OK (warming up ~3min)\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef MHZ19B_Read(MHZ19B_Data_t *data)
{
    uint8_t rx[9] = {0};
    HAL_StatusTypeDef ret;

    if (!data) return HAL_ERROR;
    data->valid = 0;

    /* 发送查询命令 */
    ret = HAL_UART_Transmit(&huart3, (uint8_t *)cmd_read, 9, 200);
    if (ret != HAL_OK)
    {
        printf("MHZ19B: TX failed\r\n");
        return ret;
    }

    /* 接收响应 */
    ret = HAL_UART_Receive(&huart3, rx, 9, 500);
    if (ret != HAL_OK)
    {
        printf("MHZ19B: RX timeout\r\n");
        return ret;
    }

    /* 校验帧头 */
    if (rx[0] != 0xFF || rx[1] != 0x86)
    {
        printf("MHZ19B: Bad header %02X %02X\r\n", rx[0], rx[1]);
        return HAL_ERROR;
    }

    /* 校验和 */
    uint8_t sum = 0;
    for (int i = 1; i < 8; i++) sum += rx[i];
    sum = (0xFF - sum) + 1;
    if (sum != rx[8])
    {
        printf("MHZ19B: Checksum fail (calc=%02X, rx=%02X)\r\n", sum, rx[8]);
        return HAL_ERROR;
    }

    data->co2 = ((uint16_t)rx[2] << 8) | rx[3];
    data->temp = (int8_t)rx[4] - 40;  /* 温度补偿值, -40°C偏移 */

    /* 预暖检查: MH-Z19B需要~3分钟稳定, 前120秒读数不可靠 */
    if (!warmup_done) {
        if (HAL_GetTick() - warmup_start > 120000) {  /* 2分钟 */
            warmup_done = 1;
            printf("MHZ19B: warmup complete\r\n");
        } else {
            data->valid = 0;
            return HAL_ERROR;
        }
    }

    /* 过滤量程极限值: 5000ppm=传感器饱和/未稳定, 0=无效 */
    if (data->co2 >= 4500 || data->co2 == 0) {
        printf("MHZ19B: out-of-range (%u ppm)\r\n", data->co2);
        data->valid = 0;
        return HAL_ERROR;
    }

    data->valid = 1;
    last_co2 = data->co2;
    return HAL_OK;
}
