/**
 * @file    uart_bridge.h
 * @author  王国维
 * @date    2026-05-17
 * @brief   UART协议网桥 - STM32与ESP8266的简单文本协议通信
 * @note    STM32发送 SNS:{json}\n，ESP8266回复 ACK:SNS\n
 *          ESP8266发送 CMD:xxx\n，STM32解析为控制命令
 */

#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include "main.h"
#include "app_tasks.h"

typedef enum {
    CMD_MODE_AUTO,
    CMD_MODE_MANUAL,
    CMD_PUMP_ON,
    CMD_PUMP_OFF,
    CMD_SET_THRESHOLD,
    CMD_FAN_ON,
    CMD_FAN_OFF,
    CMD_WINDOW_OPEN,
    CMD_WINDOW_CLOSE,
    CMD_UNKNOWN
} BridgeCmdType_t;

typedef struct {
    BridgeCmdType_t type;
    union {
        uint8_t mode;
        uint8_t pump_state;
        struct { uint8_t low; uint8_t high; } threshold;
    } params;
} BridgeCmd_t;

void UART_Bridge_Init(void);
void UART_Bridge_SendSensorData(const SensorData_t *data);
uint8_t UART_Bridge_CheckCommand(BridgeCmd_t *cmd);

#endif
