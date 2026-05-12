/**
 * @file    app_tasks.c
 * @author  王国维
 * @date    2026-05-12
 * @brief   FreeRTOS task implementation - UART Bridge版
 * @note    ESP8266独立处理MQTT，STM32只通过简单UART协议发送数据/接收命令
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "app_tasks.h"
#include "uart_bridge.h"

#include <stdio.h>
#include "sensor_dht22.h"
#include "sensor_ds18b20.h"
#include "sensor_fc28.h"
#include "sensor_bh1750.h"
#include "sensor_bmp180.h"
#include "sensor_yfs201.h"
#include "display_oled.h"

/* 消息队列 */
QueueHandle_t xQueue_SensorData = NULL;
QueueHandle_t xQueue_ControlCmd = NULL;

/* 互斥量：保护共享资源（OLED、软件I2C） */
SemaphoreHandle_t xMutex_I2C = NULL;

/*-----------------------------------------------------------*/

void vTask_Sensor(void *pvParameters)
{
    printf("[Sensor] Task started\r\n");
    SensorData_t data;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        printf("[Sensor] Reading...\r\n");
        DHT22_Data_t dht;
        DS18B20_Data_t ds;
        FC28_Data_t fc;
        BMP180_Data_t bmp;
        BH1750_Data_t light;
        YFS201_Data_t flow;

        if (DHT22_Read(&dht) == HAL_OK && dht.valid)
        {
            data.temperature = dht.temperature;
            data.humidity = dht.humidity;
            printf("[Sensor] DHT22: %.1fC %.0f%%\r\n", dht.temperature, dht.humidity);
        }
        else
        {
            printf("[Sensor] DHT22 failed\r\n");
        }

        if (DS18B20_Read(&ds) == HAL_OK && ds.valid)
        {
            data.soil_temp = ds.temperature;
            printf("[Sensor] DS18B20: %.1fC\r\n", ds.temperature);
        }

        if (FC28_Read(&fc) == HAL_OK && fc.valid)
        {
            data.soil_moisture = fc.moisture;
            data.adc_raw = fc.adc_value;
            printf("[Sensor] FC28: %u%%\r\n", fc.moisture);
        }

        if (BMP180_Read(&bmp) == HAL_OK && bmp.valid)
        {
            data.pressure = bmp.pressure;
            data.bmp_temp = bmp.temperature;
            printf("[Sensor] BMP180: %.0fhPa\r\n", bmp.pressure);
        }

        if (BH1750_Read(&light) == HAL_OK && light.valid)
        {
            data.light = light.light;
            printf("[Sensor] BH1750: %.0flux\r\n", light.light);
        }

        if (YFS201_Read(&flow) == HAL_OK && flow.valid)
        {
            data.flow_rate = flow.flow_rate;
            data.total_volume = flow.total_volume;
            printf("[Sensor] YFS201: %.2fL/min, Total:%.2fL, Pulses:%lu\r\n", flow.flow_rate, flow.total_volume, flow.pulse_count);
        }
        else
        {
            printf("[Sensor] YFS201: no flow\r\n");
        }

        data.timestamp = xTaskGetTickCount();

        /* 发送到队列（3份：LCD + Print + UART_TX） */
        if (xQueue_SensorData)
        {
            xQueueSend(xQueue_SensorData, &data, 0);
            xQueueSend(xQueue_SensorData, &data, 0);
            xQueueSend(xQueue_SensorData, &data, 0);
            printf("[Sensor] Data sent to queue\r\n");
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
    }
}

/*-----------------------------------------------------------*/

void vTask_LCD(void *pvParameters)
{
    printf("[LCD] Task started\r\n");
    SensorData_t data;
    char buf[20];

    for (;;)
    {
        if (xQueue_SensorData &&
            xQueueReceive(xQueue_SensorData, &data, pdMS_TO_TICKS(1000)) == pdPASS)
        {
            if (xSemaphoreTake(xMutex_I2C, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                OLED_Clear();
                OLED_DrawString(0, 0, "SmartFarm", FONT_SMALL);

                snprintf(buf, sizeof(buf), "T:%.1fC H:%.0f%%",
                         data.temperature, data.humidity);
                OLED_DrawString(0, 10, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "Soil:%u%%", data.soil_moisture);
                OLED_DrawString(0, 20, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "P:%.0fhPa", data.pressure);
                OLED_DrawString(64, 20, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "L:%.0flux", data.light);
                OLED_DrawString(0, 30, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "St:%.1fC", data.soil_temp);
                OLED_DrawString(64, 30, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "F:%.1fL V:%.1fL",
                         data.flow_rate, data.total_volume);
                OLED_DrawString(0, 40, buf, FONT_SMALL);

                OLED_Refresh();
                xSemaphoreGive(xMutex_I2C);
            }
        }
    }
}

/*-----------------------------------------------------------*/

void vTask_Print(void *pvParameters)
{
    printf("[Print] Task started\r\n");
    SensorData_t data;
    uint32_t cnt = 0;

    for (;;)
    {
        if (xQueue_SensorData &&
            xQueueReceive(xQueue_SensorData, &data, pdMS_TO_TICKS(2000)) == pdPASS)
        {
            cnt++;
            printf("[Print #%lu] T:%.1f H:%.0f%% Soil:%u%% St:%.1f P:%.0f L:%.0f F:%.2f V:%.2f\r\n",
                   cnt, data.temperature, data.humidity,
                   data.soil_moisture, data.soil_temp,
                   data.pressure, data.light,
                   data.flow_rate, data.total_volume);
        }
    }
}

/*-----------------------------------------------------------*/

void vTask_LED(void *pvParameters)
{
    printf("[LED] Task started\r\n");
    uint32_t count = 0;

    for (;;)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        count++;
        printf("[LED] Toggle #%lu\r\n", count);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*-----------------------------------------------------------*/

void vTask_UART_TX(void *pvParameters)
{
    printf("[UART_TX] Task started\r\n");
    SensorData_t data;

    for (;;)
    {
        if (xQueue_SensorData &&
            xQueueReceive(xQueue_SensorData, &data, pdMS_TO_TICKS(5000)) == pdPASS)
        {
            UART_Bridge_SendSensorData(&data);
            printf("[UART_TX] Sent sensor data to ESP8266\r\n");
        }
    }
}

/*-----------------------------------------------------------*/

void vTask_UART_RX(void *pvParameters)
{
    printf("[UART_RX] Task started\r\n");
    BridgeCmd_t cmd;

    for (;;)
    {
        if (UART_Bridge_CheckCommand(&cmd))
        {
            printf("[UART_RX] Cmd: %d\r\n", cmd.type);

            ControlCmd_t ctrl;
            switch (cmd.type)
            {
            case CMD_MODE_AUTO:    ctrl.type = CTRL_MODE_AUTO; break;
            case CMD_MODE_MANUAL:  ctrl.type = CTRL_MODE_MANUAL; break;
            case CMD_PUMP_ON:      ctrl.type = CTRL_PUMP_ON; break;
            case CMD_PUMP_OFF:     ctrl.type = CTRL_PUMP_OFF; break;
            case CMD_SET_THRESHOLD:
                ctrl.type = CTRL_SET_THRESHOLD;
                ctrl.params.threshold.low = cmd.params.threshold.low;
                ctrl.params.threshold.high = cmd.params.threshold.high;
                break;
            default: continue;
            }

            if (xQueue_ControlCmd)
                xQueueSend(xQueue_ControlCmd, &ctrl, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/*-----------------------------------------------------------*/

void FreeRTOS_Init(void)
{
    printf("[RTOS] Creating mutex...\r\n");
    xMutex_I2C = xSemaphoreCreateMutex();

    printf("[RTOS] Creating queues...\r\n");
    xQueue_SensorData = xQueueCreate(3, sizeof(SensorData_t));
    xQueue_ControlCmd = xQueueCreate(4, sizeof(ControlCmd_t));

    printf("[RTOS] Creating tasks...\r\n");
    BaseType_t ret;

    ret = xTaskCreate(vTask_Sensor,   "Sensor",   512, NULL, PRIORITY_SENSOR,  NULL);
    ret = xTaskCreate(vTask_LCD,      "LCD",      384, NULL, PRIORITY_LCD,     NULL);
    ret = xTaskCreate(vTask_Print,    "Print",    192, NULL, PRIORITY_PRINT,   NULL);
    ret = xTaskCreate(vTask_LED,      "LED",      128, NULL, PRIORITY_LED,     NULL);
    ret = xTaskCreate(vTask_UART_TX,  "UART_TX",  256, NULL, PRIORITY_UART_TX, NULL);
    ret = xTaskCreate(vTask_UART_RX,  "UART_RX",  128, NULL, PRIORITY_UART_RX, NULL);

    printf("[RTOS] All tasks created, starting scheduler...\r\n");
}
