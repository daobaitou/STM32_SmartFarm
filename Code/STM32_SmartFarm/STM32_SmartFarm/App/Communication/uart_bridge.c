/**
 * @file    uart_bridge.c
 * @author  王国维
 * @date    2026-05-12
 * @brief   UART协议网桥实现 - STM32与ESP8266简单文本协议
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
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
        "SNS:{\"t\":%.1f,\"h\":%.1f,\"sm\":%u,\"st\":%.1f,\"l\":%.0f,\"p\":%.1f,\"f\":%.2f,\"v\":%.2f}\n",
        data->temperature, data->humidity,
        data->soil_moisture, data->soil_temp,
        data->light, data->pressure,
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

            if (strncmp(cmd_buf, "CMD:MODE:", 9) == 0)
            {
                cmd->type = (strstr(cmd_buf + 9, "AUTO")) ? CMD_MODE_AUTO : CMD_MODE_MANUAL;
                cmd_idx = 0;
                return 1;
            }
            else if (strncmp(cmd_buf, "CMD:PUMP:", 9) == 0)
            {
                cmd->type = (strstr(cmd_buf + 9, "ON")) ? CMD_PUMP_ON : CMD_PUMP_OFF;
                cmd_idx = 0;
                return 1;
            }
            else if (strncmp(cmd_buf, "CMD:THRESH:", 11) == 0)
            {
                cmd->type = CMD_SET_THRESHOLD;
                int low = 0, high = 0;
                sscanf(cmd_buf + 11, "%d:%d", &low, &high);
                cmd->params.threshold.low = (uint8_t)low;
                cmd->params.threshold.high = (uint8_t)high;
                cmd_idx = 0;
                return 1;
            }
            /* ACK/NACK/ERR 忽略 */
            cmd_idx = 0;
        }
        else if (ch != '\r')
        {
            cmd_buf[cmd_idx++] = ch;
        }
    }
    return 0;
}
