/**
 * @file    w25q_flash.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   W25Q系列SPI Flash驱动实现
 * @note    基于软件SPI (PB3/PB4/PB5/PA15)
 */

#include "w25q_flash.h"
#include "soft_spi.h"
#include <string.h>

/* CS控制（W25Q使用软件SPI，需要在驱动层控制CS） */
#define W25Q_CS_L()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)
#define W25Q_CS_H()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)

static uint8_t w25q_read_status(void)
{
    uint8_t status;
    W25Q_CS_L();
    Soft_SPI_Transfer(W25Q_CMD_READ_STATUS);
    status = Soft_SPI_Transfer(0xFF);
    W25Q_CS_H();
    return status;
}

void W25Q_WaitBusy(void)
{
    while (w25q_read_status() & W25Q_SR_BUSY)
    {
        /* 等待写入/擦除完成，约10us延时 */
        for (volatile uint16_t i = 0; i < 100; i++);
    }
}

static void w25q_write_enable(void)
{
    W25Q_CS_L();
    Soft_SPI_Transfer(W25Q_CMD_WRITE_ENABLE);
    W25Q_CS_H();
}

HAL_StatusTypeDef W25Q_Init(void)
{
    Soft_SPI_Init();
    return HAL_OK;
}

HAL_StatusTypeDef W25Q_ReadID(W25Q_Info_t *info)
{
    uint8_t id[3];

    W25Q_CS_L();
    Soft_SPI_Transfer(W25Q_CMD_READ_ID);
    id[0] = Soft_SPI_Transfer(0xFF);  /* Manufacturer ID */
    id[1] = Soft_SPI_Transfer(0xFF);  /* Device ID high */
    id[2] = Soft_SPI_Transfer(0xFF);  /* Device ID low */
    W25Q_CS_H();

    info->manufacturer = id[0];
    info->device_id = (id[1] << 8) | id[2];

    /* 根据Device ID计算容量 */
    switch (info->device_id)
    {
        case 0x15:  /* W25Q16 */
            info->capacity = 2 * 1024 * 1024;  /* 2MB */
            break;
        case 0x16:  /* W25Q32 */
            info->capacity = 4 * 1024 * 1024;  /* 4MB */
            break;
        case 0x17:  /* W25Q64 */
            info->capacity = 8 * 1024 * 1024;  /* 8MB */
            break;
        case 0x18:  /* W25Q128 */
            info->capacity = 16 * 1024 * 1024; /* 16MB */
            break;
        default:
            info->capacity = 2 * 1024 * 1024;  /* 默认假设最小 */
            break;
    }

    /* 检查是否为Winbond芯片 */
    if (info->manufacturer != 0xEF && info->manufacturer != 0xFF)
    {
        return HAL_ERROR;  /* 未知厂商 */
    }

    return HAL_OK;
}

HAL_StatusTypeDef W25Q_Read(uint32_t addr, uint8_t *data, uint16_t len)
{
    W25Q_CS_L();
    Soft_SPI_Transfer(W25Q_CMD_READ_DATA);
    Soft_SPI_Transfer((addr >> 16) & 0xFF);
    Soft_SPI_Transfer((addr >> 8) & 0xFF);
    Soft_SPI_Transfer(addr & 0xFF);

    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = Soft_SPI_Transfer(0xFF);
    }

    W25Q_CS_H();
    return HAL_OK;
}

HAL_StatusTypeDef W25Q_Write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t remaining = len;
    uint32_t current_addr = addr;
    uint16_t offset = 0;

    while (remaining > 0)
    {
        /* 计算当前页剩余空间 */
        uint16_t page_offset = current_addr % W25Q_PAGE_SIZE;
        uint16_t write_len = W25Q_PAGE_SIZE - page_offset;

        if (write_len > remaining)
            write_len = remaining;

        /* 等待上一次操作完成 */
        W25Q_WaitBusy();

        /* 写使能 */
        w25q_write_enable();

        /* 页写入 */
        W25Q_CS_L();
        Soft_SPI_Transfer(W25Q_CMD_PAGE_PROGRAM);
        Soft_SPI_Transfer((current_addr >> 16) & 0xFF);
        Soft_SPI_Transfer((current_addr >> 8) & 0xFF);
        Soft_SPI_Transfer(current_addr & 0xFF);

        for (uint16_t i = 0; i < write_len; i++)
        {
            Soft_SPI_Transfer(data[offset + i]);
        }

        W25Q_CS_H();

        offset += write_len;
        current_addr += write_len;
        remaining -= write_len;
    }

    W25Q_WaitBusy();
    return HAL_OK;
}

HAL_StatusTypeDef W25Q_EraseSector(uint32_t sector_addr)
{
    /* sector_addr必须是4KB对齐 (sector * 4096) */
    uint32_t addr = sector_addr & ~(W25Q_SECTOR_SIZE - 1);

    W25Q_WaitBusy();
    w25q_write_enable();

    W25Q_CS_L();
    Soft_SPI_Transfer(W25Q_CMD_SECTOR_ERASE);
    Soft_SPI_Transfer((addr >> 16) & 0xFF);
    Soft_SPI_Transfer((addr >> 8) & 0xFF);
    Soft_SPI_Transfer(addr & 0xFF);
    W25Q_CS_H();

    /* 扇区擦除约需100ms */
    W25Q_WaitBusy();
    return HAL_OK;
}

HAL_StatusTypeDef W25Q_EraseBlock(uint32_t block_addr)
{
    /* block_addr必须是64KB对齐 */
    uint32_t addr = block_addr & ~(64 * 1024 - 1);

    W25Q_WaitBusy();
    w25q_write_enable();

    W25Q_CS_L();
    Soft_SPI_Transfer(W25Q_CMD_BLOCK_ERASE);
    Soft_SPI_Transfer((addr >> 16) & 0xFF);
    Soft_SPI_Transfer((addr >> 8) & 0xFF);
    Soft_SPI_Transfer(addr & 0xFF);
    W25Q_CS_H();

    /* 块擦除约需1s */
    W25Q_WaitBusy();
    return HAL_OK;
}
