/**
 * @file    uart_bridge.h
 * @author  王国维
 * @date    2026-05-17
 * @brief   UART协议网桥 v2.0 - 带校验和的帧协议
 * @note    帧格式: $content*XX\n  (XX=XOR校验和)
 *          ESP8266→STM32: $CMD:type:value*XX\n
 *          STM32→ESP8266: $SNS:{json}*XX / $ACK:type*XX
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
uint8_t parse_command(const char *content, BridgeCmd_t *cmd);

#endif
