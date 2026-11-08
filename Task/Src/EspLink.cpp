//
// ESP-01S 链路任务（Task 层 / 业务）
// ============================================================================
//
// 【这一层干什么】
//   ① AT 状态机：探模块 -> 配置 -> 连 WiFi -> 建 TCP -> 断线重连
//   ② 解析 "+IPD,<len>:<payload>"，取出 "FORWARD,2" 这样的指令
//
//   只收不发：断链 -> 指令作废 -> RotorTask 把目标归零停车。
//
// 【不在这里的】
//   UART 初始化、收发字节、环形缓冲 —— 都在 Bsp/USART/esp01s.cpp（纯驱动）。
//   本文件只调 Esp01s_Write / Esp01s_ReadByte / Esp01s_Flush / Esp01s_SetBaud。
//
// 【链路拓扑】
//   ESP32 热点 TARGET_CAR / 192.168.4.1:3333
//        ▲ TCP
//   ESP-01S（当网卡用）
//        │ USART1 115200
//   C 板 STM32F407  ← 本文件
//
// ============================================================================

#include "EspLink.h"

#include "esp01s.h"       // BSP：原始字节收发

#include "main.h"         // HAL_GetTick / HAL 类型
#include "usart.h"

#include <cstring>

#include "FreeRTOS.h"
#include "task.h"

namespace
{
    // ── 共享指令（valid/dir/gear/last_rx_tick 用临界区保护）────────────
    EspLinkCommand s_cmd = {};

    volatile uint32_t s_lastPayloadTick = 0;

    // ── +IPD 解析状态 ───────────────────────────────────────────────────
    enum RxState : uint8_t
    {
        RX_HEADER = 0,
        RX_LENGTH,
        RX_HEADER_TAIL,
        RX_DATA,
        RX_DISCARD,
    };

    constexpr uint16_t kPayloadMax = 256;
    constexpr uint16_t kCmdLineMax = 64;
    constexpr uint16_t kAtLineMax  = 96;

    RxState  s_rxState = RX_HEADER;
    char     s_atLine[kAtLineMax];
    uint16_t s_atLineLen = 0;
    uint32_t s_ipdLen = 0;
    uint32_t s_ipdIndex = 0;
    uint16_t s_tailSkip = 0;
    uint8_t  s_payload[kPayloadMax];
    char     s_cmdLine[kCmdLineMax];
    uint16_t s_cmdLineLen = 0;

    volatile bool s_wifiLost = false;
    volatile bool s_linkClosed = false;

    constexpr uint32_t kLinkTimeoutMs = 2000;   // 2s 没收到 +IPD 就认为断链

    struct AtExpect
    {
        const char* token;
        bool        success;
    };

    // 毫秒延时（AT 流程里到处要等，包一层省事）
    void sleepMs(uint32_t ms)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    // 把当前指令标记成"无效"（临界区）。
    //   断链、配置失败、建 TCP 失败时都调它 -> 控制任务立刻把目标当 0 停车。
    void commandInvalidate()
    {
        taskENTER_CRITICAL();
        s_cmd.valid = 0;
        taskEXIT_CRITICAL();
    }

    // 解析一行指令："FORWARD,2" / "REVERSE,3" / "STOP,1" / "IDLE,0"
    //   手写解析不用 sscanf：这里只需要整数，而且 newlib-nano 的浮点 scanf 不可靠。
    //   方向不认识、挡位不在 1~4 -> 直接丢弃，不更新指令（保留上一条）。
    void handleCommandLine(const char* line)
    {
        // 格式：DIR,gear        （DIR = FORWARD / REVERSE / STOP / IDLE）

        char dir[16] = {0};
        const char* p = line;

        int di = 0;
        while (*p != '\0' && *p != ',' && di < 15) dir[di++] = *p++;
        dir[di] = '\0';
        if (*p == ',') ++p;

        // 只读挡位一个整数
        unsigned gear = 0;
        while (*p >= '0' && *p <= '9')
        {
            gear = gear * 10u + static_cast<unsigned>(*p - '0');
            ++p;
        }
        if (gear > 9) gear = 9;   // 防止异常长数字


        if (std::strcmp(dir, "IDLE") == 0)
        {
            taskENTER_CRITICAL();
            s_cmd.valid        = 1;
            s_cmd.dir          = ESLINK_DIR_STOP;
            s_cmd.last_rx_tick = HAL_GetTick();
            taskEXIT_CRITICAL();
            return;
        }

        if (gear < 1 || gear > 4) return;

        uint8_t d;
        if      (std::strcmp(dir, "FORWARD") == 0) d = ESLINK_DIR_FORWARD;
        else if (std::strcmp(dir, "REVERSE") == 0) d = ESLINK_DIR_REVERSE;
        else if (std::strcmp(dir, "STOP")    == 0) d = ESLINK_DIR_STOP;
        else return;

        taskENTER_CRITICAL();
        s_cmd.valid = 1;
        s_cmd.dir = d;
        s_cmd.gear = static_cast<uint8_t>(gear);
        s_cmd.last_rx_tick = HAL_GetTick();
        taskEXIT_CRITICAL();
    }

    // 处理一个完整的 +IPD 载荷：它是一行或多行 ASCII，按换行切开逐行解析。
    void processPayload(const uint8_t* data, uint32_t len)
    {
        for (uint32_t i = 0; i < len; ++i)
        {
            const char c = static_cast<char>(data[i]);
            if (c == '\n' || c == '\r')
            {
                if (s_cmdLineLen > 0)
                {
                    s_cmdLine[s_cmdLineLen] = '\0';
                    handleCommandLine(s_cmdLine);
                    s_cmdLineLen = 0;
                }
            }
            else if (s_cmdLineLen < kCmdLineMax - 1)
            {
                s_cmdLine[s_cmdLineLen++] = c;
            }
            else
            {
                s_cmdLineLen = 0;   // 超长行直接丢弃
            }
        }

        s_lastPayloadTick = HAL_GetTick();
    }

    // 读完全度后决定接下来的字节怎么处理：
    //   长度 0        -> 空包，回到找包头
    //   长度 > 上限   -> 丢弃模式（字节照数，但不缓存）
    //   正常          -> 进入数据模式
    void startPayload()
    {
        s_ipdIndex = 0;
        if (s_ipdLen == 0)
        {
            s_rxState = RX_HEADER;
        }
        else if (s_ipdLen > kPayloadMax)
        {
            s_ipdIndex = s_ipdLen;   // DISCARD 用剩余字节数
            s_rxState = RX_DISCARD;
        }
        else
        {
            s_rxState = RX_DATA;
        }
    }

    // 清空解析状态（每次建链前后、发送新命令前调用）
    void resetParser()
    {
        s_rxState = RX_HEADER;
        s_atLineLen = 0;
        s_cmdLineLen = 0;
        s_ipdLen = 0;
        s_ipdIndex = 0;
        s_tailSkip = 0;
        s_wifiLost = false;
        s_linkClosed = false;
    }

    // +IPD 报文的逐字节状态机。
    //   串口是纯字节流，必须先找到 "+IPD,<长度>:" 这个包头，
    //   才知道后面多少字节是真正的数据，不能一上来就当指令解析。
    void parseByte(char c)
    {
        switch (s_rxState)
        {
        case RX_HEADER:
            if (c == '\n')
            {
                s_atLine[s_atLineLen] = '\0';
                if (std::strstr(s_atLine, "WIFI DISCONNECT") != nullptr) s_wifiLost = true;
                if (std::strstr(s_atLine, "CLOSED") != nullptr) s_linkClosed = true;
                handleCommandLine(s_atLine);   // 兼容透传固件的裸指令
                s_atLineLen = 0;
            }
            else if (c != '\r')
            {
                if (s_atLineLen < kAtLineMax - 1)
                {
                    s_atLine[s_atLineLen++] = c;
                }
                else
                {
                    s_atLineLen = 0;
                }
            }

            // 不要求行首，找到 "+IPD," 就进入长度解析
            if (s_atLineLen >= 5 &&
                std::memcmp(s_atLine + s_atLineLen - 5, "+IPD,", 5) == 0)
            {
                s_atLineLen = 0;
                s_ipdLen = 0;
                s_rxState = RX_LENGTH;
            }
            break;

        case RX_LENGTH:
            if (c >= '0' && c <= '9')
            {
                if (s_ipdLen <= 99999u)
                {
                    s_ipdLen = s_ipdLen * 10u + static_cast<uint32_t>(c - '0');
                }
                else
                {
                    s_rxState = RX_HEADER;   // 长度离谱，重新找包头
                }
            }
            else if (c == ':')
            {
                s_rxState = RX_HEADER;
                startPayload();
            }
            else if (c == ',')
            {
                // CIPDINFO=1 时格式是 +IPD,<len>,<ip>,<port>:<data>
                s_tailSkip = 64;
                s_rxState = RX_HEADER_TAIL;
            }
            else
            {
                s_rxState = RX_HEADER;
            }
            break;

        case RX_HEADER_TAIL:
            if (c == ':')
            {
                s_rxState = RX_HEADER;
                startPayload();
            }
            else if (--s_tailSkip == 0)
            {
                s_rxState = RX_HEADER;
            }
            break;

        case RX_DATA:
            s_payload[s_ipdIndex++] = static_cast<uint8_t>(c);
            if (s_ipdIndex >= s_ipdLen)
            {
                processPayload(s_payload, s_ipdLen);
                s_rxState = RX_HEADER;
            }
            break;

        case RX_DISCARD:
            if (--s_ipdIndex == 0)
            {
                s_rxState = RX_HEADER;
            }
            break;
        }
    }

    // ── AT 命令收发 ─────────────────────────────────────────────────────
    bool atSend(const char* cmd)
    {
        Esp01s_Flush();
        return Esp01s_Write(cmd, static_cast<uint32_t>(std::strlen(cmd))) &&
               Esp01s_Write("\r\n", 2);
    }

    // 等应答：逐行读回来，和 expects[] 里的关键字比对。
    //   返回匹配到的那一条；超时返回 nullptr。
    //   count 是表的长度，顺序有意义（"CONNECT FAIL" 必须排在 "CONNECT" 前面）。
    const AtExpect* atWait(const AtExpect* expects, uint8_t count, uint32_t timeoutMs)
    {
        char line[160];
        uint16_t len = 0;
        const uint32_t start = HAL_GetTick();

        while ((HAL_GetTick() - start) < timeoutMs)
        {
            uint8_t b;
            while (Esp01s_ReadByte(&b))
            {
                if (b == '\n')
                {
                    line[len] = '\0';
                    for (uint8_t i = 0; i < count; ++i)
                    {
                        if (std::strstr(line, expects[i].token) != nullptr)
                        {
                            return &expects[i];
                        }
                    }
                    len = 0;
                }
                else if (b != '\r')
                {
                    if (len < sizeof(line) - 1)
                    {
                        line[len++] = static_cast<char>(b);
                    }
                    else
                    {
                        len = 0;
                    }
                }
            }
            sleepMs(2);
        }
        return nullptr;
    }

    // 发命令 + 等应答，一条龙。
    //   只有匹配到 success=true 的那条才返回 true。
    bool atExec(const char* cmd, const AtExpect* expects, uint8_t count, uint32_t timeoutMs)
    {
        if (!atSend(cmd)) return false;
        const AtExpect* match = atWait(expects, count, timeoutMs);
        return (match != nullptr) && match->success;
    }

    // ── 探测 / 配置 / 入网 / 建链 ───────────────────────────────────────
    bool probeAt(uint32_t timeoutMs)
    {
        static const AtExpect expectOk[] = {{"OK", true}};
        for (int i = 0; i < 3; ++i)
        {
            if (atExec("AT", expectOk, 1, timeoutMs)) return true;
        }
        return false;
    }

    // 确保模块处于 AT 指令模式（可能卡在透传模式里）
    //   ① 直接发 AT 试
    //   ② 失败 -> 发 "+++" 退出透传（前后要静默，且不能带换行）
    //   ③ 再失败 -> 强制 115200 波特率重试
    bool ensureAt()
    {
        if (probeAt(500)) return true;

        // 可能停在 AT+CIPMODE=1 透传模式。+++ 前后要有静默，且不能带 CRLF。
        Esp01s_Flush();
        (void)Esp01s_Write("+++", 3);
        sleepMs(1500);
        (void)Esp01s_Write("\r\n", 2);
        sleepMs(200);
        Esp01s_Flush();
        if (probeAt(500)) return true;

        // 波特率探测：原厂 AT 常见 115200，部分模组出厂是 9600
        Esp01s_SetBaud(115200);
        sleepMs(100);
        return probeAt(1000);
    }

    // 配置 AT 链路（每次重连都做一遍）
    //   ATE0          关回显（否则收到的字节里混着发出去的命令）
    //   AT+CWMODE=1   设成 STA（连别人热点）
    //   AT+CIPMODE=0  关透传（老固件可能不支持，失败也继续）
    //   AT+CIPMUX=0   单连接
    //   AT+CIPDINFO=0 让 +IPD 不带 ip/port（解析简单）
    bool setupLink()
    {
        static const AtExpect expectOk[] = {{"OK", true}};

        if (!atExec("ATE0", expectOk, 1, 1000)) return false;
        sleepMs(60);
        if (!atExec("AT+CWMODE=1", expectOk, 1, 1000)) return false;
        sleepMs(60);
        (void)atExec("AT+CIPMODE=0", expectOk, 1, 1000);   // 老固件可能不支持
        sleepMs(60);
        if (!atExec("AT+CIPMUX=0", expectOk, 1, 1000)) return false;
        sleepMs(60);
        (void)atExec("AT+CIPDINFO=0", expectOk, 1, 1000);  // 让 +IPD 不带 ip/port
        sleepMs(60);
        return true;
    }

    // 连 WiFi 热点（名称/密码写死在这里）
    //   AT+CWJAP 最长要等 30 秒，失败重试 2 次
    bool joinAp()
    {
        static const AtExpect expect[] =
        {
            {"FAIL",    false},
            {"+CWJAP:", false},
            {"ERROR",   false},
            {"OK",      true},
        };

        for (int i = 0; i < 2; ++i)
        {
            if (atExec("AT+CWJAP=\"TARGET_CAR\",\"cquqianli\"", expect, 4, 30000))
            {
                return true;
            }
            sleepMs(2000);
        }
        return false;
    }

    // 建 TCP：连 ESP32 的 192.168.4.1:3333
    //   ⚠️ 匹配表里 "CONNECT FAIL" 必须排在 "CONNECT" 前面，
    //      否则失败会被当成成功。
    bool connectTcp()
    {
        static const AtExpect expect[] =
        {
            {"CONNECT FAIL",      false},   // 必须排在 "CONNECT" 前面
            {"ALREADY CONNECTED", true},
            {"CONNECT",           true},
            {"OK",                true},
            {"ERROR",             false},
            {"no ip",             false},
        };
        return atExec("AT+CIPSTART=\"TCP\",\"192.168.4.1\",3333", expect, 6, 5000);
    }

    // 已连上 TCP 时的运行循环
    //   ① 把收到的字节喂给 +IPD 解析器
    //   ② 收到 WIFI DISCONNECT / CLOSED -> 立刻返回（交给主循环重连）
    //   ③ 2 秒没收到 +IPD -> 也返回（链路卡死）
    void runOnline()
    {
        s_wifiLost = false;
        s_linkClosed = false;

        for (;;)
        {
            uint8_t b;
            while (Esp01s_ReadByte(&b))
            {
                parseByte(static_cast<char>(b));
            }

            if (s_wifiLost || s_linkClosed) return;

            const uint32_t now = HAL_GetTick();
            if ((now - s_lastPayloadTick) > kLinkTimeoutMs) return;

            sleepMs(2);
        }
    }

    // 链路任务主循环：连接状态机
    //   探模块 -> 配置 -> 入网 -> 建 TCP -> 在线收数据 -> 断了重来
    //   连续 5 轮失败会发 AT+RST 复位模块，从头再来。
    void espTask(void* arg)
    {
        (void)arg;

        sleepMs(1500);   // 等 ESP-01S 启动日志吐完

        bool wifiJoined = false;
        uint32_t tcpFailCount = 0;
        uint32_t hardFailCount = 0;

        static const AtExpect expectClose[] = {{"OK", true}, {"ERROR", false}};

        for (;;)
        {
            if (!ensureAt())
            {
                commandInvalidate();
                wifiJoined = false;
                sleepMs(2000);
                continue;
            }

            if (!setupLink())
            {
                commandInvalidate();
                (void)atExec("AT+CIPCLOSE", expectClose, 2, 1000);
                wifiJoined = false;
                sleepMs(1000);
                continue;
            }

            if (!wifiJoined)
            {
                if (!joinAp())
                {
                    commandInvalidate();
                    sleepMs(3000);
                    continue;
                }
                wifiJoined = true;
            }

            bool connected = false;
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                Esp01s_Flush();
                resetParser();
                if (connectTcp())
                {
                    connected = true;
                    break;
                }
                sleepMs(1000);
            }

            if (!connected)
            {
                commandInvalidate();
                if (++tcpFailCount >= 2)
                {
                    wifiJoined = false;   // 连续失败，重新入网
                    tcpFailCount = 0;
                }
                sleepMs(1000);
                continue;
            }

            tcpFailCount = 0;

            resetParser();
            s_lastPayloadTick = HAL_GetTick();
            runOnline();

            commandInvalidate();

            if (s_wifiLost) wifiJoined = false;

            // 关掉旧连接；ESP32 那边 send 失败后会回到 accept 等重连
            (void)atExec("AT+CIPCLOSE", expectClose, 2, 1000);
            sleepMs(500);

            // 连续多轮失败就复位 ESP-01S，从头再来
            if (++hardFailCount >= 5)
            {
                hardFailCount = 0;
                (void)Esp01s_Write("AT+RST\r\n", 8);
                sleepMs(3000);
                Esp01s_Flush();
                wifiJoined = false;
            }
        }
    }

} // namespace


void EspLink_Task_Init(void)
{
    (void)xTaskCreate(espTask, "esp01s", 1024, nullptr, tskIDLE_PRIORITY + 2, nullptr);
}

void EspLink_GetCommand(EspLinkCommand* out)
{
    if (out == nullptr) return;

    taskENTER_CRITICAL();
    out->valid = s_cmd.valid;
    out->dir = s_cmd.dir;
    out->gear = s_cmd.gear;
    out->last_rx_tick = s_cmd.last_rx_tick;
    taskEXIT_CRITICAL();
}
