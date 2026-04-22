/**
 * @file    esp8266_at.c
 * @author  王国维
 * @date    2026-04-22
 * @brief   ESP8266 AT命令驱动实现 - TCP通信模式
 * @note    通过AT+CIPSTART/CIPSEND/+IPD实现TCP数据收发
 *          所有函数受xMutex_USART2保护，防止多任务竞争
 */

#include "esp8266_at.h"
#include "usart2_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

/* USART2 互斥锁 - 在MQTT_Init中创建 */
SemaphoreHandle_t xMutex_USART2 = NULL;

static char at_resp_buf[256];
static ConnState_t conn_state = CONN_IDLE;

/* +IPD数据缓冲 */
static uint8_t ipd_buf[200];
static uint16_t ipd_len = 0;
static uint8_t ipd_ready = 0;

/* 获取互斥锁（带超时） */
static uint8_t take_mutex(uint32_t timeout_ms)
{
    if (!xMutex_USART2) return 1;
    return xSemaphoreTake(xMutex_USART2, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

static void give_mutex(void)
{
    if (xMutex_USART2)
        xSemaphoreGive(xMutex_USART2);
}

/* 发送AT命令并等待响应 */
AT_Result_t ESP8266_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    if (!take_mutex(timeout_ms + 1000)) return AT_TIMEOUT;

    USART2_Flush();
    USART2_SendData((const uint8_t *)cmd, strlen(cmd));
    USART2_SendData((const uint8_t *)"\r\n", 2);

    uint32_t start = HAL_GetTick();
    uint16_t resp_len = 0;

    while (HAL_GetTick() - start < timeout_ms)
    {
        uint8_t ch;
        while (USART2_ReadByte(&ch))
        {
            if (resp_len < sizeof(at_resp_buf) - 1)
                at_resp_buf[resp_len++] = ch;
        }

        if (resp_len > 0)
        {
            at_resp_buf[resp_len] = '\0';
            if (expect && strstr(at_resp_buf, expect))
            {
                give_mutex();
                return AT_OK;
            }
            if (strstr(at_resp_buf, "ERROR") || strstr(at_resp_buf, "FAIL"))
            {
                give_mutex();
                return AT_ERROR;
            }
            if (strstr(at_resp_buf, "+IPD"))
            {
                char *p = at_resp_buf + 4;
                if (*p == ',') p++;
                uint16_t dlen = 0;
                while (*p >= '0' && *p <= '9') dlen = dlen * 10 + (*p++ - '0');
                if (*p == ':') p++;
                if (dlen > 0 && dlen <= sizeof(ipd_buf))
                {
                    memcpy(ipd_buf, p, dlen);
                    ipd_len = dlen;
                    ipd_ready = 1;
                }
                give_mutex();
                return AT_OK;
            }
        }
        HAL_Delay(10);
    }

    give_mutex();
    return AT_TIMEOUT;
}

AT_Result_t ESP8266_AT_Init(void)
{
    /* 创建互斥锁（仅第一次） */
    if (!xMutex_USART2)
    {
        xMutex_USART2 = xSemaphoreCreateMutex();
        if (!xMutex_USART2) return AT_FAIL;
    }

    /* ESP8266 should be ready - caller waits before calling this */
    AT_Result_t ret;
    ret = ESP8266_SendCmd("AT", "OK", 2000);
    if (ret != AT_OK) return ret;

    ret = ESP8266_SendCmd("ATE0", "OK", 1000);
    if (ret != AT_OK) return ret;

    ret = ESP8266_SendCmd("AT+CWMODE=1", "OK", 2000);
    if (ret != AT_OK) return ret;

    ret = ESP8266_SendCmd("AT+CIPMUX=0", "OK", 2000);
    if (ret != AT_OK) return ret;

    return AT_OK;
}

AT_Result_t ESP8266_ConnectWiFi(const char *ssid, const char *password)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

    AT_Result_t ret = ESP8266_SendCmd(cmd, "WIFI GOT IP", 20000);
    if (ret == AT_OK)
        conn_state = CONN_WIFI_CONNECTED;
    return ret;
}

AT_Result_t ESP8266_TCP_Connect(const char *host, uint16_t port)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, port);

    AT_Result_t ret = ESP8266_SendCmd(cmd, "CONNECT", 15000);
    if (ret == AT_OK)
        conn_state = CONN_TCP_CONNECTED;
    return ret;
}

AT_Result_t ESP8266_TCP_Send(const uint8_t *data, uint16_t len)
{
    if (!take_mutex(6000)) return AT_TIMEOUT;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", len);

    USART2_Flush();
    USART2_SendData((const uint8_t *)cmd, strlen(cmd));
    USART2_SendData((const uint8_t *)"\r\n", 2);

    uint32_t start = HAL_GetTick();
    uint16_t resp_len = 0;
    uint8_t got_prompt = 0;

    /* 等待 > 提示符 */
    while (HAL_GetTick() - start < 3000)
    {
        uint8_t ch;
        while (USART2_ReadByte(&ch))
        {
            if (resp_len < sizeof(at_resp_buf) - 1)
                at_resp_buf[resp_len++] = ch;
        }
        if (resp_len > 0)
        {
            at_resp_buf[resp_len] = '\0';
            if (strstr(at_resp_buf, ">") || strstr(at_resp_buf, "OK"))
            {
                got_prompt = 1;
                break;
            }
            if (strstr(at_resp_buf, "ERROR"))
            {
                give_mutex();
                return AT_ERROR;
            }
        }
        HAL_Delay(5);
    }

    if (!got_prompt)
    {
        give_mutex();
        return AT_TIMEOUT;
    }

    /* 发送数据 */
    USART2_SendData(data, len);

    /* 等待SEND OK */
    start = HAL_GetTick();
    resp_len = 0;

    while (HAL_GetTick() - start < 3000)
    {
        uint8_t ch;
        while (USART2_ReadByte(&ch))
        {
            if (resp_len < sizeof(at_resp_buf) - 1)
                at_resp_buf[resp_len++] = ch;
        }
        if (resp_len > 0)
        {
            at_resp_buf[resp_len] = '\0';
            if (strstr(at_resp_buf, "SEND OK"))
            {
                give_mutex();
                return AT_OK;
            }
            if (strstr(at_resp_buf, "ERROR"))
            {
                give_mutex();
                return AT_ERROR;
            }
            /* 发送过程中收到+IPD */
            if (strstr(at_resp_buf, "+IPD"))
            {
                char *p = at_resp_buf + 4;
                if (*p == ',') p++;
                uint16_t dlen = 0;
                while (*p >= '0' && *p <= '9') dlen = dlen * 10 + (*p++ - '0');
                if (*p == ':') p++;
                if (dlen > 0 && dlen <= sizeof(ipd_buf))
                {
                    memcpy(ipd_buf, p, dlen);
                    ipd_len = dlen;
                    ipd_ready = 1;
                }
            }
        }
        HAL_Delay(5);
    }

    give_mutex();
    return AT_TIMEOUT;
}

AT_Result_t ESP8266_TCP_Read(uint8_t *buf, uint16_t buf_size, uint16_t *read_len)
{
    if (!take_mutex(1000))
    {
        *read_len = 0;
        return AT_TIMEOUT;
    }

    if (!ipd_ready)
    {
        uint8_t ch;
        static char poll_buf[160];
        static uint16_t poll_len = 0;

        while (USART2_ReadByte(&ch))
        {
            if (poll_len < sizeof(poll_buf) - 1)
                poll_buf[poll_len++] = ch;
        }

        if (poll_len > 0)
        {
            poll_buf[poll_len] = '\0';
            char *p = strstr(poll_buf, "+IPD");
            if (p)
            {
                p += 4;
                if (*p == ',') p++;
                uint16_t dlen = 0;
                while (*p >= '0' && *p <= '9') dlen = dlen * 10 + (*p++ - '0');
                if (*p == ':') p++;
                if (dlen > 0 && dlen <= sizeof(ipd_buf))
                {
                    memcpy(ipd_buf, p, dlen);
                    ipd_len = dlen;
                    ipd_ready = 1;
                }
            }
            poll_len = 0;
        }
    }

    if (ipd_ready)
    {
        uint16_t copy_len = (ipd_len < buf_size) ? ipd_len : buf_size;
        memcpy(buf, ipd_buf, copy_len);
        *read_len = copy_len;
        ipd_ready = 0;
        ipd_len = 0;
        give_mutex();
        return AT_OK;
    }

    *read_len = 0;
    give_mutex();
    return AT_TIMEOUT;
}

AT_Result_t ESP8266_TCP_Close(void)
{
    AT_Result_t ret = ESP8266_SendCmd("AT+CIPCLOSE", "OK", 3000);
    conn_state = CONN_WIFI_CONNECTED;
    return ret;
}

uint8_t ESP8266_TCP_HasData(void)
{
    if (ipd_ready) return 1;
    return USART2_Available() > 0 ? 1 : 0;
}

ConnState_t ESP8266_GetState(void)
{
    return conn_state;
}

AT_Result_t ESP8266_Reset(void)
{
    AT_Result_t ret = ESP8266_SendCmd("AT+RST", "OK", 3000);
    if (ret == AT_OK)
    {
        conn_state = CONN_IDLE;
        HAL_Delay(3000);
    }
    return ret;
}
