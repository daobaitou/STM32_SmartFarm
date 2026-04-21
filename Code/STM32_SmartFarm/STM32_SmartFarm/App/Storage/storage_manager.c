/**
 * @file    storage_manager.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   存储管理层实现
 * @note    配置参数: Sector 0, 数据日志: Sector 1-2
 *          日志使用扫描法查找有效记录，无需频繁更新头部
 */

#include "storage_manager.h"
#include "w25q_flash.h"
#include <string.h>

/* 日志记录有效标记 */
#define LOG_RECORD_VALID    0xAA
#define LOG_RECORD_INVALID  0xFF

/* 日志头（Sector 1开头，仅在初始化和清空时更新） */
typedef struct {
    uint16_t magic;
    uint16_t reserved;
} LogMeta_t;

#define LOG_META_MAGIC  0xA5A5

/* 日志数据起始偏移（跳过meta和扇区对齐） */
#define LOG_DATA_START  (STORAGE_LOG_ADDR + 32)

static uint16_t log_next_index;   /* 下一条写入位置 */
static uint16_t log_count;        /* 有效记录数 */

/* 扫描日志区域，找到最后一条有效记录 */
static void scan_log_area(void)
{
    log_next_index = 0;
    log_count = 0;

    uint8_t marker;
    for (uint16_t i = 0; i < LOG_MAX_RECORDS; i++)
    {
        uint32_t addr = LOG_DATA_START + (uint32_t)i * LOG_RECORD_SIZE;
        W25Q_Read(addr, &marker, 1);

        if (marker == LOG_RECORD_VALID)
        {
            log_count++;
            log_next_index = i + 1;
        }
        else
        {
            /* 遇到空位，后续都是空的（顺序写入） */
            break;
        }
    }

    if (log_next_index >= LOG_MAX_RECORDS)
        log_next_index = 0;  /* 满了，下次写会触发清空 */
}

HAL_StatusTypeDef Storage_Init(void)
{
    HAL_StatusTypeDef ret = W25Q_Init();
    if (ret != HAL_OK)
        return ret;

    /* 检查日志区域是否已初始化 */
    LogMeta_t meta;
    uint8_t buf[sizeof(LogMeta_t)];
    W25Q_Read(STORAGE_LOG_ADDR, buf, sizeof(LogMeta_t));
    memcpy(&meta, buf, sizeof(LogMeta_t));

    if (meta.magic != LOG_META_MAGIC)
    {
        /* 首次使用，初始化日志区域 */
        W25Q_EraseSector(STORAGE_LOG_ADDR);
        W25Q_EraseSector(STORAGE_LOG_ADDR + W25Q_SECTOR_SIZE);

        meta.magic = LOG_META_MAGIC;
        meta.reserved = 0;
        memcpy(buf, &meta, sizeof(LogMeta_t));
        W25Q_Write(STORAGE_LOG_ADDR, buf, sizeof(LogMeta_t));
    }

    /* 扫描查找有效记录 */
    scan_log_area();

    return HAL_OK;
}

HAL_StatusTypeDef Storage_GetDefaultConfig(SystemConfig_t *config)
{
    config->soil_threshold_low = 30.0f;
    config->soil_threshold_high = 70.0f;
    memset(config->wifi_ssid, 0, sizeof(config->wifi_ssid));
    memset(config->wifi_password, 0, sizeof(config->wifi_password));
    config->irrigation_mode = 0;
    config->pump_state = 0;
    config->total_water_used = 0;
    config->magic = STORAGE_CONFIG_MAGIC;
    return HAL_OK;
}

HAL_StatusTypeDef Storage_LoadConfig(SystemConfig_t *config)
{
    uint8_t buf[sizeof(SystemConfig_t)];
    W25Q_Read(STORAGE_CONFIG_ADDR, buf, sizeof(SystemConfig_t));
    memcpy(config, buf, sizeof(SystemConfig_t));

    if (config->magic != STORAGE_CONFIG_MAGIC)
    {
        Storage_GetDefaultConfig(config);
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef Storage_SaveConfig(const SystemConfig_t *config)
{
    uint8_t buf[sizeof(SystemConfig_t)];
    memcpy(buf, config, sizeof(SystemConfig_t));

    W25Q_EraseSector(STORAGE_CONFIG_ADDR);
    W25Q_Write(STORAGE_CONFIG_ADDR, buf, sizeof(SystemConfig_t));

    return HAL_OK;
}

HAL_StatusTypeDef Storage_WriteLog(const LogRecord_t *record)
{
    /* 日志满时清空重来 */
    if (log_next_index >= LOG_MAX_RECORDS)
    {
        W25Q_EraseSector(STORAGE_LOG_ADDR);
        W25Q_EraseSector(STORAGE_LOG_ADDR + W25Q_SECTOR_SIZE);

        /* 重新写meta */
        LogMeta_t meta = { LOG_META_MAGIC, 0 };
        uint8_t mbuf[sizeof(LogMeta_t)];
        memcpy(mbuf, &meta, sizeof(LogMeta_t));
        W25Q_Write(STORAGE_LOG_ADDR, mbuf, sizeof(LogMeta_t));

        log_next_index = 0;
        log_count = 0;
    }

    /* 构造写入数据：标记 + 记录 */
    uint8_t buf[LOG_RECORD_SIZE];
    memset(buf, 0, LOG_RECORD_SIZE);
    buf[0] = LOG_RECORD_VALID;
    memcpy(&buf[1], record, sizeof(LogRecord_t));

    uint32_t addr = LOG_DATA_START + (uint32_t)log_next_index * LOG_RECORD_SIZE;
    W25Q_Write(addr, buf, LOG_RECORD_SIZE);

    log_next_index++;
    log_count++;
    if (log_count > LOG_MAX_RECORDS)
        log_count = LOG_MAX_RECORDS;

    return HAL_OK;
}

HAL_StatusTypeDef Storage_ReadLog(uint16_t index, LogRecord_t *record)
{
    if (index >= log_count)
        return HAL_ERROR;

    uint32_t addr = LOG_DATA_START + (uint32_t)index * LOG_RECORD_SIZE;
    uint8_t buf[LOG_RECORD_SIZE];
    W25Q_Read(addr, buf, LOG_RECORD_SIZE);

    if (buf[0] != LOG_RECORD_VALID)
        return HAL_ERROR;

    memcpy(record, &buf[1], sizeof(LogRecord_t));
    return HAL_OK;
}

uint16_t Storage_GetLogCount(void)
{
    return log_count;
}

HAL_StatusTypeDef Storage_ClearLog(void)
{
    W25Q_EraseSector(STORAGE_LOG_ADDR);
    W25Q_EraseSector(STORAGE_LOG_ADDR + W25Q_SECTOR_SIZE);

    LogMeta_t meta = { LOG_META_MAGIC, 0 };
    uint8_t mbuf[sizeof(LogMeta_t)];
    memcpy(mbuf, &meta, sizeof(LogMeta_t));
    W25Q_Write(STORAGE_LOG_ADDR, mbuf, sizeof(LogMeta_t));

    log_next_index = 0;
    log_count = 0;

    return HAL_OK;
}
