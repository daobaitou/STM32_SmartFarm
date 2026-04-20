/**
 * @file    app_tasks.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   FreeRTOS task implementation
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "app_tasks.h"

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

        /* 采集所有传感器 */
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
        }

        data.timestamp = xTaskGetTickCount();

        /* 发送到队列（覆盖式，保证最新数据） */
        if (xQueue_SensorData)
        {
            xQueueOverwrite(xQueue_SensorData, &data);
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
            printf("[LCD] Got data\r\n");
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

                OLED_Refresh();
                printf("[LCD] OLED refreshed\r\n");
                xSemaphoreGive(xMutex_I2C);
            }
            else
            {
                printf("[LCD] I2C mutex timeout\r\n");
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
            printf("[Print #%lu] T:%.1f H:%.0f%% Soil:%u%% St:%.1f P:%.0f L:%.0f\r\n",
                   cnt, data.temperature, data.humidity,
                   data.soil_moisture, data.soil_temp,
                   data.pressure, data.light);
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

void FreeRTOS_Init(void)
{
    printf("[RTOS] Creating mutex...\r\n");
    xMutex_I2C = xSemaphoreCreateMutex();
    if (xMutex_I2C)
        printf("[RTOS] Mutex OK\r\n");
    else
        printf("[RTOS] Mutex FAILED\r\n");

    printf("[RTOS] Creating queue...\r\n");
    xQueue_SensorData = xQueueCreate(1, sizeof(SensorData_t));
    if (xQueue_SensorData)
        printf("[RTOS] Queue OK\r\n");
    else
        printf("[RTOS] Queue FAILED\r\n");

    printf("[RTOS] Creating tasks...\r\n");
    BaseType_t ret;

    ret = xTaskCreate(vTask_Sensor, "Sensor", 512, NULL, PRIORITY_SENSOR, NULL);
    printf("[RTOS] Sensor task: %d\r\n", ret);

    ret = xTaskCreate(vTask_LCD, "LCD", 384, NULL, PRIORITY_LCD, NULL);
    printf("[RTOS] LCD task: %d\r\n", ret);

    ret = xTaskCreate(vTask_Print, "Print", 256, NULL, PRIORITY_PRINT, NULL);
    printf("[RTOS] Print task: %d\r\n", ret);

    ret = xTaskCreate(vTask_LED, "LED", 128, NULL, PRIORITY_LED, NULL);
    printf("[RTOS] LED task: %d\r\n", ret);

    printf("[RTOS] All tasks created, starting scheduler...\r\n");
}
