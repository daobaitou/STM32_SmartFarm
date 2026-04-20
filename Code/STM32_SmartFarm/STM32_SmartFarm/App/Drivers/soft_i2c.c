/**
 * @file    soft_i2c.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   GPIO模拟I2C驱动 - 解决STM32F103硬件I2C锁死问题
 * @note    基于PB6(SCL)/PB7(SDA), 开漏输出 + 外部上拉
 *          改进版: 关中断保护时序 + 总线恢复 + 超时检测
 */

#include "soft_i2c.h"
#include "gpio.h"

#define SCL_H()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define SCL_L()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)
#define SDA_H()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define SDA_L()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define SDA_READ()  HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)

/* 更保守的延时, 约10us, 实现~50kHz I2C时钟 */
static void soft_i2c_delay(void)
{
    for (volatile uint16_t i = 0; i < 60; i++);
}

/* 总线恢复 - 如果SDA被从设备拉低 */
static void soft_i2c_bus_reset(void)
{
    SDA_H();
    SCL_H();

    /* 发送9个SCL脉冲释放从设备 */
    for (uint8_t i = 0; i < 9; i++)
    {
        SCL_L();
        soft_i2c_delay();
        SCL_H();
        soft_i2c_delay();
    }

    /* 产生STOP条件 */
    SDA_L();
    soft_i2c_delay();
    SCL_H();
    soft_i2c_delay();
    SDA_H();
    soft_i2c_delay();
}

void Soft_I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 初始化总线状态 */
    SCL_H();
    SDA_H();

    /* 确保总线空闲 */
    if (SDA_READ() == GPIO_PIN_RESET)
    {
        soft_i2c_bus_reset();
    }
}

void Soft_I2C_Start(void)
{
    /* 关中断保护START时序 */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    SDA_H();
    SCL_H();
    soft_i2c_delay();
    SDA_L();
    soft_i2c_delay();
    SCL_L();
    soft_i2c_delay();

    __set_PRIMASK(primask);
}

void Soft_I2C_Stop(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    SDA_L();
    soft_i2c_delay();
    SCL_H();
    soft_i2c_delay();
    SDA_H();
    soft_i2c_delay();

    __set_PRIMASK(primask);
}

/* 带超时的ACK等待 */
uint8_t Soft_I2C_WaitAck(void)
{
    uint8_t ack;
    uint16_t timeout = 1000;

    SDA_H();
    soft_i2c_delay();

    /* 等待SCL高电平, 超时则返回NACK */
    SCL_H();
    while (timeout-- > 0)
    {
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET)
            break;
    }

    soft_i2c_delay();
    ack = SDA_READ();
    SCL_L();
    soft_i2c_delay();

    return ack;
}

void Soft_I2C_SendAck(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    SDA_L();
    soft_i2c_delay();
    SCL_H();
    soft_i2c_delay();
    SCL_L();
    soft_i2c_delay();

    __set_PRIMASK(primask);
}

void Soft_I2C_SendNack(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    SDA_H();
    soft_i2c_delay();
    SCL_H();
    soft_i2c_delay();
    SCL_L();
    soft_i2c_delay();

    __set_PRIMASK(primask);
}

void Soft_I2C_WriteByte(uint8_t data)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    for (uint8_t i = 0; i < 8; i++)
    {
        if (data & 0x80)
            SDA_H();
        else
            SDA_L();

        soft_i2c_delay();
        SCL_H();
        soft_i2c_delay();
        SCL_L();
        soft_i2c_delay();

        data <<= 1;
    }

    __set_PRIMASK(primask);
}

uint8_t Soft_I2C_ReadByte(void)
{
    uint8_t data = 0;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    SDA_H();

    for (uint8_t i = 0; i < 8; i++)
    {
        data <<= 1;
        SCL_H();
        soft_i2c_delay();

        if (SDA_READ())
            data |= 0x01;

        SCL_L();
        soft_i2c_delay();
    }

    __set_PRIMASK(primask);

    return data;
}

HAL_StatusTypeDef Soft_I2C_Write(uint8_t addr, uint8_t *data, uint16_t len)
{
    uint8_t retry;

    for (retry = 0; retry < 3; retry++)
    {
        Soft_I2C_Start();
        Soft_I2C_WriteByte(addr << 1);

        if (Soft_I2C_WaitAck() != 0)
        {
            Soft_I2C_Stop();
            soft_i2c_bus_reset();
            continue;
        }

        for (uint16_t i = 0; i < len; i++)
        {
            Soft_I2C_WriteByte(data[i]);
            if (Soft_I2C_WaitAck() != 0)
            {
                Soft_I2C_Stop();
                soft_i2c_bus_reset();
                goto retry_write;
            }
        }

        Soft_I2C_Stop();
        return HAL_OK;

retry_write:
        continue;
    }

    return HAL_ERROR;
}

HAL_StatusTypeDef Soft_I2C_Read(uint8_t addr, uint8_t *data, uint16_t len)
{
    uint8_t retry;

    for (retry = 0; retry < 3; retry++)
    {
        Soft_I2C_Start();
        Soft_I2C_WriteByte((addr << 1) | 0x01);

        if (Soft_I2C_WaitAck() != 0)
        {
            Soft_I2C_Stop();
            soft_i2c_bus_reset();
            continue;
        }

        for (uint16_t i = 0; i < len; i++)
        {
            data[i] = Soft_I2C_ReadByte();
            if (i < len - 1)
                Soft_I2C_SendAck();
            else
                Soft_I2C_SendNack();
        }

        Soft_I2C_Stop();
        return HAL_OK;
    }

    return HAL_ERROR;
}

HAL_StatusTypeDef Soft_I2C_WriteThenRead(uint8_t addr, uint8_t *wdata, uint16_t wlen, uint8_t *rdata, uint16_t rlen)
{
    uint8_t retry;

    for (retry = 0; retry < 3; retry++)
    {
        Soft_I2C_Start();
        Soft_I2C_WriteByte(addr << 1);

        if (Soft_I2C_WaitAck() != 0)
        {
            Soft_I2C_Stop();
            soft_i2c_bus_reset();
            continue;
        }

        for (uint16_t i = 0; i < wlen; i++)
        {
            Soft_I2C_WriteByte(wdata[i]);
            if (Soft_I2C_WaitAck() != 0)
            {
                Soft_I2C_Stop();
                soft_i2c_bus_reset();
                goto retry_wr;
            }
        }

        Soft_I2C_Start();
        Soft_I2C_WriteByte((addr << 1) | 0x01);

        if (Soft_I2C_WaitAck() != 0)
        {
            Soft_I2C_Stop();
            soft_i2c_bus_reset();
            continue;
        }

        for (uint16_t i = 0; i < rlen; i++)
        {
            rdata[i] = Soft_I2C_ReadByte();
            if (i < rlen - 1)
                Soft_I2C_SendAck();
            else
                Soft_I2C_SendNack();
        }

        Soft_I2C_Stop();
        return HAL_OK;

retry_wr:
        continue;
    }

    return HAL_ERROR;
}
