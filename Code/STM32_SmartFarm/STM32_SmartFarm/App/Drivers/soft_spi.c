/**
 * @file    soft_spi.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   GPIO模拟SPI驱动 - W25Q Flash专用
 * @note    SPI Mode 0 (CPOL=0, CPHA=0), MSB first
 *          关中断保护字节传输时序
 */

#include "soft_spi.h"

/* PB3=CLK, PB4=MOSI, PB5=MISO, PA15=CS */
#define CLK_H()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET)
#define CLK_L()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET)
#define MOSI_H()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)
#define MOSI_L()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)
#define MISO_RD() HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5)
#define CS_H()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)
#define CS_L()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)

static void spi_delay(void)
{
    for (volatile uint8_t i = 0; i < 4; i++);
}

void Soft_SPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* CLK(PB3), MOSI(PB4): 推挽输出 */
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* MISO(PB5): 上拉输入 */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* CS(PA15): 推挽输出，默认高电平（未选中） */
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    CLK_L();
    CS_H();
}

uint8_t Soft_SPI_Transfer(uint8_t data)
{
    uint8_t rx = 0;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    for (uint8_t i = 0; i < 8; i++)
    {
        /* MOSI: MSB first */
        if (data & 0x80)
            MOSI_H();
        else
            MOSI_L();
        data <<= 1;

        spi_delay();
        CLK_H();
        spi_delay();

        /* MISO: sample on rising edge */
        rx <<= 1;
        if (MISO_RD())
            rx |= 0x01;

        CLK_L();
    }

    __set_PRIMASK(primask);

    return rx;
}
