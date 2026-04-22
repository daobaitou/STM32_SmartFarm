/**
 * @file    esp8266_at.h
 * @author  王国维
 * @date    2026-04-22
 * @brief   ESP8266 AT命令驱动接口 - TCP通信模式
 * @note    Cytron固件AT MQTT命令不可用，改用原始TCP+MQTT协议
 */

#ifndef ESP8266_AT_H
#define ESP8266_AT_H

#include "main.h"
#include <stdint.h>

typedef enum {
    AT_OK = 0,
    AT_ERROR,
    AT_TIMEOUT,
    AT_FAIL
} AT_Result_t;

typedef enum {
    CONN_IDLE = 0,
    CONN_WIFI_CONNECTED,
    CONN_TCP_CONNECTED
} ConnState_t;

AT_Result_t ESP8266_AT_Init(void);
AT_Result_t ESP8266_ConnectWiFi(const char *ssid, const char *password);
AT_Result_t ESP8266_TCP_Connect(const char *host, uint16_t port);
AT_Result_t ESP8266_TCP_Send(const uint8_t *data, uint16_t len);
AT_Result_t ESP8266_TCP_Read(uint8_t *buf, uint16_t buf_size, uint16_t *read_len);
AT_Result_t ESP8266_TCP_Close(void);
uint8_t ESP8266_TCP_HasData(void);
ConnState_t ESP8266_GetState(void);
AT_Result_t ESP8266_Reset(void);
AT_Result_t ESP8266_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms);

#endif
