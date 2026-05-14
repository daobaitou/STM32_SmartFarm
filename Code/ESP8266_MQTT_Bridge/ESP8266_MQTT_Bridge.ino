/**
 * @file    ESP8266_MQTT_Bridge.ino
 * @author  王国维
 * @date    2026-05-12
 * @brief   ESP8266 MQTT网桥 - 独立处理WiFi+MQTT，通过UART与STM32交换数据
 * @note    STM32发送 SNS:{json}\n，ESP8266发布到MQTT
 *          ESP8266订阅控制主题，收到命令后发送 CMD:xxx\n 给STM32
 *
 * 接线: STM32 PA2(TX) → D8(GPIO15/RX), STM32 PA3(RX) ← D7(GPIO13/TX)
 * 依赖: PubSubClient, ArduinoJson v6
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "config.h"

SoftwareSerial stmSerial(UART_RX_PIN, UART_TX_PIN);
WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastReconnectAttempt = 0;
unsigned long lastPublishTime = 0;
uint32_t publishCount = 0;

/* ---------- WiFi ---------- */
void setup_wifi() {
  Serial.println("[WiFi] Connecting to " WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Failed, will retry in loop");
  }
}

/* ---------- MQTT ---------- */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();

  Serial.println("[MQTT] Received: " + String(topic) + " => " + msg);

  String cmd = "";

  if (String(topic) == TOPIC_CTRL_MODE) {
    /* 支持 "AUTO" / "MANUAL" 或 JSON {"mode":"auto"} */
    if (msg.equalsIgnoreCase("AUTO") || msg.indexOf("auto") >= 0) {
      cmd = "CMD:MODE:AUTO\n";
    } else {
      cmd = "CMD:MODE:MANUAL\n";
    }
  }
  else if (String(topic) == TOPIC_CTRL_PUMP) {
    /* 支持 "ON" / "OFF" 或 JSON {"state":"on"} */
    if (msg.equalsIgnoreCase("ON") || msg.indexOf("on") >= 0) {
      cmd = "CMD:PUMP:ON\n";
    } else {
      cmd = "CMD:PUMP:OFF\n";
    }
  }
  else if (String(topic) == TOPIC_CTRL_THRESH) {
    /* 解析阈值 JSON: {"low":35,"high":70} */
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (!err) {
      int low = doc["low"] | doc["threshold_low"] | 30;
      int high = doc["high"] | doc["threshold_high"] | 70;
      cmd = "CMD:THRESH:" + String(low) + ":" + String(high) + "\n";
    } else {
      /* 尝试简单格式 "35:70" */
      if (msg.indexOf(":") >= 0) {
        cmd = "CMD:THRESH:" + msg + "\n";
      }
    }
  }

  if (cmd.length() > 0) {
    stmSerial.print(cmd);
    Serial.println("[UART] Sent to STM32: " + cmd);
  }
}

bool reconnect_mqtt() {
  Serial.println("[MQTT] Connecting to " MQTT_BROKER "...");
  if (mqtt.connect(MQTT_CLIENT)) {
    Serial.println("[MQTT] Connected!");
    mqtt.subscribe(TOPIC_CTRL_MODE, 1);
    mqtt.subscribe(TOPIC_CTRL_PUMP, 1);
    mqtt.subscribe(TOPIC_CTRL_THRESH, 1);
    Serial.println("[MQTT] Subscribed to control topics");
    return true;
  }
  Serial.println("[MQTT] Failed, rc=" + String(mqtt.state()));
  return false;
}

/* ---------- 传感器数据处理 ---------- */
void publish_sensor_data(const String& compactJson) {
  /* 解析紧凑JSON */
  StaticJsonDocument<256> input;
  DeserializationError err = deserializeJson(input, compactJson);
  if (err) {
    Serial.println("[JSON] Parse error: " + String(err.c_str()));
    stmSerial.println("ERR:JSON");
    return;
  }

  /* 构建完整JSON */
  StaticJsonDocument<512> output;
  output["device_id"] = "farm_001";
  output["timestamp"] = millis() / 1000;

  JsonObject data = output.createNestedObject("data");
  data["temperature"]       = input["t"] | 0.0f;
  data["humidity"]          = input["h"] | 0.0f;
  data["soil_moisture"]     = input["sm"] | 0;
  data["soil_temperature"]  = input["st"] | 0.0f;
  data["light"]             = input["l"] | 0.0f;
  data["pressure"]          = input["p"] | 0.0f;
  data["co2"]              = input["c"] | 0;
  data["water_flow"]        = input["f"] | 0.0f;
  data["total_volume"]      = input["v"] | 0.0f;

  char payload[512];
  serializeJson(output, payload);

  if (mqtt.publish(TOPIC_SENSOR, payload)) {
    publishCount++;
    stmSerial.println("ACK:SNS");
    Serial.println("[MQTT] Published #" + String(publishCount) + " (" + String(strlen(payload)) + " bytes)");
  } else {
    stmSerial.println("NACK:SNS");
    Serial.println("[MQTT] Publish failed!");
  }
}

/* ---------- UART 协议处理 ---------- */
void handle_uart() {
  static String buffer = "";

  /* 同时监听 SoftwareSerial(STM32) 和 硬件Serial(调试) */
  while (stmSerial.available() || Serial.available()) {
    char c;
    if (stmSerial.available()) {
      c = (char)stmSerial.read();
    } else {
      c = (char)Serial.read();
    }

    if (c == '\n') {
      buffer.trim();
      if (buffer.startsWith("SNS:")) {
        String json = buffer.substring(4);
        Serial.println("[UART] Received: " + json);
        publish_sensor_data(json);
      }
      buffer = "";
    } else if (c != '\r') {
      buffer += c;
      if (buffer.length() > 300) {
        Serial.println("[UART] Frame too long, discarding");
        buffer = "";
      }
    }
  }
}

/* ---------- 状态指示 ---------- */
void update_led() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;

  unsigned long now = millis();

  if (!mqtt.connected()) {
    /* 未连接：快速闪烁 */
    if (now - lastBlink > 200) {
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState);
      lastBlink = now;
    }
  } else {
    /* 已连接：常亮 */
    digitalWrite(LED_BUILTIN, LOW);  // NodeMCU LED active LOW
  }
}

/* ========== Arduino 入口 ========== */
void setup() {
  Serial.begin(115200);
  stmSerial.begin(UART_BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("\n\n=== ESP8266 MQTT Bridge v1.0 ===");
  Serial.println("[Init] UART to STM32 @ " + String(UART_BAUD));

  setup_wifi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqtt_callback);
  mqtt.setBufferSize(512);

  if (WiFi.status() == WL_CONNECTED) {
    reconnect_mqtt();
  }

  Serial.println("[Init] Ready!\n");
}

void loop() {
  /* 维持WiFi连接 */
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost connection, reconnecting...");
    setup_wifi();
  }

  /* 维持MQTT连接 */
  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnect_mqtt()) {
        lastReconnectAttempt = 0;
      }
    }
  }
  mqtt.loop();

  /* 处理STM32发来的数据 */
  handle_uart();

  /* LED状态指示 */
  update_led();

  /* 每30秒打印一次状态 */
  unsigned long now = millis();
  if (now - lastPublishTime > 30000) {
    lastPublishTime = now;
    Serial.println("[Status] WiFi:" + String(WiFi.status() == WL_CONNECTED ? "OK" : "DOWN") +
                   " MQTT:" + String(mqtt.connected() ? "OK" : "DOWN") +
                   " Published:" + String(publishCount) +
                   " RSSI:" + String(WiFi.RSSI()) + "dBm");
  }
}
