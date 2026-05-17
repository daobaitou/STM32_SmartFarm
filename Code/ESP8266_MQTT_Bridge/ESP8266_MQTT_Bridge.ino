/**
 * @file    ESP8266_MQTT_Bridge.ino
 * @author  王国维
 * @date    2026-05-17
 * @brief   ESP8266 MQTT网桥 v2.0 - 改进UART协议可靠性
 * @note    协议格式: $CMD:type:value*CS\n  (CS=XOR校验和)
 *          STM32发送: $SNS:{json}*XX\n
 *          ESP8266→STM32: $CMD:type:value*XX\n
 *          STM32→ESP8266: $ACK:type*XX / $NACK:type*XX
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
unsigned long lastStatusTime = 0;
uint32_t publishCount = 0;
uint32_t cmdSentCount = 0;
uint32_t ackRecvCount = 0;

/* ---------- XOR校验和 ---------- */
uint8_t xorChecksum(const String& s) {
  uint8_t cs = 0;
  for (unsigned int i = 0; i < s.length(); i++) {
    cs ^= (uint8_t)s[i];
  }
  return cs;
}

String toHex(uint8_t val) {
  String h = String(val, HEX);
  h.toUpperCase();
  if (h.length() < 2) h = "0" + h;
  return h;
}

/* 封装协议帧: $content*XX\n */
String makeFrame(const String& content) {
  uint8_t cs = xorChecksum(content);
  return "$" + content + "*" + toHex(cs) + "\n";
}

/* 验证帧校验: 返回content部分，或空串 */
bool verifyFrame(const String& raw, String& content) {
  /* 格式: $xxx*HH\n */
  if (raw.length() < 4 || raw[0] != '$') return false;
  int starPos = raw.lastIndexOf('*');
  if (starPos < 1) return false;

  content = raw.substring(1, starPos);
  String csStr = raw.substring(starPos + 1);
  csStr.trim();

  uint8_t expected = xorChecksum(content);
  uint8_t actual = (uint8_t)strtol(csStr.c_str(), NULL, 16);
  return (expected == actual);
}

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

/* ---------- 发送命令到STM32(带重试) ---------- */
bool sendCmdToSTM32(const String& cmdContent) {
  String frame = makeFrame(cmdContent);
  String expectedAck = "ACK:" + cmdContent.substring(0, cmdContent.indexOf(':'));

  for (int retry = 0; retry < 3; retry++) {
    stmSerial.print(frame);
    stmSerial.flush();
    cmdSentCount++;
    Serial.println("[UART] TX [" + String(retry+1) + "/3]: " + frame.substring(0, frame.length()-1));

    /* 等待ACK (200ms超时) */
    unsigned long start = millis();
    String respBuf = "";
    while (millis() - start < 200) {
      while (stmSerial.available()) {
        char c = (char)stmSerial.read();
        if (c == '\n') {
          respBuf.trim();
          if (respBuf.startsWith("$")) {
            String content;
            if (verifyFrame(respBuf, content)) {
              Serial.println("[UART] ACK: " + content);
              ackRecvCount++;
              return true;
            }
          }
          respBuf = "";
        } else if (c != '\r') {
          respBuf += c;
        }
      }
      delay(1);
    }
    Serial.println("[UART] No ACK, retrying...");
  }
  Serial.println("[UART] FAILED after 3 retries!");
  return false;
}

/* ---------- MQTT ---------- */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();

  Serial.println("[MQTT] RX: " + String(topic) + " => " + msg);

  String cmdContent = "";

  if (String(topic) == TOPIC_CTRL_MODE) {
    if (msg.equalsIgnoreCase("AUTO") || msg.indexOf("auto") >= 0) {
      cmdContent = "CMD:MODE:AUTO";
    } else {
      cmdContent = "CMD:MODE:MANUAL";
    }
  }
  else if (String(topic) == TOPIC_CTRL_PUMP) {
    if (msg.equalsIgnoreCase("ON") || msg.indexOf("on") >= 0) {
      cmdContent = "CMD:PUMP:ON";
    } else {
      cmdContent = "CMD:PUMP:OFF";
    }
  }
  else if (String(topic) == TOPIC_CTRL_FAN) {
    if (msg.equalsIgnoreCase("ON") || msg.indexOf("on") >= 0) {
      cmdContent = "CMD:FAN:ON";
    } else {
      cmdContent = "CMD:FAN:OFF";
    }
  }
  else if (String(topic) == TOPIC_CTRL_WINDOW) {
    if (msg.equalsIgnoreCase("OPEN") || msg.indexOf("open") >= 0) {
      cmdContent = "CMD:WDOW:OPEN";
    } else {
      cmdContent = "CMD:WDOW:CLOSE";
    }
  }
  else if (String(topic) == TOPIC_CTRL_THRESH) {
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (!err) {
      int low = doc["low"] | 30;
      int high = doc["high"] | 70;
      cmdContent = "CMD:THRESH:" + String(low) + ":" + String(high);
    }
  }

  if (cmdContent.length() > 0) {
    sendCmdToSTM32(cmdContent);
  }
}

bool reconnect_mqtt() {
  Serial.println("[MQTT] Connecting to " MQTT_BROKER "...");
  if (mqtt.connect(MQTT_CLIENT)) {
    Serial.println("[MQTT] Connected!");
    mqtt.subscribe(TOPIC_CTRL_MODE, 1);
    mqtt.subscribe(TOPIC_CTRL_PUMP, 1);
    mqtt.subscribe(TOPIC_CTRL_THRESH, 1);
    mqtt.subscribe(TOPIC_CTRL_FAN, 1);
    mqtt.subscribe(TOPIC_CTRL_WINDOW, 1);
    Serial.println("[MQTT] Subscribed to all control topics");
    return true;
  }
  Serial.println("[MQTT] Failed, rc=" + String(mqtt.state()));
  return false;
}

/* ---------- 传感器数据处理 ---------- */
void publish_sensor_data(const String& content) {
  StaticJsonDocument<256> input;
  DeserializationError err = deserializeJson(input, content);
  if (err) {
    Serial.println("[JSON] Parse error: " + String(err.c_str()));
    stmSerial.print(makeFrame("NACK:SNS"));
    return;
  }

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
    stmSerial.print(makeFrame("ACK:SNS"));
    Serial.println("[MQTT] Published #" + String(publishCount) + " (" + String(strlen(payload)) + " bytes)");
  } else {
    stmSerial.print(makeFrame("NACK:SNS"));
    Serial.println("[MQTT] Publish failed!");
  }
}

/* ---------- UART 协议处理 ---------- */
void handle_uart() {
  static String buffer = "";

  while (stmSerial.available()) {
    char c = (char)stmSerial.read();

    if (c == '\n') {
      buffer.trim();
      if (buffer.length() == 0) { continue; }

      String content;
      if (buffer[0] == '$' && verifyFrame(buffer, content)) {
        /* 带校验的帧 */
        if (content.startsWith("SNS:")) {
          String json = content.substring(4);
          Serial.println("[UART] RX sensor data (" + String(json.length()) + " bytes)");
          publish_sensor_data(json);
        }
        else if (content.startsWith("ACK:") || content.startsWith("NACK:")) {
          Serial.println("[UART] RX response: " + content);
        }
        else {
          Serial.println("[UART] RX unknown: " + content);
        }
      }
      else {
        /* 兼容旧格式(无校验) */
        if (buffer.startsWith("SNS:")) {
          String json = buffer.substring(4);
          Serial.println("[UART] RX legacy sensor data");
          publish_sensor_data(json);
        }
        else {
          Serial.println("[UART] RX raw: " + buffer);
        }
      }
      buffer = "";
    }
    else if (c != '\r') {
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
    if (now - lastBlink > 200) {
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState);
      lastBlink = now;
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}

/* ========== Arduino 入口 ========== */
void setup() {
  Serial.begin(115200);
  stmSerial.begin(UART_BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("\n\n=== ESP8266 MQTT Bridge v2.0 ===");
  Serial.println("[Init] UART to STM32 @ " + String(UART_BAUD));
  Serial.println("[Init] Protocol: $content*XX (XOR checksum)");

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
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost connection, reconnecting...");
    setup_wifi();
  }

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

  handle_uart();
  update_led();

  unsigned long now = millis();
  if (now - lastStatusTime > 30000) {
    lastStatusTime = now;
    Serial.println("[Status] WiFi:" + String(WiFi.status() == WL_CONNECTED ? "OK" : "DOWN") +
                   " MQTT:" + String(mqtt.connected() ? "OK" : "DOWN") +
                   " Published:" + String(publishCount) +
                   " CmdSent:" + String(cmdSentCount) +
                   " AckRecv:" + String(ackRecvCount) +
                   " RSSI:" + String(WiFi.RSSI()) + "dBm");
  }
}
