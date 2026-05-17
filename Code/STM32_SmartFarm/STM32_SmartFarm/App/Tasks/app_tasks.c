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
#include <string.h>
#include "sensor_dht22.h"
#include "sensor_ds18b20.h"
#include "sensor_fc28.h"
#include "sensor_bh1750.h"
#include "sensor_bmp180.h"
#include "sensor_yfs201.h"
#include "sensor_mhz19b.h"
#include "display_oled.h"
#include "pump_control.h"
#include "alarm_driver.h"
#include "servo_control.h"
#include "fan_control.h"
#include "power_detect.h"

/* 消息队列 */
QueueHandle_t xQueue_SensorData = NULL;
QueueHandle_t xQueue_ControlCmd = NULL;

/* 互斥量：保护共享资源（OLED、软件I2C） */
SemaphoreHandle_t xMutex_I2C = NULL;

/* 灌溉控制全局状态 */
static uint8_t irrigation_mode = 0;   /* 0=自动, 1=手动 */
static uint8_t pump_on = 0;
static float threshold_low = 30.0f;
static float threshold_high = 70.0f;
static volatile uint8_t last_soil = 0;

/* 环境调节全局状态 */
static uint8_t fan_on = 0;
static uint8_t window_open = 0;
static float temp_threshold_high = 35.0f;
static float humidity_threshold_high = 80.0f;
static volatile float last_temp = 0;
static volatile float last_humidity = 0;

/* 电源状态 */
static uint8_t power_backup = 0;

/* 轻量串口输出，不用printf（ARMCC printf栈消耗过大导致Irrigation任务崩溃） */
static void debug_print(const char *s)
{
    while (*s) {
        while(!(USART1->SR & USART_SR_TXE));
        USART1->DR = *s++;
    }
}

/*-----------------------------------------------------------*/

void vTask_Sensor(void *pvParameters)
{
    printf("[Sensor] Task started\r\n");
    SensorData_t data;
    memset(&data, 0, sizeof(data));
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t cycle = 0;

    for (;;)
    {
        printf("[Sensor] Reading...\r\n");
        DHT22_Data_t dht;
        FC28_Data_t fc;
        YFS201_Data_t flow;

        /* 每个周期都读：DHT22(温湿度影响风扇/窗户/灌溉控制) + FC28(土壤湿度影响灌溉) */
        if (DHT22_Read(&dht) == HAL_OK && dht.valid)
        {
            data.temperature = dht.temperature;
            data.humidity = dht.humidity;
            last_temp = dht.temperature;
            last_humidity = dht.humidity;
            printf("[Sensor] DHT22: %.1fC %.0f%%\r\n", dht.temperature, dht.humidity);
        }
        else
        {
            printf("[Sensor] DHT22 failed\r\n");
        }

        if (FC28_Read(&fc) == HAL_OK && fc.valid)
        {
            data.soil_moisture = fc.moisture;
            data.adc_raw = fc.adc_value;
            last_soil = fc.moisture;
            printf("[Sensor] FC28: %u%% ADC:%u\r\n", fc.moisture, fc.adc_value);
        }

        if (YFS201_Read(&flow) == HAL_OK && flow.valid)
        {
            data.flow_rate = flow.flow_rate;
            data.total_volume = flow.total_volume;
            printf("[Sensor] YFS201: %.2fL/min Total:%.2fL\r\n", flow.flow_rate, flow.total_volume);
        }

        /* 每2个周期(6s)读：DS18B20 + BH1750 (变化较慢) */
        if (cycle % 2 == 0)
        {
            DS18B20_Data_t ds;
            if (DS18B20_Read(&ds) == HAL_OK && ds.valid)
            {
                data.soil_temp = ds.temperature;
                printf("[Sensor] DS18B20: %.1fC\r\n", ds.temperature);
            }

            BH1750_Data_t light;
            if (BH1750_Read(&light) == HAL_OK && light.valid)
            {
                data.light = light.light;
                printf("[Sensor] BH1750: %.0flux\r\n", light.light);
            }
        }

        /* 每6个周期(18s)读：BMP180 (大气压变化很慢) */
        if (cycle % 6 == 3)
        {
            BMP180_Data_t bmp;
            if (BMP180_Read(&bmp) == HAL_OK && bmp.valid)
            {
                data.pressure = bmp.pressure;
                data.bmp_temp = bmp.temperature;
                printf("[Sensor] BMP180: %.0fhPa\r\n", bmp.pressure);
            }
        }

        /* 每10个周期(30s)读：MH-Z19B (CO2变化慢，预热3分钟) */
        if (cycle % 10 == 5)
        {
            MHZ19B_Data_t mhz;
            if (MHZ19B_Read(&mhz) == HAL_OK && mhz.valid) {
                data.co2 = mhz.co2;
                printf("[Sensor] MHZ19B: CO2=%uppm\r\n", mhz.co2);
            }
        }

        cycle++;

        data.timestamp = xTaskGetTickCount();

        /* 发送到队列（3份：LCD + Print + UART_TX） */
        if (xQueue_SensorData)
        {
            xQueueSend(xQueue_SensorData, &data, 0);
            xQueueSend(xQueue_SensorData, &data, 0);
            xQueueSend(xQueue_SensorData, &data, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(3000));
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

                snprintf(buf, sizeof(buf), "S:%u%% P:%.0fPa",
                         data.soil_moisture, data.pressure);
                OLED_DrawString(0, 20, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "L:%.0f St:%.1f",
                         data.light, data.soil_temp);
                OLED_DrawString(0, 30, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "C:%u F:%.1f/%.1fL",
                         data.co2, data.flow_rate, data.total_volume);
                OLED_DrawString(0, 40, buf, FONT_SMALL);

                snprintf(buf, sizeof(buf), "%s %s P:%s F:%s W:%s",
                         PowerDetect_IsBackup() ? "BAT" : "AC",
                         irrigation_mode ? "MAN" : "AUTO",
                         pump_on ? "ON" : "OFF",
                         fan_on ? "ON" : "OFF",
                         window_open ? "OP" : "CL");
                OLED_DrawString(0, 50, buf, FONT_SMALL);

                if (Alarm_IsActive())
                    OLED_DrawString(120, 50, "!", FONT_SMALL);

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
            printf("[Print #%lu] T:%d H:%d%% Soil:%u%% St:%d P:%d L:%d CO2:%u F:%d V:%d\r\n",
                   cnt, (int)data.temperature, (int)data.humidity,
                   data.soil_moisture, (int)data.soil_temp,
                   (int)data.pressure, (int)data.light, data.co2,
                   (int)data.flow_rate, (int)data.total_volume);
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
    printf("[UART_RX] Task started @115200\r\n");
    BridgeCmd_t cmd;
    uint32_t last_dbg_time = 0;

    for (;;)
    {
        if (UART_Bridge_CheckCommand(&cmd))
        {
            printf("[UART_RX] Cmd received: %d\r\n", cmd.type);

            ControlCmd_t ctrl;
            switch (cmd.type)
            {
            case CMD_MODE_AUTO:    ctrl.type = CTRL_MODE_AUTO; break;
            case CMD_MODE_MANUAL:  ctrl.type = CTRL_MODE_MANUAL; break;
            case CMD_PUMP_ON:      ctrl.type = CTRL_PUMP_ON; break;
            case CMD_PUMP_OFF:     ctrl.type = CTRL_PUMP_OFF; break;
            case CMD_FAN_ON:       ctrl.type = CTRL_FAN_ON; break;
            case CMD_FAN_OFF:      ctrl.type = CTRL_FAN_OFF; break;
            case CMD_WINDOW_OPEN:  ctrl.type = CTRL_WINDOW_OPEN; break;
            case CMD_WINDOW_CLOSE:  ctrl.type = CTRL_WINDOW_CLOSE; break;
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

        /* 每5秒打印接收字节计数 */
        uint32_t now = xTaskGetTickCount();
        if (now - last_dbg_time > pdMS_TO_TICKS(5000))
        {
            last_dbg_time = now;
            uint16_t rx_cnt = UART_Bridge_GetRxCount();
            printf("[UART_RX] Bytes received: %u\r\n", rx_cnt);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/*-----------------------------------------------------------*/

static uint8_t button_debounce(GPIO_TypeDef *port, uint16_t pin, uint8_t idx)
{
    static uint8_t last_state[4] = {1, 1, 1, 1};
    static uint32_t last_time[4] = {0, 0, 0, 0};
    static uint8_t fired[4] = {0, 0, 0, 0};

    uint8_t current = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 0 : 1;
    uint32_t now = HAL_GetTick();

    if (current != last_state[idx]) {
        last_state[idx] = current;
        last_time[idx] = now;
        fired[idx] = 0;
    } else if (current == 0 && !fired[idx] && (now - last_time[idx] > 50)) {
        fired[idx] = 1;
        return 1;
    }
    return 0;
}

void vTask_Irrigation(void *pvParameters)
{
    /* 直接寄存器输出（不用printf，ARMCC printf栈消耗过大） */
    const char *startmsg = "[Irrigation] started\r\n";
    while (*startmsg) {
        while(!(USART1->SR & USART_SR_TXE));
        USART1->DR = *startmsg++;
    }

    /* 确保蜂鸣器初始静音 */
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);

    ControlCmd_t cmd;

    for (;;)
    {
        /* 断电检测 */
        if (PowerDetect_IsBackup() && !power_backup)
        {
            power_backup = 1;
            debug_print("[POWER] BACKUP mode\r\n");
            Fan_Off(); fan_on = 0;
            Servo_Close(); window_open = 0;
            Alarm_Beep(500);
        }
        if (!PowerDetect_IsBackup() && power_backup)
        {
            power_backup = 0;
            debug_print("[POWER] MAIN restored\r\n");
            Alarm_Click();
        }

        /* 处理按钮输入 */
        if (button_debounce(KEY1_GPIO_Port, KEY1_Pin, 0)) {
            irrigation_mode = !irrigation_mode;
            Alarm_Click();
            debug_print(irrigation_mode ? "[BTN1] MANUAL\r\n" : "[BTN1] AUTO\r\n");
        }
        if (button_debounce(KEY2_GPIO_Port, KEY2_Pin, 1)) {
            if (pump_on) { Pump_Off(); pump_on = 0; }
            else { Pump_On(); pump_on = 1; }
            Alarm_Click();
            debug_print(pump_on ? "[BTN2] Pump ON\r\n" : "[BTN2] Pump OFF\r\n");
        }
        if (button_debounce(KEY3_GPIO_Port, KEY3_Pin, 2)) {
            if (Alarm_IsActive()) {
                Alarm_Clear();
                debug_print("[BTN3] Alarm cleared\r\n");
            } else {
                Alarm_Click();
                debug_print("[BTN3] No alarm\r\n");
            }
        }
        if (button_debounce(KEY4_GPIO_Port, KEY4_Pin, 3)) {
            if (fan_on || window_open) {
                Fan_Off(); fan_on = 0;
                Servo_Close(); window_open = 0;
            } else {
                Fan_On(); fan_on = 1;
                Servo_Open(); window_open = 1;
            }
            Alarm_Click();
            debug_print((fan_on || window_open) ? "[BTN4] Fan+Window ON\r\n" : "[BTN4] Fan+Window OFF\r\n");
        }

        /* 处理MQTT/远程控制命令 */
        if (xQueue_ControlCmd &&
            xQueueReceive(xQueue_ControlCmd, &cmd, pdMS_TO_TICKS(100)) == pdPASS)
        {
            debug_print("[MQTT] Executing command\r\n");
            switch (cmd.type) {
            case CTRL_MODE_AUTO:    irrigation_mode = 0;
                debug_print("[MQTT] Mode=AUTO\r\n"); break;
            case CTRL_MODE_MANUAL:  irrigation_mode = 1;
                debug_print("[MQTT] Mode=MANUAL\r\n"); break;
            case CTRL_PUMP_ON:      irrigation_mode = 1; Pump_On(); pump_on = 1;
                debug_print("[MQTT] Pump=ON\r\n"); break;
            case CTRL_PUMP_OFF:     irrigation_mode = 1; Pump_Off(); pump_on = 0;
                debug_print("[MQTT] Pump=OFF\r\n"); break;
            case CTRL_FAN_ON:       irrigation_mode = 1; Fan_On(); fan_on = 1;
                debug_print("[MQTT] Fan=ON\r\n"); break;
            case CTRL_FAN_OFF:      irrigation_mode = 1; Fan_Off(); fan_on = 0;
                debug_print("[MQTT] Fan=OFF\r\n"); break;
            case CTRL_WINDOW_OPEN:  irrigation_mode = 1; Servo_Open(); window_open = 1;
                debug_print("[MQTT] Window=OPEN\r\n"); break;
            case CTRL_WINDOW_CLOSE: irrigation_mode = 1; Servo_Close(); window_open = 0;
                debug_print("[MQTT] Window=CLOSE\r\n"); break;
            case CTRL_SET_THRESHOLD:
                threshold_low = cmd.params.threshold.low;
                threshold_high = cmd.params.threshold.high;
                debug_print("[MQTT] Threshold set\r\n"); break;
            default: break;
            }
        }

        /* 自动灌溉逻辑 */
        if (irrigation_mode == 0 && last_soil > 0) {
            if (last_soil < threshold_low && !pump_on) {
                Pump_On(); pump_on = 1;
            } else if (last_soil > threshold_high && pump_on) {
                Pump_Off(); pump_on = 0;
            }
        }

        /* 报警判断 */
        if (last_soil > 0 && last_soil < threshold_low && !Alarm_IsActive()) {
            Alarm_Trigger(ALARM_SOIL_DRY);
        } else if (last_soil > threshold_high && Alarm_IsActive() && !pump_on) {
            Alarm_Clear();
        }

        /* 自动风扇/窗户逻辑 */
        if (irrigation_mode == 0 && last_temp > 0) {
            if (last_temp > temp_threshold_high && !fan_on) {
                Fan_On(); fan_on = 1;
            } else if (last_temp < temp_threshold_high - 3.0f && fan_on) {
                Fan_Off(); fan_on = 0;
            }
            if (last_humidity > humidity_threshold_high && !window_open) {
                Servo_Open(); window_open = 1;
            } else if (last_humidity < humidity_threshold_high - 5.0f && window_open) {
                Servo_Close(); window_open = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/*-----------------------------------------------------------*/

void FreeRTOS_Init(void)
{
    BaseType_t ret;

    printf("[RTOS] Heap free: %u bytes\r\n", (unsigned int)xPortGetFreeHeapSize());

    printf("[RTOS] Creating mutex...\r\n");
    xMutex_I2C = xSemaphoreCreateMutex();

    printf("[RTOS] Creating queues...\r\n");
    xQueue_SensorData = xQueueCreate(3, sizeof(SensorData_t));
    xQueue_ControlCmd = xQueueCreate(4, sizeof(ControlCmd_t));

    printf("[RTOS] Creating tasks...\r\n");

    Pump_Init();
    Alarm_Init();

    ret = xTaskCreate(vTask_Sensor,     "Sensor",     512, NULL, PRIORITY_SENSOR,      NULL);
    printf("[RTOS] Sensor: %s (free=%u)\r\n", ret==pdPASS?"OK":"FAIL", (unsigned int)xPortGetFreeHeapSize());

    ret = xTaskCreate(vTask_Irrigation, "Irrigation", 384, NULL, PRIORITY_IRRIGATION,  NULL);
    printf("[RTOS] Irrigation: %s (free=%u)\r\n", ret==pdPASS?"OK":"FAIL", (unsigned int)xPortGetFreeHeapSize());

    ret = xTaskCreate(vTask_LCD,        "LCD",        384, NULL, PRIORITY_LCD,         NULL);
    printf("[RTOS] LCD: %s (free=%u)\r\n", ret==pdPASS?"OK":"FAIL", (unsigned int)xPortGetFreeHeapSize());

    ret = xTaskCreate(vTask_Print,      "Print",      256, NULL, PRIORITY_PRINT,       NULL);
    printf("[RTOS] Print: %s (free=%u)\r\n", ret==pdPASS?"OK":"FAIL", (unsigned int)xPortGetFreeHeapSize());

    ret = xTaskCreate(vTask_LED,        "LED",        128, NULL, PRIORITY_LED,         NULL);
    printf("[RTOS] LED: %s (free=%u)\r\n", ret==pdPASS?"OK":"FAIL", (unsigned int)xPortGetFreeHeapSize());

    ret = xTaskCreate(vTask_UART_TX,    "UART_TX",    512, NULL, PRIORITY_UART_TX,     NULL);
    printf("[RTOS] UART_TX: %s (free=%u)\r\n", ret==pdPASS?"OK":"FAIL", (unsigned int)xPortGetFreeHeapSize());

    ret = xTaskCreate(vTask_UART_RX,    "UART_RX",    256, NULL, PRIORITY_UART_RX,     NULL);
    printf("[RTOS] UART_RX: %s (free=%u)\r\n", ret==pdPASS?"OK":"FAIL", (unsigned int)xPortGetFreeHeapSize());

    printf("[RTOS] All tasks created, starting scheduler...\r\n");
}
