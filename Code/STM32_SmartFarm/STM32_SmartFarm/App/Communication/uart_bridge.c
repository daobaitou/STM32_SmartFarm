/**
 * @file    uart_bridge.c
 * @author  王国维
 * @date    2026-05-17
 * @brief   UART协议网桥 v2.0 - 带校验和的帧协议
 * @note    帧格式: $content*XX\n  (XX=XOR校验)
 *          ESP8266→STM32: $CMD:type:value*XX\n
 *          STM32→ESP8266: $SNS:{json}*XX\n
 *          STM32→ESP8266: $ACK:type*XX / $NACK:type*XX
 */

#include "uart_bridge.h"
#include "usart2_driver.h"
#include <stdio.h>
#include <string.h>

static char cmd_buf[96];
static uint8_t cmd_idx = 0;

/* XOR校验和计算 */
static uint8_t xor_checksum(const char *s, uint8_t len)
{
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len; i++)
        cs ^= (uint8_t)s[i];
    return cs;
}

/* 十六进制字符转数值 */
static uint8_t hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

/* 发送带校验的帧 */
static void send_frame(const char *content)
{
    uint8_t len = strlen(content);
    uint8_t cs = xor_checksum(content, len);

    char buf[128];
    int n = snprintf(buf, sizeof(buf), "$%s*%02X\n", content, cs);
    if (n > 0 && n < (int)sizeof(buf))
        USART2_SendData((uint8_t *)buf, n);
}

void UART_Bridge_Init(void)
{
    USART2_Flush();
    cmd_idx = 0;
}

void UART_Bridge_SendSensorData(const SensorData_t *data)
{
    char content[140];
    int len = snprintf(content, sizeof(content),
        "SNS:{\"t\":%.1f,\"h\":%.1f,\"sm\":%u,\"st\":%.1f,\"l\":%.0f,\"p\":%.1f,\"c\":%u,\"f\":%.2f,\"v\":%.2f}",
        data->temperature, data->humidity,
        data->soil_moisture, data->soil_temp,
        data->light, data->pressure, data->co2,
        data->flow_rate, data->total_volume);

    if (len > 0 && len < (int)sizeof(content))
        send_frame(content);
}

/* 发送ACK确认 */
static void send_ack(const char *type)
{
    char content[32];
    snprintf(content, sizeof(content), "ACK:%s", type);
    send_frame(content);
}

/* 解析协议帧, 返回1=有效命令, 0=无命令或校验失败 */
uint8_t UART_Bridge_CheckCommand(BridgeCmd_t *cmd)
{
    uint8_t ch;
    while (USART2_ReadByte(&ch) && cmd_idx < sizeof(cmd_buf) - 1)
    {
        if (ch == '\n')
        {
            cmd_buf[cmd_idx] = '\0';
            cmd_idx = 0;

            /* 尝试新格式: $content*XX */
            char *dollar = strchr(cmd_buf, '$');
            if (dollar)
            {
                char *star = strrchr(dollar, '*');
                if (star)
                {
                    /* 提取校验和 */
                    uint8_t recv_cs = (hex_val(star[1]) << 4) | hex_val(star[2]);
                    *star = '\0';
                    char *content = dollar + 1;
                    uint8_t calc_cs = xor_checksum(content, strlen(content));

                    if (recv_cs != calc_cs)
                    {
                        send_ack("CSERR");
                        return 0;
                    }

                    /* 解析命令类型 */
                    return parse_command(content, cmd);
                }
            }

            /* 兼容旧格式(无校验): CMD:xxx */
            if (strncmp(cmd_buf, "CMD:", 4) == 0)
            {
                return parse_command(cmd_buf + 4, cmd);
            }
        }
        else if (ch != '\r')
        {
            cmd_buf[cmd_idx++] = ch;
        }
    }
    return 0;
}

/* 解析命令内容(不带$和校验) */
uint8_t parse_command(const char *content, BridgeCmd_t *cmd)
{
    /* CMD:MODE:AUTO / CMD:MODE:MANUAL */
    if (strncmp(content, "CMD:MODE:", 9) == 0)
    {
        cmd->type = (strstr(content + 9, "AUTO")) ? CMD_MODE_AUTO : CMD_MODE_MANUAL;
        send_ack("MODE");
        return 1;
    }
    /* CMD:PUMP:ON / CMD:PUMP:OFF */
    else if (strncmp(content, "CMD:PUMP:", 9) == 0)
    {
        cmd->type = (strstr(content + 9, "ON")) ? CMD_PUMP_ON : CMD_PUMP_OFF;
        send_ack("PUMP");
        return 1;
    }
    /* CMD:FAN:ON / CMD:FAN:OFF */
    else if (strncmp(content, "CMD:FAN:", 8) == 0)
    {
        cmd->type = (strstr(content + 8, "ON")) ? CMD_FAN_ON : CMD_FAN_OFF;
        send_ack("FAN");
        return 1;
    }
    /* CMD:WDOW:OPEN / CMD:WDOW:CLOSE */
    else if (strncmp(content, "CMD:WDOW:", 9) == 0)
    {
        cmd->type = (strstr(content + 9, "OPEN")) ? CMD_WINDOW_OPEN : CMD_WINDOW_CLOSE;
        send_ack("WDOW");
        return 1;
    }
    /* CMD:THRESH:low:high */
    else if (strncmp(content, "CMD:THRESH:", 11) == 0)
    {
        cmd->type = CMD_SET_THRESHOLD;
        int low = 0, high = 0;
        sscanf(content + 11, "%d:%d", &low, &high);
        cmd->params.threshold.low = (uint8_t)low;
        cmd->params.threshold.high = (uint8_t)high;
        send_ack("THRESH");
        return 1;
    }

    /* 未知命令 */
    return 0;
}
