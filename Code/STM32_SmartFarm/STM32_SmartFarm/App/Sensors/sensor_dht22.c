/**
 * @file    sensor_dht22.c
 * @author  王国维
 * @date    2026-04-17
 * @brief   DHT22温湿度传感器驱动 - 直接寄存器操作版
 */

#include "sensor_dht22.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>

#define DHT22_PORT  GPIOA
#define DHT22_PIN   GPIO_PIN_1
#define PIN_BIT     (1U << 1)

#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)

/* PA1在CRL[7:4], 推挽输出50MHz=0x3, 上拉输入=0x8 */
#define MODE_OUT()  (DHT22_PORT->CRL = (DHT22_PORT->CRL & ~0x000000F0) | 0x00000030)
#define MODE_IN()   (DHT22_PORT->CRL = (DHT22_PORT->CRL & ~0x000000F0) | 0x00000080)

#define OW_LOW()    (DHT22_PORT->BRR  = PIN_BIT)
#define OW_HIGH()   (DHT22_PORT->BSRR = PIN_BIT)
#define OW_READ()   ((DHT22_PORT->IDR & PIN_BIT) ? 1 : 0)

static void delay_us(uint32_t us)
{
    uint32_t start = DWT_CYCCNT;
    while ((DWT_CYCCNT - start) < us * 72);
}

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT_CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

HAL_StatusTypeDef DHT22_Init(void)
{
    DWT_Init();
    MODE_OUT();
    OW_HIGH();
    HAL_Delay(2000);
    return HAL_OK;
}

HAL_StatusTypeDef DHT22_Read(DHT22_Data_t *data)
{
    uint8_t buf[5] = {0};
    uint16_t timeout;

    memset(data, 0, sizeof(DHT22_Data_t));

    __disable_irq();

    /* 起始信号: 拉低1~2ms */
    MODE_OUT();
    OW_LOW();
    delay_us(1500);
    OW_HIGH();
    delay_us(30);

    /* 切换输入 */
    MODE_IN();
    DHT22_PORT->BSRR = PIN_BIT;

    /* 1. 等待传感器拉低（响应LOW ~80us） */
    timeout = 200;
    while (OW_READ())
    {
        if (--timeout == 0) { __enable_irq(); return HAL_ERROR; }
        delay_us(1);
    }

    /* 2. 等待传感器释放（响应HIGH ~80us） */
    timeout = 200;
    while (!OW_READ())
    {
        if (--timeout == 0) { __enable_irq(); return HAL_ERROR; }
        delay_us(1);
    }

    /* 3. 等待响应HIGH结束，传感器拉低开始发数据 */
    timeout = 200;
    while (OW_READ())
    {
        if (--timeout == 0) { __enable_irq(); return HAL_ERROR; }
        delay_us(1);
    }

    /* 读取40位数据：测量高电平脉冲宽度判断0/1 */
    for (int i = 0; i < 5; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            /* 等待50us低电平结束（上升沿） */
            timeout = 200;
            while (!OW_READ())
            {
                if (--timeout == 0) { __enable_irq(); return HAL_ERROR; }
                delay_us(1);
            }

            /* 计数高电平持续时长 */
            uint16_t high_count = 0;
            while (OW_READ())
            {
                delay_us(1);
                if (++high_count > 100) { __enable_irq(); return HAL_ERROR; }
            }

            /* '0': 高电平26-28us → high_count≈22; '1': 高电平70us → high_count≈58 */
            if (high_count > 40)
                buf[i] |= (1 << j);
        }
    }

    __enable_irq();

    printf("  Raw: %02X %02X %02X %02X %02X\r\n",
           buf[0], buf[1], buf[2], buf[3], buf[4]);

    /* 校验 */
    if (buf[4] != ((buf[0] + buf[1] + buf[2] + buf[3]) & 0xFF))
    {
        printf("  FAIL: checksum mismatch\r\n");
        return HAL_ERROR;
    }

    data->humidity = (float)((buf[0] << 8) | buf[1]) / 10.0f;
    data->temperature = (float)(((buf[2] & 0x7F) << 8) | buf[3]) / 10.0f;
    if (buf[2] & 0x80) data->temperature = -data->temperature;
    data->valid = 1;

    return HAL_OK;
}
