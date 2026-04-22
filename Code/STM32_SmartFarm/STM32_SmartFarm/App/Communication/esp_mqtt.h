/**
 * @file    esp_mqtt.h
 * @author  王国维
 * @date    2026-04-21
 * @brief   MQTT高层接口（JSON格式化、命令解析、主题管理）
 */

#ifndef ESP_MQTT_H
#define ESP_MQTT_H

#include "main.h"
#include "app_tasks.h"
#include <stdint.h>

#define DEVICE_ID  "farm_001"

/* Topic字符串 */
extern char g_topic_sensor[64];
extern char g_topic_ctrl_mode[64];
extern char g_topic_ctrl_pump[64];
extern char g_topic_cfg_threshold[64];

/* 控制命令类型 */
typedef enum {
    CTRL_MODE_AUTO = 0,
    CTRL_MODE_MANUAL,
    CTRL_PUMP_ON,
    CTRL_PUMP_OFF,
    CTRL_SET_THRESHOLD,
    CTRL_UNKNOWN
} CtrlCmdType_t;

/* 控制命令结构 */
typedef struct {
    CtrlCmdType_t type;
    union {
        uint8_t mode;
        uint8_t pump_state;
        struct {
            float low;
            float high;
        } threshold;
    } params;
} ControlCmd_t;

/* 初始化MQTT子系统（连接WiFi + Broker + 订阅） */
HAL_StatusTypeDef MQTT_Init(const char *ssid, const char *password,
                            const char *broker, uint16_t port);

/* 格式化传感器数据为JSON并发布 */
HAL_StatusTypeDef MQTT_PublishSensorData(const SensorData_t *data);

/* 检查下行命令 */
BaseType_t MQTT_CheckIncoming(ControlCmd_t *cmd);

/* 重连 */
HAL_StatusTypeDef MQTT_Reconnect(void);

/* 连接状态查询 */
uint8_t MQTT_IsConnected(void);

/* 心跳保活 */
void MQTT_KeepAlive(void);

#endif
