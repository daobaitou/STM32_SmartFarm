/**
 * @file    storage_manager.h
 * @author  王国维
 * @date    2026-04-20
 * @brief   存储管理层 - 配置参数持久化 + 传感器数据日志
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "main.h"
#include <stdint.h>

/* 存储区域划分 */
#define STORAGE_CONFIG_ADDR     0x000000   /* Sector 0: 系统配置 4KB */
#define STORAGE_LOG_ADDR        0x001000   /* Sector 1-2: 数据日志 8KB */
#define STORAGE_EVENT_ADDR      0x003000   /* Sector 3: 事件记录 4KB */

#define STORAGE_CONFIG_MAGIC    0xDEADBEEF

/* 数据日志：每条记录 32 字节 */
#define LOG_RECORD_SIZE         32
#define LOG_MAX_RECORDS         250    /* 8KB / 32 = 250 条 */

/* 系统配置参数 */
typedef struct {
    float soil_threshold_low;          /* 土壤湿度下限 (%) */
    float soil_threshold_high;         /* 土壤湿度上限 (%) */
    char wifi_ssid[32];
    char wifi_password[64];
    uint8_t irrigation_mode;           /* 0=自动, 1=手动 */
    uint8_t pump_state;                /* 水泵最后状态 */
    uint32_t total_water_used;         /* 累计用水量 (mL) */
    uint32_t magic;                    /* 0xDEADBEEF 校验标记 */
} SystemConfig_t;

/* 传感器数据日志记录 */
typedef struct {
    float temperature;
    float humidity;
    uint8_t soil_moisture;
    float light;
    float pressure;
    uint32_t timestamp;
} LogRecord_t;  /* 28 bytes, padded to 32 */

HAL_StatusTypeDef Storage_Init(void);
HAL_StatusTypeDef Storage_LoadConfig(SystemConfig_t *config);
HAL_StatusTypeDef Storage_SaveConfig(const SystemConfig_t *config);
HAL_StatusTypeDef Storage_GetDefaultConfig(SystemConfig_t *config);
HAL_StatusTypeDef Storage_WriteLog(const LogRecord_t *record);
HAL_StatusTypeDef Storage_ReadLog(uint16_t index, LogRecord_t *record);
uint16_t Storage_GetLogCount(void);
HAL_StatusTypeDef Storage_ClearLog(void);

#endif /* STORAGE_MANAGER_H */
