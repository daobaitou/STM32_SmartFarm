"""
SmartFarm Flask Web Configuration
"""

import os
import sys

# MQTT Configuration
MQTT_BROKER = "127.0.0.1"  # 本机Mosquitto
MQTT_PORT = 1883
MQTT_CLIENT_ID = "smartfarm_web"
MQTT_TOPIC_SENSOR = "smartfarm/farm_001/sensor/data"

# Control Topics
MQTT_TOPICS_CONTROL = {
    "mode": "smartfarm/farm_001/control/mode",
    "pump": "smartfarm/farm_001/control/pump",
    "fan": "smartfarm/farm_001/control/fan",
    "window": "smartfarm/farm_001/control/window",
    "threshold": "smartfarm/farm_001/config/threshold",
}

# Database (本地Windows用data目录，服务器用/data)
if sys.platform == "win32":
    DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "smartfarm.db")
else:
    DB_PATH = "/data/smartfarm.db"

# Flask
SECRET_KEY = "smartfarm_secret_key_2026"