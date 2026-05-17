#ifndef CONFIG_H
#define CONFIG_H

/* WiFi */
#define WIFI_SSID     "KmustAuto"
#define WIFI_PASSWORD "123456789"

/* MQTT Broker */
#define MQTT_BROKER   "124.223.5.91"
#define MQTT_PORT     1883
#define MQTT_CLIENT   "farm_001_esp"

/* Topics */
#define TOPIC_SENSOR      "smartfarm/farm_001/sensor/data"
#define TOPIC_CTRL_MODE   "smartfarm/farm_001/control/mode"
#define TOPIC_CTRL_PUMP   "smartfarm/farm_001/control/pump"
#define TOPIC_CTRL_THRESH "smartfarm/farm_001/config/threshold"
#define TOPIC_CTRL_FAN    "smartfarm/farm_001/control/fan"
#define TOPIC_CTRL_WINDOW "smartfarm/farm_001/control/window"

/* UART to STM32 (SoftwareSerial) - 9600更可靠 */
#define UART_BAUD     9600
#define UART_RX_PIN   15   // D8 = GPIO15
#define UART_TX_PIN   13   // D7 = GPIO13

#endif
