# 🌱 SmartFarm

> 基于STM32的智能农业监测与灌溉控制系统

## 简介

一套完整的智慧农业解决方案，实现环境监测、自动灌溉、远程控制。

**核心功能：**
- 📊 多传感器实时监测（温湿度、土壤、光照、CO2、气压）
- 💧 自动灌溉控制（土壤湿度阈值触发）
- 🌡️ 自动环境调节（风扇降温、窗户通风）
- 📱 手机Web远程监控
- ⚡ 断电检测与切换

## 技术栈

| 端 | 技术 |
|---|---|
| 嵌入式 | STM32F103C8T6 + FreeRTOS + HAL |
| 通信 | ESP8266 + MQTT |
| 后端 | Flask + SQLite + Gunicorn |
| 部署 | 腾讯云 + Nginx + systemd |

## 项目结构

```
├── Code/                  # STM32嵌入式代码
│   ├── STM32_SmartFarm/       # 主工程
│   └── ESP8266_MQTT_Bridge/   # ESP8266固件
├── Web/                   # Flask Web端
├── Sensor/                # 传感器图片资料
└── 文档/                  # 设计文档与论文
```

## 系统架构

```mermaid
flowchart LR
    subgraph 嵌入式终端
        S[STM32<br/>FreeRTOS<br/>传感器采集 + 灌溉控制]
        E[ESP8266<br/>Arduino<br/>WiFi + MQTT]
        S <-.->|UART 文本协议| E
    end

    subgraph 云服务器
        M[Mosquitto<br/>MQTT Broker]
        F[Flask + Gunicorn<br/>Web后端]
        DB[(SQLite<br/>数据存储)]
        N[Nginx<br/>反向代理]
    end

    E -->|MQTT 上行<br/>传感器数据| M
    M -->|MQTT 订阅| F
    F --> DB
    F --> N

    Phone[📱 手机浏览器<br/>Web 监控界面] -->|HTTP REST API| N
    N -->|MQTT 下行<br/>控制命令| M
    M -->|CMD:PUMP/ON| E
    E -->|CMD:PUMP:ON| S

    style S fill:#e8f5e9,stroke:#4caf50,color:#1b5e20
    style E fill:#e3f2fd,stroke:#2196f3,color:#0d47a1
    style M fill:#fff3e0,stroke:#ff9800,color:#e65100
    style F fill:#f3e5f5,stroke:#9c27b0,color:#4a148c
    style DB fill:#efebe9,stroke:#795548,color:#3e2723
    style N fill:#e0f7fa,stroke:#00bcd4,color:#006064
    style Phone fill:#fafafa,stroke:#607d8b,color:#37474f
```

## FreeRTOS 任务架构

```mermaid
flowchart TB
    subgraph 优先级6
        IRR[Irrigation<br/>灌溉 + 风扇 + 窗户 + 按键]
    end
    subgraph 优先级4
        RX[UART_RX<br/>接收控制命令]
    end
    subgraph 优先级3
        SEN[Sensor<br/>传感器采集]
    end
    subgraph 优先级2
        LCD[LCD<br/>OLED 显示]
        TX[UART_TX<br/>数据发送]
        PRT[Print<br/>串口调试]
    end
    subgraph 优先级0
        LED[LED<br/>状态指示]
    end

    SEN -->|xQueue_SensorData| LCD
    SEN -->|xQueue_SensorData| TX
    SEN -->|xQueue_SensorData| PRT
    RX -->|xQueue_ControlCmd| IRR

    style IRR fill:#ffebee,stroke:#e53935,color:#b71c1c
    style RX fill:#fff3e0,stroke:#fb8c00,color:#e65100
    style SEN fill:#e8f5e9,stroke:#43a047,color:#1b5e20
    style LCD fill:#e3f2fd,stroke:#1e88e5,color:#0d47a1
    style TX fill:#e3f2fd,stroke:#1e88e5,color:#0d47a1
    style PRT fill:#e3f2fd,stroke:#1e88e5,color:#0d47a1
    style LED fill:#f5f5f5,stroke:#9e9e9e,color:#424242
```

## 传感器配置

| 传感器 | 型号 | 测量内容 | 精度 |
|-------|------|---------|------|
| 温湿度 | DHT22 | 空气温度/湿度 | ±0.5°C / ±2%RH |
| 土壤湿度 | FC28 | 土壤含水量 | ADC 模拟量 |
| 土壤温度 | DS18B20 | 土壤温度 | ±0.5°C |
| 光照 | BH1750 | 光照强度 | 数字输出 |
| CO2 | MH-Z19B | 二氧化碳浓度 | ±50ppm |
| 气压 | BMP180 | 大气压强 | 300-1100hPa |
| 水流 | YF-S201 | 灌溉水量 | 脉冲计数 |

## 控制模块

| 模块 | 触发条件 | 动作 |
|-----|---------|------|
| 水泵 | 土壤湿度 < 30% | 自动灌溉 |
| 风扇 | 温度 > 35°C | 自动降温 |
| 舵机窗户 | 湿度 > 80% | 自动通风 |
| 蜂鸣器+LED | 土壤过干 | 声光报警 |

## 开发进度

- ✅ 嵌入式端完成（STM32 + FreeRTOS + 传感器驱动）
- ✅ ESP8266 MQTT通信完成
- ✅ Flask Web端部署完成
- ⏳ QT上位机（待开发）
- ⏳ 论文撰写

## 作者

王国维 · 毕业设计项目

---

*本README与项目同步更新*