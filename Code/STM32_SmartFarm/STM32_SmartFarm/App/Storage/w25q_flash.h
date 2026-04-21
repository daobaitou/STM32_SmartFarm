/**
 * @file    w25q_flash.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   W25Q系列SPI Flash驱动 (W25Q16/32/64兼容)
 * @note    基于软件SPI (PB3/PB4/PB5/PA15)
 */

#ifndef W25Q_FLASH_H
#define W25Q_FLASH_H

#include "main.h"

/* W25Q commands */
#define W25Q_CMD_WRITE_ENABLE    0x06
#define W25Q_CMD_WRITE_DISABLE   0x04
#define W25Q_CMD_READ_STATUS     0x05
#define W25Q_CMD_WRITE_STATUS    0x01
#define W25Q_CMD_READ_DATA       0x03
#define W25Q_CMD_PAGE_PROGRAM    0x02
#define W25Q_CMD_SECTOR_ERASE    0x20
#define W25Q_CMD_BLOCK_ERASE     0xD8
#define W25Q_CMD_CHIP_ERASE      0xC7
#define W25Q_CMD_READ_ID         0x9F

/* W25Q Flash geometry */
#define W25Q_PAGE_SIZE           256
#define W25Q_SECTOR_SIZE         4096
#define W25Q_SECTOR_COUNT_MIN    512    /* W25Q16 = 512 sectors = 2MB */

/* Status register bits */
#define W25Q_SR_BUSY             0x01
#define W25Q_SR_WEL              0x02

typedef struct {
    uint8_t  manufacturer;   /* 0xEF = Winbond */
    uint16_t device_id;      /* 0x15=W25Q16, 0x16=W25Q32, 0x17=W25Q64 */
    uint32_t capacity;       /* bytes */
} W25Q_Info_t;

HAL_StatusTypeDef W25Q_Init(void);
HAL_StatusTypeDef W25Q_ReadID(W25Q_Info_t *info);
HAL_StatusTypeDef W25Q_Read(uint32_t addr, uint8_t *data, uint16_t len);
HAL_StatusTypeDef W25Q_Write(uint32_t addr, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef W25Q_EraseSector(uint32_t sector_addr);
HAL_StatusTypeDef W25Q_EraseBlock(uint32_t block_addr);
void W25Q_WaitBusy(void);

#endif /* W25Q_FLASH_H */
