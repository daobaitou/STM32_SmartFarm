/**
 * @file    soft_spi.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   GPIO模拟SPI驱动 - W25Q Flash专用
 * @note    PB3=CLK, PB4=MOSI, PB5=MISO, PA15=CS
 *          SPI Mode 0 (CPOL=0, CPHA=0), MSB first
 */

#ifndef SOFT_SPI_H
#define SOFT_SPI_H

#include "main.h"

void Soft_SPI_Init(void);
uint8_t Soft_SPI_Transfer(uint8_t data);

#endif /* SOFT_SPI_H */
