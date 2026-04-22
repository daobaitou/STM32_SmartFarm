/**
 * @file    app_tasks.h
 * @brief   FreeRTOS task definitions and sensor data structure
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Task priorities */
#define PRIORITY_SENSOR     3
#define PRIORITY_LCD        2
#define PRIORITY_PRINT      2
#define PRIORITY_LED        0
#define PRIORITY_MQTT_PUB   2
#define PRIORITY_MQTT_SUB   4

/* Sensor data structure for queue */
typedef struct {
    float temperature;
    float humidity;
    uint8_t soil_moisture;
    uint16_t adc_raw;
    float soil_temp;
    float pressure;
    float bmp_temp;
    float light;
    float flow_rate;
    float total_volume;
    uint32_t timestamp;
} SensorData_t;

/* Global handles */
extern QueueHandle_t xQueue_SensorData;
extern QueueHandle_t xQueue_ControlCmd;
extern SemaphoreHandle_t xMutex_I2C;

/* Function prototypes */
void FreeRTOS_Init(void);
void vTask_Sensor(void *pvParameters);
void vTask_LCD(void *pvParameters);
void vTask_Print(void *pvParameters);
void vTask_LED(void *pvParameters);
void vTask_MQTT_Pub(void *pvParameters);
void vTask_MQTT_Sub(void *pvParameters);

#endif /* APP_TASKS_H */

