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
#define PRIORITY_IRRIGATION 6
#define PRIORITY_SENSOR     3
#define PRIORITY_LCD        2
#define PRIORITY_PRINT      2
#define PRIORITY_LED        0
#define PRIORITY_UART_TX    2
#define PRIORITY_UART_RX    4

/* Control command types */
typedef enum {
    CTRL_MODE_AUTO,
    CTRL_MODE_MANUAL,
    CTRL_PUMP_ON,
    CTRL_PUMP_OFF,
    CTRL_SET_THRESHOLD,
    CTRL_FAN_ON,
    CTRL_FAN_OFF,
    CTRL_WINDOW_OPEN,
    CTRL_WINDOW_CLOSE
} CtrlCmdType_t;

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
    uint16_t co2;
    uint32_t timestamp;
} SensorData_t;

/* Control command structure */
typedef struct {
    CtrlCmdType_t type;
    union {
        uint8_t mode;
        uint8_t pump_state;
        struct { float low; float high; } threshold;
    } params;
} ControlCmd_t;

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
void vTask_UART_TX(void *pvParameters);
void vTask_UART_RX(void *pvParameters);
void vTask_Irrigation(void *pvParameters);

#endif /* APP_TASKS_H */
