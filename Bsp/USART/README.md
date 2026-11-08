# ESP-01S 通信详解（Bsp/USART + Task/EspLink）

> 这块板子怎么通过一个 20 块钱的 ESP8266 模组，跟 ESP32 导轨板通信。
>
> 本文分两部分：**上半部分是协议原理**（AT 命令、`+IPD` 报文），**下半部分是代码怎么实现**。

---

# 第一部分：通信原理

## 1. ESP-01S 是什么

一块 **ESP8266** 模组，出厂烧的是**乐鑫的 AT 固件**。

它自己不会"理解"你的业务，只会：

```
你给它发一串 AT 命令（ASCII）  →  它照做
它收到网络数据时             →  主动吐一段 "+IPD,..." 给你
```

所以它在本项目里的角色是：**一张"串口转 WiFi"的网卡**。

```
STM32（没有 WiFi）
   │ USART1 115200，发 AT 命令
   ▼
ESP-01S（ESP8266 + AT 固件）
   │ WiFi
   ▼
ESP32 热点 TARGET_CAR
```

---

## 2. 两种工作模式：AT 模式 vs 透传模式

AT 固件有两种收发模式，**本项目必须用 AT 模式**。

| | AT 指令模式（`CIPMODE=0`）| 透传模式（`CIPMODE=1`）|
|---|---|---|
| 发数据 | `AT+CIPSEND=n` → 等 `>` → 发 n 字节 | 直接往串口写，全当数据 |
| 收数据 | 带 `+IPD,<len>:` 包头 | 裸数据，无包头 |
| 模块状态 | `WIFI DISCONNECT` / `CLOSED` 等**以文本行出现** | 混在数据流里，**分不清** |
| 适合 | **指令 + 需要知道断没断线** | 纯数据流（音频、大文件）|

**为什么选 AT 模式**：

我们需要知道"WiFi 断了没有""TCP 断了没有"。透传模式下这些提示会被当成普通数据塞给你，无法区分；AT 模式下它们是独立的一行，直接 `strstr` 就能认出来。

---

## 3. AT 命令的收发机制

**发**：一行 ASCII + `\r\n`

```
AT+CWMODE=1\r\n
```

**收**：模块回一行或多行 ASCII

```
AT+CWMODE=1\r\n
OK\r\n
```

所以驱动层的做法是：**发出去之后，一行一行读回来，用 `strstr` 找关键字**。

```cpp
struct AtExpect {
    const char* token;    // 要找的字符串
    bool        success;  // 找到它算成功还是失败
};
```

⚠️ **匹配表的顺序有意义**：因为用 `strstr`（子串匹配），

```cpp
{"CONNECT FAIL",      false},   // ← 必须在前
{"CONNECT",           true},    // ← 它也会匹配到上面的字符串
```

**第一个命中的说了算。**

---

## 4. `+IPD` —— 数据是"主动推"过来的

ESP-01S 收到 TCP 数据时，不会等你来查，而是**直接往串口吐**：

```
+IPD,10:FORWARD,2\n
└──┬─┘ └┬┘ └────┬────┘
 包头  长度   真正的数据（10 字节）
```

### 两种格式（由 `AT+CIPDINFO` 决定）

| 设置 | 格式 |
|---|---|
| `AT+CIPDINFO=0`（本项目用这个）| `+IPD,<len>:<data>` |
| `AT+CIPDINFO=1` | `+IPD,<len>,<ip>,<port>:<data>` |

**为什么选 0**：包头短、好解析。选 1 的话中间那段 IP/端口长度不固定，得多写一个"跳过"状态（代码里 `RX_HEADER_TAIL` 就是为兼容它留的）。

### 关键：`len` 是**字节数**，不是字符数

本项目的指令全是 ASCII（1 字符 = 1 字节），所以无所谓。如果以后要传中文/二进制，必须严格按字节数。

---

## 5. 完整连接流程（从断电到能收数据）

```
上电
 │
 ├─ sleep 1.5 s（等模块启动日志吐完）
 │
 ├─ ① 探模块：发 "AT"，最多 3 次
 │     └ 失败 → 发 "+++" 退出可能的透传模式 → 再试
 │     └ 还失败 → 强制 115200 波特率 → 再试
 │
 ├─ ② 配置链路（每次重连都做）
 │     ATE0           关回显（不关的话接收里混着发出去的命令）
 │     AT+CWMODE=1    设成 STA（连别人热点）
 │     AT+CIPMODE=0   关透传（老固件可能不支持，失败也继续）
 │     AT+CIPMUX=0    单连接
 │     AT+CIPDINFO=0  让 +IPD 不带 ip/port
 │
 ├─ ③ 连 WiFi
 │     AT+CWJAP="TARGET_CAR","<密码>"     ← 最长 30 秒，失败重试 2 次
 │
 ├─ ④ 建 TCP
 │     AT+CIPSTART="TCP","192.168.4.1",3333   ← 5 秒超时，最多试 3 次
 │
 ├─ ⑤ 在线：解析 +IPD（见下）
 │     └ 收到 WIFI DISCONNECT / CLOSED → 退出
 │     └ 2 秒没收到任何 +IPD      → 退出（认为链路卡死）
 │
 └─ ⑥ 断开后：AT+CIPCLOSE → 回到 ② 重来
       连续 5 轮失败 → 发 AT+RST 复位模块
```

---

## 6. 断链检测（三重保险）

单靠一种方式不够，代码里同时用了三种：

| 方式 | 触发条件 | 何时有用 |
|---|---|---|
| ① `WIFI DISCONNECT` 关键字 | WiFi 掉线 | 路由器/热点关了 |
| ② `CLOSED` 关键字 | TCP 连接被对端关闭 | ESP32 主动断开 |
| ③ **2 秒没有 `+IPD`** | 超时 | ESP32 断电/卡死（它连 `CLOSED` 都发不出来）|

第 ③ 种是最后一道防线 —— **没有心跳就是死链**。

---

## 7. 数据流全景

```
        ┌─────────────── BSP 层（只收发字节）───────────────┐
        │                                                   │
   PB7 ─┤ USART1 RXNE 中断                                  │
        │   → 环形缓冲 s_ring[2048]                         │
        │                              Esp01s_ReadByte() ───┼──┐
        │                                                   │  │
        │  Esp01s_Write() ── USART1 TX ──► PA9              │  │
        └───────────────────────────────────────────────────┘  │
                                                               ▼
        ┌────────────── Task 层（业务）──────────────────────────┐
        │  parseByte() 状态机：找 "+IPD," → 读长度 → 收数据      │
        │  → handleCommandLine("FORWARD,2") → s_cmd             │
        └───────────────────────────┬───────────────────────────┘
                                    ▼
                        EspLink_GetCommand(&cmd)
                                    ▼
                        RotorTask（1 kHz）→ CAN → GM6020
```

---

# 第二部分：代码实现

## 8. 分层

| 层 | 文件 | 职责 | 不知道什么 |
|---|---|---|---|
| **BSP** | `Bsp/USART/esp01s.*`（149 行）| UART 初始化、中断收字节、环形缓冲、发原始字节 | 不知道 AT、不知道 WiFi、不知道 TCP |
| **Task** | `Task/EspLink.*`（613 行）| AT 状态机、报文解析、指令提取 | 不知道寄存器、不知道中断 |

对外只有 5 个 BSP 函数：

```c
void Esp01s_Init(void);                        // 初始化
bool Esp01s_Write(const void* data, uint32_t len);   // 发原始字节
bool Esp01s_ReadByte(uint8_t* out);            // 取 1 字节
void Esp01s_Flush(void);                       // 清接收缓冲
void Esp01s_SetBaud(uint32_t baud);            // 改波特率（探模块用）
```

---

## 9. 接收：中断 + 环形缓冲

**为什么用中断而不是轮询**：1 kHz 的控制任务不能一直等着串口。中断来一个字节就往缓冲里丢，互不干扰。

**为什么用环形缓冲而不是 HAL 的回调**：`HAL_UART_Receive_IT` 每收一个字节要走一遍 `HAL_UART_IRQHandler`（判一堆标志）+ 回调 + 重新武装，开销是直接读 `DR` 的好几倍。自己写 ISR 只有十几条指令。

```c
// USART1_IRQHandler（Bsp/USART/esp01s.cpp）
extern "C" void USART1_IRQHandler(void)
{
    const uint32_t sr = USART1->SR;
    if (sr & (RXNE | ORE | FE | NE | PE)) {
        const uint8_t b = USART1->DR;   // 读一次 DR 清全部错误标志
        if (sr & RXNE) ringPushFromIsr(b);
    }
}
```

**SPSC 环形缓冲**（单生产者单消费者）：

```
中断只动 s_head（写）  ←→  任务只动 s_tail（读）
装满就丢最新字节，不阻塞
```

---

## 10. 解析：`+IPD` 状态机

因为串口是**没有边界的字节流**，必须靠状态机一点点找：

```
RX_HEADER      找 "+IPD,"（不要求行首，只看结尾 5 个字符）
    ↓
RX_LENGTH      读数字直到 ':'
    ├─ 遇到 ','  → RX_HEADER_TAIL（CIPDINFO=1 的 ip/port 段，最多跳 64 字符）
    ↓
RX_DATA        收满 len 字节 → processPayload()
    ↓
RX_HEADER      回到找包头
```

包太大（`> 256` 字节）则进 `RX_DISCARD`：**数据不缓存，但字节数必须数完**，否则后面全乱。

---

## 11. 解析：一行指令 → 结构体

```cpp
// Task/EspLink.cpp
processPayload()    按 '\n' 把载荷切成一行行
    ↓
handleCommandLine("FORWARD,2")
    ↓
s_cmd = { valid=1, dir=FORWARD, gear=2, last_rx_tick=now }
```

指令表：

| 指令 | 含义 |
|---|---|
| `FORWARD,<挡>` | 正转 |
| `REVERSE,<挡>` | 反转 |
| `STOP,<挡>` | 停 |
| `IDLE,0` | 保活：刷新时间戳（防止 800 ms 超时）但让车停住 |

**任何不合法（方向不认识 / 挡位不在 1~4）→ 整条丢弃**，保留上一条指令。

---

## 12. 超时与重试一览

| 步骤 | 单次超时 | 重试 | 备注重 |
|---|---|---|---|
| 探模块 `AT` | 500 ms | 3 次 | 失败会 `+++` 和换波特率 |
| `ATE0` / `CWMODE` / `CIPMODE` / `CIPMUX` / `CIPDINFO` | 1000 ms | — | 每条之间间隔 60 ms |
| `CWJAP` 连 WiFi | **30000 ms** | 2 次 | 失败间隔 2 s |
| `CIPSTART` 建 TCP | 5000 ms | 3 次 | 失败间隔 1 s |
| `+IPD` 断链判定 | **2000 ms** | — | 超时即认为卡死 |
| `AT+RST` 后等待 | 3000 ms | — | 连续 5 轮失败才触发 |
