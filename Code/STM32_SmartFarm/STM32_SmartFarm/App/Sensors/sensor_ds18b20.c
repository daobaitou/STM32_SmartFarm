/**
 * @file    sensor_ds18b20.c
 * @author  王国维
 * @date    2026-04-17
 * @brief   DS18B20驱动 - 直接寄存器模式切换，消除HAL开销
 */

#include "sensor_ds18b20.h"
#include "gpio.h"
#include <string.h>

#define DS18B20_PORT  GPIOA
#define DS18B20_PIN   GPIO_PIN_6
#define PIN_BIT       (1U << 6)

#define DWT_CYCCNT   (*(volatile uint32_t *)0xE0001004)

/* PA6在CRL[27:24]，推挽输出50MHz=0x3, 上拉输入=0x8 */
#define MODE_OUT()  (DS18B20_PORT->CRL = (DS18B20_PORT->CRL & ~0x0F000000) | 0x03000000)
#define MODE_IN()   (DS18B20_PORT->CRL = (DS18B20_PORT->CRL & ~0x0F000000) | 0x08000000)

#define OW_LOW()    (DS18B20_PORT->BRR  = PIN_BIT)
#define OW_HIGH()   (DS18B20_PORT->BSRR = PIN_BIT)
#define OW_READ()   ((DS18B20_PORT->IDR & PIN_BIT) ? 1 : 0)

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

static uint8_t ow_reset(void)
{
    uint16_t timeout;

    MODE_OUT();
    OW_LOW();
    delay_us(750);
    OW_HIGH();
    delay_us(15);

    MODE_IN();
    DS18B20_PORT->BSRR = PIN_BIT; /* 确保ODR=1，上拉生效 */

    /* 等待DS18B20拉低（存在脉冲） */
    timeout = 200;
    while (OW_READ())
    {
        if (--timeout == 0) return 0;
        delay_us(1);
    }

    /* 等待DS18B20释放 */
    timeout = 240;
    while (!OW_READ())
    {
        if (--timeout == 0) return 0;
        delay_us(1);
    }

    return 1;
}

static void ow_write_bit(uint8_t bit)
{
    if (bit)
    {
        /* 写1: 拉低2us, 释放, 等待60us */
        MODE_OUT();
        OW_LOW();
        delay_us(2);
        OW_HIGH();
        delay_us(60);
    }
    else
    {
        /* 写0: 拉低60us, 释放, 等待2us */
        MODE_OUT();
        OW_LOW();
        delay_us(60);
        OW_HIGH();
        delay_us(2);
    }
}

static uint8_t ow_read_bit(void)
{
    uint8_t val;

    /* 拉低2us启动读时隙 */
    MODE_OUT();
    OW_LOW();
    delay_us(2);
    OW_HIGH();

    /* 切换输入，12us后采样 */
    MODE_IN();
    DS18B20_PORT->BSRR = PIN_BIT;
    delay_us(12);

    val = OW_READ();
    delay_us(50);

    return val;
}

static void ow_write_byte(uint8_t d)
{
    for (int i = 0; i < 8; i++)
    {
        ow_write_bit(d & 0x01);
        d >>= 1;
    }
}

static uint8_t ow_read_byte(void)
{
    uint8_t d = 0;
    for (int i = 0; i < 8; i++)
    {
        d >>= 1;
        if (ow_read_bit()) d |= 0x80;
    }
    return d;
}

HAL_StatusTypeDef DS18B20_Init(void)
{
    DWT_Init();
    MODE_OUT();
    OW_HIGH();
    HAL_Delay(100);
    return HAL_OK;
}

HAL_StatusTypeDef DS18B20_Read(DS18B20_Data_t *data)
{
    uint8_t tl, th;

    memset(data, 0, sizeof(DS18B20_Data_t));

    __disable_irq();
    if (!ow_reset()) { __enable_irq(); return HAL_ERROR; }
    ow_write_byte(0xCC);
    ow_write_byte(0x44);
    __enable_irq();

    HAL_Delay(750);

    __disable_irq();
    if (!ow_reset()) { __enable_irq(); return HAL_ERROR; }
    ow_write_byte(0xCC);
    ow_write_byte(0xBE);
    tl = ow_read_byte();
    th = ow_read_byte();
    __enable_irq();

    int16_t raw = (th << 8) | tl;
    float temp = (float)raw / 16.0f;

    if (temp < -55.0f || temp > 125.0f)
        return HAL_ERROR;

    data->temperature = temp;
    data->valid = 1;
    return HAL_OK;
}
