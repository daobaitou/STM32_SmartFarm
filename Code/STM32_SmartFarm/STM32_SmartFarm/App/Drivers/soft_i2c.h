/**
 * @file    soft_i2c.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   GPIO模拟I2C驱动
 */

#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include "main.h"

void Soft_I2C_Init(void);

/* 底层时序 */
void Soft_I2C_Start(void);
void Soft_I2C_Stop(void);
void Soft_I2C_WriteByte(uint8_t data);
uint8_t Soft_I2C_ReadByte(void);
uint8_t Soft_I2C_WaitAck(void);
void Soft_I2C_SendAck(void);
void Soft_I2C_SendNack(void);

/* 高层接口 */
HAL_StatusTypeDef Soft_I2C_Write(uint8_t addr, uint8_t *data, uint16_t len);
HAL_StatusTypeDef Soft_I2C_Read(uint8_t addr, uint8_t *data, uint16_t len);
HAL_StatusTypeDef Soft_I2C_WriteThenRead(uint8_t addr, uint8_t *wdata, uint16_t wlen, uint8_t *rdata, uint16_t rlen);

#endif
