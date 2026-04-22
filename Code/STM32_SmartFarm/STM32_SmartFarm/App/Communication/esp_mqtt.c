/**
 * @file    esp_mqtt.c
 * @author  王国维
 * @date    2026-04-22
 * @brief   MQTT高层接口实现 - 原始TCP + MQTT 3.1.1协议
 * @note    手动编码MQTT CONNECT/PUBLISH/SUBSCRIBE/PINGREQ包
 *          通过ESP8266 TCP连接发送，解析+IPD接收的MQTT响应
 */

#include "esp_mqtt.h"
#include "esp8266_at.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Topic字符串 */
char g_topic_sensor[64];
char g_topic_ctrl_mode[64];
char g_topic_ctrl_pump[64];
char g_topic_cfg_threshold[64];

/* 配置 */
static char wifi_ssid[32];
static char wifi_password[64];
static char mqtt_broker[64];
static uint16_t mqtt_port = 1883;

/* MQTT状态 */
static uint8_t mqtt_connected = 0;
static uint16_t mqtt_pkt_id = 1;

/* MQTT编码缓冲区 */
static uint8_t mqtt_tx_buf[384];
static uint8_t mqtt_rx_buf[256];

/* ---- MQTT 3.1.1 协议编码 ---- */

/* 编码剩余长度字段 */
static uint16_t encode_remaining_len(uint8_t *buf, uint32_t len)
{
    uint16_t idx = 0;
    do {
        uint8_t byte = len & 0x7F;
        len >>= 7;
        if (len > 0) byte |= 0x80;
        buf[idx++] = byte;
    } while (len > 0);
    return idx;
}

/* 编码UTF-8字符串（长度前缀） */
static uint16_t encode_utf8_string(uint8_t *buf, const char *str)
{
    uint16_t len = strlen(str);
    buf[0] = (len >> 8) & 0xFF;
    buf[1] = len & 0xFF;
    memcpy(buf + 2, str, len);
    return len + 2;
}

/* 构造MQTT CONNECT包 */
static uint16_t mqtt_build_connect(uint8_t *buf, const char *client_id)
{
    uint16_t idx = 0;

    /* Variable header */
    idx += encode_utf8_string(buf + idx, "MQTT");    /* Protocol Name */
    buf[idx++] = 0x04;                                 /* Protocol Level 3.1.1 */
    buf[idx++] = 0x02;                                 /* Connect Flags: Clean Session */
    buf[idx++] = 0x00; buf[idx++] = 0x3C;             /* Keep Alive: 60s */

    /* Payload: Client ID */
    idx += encode_utf8_string(buf + idx, client_id);

    /* Fixed header */
    uint8_t rem_buf[4];
    uint16_t rem_len = encode_remaining_len(rem_buf, idx);

    memmove(buf + 1 + rem_len, buf, idx);
    buf[0] = 0x10; /* CONNECT */
    memcpy(buf + 1, rem_buf, rem_len);

    return 1 + rem_len + idx;
}

/* 构造MQTT PUBLISH包 */
static uint16_t mqtt_build_publish(uint8_t *buf, const char *topic,
                                    const uint8_t *payload, uint16_t payload_len,
                                    uint8_t qos)
{
    uint16_t idx = 0;

    /* Variable header: Topic */
    idx += encode_utf8_string(buf + idx, topic);

    /* Packet ID (QoS > 0) */
    if (qos > 0)
    {
        buf[idx++] = (mqtt_pkt_id >> 8) & 0xFF;
        buf[idx++] = mqtt_pkt_id & 0xFF;
        mqtt_pkt_id++;
    }

    /* Payload */
    memcpy(buf + idx, payload, payload_len);
    idx += payload_len;

    /* Fixed header */
    uint8_t rem_buf[4];
    uint16_t rem_len = encode_remaining_len(rem_buf, idx);
    uint8_t flags = 0x30; /* PUBLISH, QoS0 */
    if (qos == 1) flags = 0x32;
    if (qos == 2) flags = 0x34;

    memmove(buf + 1 + rem_len, buf, idx);
    buf[0] = flags;
    memcpy(buf + 1, rem_buf, rem_len);

    return 1 + rem_len + idx;
}

/* 构造MQTT SUBSCRIBE包 */
static uint16_t mqtt_build_subscribe(uint8_t *buf, const char *topic, uint8_t qos)
{
    uint16_t idx = 0;

    /* Variable header: Packet ID */
    buf[idx++] = (mqtt_pkt_id >> 8) & 0xFF;
    buf[idx++] = mqtt_pkt_id & 0xFF;
    uint16_t sub_pkt_id = mqtt_pkt_id;
    mqtt_pkt_id++;

    /* Payload: Topic filter + QoS */
    idx += encode_utf8_string(buf + idx, topic);
    buf[idx++] = qos;

    /* Fixed header */
    uint8_t rem_buf[4];
    uint16_t rem_len = encode_remaining_len(rem_buf, idx);

    memmove(buf + 1 + rem_len, buf, idx);
    buf[0] = 0x82; /* SUBSCRIBE (0x80 | 0x02 reserved) */
    memcpy(buf + 1, rem_buf, rem_len);

    return 1 + rem_len + idx;
}

/* 构造MQTT PINGREQ包 */
static uint16_t mqtt_build_pingreq(uint8_t *buf)
{
    buf[0] = 0xC0; /* PINGREQ */
    buf[1] = 0x00;
    return 2;
}

/* 构造MQTT DISCONNECT包 */
static uint16_t mqtt_build_disconnect(uint8_t *buf)
{
    buf[0] = 0xE0; /* DISCONNECT */
    buf[1] = 0x00;
    return 2;
}

/* ---- MQTT 协议解析 ---- */

/* 解析MQTT包类型 */
static uint8_t mqtt_parse_type(const uint8_t *data)
{
    return (data[0] >> 4) & 0x0F;
}

/* 解析剩余长度 */
static uint32_t mqtt_parse_remaining_len(const uint8_t *data, uint16_t *offset)
{
    uint32_t len = 0;
    uint16_t mul = 1;
    *offset = 1;
    do {
        len += (data[*offset] & 0x7F) * mul;
        mul *= 128;
    } while (data[(*offset)++] & 0x80);
    return len;
}

/* 从MQTT PUBLISH包中提取topic和payload */
static void mqtt_parse_publish(const uint8_t *data, uint16_t data_len,
                               char *topic, uint16_t topic_size,
                               uint8_t *payload, uint16_t payload_size,
                               uint16_t *payload_len)
{
    uint16_t offset;
    mqtt_parse_remaining_len(data, &offset);

    /* Topic */
    uint16_t topic_len = (data[offset] << 8) | data[offset + 1];
    offset += 2;
    if (topic_len >= topic_size) topic_len = topic_size - 1;
    memcpy(topic, data + offset, topic_len);
    topic[topic_len] = '\0';
    offset += topic_len;

    /* QoS > 0 时跳过 Packet ID */
    uint8_t qos = (data[0] >> 1) & 0x03;
    if (qos > 0) offset += 2;

    /* Payload */
    uint16_t remaining = data_len - offset;
    if (remaining > payload_size) remaining = payload_size;
    memcpy(payload, data + offset, remaining);
    *payload_len = remaining;
}

/* ---- 高层API ---- */

static void build_topics(void)
{
    snprintf(g_topic_sensor, sizeof(g_topic_sensor),
             "smartfarm/%s/sensor/data", DEVICE_ID);
    snprintf(g_topic_ctrl_mode, sizeof(g_topic_ctrl_mode),
             "smartfarm/%s/control/mode", DEVICE_ID);
    snprintf(g_topic_ctrl_pump, sizeof(g_topic_ctrl_pump),
             "smartfarm/%s/control/pump", DEVICE_ID);
    snprintf(g_topic_cfg_threshold, sizeof(g_topic_cfg_threshold),
             "smartfarm/%s/config/threshold", DEVICE_ID);
}

HAL_StatusTypeDef MQTT_Init(const char *ssid, const char *password,
                            const char *broker, uint16_t port)
{
    strncpy(wifi_ssid, ssid, sizeof(wifi_ssid) - 1);
    strncpy(wifi_password, password, sizeof(wifi_password) - 1);
    strncpy(mqtt_broker, broker, sizeof(mqtt_broker) - 1);
    mqtt_port = port;

    build_topics();

    printf("[MQTT] Initializing ESP8266...\r\n");
    if (ESP8266_AT_Init() != AT_OK)
    {
        printf("[MQTT] ESP8266 AT init failed\r\n");
        return HAL_ERROR;
    }

    printf("[MQTT] Connecting WiFi: %s\r\n", ssid);
    if (ESP8266_ConnectWiFi(ssid, password) != AT_OK)
    {
        printf("[MQTT] WiFi connect failed\r\n");
        return HAL_ERROR;
    }

    printf("[MQTT] Connecting broker: %s:%u\r\n", broker, port);
    if (ESP8266_TCP_Connect(broker, port) != AT_OK)
    {
        printf("[MQTT] TCP connect failed\r\n");
        return HAL_ERROR;
    }

    /* 发送MQTT CONNECT包 */
    uint16_t pkt_len = mqtt_build_connect(mqtt_tx_buf, DEVICE_ID);
    printf("[MQTT] Sending CONNECT (%u bytes)\r\n", pkt_len);
    if (ESP8266_TCP_Send(mqtt_tx_buf, pkt_len) != AT_OK)
    {
        printf("[MQTT] CONNECT send failed\r\n");
        return HAL_ERROR;
    }

    /* 等待CONNACK */
    uint16_t rx_len = 0;
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 5000)
    {
        if (ESP8266_TCP_Read(mqtt_rx_buf, sizeof(mqtt_rx_buf), &rx_len) == AT_OK && rx_len > 0)
        {
            if (mqtt_parse_type(mqtt_rx_buf) == 2) /* CONNACK */
            {
                uint8_t rc = mqtt_rx_buf[3];
                if (rc == 0)
                {
                    mqtt_connected = 1;
                    printf("[MQTT] Connected to broker!\r\n");
                    break;
                }
                else
                {
                    printf("[MQTT] CONNACK error: %u\r\n", rc);
                    return HAL_ERROR;
                }
            }
        }
        HAL_Delay(50);
    }

    if (!mqtt_connected)
    {
        printf("[MQTT] CONNACK timeout\r\n");
        return HAL_ERROR;
    }

    /* 订阅下行主题 */
    mqtt_pkt_id = 1;
    for (int retry = 0; retry < 3; retry++)
    {
        pkt_len = mqtt_build_subscribe(mqtt_tx_buf, g_topic_ctrl_mode, 1);
        ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
        HAL_Delay(200);

        pkt_len = mqtt_build_subscribe(mqtt_tx_buf, g_topic_ctrl_pump, 1);
        ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
        HAL_Delay(200);

        pkt_len = mqtt_build_subscribe(mqtt_tx_buf, g_topic_cfg_threshold, 1);
        ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
        HAL_Delay(500);

        /* 读取SUBACK响应 */
        ESP8266_TCP_Read(mqtt_rx_buf, sizeof(mqtt_rx_buf), &rx_len);
        printf("[MQTT] Subscribed (attempt %d)\r\n", retry + 1);
    }

    return HAL_OK;
}

HAL_StatusTypeDef MQTT_PublishSensorData(const SensorData_t *data)
{
    char json[320];
    int json_len = snprintf(json, sizeof(json),
        "{\"device_id\":\"%s\",\"timestamp\":%lu,\"data\":{"
        "\"temperature\":%.1f,\"humidity\":%.1f,"
        "\"soil_moisture\":%u,\"soil_temperature\":%.1f,"
        "\"light\":%.0f,\"pressure\":%.1f,"
        "\"water_flow\":%.2f,\"total_volume\":%.2f}}",
        DEVICE_ID, data->timestamp,
        data->temperature, data->humidity,
        data->soil_moisture, data->soil_temp,
        data->light, data->pressure,
        data->flow_rate, data->total_volume);

    if (json_len <= 0 || json_len >= sizeof(json))
        return HAL_ERROR;

    uint16_t pkt_len = mqtt_build_publish(mqtt_tx_buf, g_topic_sensor,
                                           (const uint8_t *)json, json_len, 0);
    AT_Result_t ret = ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
    return (ret == AT_OK) ? HAL_OK : HAL_ERROR;
}

static float extract_number(const char *json, const char *key)
{
    char search[32];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == ':' || *p == '"')) p++;
    return (float)atof(p);
}

BaseType_t MQTT_CheckIncoming(ControlCmd_t *cmd)
{
    uint16_t rx_len = 0;

    if (ESP8266_TCP_Read(mqtt_rx_buf, sizeof(mqtt_rx_buf), &rx_len) != AT_OK || rx_len == 0)
        return pdFALSE;

    uint8_t pkt_type = mqtt_parse_type(mqtt_rx_buf);

    /* 处理PUBLISH包（下行消息） */
    if (pkt_type == 3)
    {
        char topic[64] = {0};
        uint8_t payload[256] = {0};
        uint16_t payload_len = 0;

        mqtt_parse_publish(mqtt_rx_buf, rx_len, topic, sizeof(topic),
                           payload, sizeof(payload), &payload_len);
        payload[payload_len < sizeof(payload) ? payload_len : sizeof(payload) - 1] = '\0';

        printf("[MQTT] Recv: %s -> %s\r\n", topic, (char *)payload);

        if (strstr(topic, "control/mode"))
        {
            cmd->type = strstr((char *)payload, "auto") ? CTRL_MODE_AUTO :
                        strstr((char *)payload, "manual") ? CTRL_MODE_MANUAL : CTRL_UNKNOWN;
        }
        else if (strstr(topic, "control/pump"))
        {
            cmd->type = (strstr((char *)payload, "on") || strstr((char *)payload, "1")) ? CTRL_PUMP_ON :
                        (strstr((char *)payload, "off") || strstr((char *)payload, "0")) ? CTRL_PUMP_OFF : CTRL_UNKNOWN;
        }
        else if (strstr(topic, "config/threshold"))
        {
            cmd->type = CTRL_SET_THRESHOLD;
            cmd->params.threshold.low = extract_number((char *)payload, "threshold_low");
            cmd->params.threshold.high = extract_number((char *)payload, "threshold_high");
        }
        else
        {
            cmd->type = CTRL_UNKNOWN;
        }
        return pdTRUE;
    }

    /* PINGRESP - 忽略 */
    if (pkt_type == 13)
        return pdFALSE;

    /* SUBACK - 忽略 */
    if (pkt_type == 9)
        return pdFALSE;

    return pdFALSE;
}

HAL_StatusTypeDef MQTT_Reconnect(void)
{
    printf("[MQTT] Reconnecting...\r\n");
    mqtt_connected = 0;

    /* 关闭旧TCP连接 */
    ESP8266_TCP_Close();
    HAL_Delay(1000);

    /* 尝试TCP重连 */
    if (ESP8266_TCP_Connect(mqtt_broker, mqtt_port) != AT_OK)
    {
        /* TCP失败，尝试WiFi重连 */
        if (ESP8266_ConnectWiFi(wifi_ssid, wifi_password) != AT_OK)
        {
            printf("[MQTT] Reconnect failed (WiFi)\r\n");
            return HAL_ERROR;
        }
        if (ESP8266_TCP_Connect(mqtt_broker, mqtt_port) != AT_OK)
        {
            printf("[MQTT] Reconnect failed (TCP)\r\n");
            return HAL_ERROR;
        }
    }

    /* MQTT CONNECT */
    uint16_t pkt_len = mqtt_build_connect(mqtt_tx_buf, DEVICE_ID);
    if (ESP8266_TCP_Send(mqtt_tx_buf, pkt_len) != AT_OK)
        return HAL_ERROR;

    /* 等待CONNACK */
    uint16_t rx_len = 0;
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 5000)
    {
        if (ESP8266_TCP_Read(mqtt_rx_buf, sizeof(mqtt_rx_buf), &rx_len) == AT_OK && rx_len > 0)
        {
            if (mqtt_parse_type(mqtt_rx_buf) == 2 && mqtt_rx_buf[3] == 0)
            {
                mqtt_connected = 1;
                break;
            }
        }
        HAL_Delay(50);
    }

    if (!mqtt_connected)
        return HAL_ERROR;

    /* 重新订阅 */
    pkt_len = mqtt_build_subscribe(mqtt_tx_buf, g_topic_ctrl_mode, 1);
    ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
    HAL_Delay(200);
    pkt_len = mqtt_build_subscribe(mqtt_tx_buf, g_topic_ctrl_pump, 1);
    ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
    HAL_Delay(200);
    pkt_len = mqtt_build_subscribe(mqtt_tx_buf, g_topic_cfg_threshold, 1);
    ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
    HAL_Delay(500);

    printf("[MQTT] Reconnected OK\r\n");
    return HAL_OK;
}

uint8_t MQTT_IsConnected(void)
{
    return mqtt_connected;
}

/* 发送PINGREQ保持连接（由MQTT_Pub任务周期性调用） */
void MQTT_KeepAlive(void)
{
    if (!mqtt_connected) return;
    uint16_t pkt_len = mqtt_build_pingreq(mqtt_tx_buf);
    ESP8266_TCP_Send(mqtt_tx_buf, pkt_len);
}
