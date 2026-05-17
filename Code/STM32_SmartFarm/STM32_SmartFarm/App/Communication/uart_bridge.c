/**
 * @file    uart_bridge.c
 * @author  王国维
 * @date    2026-05-17
 * @brief   UART协议网桥 v2.0 - 带校验和的帧协议(栈优化版)
 * @note    帧格式: $content*XX\n  (XX=XOR校验)
 *          所有帧格式化在一个buffer内完成，避免嵌套调用栈溢出
 */

#include "uart_bridge.h"
#include "usart2_driver.h"
#include <stdio.h>
#include <string.h>

static char cmd_buf[96];
static uint8_t cmd_idx = 0;

/* XOR校验和 */
static uint8_t xor_cs(const char *s)
{
    uint8_t cs = 0;
    while (*s) cs ^= (uint8_t)*s++;
    return cs;
}

static uint8_t hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

/* 内联帧发送: 在buf中组装 $content*XX\n 并发送 */
static void send_inline(const char *content)
{
    /* 直接操作USART发送，不分配额外buffer */
    USART2_SendData((uint8_t *)"$", 1);
    USART2_SendData((uint8_t *)content, strlen(content));

    uint8_t cs = xor_cs(content);
    char tail[5];
    tail[0] = '*';
    tail[1] = "0123456789ABCDEF"[cs >> 4];
    tail[2] = "0123456789ABCDEF"[cs & 0x0F];
    tail[3] = '\n';
    USART2_SendData((uint8_t *)tail, 4);
}

void UART_Bridge_Init(void)
{
    USART2_Flush();
    cmd_idx = 0;
}

void UART_Bridge_SendSensorData(const SensorData_t *data)
{
    /* 单buffer完成: $SNS:{json}*XX\n */
    char buf[180];
    int len = snprintf(buf, sizeof(buf),
        "SNS:{\"t\":%.1f,\"h\":%.1f,\"sm\":%u,\"st\":%.1f,\"l\":%.0f,\"p\":%.1f,\"c\":%u,\"f\":%.2f,\"v\":%.2f}",
        data->temperature, data->humidity,
        data->soil_moisture, data->soil_temp,
        data->light, data->pressure, data->co2,
        data->flow_rate, data->total_volume);

    if (len > 0 && len < (int)sizeof(buf) - 5)
    {
        /* 在buf后面追加校验 */
        uint8_t cs = xor_cs(buf);
        buf[len] = '*';
        buf[len+1] = "0123456789ABCDEF"[cs >> 4];
        buf[len+2] = "0123456789ABCDEF"[cs & 0x0F];
        buf[len+3] = '\n';
        buf[len+4] = '\0';
        USART2_SendData((uint8_t *)buf, len + 4);
    }
}

/* 发送ACK: 不用snprintf，直接拼接发送 */
static void send_ack(const char *type)
{
    char buf[20];
    buf[0] = 'A'; buf[1] = 'C'; buf[2] = 'K'; buf[3] = ':';
    int i = 4;
    while (*type && i < 16) buf[i++] = *type++;
    buf[i] = '\0';
    send_inline(buf);
}

uint8_t UART_Bridge_CheckCommand(BridgeCmd_t *cmd)
{
    uint8_t ch;
    while (USART2_ReadByte(&ch) && cmd_idx < sizeof(cmd_buf) - 1)
    {
        if (ch == '\n')
        {
            cmd_buf[cmd_idx] = '\0';
            cmd_idx = 0;

            /* 新格式: $content*XX */
            char *dollar = strchr(cmd_buf, '$');
            if (dollar)
            {
                char *star = strrchr(dollar, '*');
                if (star)
                {
                    uint8_t recv_cs = (hex_val(star[1]) << 4) | hex_val(star[2]);
                    *star = '\0';
                    char *content = dollar + 1;
                    uint8_t calc_cs = xor_cs(content);

                    if (recv_cs != calc_cs)
                    {
                        send_ack("CSERR");
                        return 0;
                    }

                    return parse_command(content, cmd);
                }
            }

            /* 兼容旧格式: CMD:xxx */
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

uint8_t parse_command(const char *content, BridgeCmd_t *cmd)
{
    if (strncmp(content, "CMD:MODE:", 9) == 0)
    {
        cmd->type = (strstr(content + 9, "AUTO")) ? CMD_MODE_AUTO : CMD_MODE_MANUAL;
        send_ack("MODE");
        return 1;
    }
    else if (strncmp(content, "CMD:PUMP:", 9) == 0)
    {
        cmd->type = (strstr(content + 9, "ON")) ? CMD_PUMP_ON : CMD_PUMP_OFF;
        send_ack("PUMP");
        return 1;
    }
    else if (strncmp(content, "CMD:FAN:", 8) == 0)
    {
        cmd->type = (strstr(content + 8, "ON")) ? CMD_FAN_ON : CMD_FAN_OFF;
        send_ack("FAN");
        return 1;
    }
    else if (strncmp(content, "CMD:WDOW:", 9) == 0)
    {
        cmd->type = (strstr(content + 9, "OPEN")) ? CMD_WINDOW_OPEN : CMD_WINDOW_CLOSE;
        send_ack("WDOW");
        return 1;
    }
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

    return 0;
}
