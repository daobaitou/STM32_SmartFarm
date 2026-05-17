/**
 * @file    uart_bridge.c
 * @author  王国维
 * @date    2026-05-17
 * @brief   UART协议网桥 - 简化版(无校验，避免栈溢出)
 * @note    STM32发送 SNS:{json}\n，ESP8266发布到MQTT
 *          ESP8266发送 CMD:xxx\n，STM32解析为控制命令
 */

#include "uart_bridge.h"
#include "usart2_driver.h"
#include <stdio.h>
#include <string.h>

static char cmd_buf[64];
static uint8_t cmd_idx = 0;

void UART_Bridge_Init(void)
{
    USART2_Flush();
    cmd_idx = 0;
}

void UART_Bridge_SendSensorData(const SensorData_t *data)
{
    /* 单buffer格式化发送 */
    char buf[160];
    int len = snprintf(buf, sizeof(buf),
        "SNS:{\"t\":%.1f,\"h\":%.1f,\"sm\":%u,\"st\":%.1f,\"l\":%.0f,\"p\":%.1f,\"c\":%u,\"f\":%.2f,\"v\":%.2f}\n",
        data->temperature, data->humidity,
        data->soil_moisture, data->soil_temp,
        data->light, data->pressure, data->co2,
        data->flow_rate, data->total_volume);

    if (len > 0 && len < (int)sizeof(buf))
        USART2_SendData((uint8_t *)buf, len);
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

            /* 简单格式: CMD:MODE:AUTO 等 */
            if (strncmp(cmd_buf, "CMD:MODE:", 9) == 0)
            {
                cmd->type = (strstr(cmd_buf + 9, "AUTO")) ? CMD_MODE_AUTO : CMD_MODE_MANUAL;
                return 1;
            }
            else if (strncmp(cmd_buf, "CMD:PUMP:", 9) == 0)
            {
                cmd->type = (strstr(cmd_buf + 9, "ON")) ? CMD_PUMP_ON : CMD_PUMP_OFF;
                return 1;
            }
            else if (strncmp(cmd_buf, "CMD:FAN:", 8) == 0)
            {
                cmd->type = (strstr(cmd_buf + 8, "ON")) ? CMD_FAN_ON : CMD_FAN_OFF;
                return 1;
            }
            else if (strncmp(cmd_buf, "CMD:WDOW:", 9) == 0)
            {
                cmd->type = (strstr(cmd_buf + 9, "OPEN")) ? CMD_WINDOW_OPEN : CMD_WINDOW_CLOSE;
                return 1;
            }
            else if (strncmp(cmd_buf, "CMD:THRESH:", 11) == 0)
            {
                cmd->type = CMD_SET_THRESHOLD;
                int low = 0, high = 0;
                sscanf(cmd_buf + 11, "%d:%d", &low, &high);
                cmd->params.threshold.low = (uint8_t)low;
                cmd->params.threshold.high = (uint8_t)high;
                return 1;
            }
        }
        else if (ch != '\r')
        {
            cmd_buf[cmd_idx++] = ch;
        }
    }
    return 0;
}