"""
SmartFarm Flask Web Application
- MQTT订阅传感器数据 → SQLite存储
- REST API + 移动端Web界面
- 远程控制命令发布
"""

import json
import sqlite3
import threading
import time
from datetime import datetime, timedelta

import paho.mqtt.client as mqtt
from flask import (
    Flask, jsonify, render_template, request,
)

import config

app = Flask(__name__)
app.secret_key = config.SECRET_KEY

# ── 全局状态 ──────────────────────────────────────────────────
latest_data = {
    "temperature": 0, "humidity": 0, "soil_moisture": 0,
    "soil_temp": 0, "light": 0, "pressure": 0,
    "co2": 0, "flow_rate": 0, "total_volume": 0,
    "timestamp": 0, "update_time": "等待数据..."
}

# ── SQLite 数据库 ─────────────────────────────────────────────

def get_db():
    """获取数据库连接"""
    conn = sqlite3.connect(config.DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    """初始化数据库表"""
    import os
    os.makedirs(os.path.dirname(config.DB_PATH), exist_ok=True)
    conn = get_db()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sensor_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp REAL NOT NULL,
            temperature REAL DEFAULT 0,
            humidity REAL DEFAULT 0,
            soil_moisture INTEGER DEFAULT 0,
            soil_temp REAL DEFAULT 0,
            light REAL DEFAULT 0,
            pressure REAL DEFAULT 0,
            co2 INTEGER DEFAULT 0,
            flow_rate REAL DEFAULT 0,
            total_volume REAL DEFAULT 0
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS control_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp REAL NOT NULL,
            action TEXT NOT NULL,
            value TEXT NOT NULL
        )
    """)
    conn.commit()
    conn.close()
    print(f"[DB] Initialized: {config.DB_PATH}")

def insert_sensor_data(data):
    """插入传感器数据"""
    try:
        conn = get_db()
        conn.execute("""
            INSERT INTO sensor_data (timestamp, temperature, humidity, soil_moisture,
                soil_temp, light, pressure, co2, flow_rate, total_volume)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            data.get("timestamp", time.time()),
            data.get("temperature", 0),
            data.get("humidity", 0),
            data.get("soil_moisture", 0),
            data.get("soil_temp", 0),
            data.get("light", 0),
            data.get("pressure", 0),
            data.get("co2", 0),
            data.get("flow_rate", 0),
            data.get("total_volume", 0),
        ))
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"[DB] Insert error: {e}")

# ── MQTT 客户端 ───────────────────────────────────────────────

mqtt_client = None

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[MQTT] Connected to broker")
        client.subscribe(config.MQTT_TOPIC_SENSOR)
    else:
        print(f"[MQTT] Connect failed: {rc}")

def on_message(client, userdata, msg):
    """收到传感器数据 → 更新全局状态 + 存入数据库"""
    global latest_data
    try:
        payload = json.loads(msg.payload.decode())
        d = payload.get("data", payload)
        now = time.time()
        latest_data = {
            "temperature": d.get("temperature", 0),
            "humidity": d.get("humidity", 0),
            "soil_moisture": d.get("soil_moisture", 0),
            "soil_temp": d.get("soil_temperature", 0),
            "light": d.get("light", 0),
            "pressure": d.get("pressure", 0),
            "co2": d.get("co2", 0),
            "flow_rate": d.get("water_flow", 0),
            "total_volume": d.get("total_volume", 0),
            "timestamp": now,
            "update_time": datetime.now().strftime("%H:%M:%S"),
        }
        insert_sensor_data(latest_data)
        print(f"[MQTT] Data: T={latest_data['temperature']} H={latest_data['humidity']}")
    except Exception as e:
        print(f"[MQTT] Parse error: {e}")

def mqtt_start():
    """后台线程启动MQTT客户端"""
    global mqtt_client
    mqtt_client = mqtt.Client(client_id=config.MQTT_CLIENT_ID)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    try:
        mqtt_client.connect(config.MQTT_BROKER, config.MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"[MQTT] Connection error: {e}, retrying in 5s...")
        time.sleep(5)
        mqtt_start()

def mqtt_publish(topic, message):
    """发布控制命令"""
    if mqtt_client:
        mqtt_client.publish(topic, message, qos=1)
        print(f"[MQTT] Publish: {topic} → {message}")

# ── Web 路由 ─────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("dashboard.html", data=latest_data)

@app.route("/history")
def history():
    return render_template("history.html", data=latest_data)

@app.route("/control")
def control():
    return render_template("control.html", data=latest_data)

# ── REST API ─────────────────────────────────────────────────

@app.route("/api/latest")
def api_latest():
    return jsonify(latest_data)

@app.route("/api/history")
def api_history():
    hours = request.args.get("hours", 24, type=int)
    since = time.time() - hours * 3600
    try:
        conn = get_db()
        rows = conn.execute(
            "SELECT * FROM sensor_data WHERE timestamp > ? ORDER BY timestamp ASC",
            (since,)
        ).fetchall()
        conn.close()
        result = []
        for r in rows:
            result.append({
                "timestamp": r["timestamp"],
                "temperature": r["temperature"],
                "humidity": r["humidity"],
                "soil_moisture": r["soil_moisture"],
                "soil_temp": r["soil_temp"],
                "light": r["light"],
                "pressure": r["pressure"],
                "co2": r["co2"],
                "flow_rate": r["flow_rate"],
                "total_volume": r["total_volume"],
            })
        return jsonify(result)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/control/<device>", methods=["POST"])
def api_control(device):
    if device not in config.MQTT_TOPICS_CONTROL:
        return jsonify({"error": "unknown device"}), 400

    topic = config.MQTT_TOPICS_CONTROL[device]
    body = request.get_json(silent=True) or {}
    value = body.get("value", "")

    if device == "mode":
        msg = value if value in ("AUTO", "MANUAL") else "AUTO"
    elif device == "threshold":
        msg = json.dumps(body)
    else:
        msg = value if value in ("ON", "OFF", "OPEN", "CLOSE") else "OFF"

    mqtt_publish(topic, msg)

    try:
        conn = get_db()
        conn.execute(
            "INSERT INTO control_log (timestamp, action, value) VALUES (?, ?, ?)",
            (time.time(), device, msg)
        )
        conn.commit()
        conn.close()
    except Exception:
        pass

    return jsonify({"status": "ok", "topic": topic, "message": msg})

@app.route("/api/stats")
def api_stats():
    try:
        conn = get_db()
        total = conn.execute("SELECT COUNT(*) as c FROM sensor_data").fetchone()["c"]
        latest = conn.execute(
            "SELECT timestamp FROM sensor_data ORDER BY timestamp DESC LIMIT 1"
        ).fetchone()
        conn.close()
        return jsonify({
            "total_records": total,
            "latest_timestamp": latest["timestamp"] if latest else 0,
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ── 初始化（模块加载时执行，兼容gunicorn） ─────────────────────

init_db()

_t = threading.Thread(target=mqtt_start, daemon=True)
_t.start()
print("[Web] MQTT subscriber started")

if __name__ == "__main__":
    print("[Web] Starting Flask on http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=False)