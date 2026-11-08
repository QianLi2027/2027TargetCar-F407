# 靶车主体控制板（STM32F407 + GM6020）

> ESP32 导轨板通过 WiFi/TCP 把「方向 + 挡位」发过来，本板驱动一个 GM6020 转靶车主体。

手机 ──WiFi──► ESP32（导轨板，热点 TARGET_CAR / 192.168.4.1）
                  │ TCP 3333
                  ▼
               ESP-01S（装在本板上，当网卡用，AT 固件）
                  │ USART1 115200
                  ▼
            本板 STM32F407 ──CAN 1 Mbps──► GM6020（ID = 2）



## 软件结构（4 层）

```
Task/                    ← 业务：干"靶车该干什么"
├── Rotor.cpp            6020 速度环（1 kHz）
├── EspLink.cpp          链路状态机（AT 命令 + 报文解析）
└── TaskBase.h           FreeRTOS 任务封装

Device/CAN/              ← 设备：电机对象
├── can_app.*            给 C 文件用的 C 接口
├── can_device_config.*  接线表（哪个反馈 ID 是哪个电机）
├── can_dispatcher.*     帧分发
└── Motor/
    ├── dji_motor.*      DJI 电机协议（解码 + 组帧）
    ├── motor_manager.*  电机注册表
    └── motor_common.h   统一反馈结构 + 超时判定

Bsp/                     ← 板级支持：只做"能收发"
├── CAN/bsp_can.*        CAN 收发
├── CAN/magictools.h     clamp()
└── USART/esp01s.*       USART1 收发 + 环形缓冲

Core/                    ← CubeMX 生成
├── main.c / can.c / usart.c / gpio.c / freertos.c / *_it.c
```


## 控制

```
指令 → 目标转速 → 斜坡 → [前馈 + PID] → 两级限幅 → 电流 → CAN
```

| 环节 | 说明 |
|---|---|
| **目标** | 每轮先归零，只有"电机在线 + 链路正常 + 挡位 1~4"才覆盖 |
| **斜坡** | 每 1 ms 最多变 0.18 rpm（1000 ms 从 0 到 180 rpm） |
| **前馈** | 加速度 + 库仑摩擦 + 粘滞摩擦 + 停车刹车 + 起步追赶 |
| **PID** | kp 20 / ki 0.5 / kd 0（参数写死在源码里） |
| **限幅** | 变化率 ≤ 400/ms，绝对值 ≤ ±16000 |

**挡位转速**：

| 挡位 | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| rpm | 25（RMUL/RMUC 考核值） | 45 | 90 | 120 |

**前馈参数**（`Rotor.cpp` 顶部，改完要重烧）：

| 参数 | 值 | 含义 |
|---|---|---|
| `s_ramp_ms` | 1000 | 0→180 rpm 的斜坡时间 |
| `s_accel_ff` | 60 | 加速度前馈：电流 / (rpm/s) |
| `s_fric_ff` | 25 | 库仑摩擦前馈 |
| `s_visc_ff` | 20 | 粘滞摩擦前馈 |


**链路超时**：800 ms 没收到新指令 → 目标归零（自然停车，不会"离线自转"）。

---

## 和 ESP 板的链路

ESP32 每 100 ms 发一行 ASCII：

```
FORWARD,2      方向,挡位      （FORWARD / REVERSE / STOP）
IDLE,0         保活：刷新时间戳 + 停车
```

