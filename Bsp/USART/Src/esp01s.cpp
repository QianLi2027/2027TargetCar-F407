//
// ESP-01S 的板级支持（BSP 层）—— 实现
// ============================================================================
//
// 只做三件事：
//   ① USART1 初始化（GPIO / 波特率 / NVIC）
//   ② 中断接收 -> 无锁环形缓冲
//   ③ 原始字节收发
//
// 所有 AT 命令、连 WiFi、建 TCP、报文解析、遥测组包 —— 都在 Task/EspLink.cpp。
//
// ============================================================================

#include "esp01s.h"

#include "main.h"
#include "usart.h"

namespace
{
    // ── 接收环形缓冲（SPSC：中断写 head，任务读 tail）──────────────────
    constexpr uint16_t kRingSize = 2048;          // 必须是 2 的幂
    constexpr uint16_t kRingMask = kRingSize - 1;

    volatile uint8_t  s_ring[kRingSize];
    volatile uint16_t s_head = 0;
    volatile uint16_t s_tail = 0;

    // 中断里调用：写一个字节（满了就丢）
    void ringPushFromIsr(uint8_t b)
    {
        const uint16_t next = static_cast<uint16_t>((s_head + 1u) & kRingMask);
        if (next == s_tail)
        {
            return;
        }
        s_ring[s_head] = b;
        __DMB();
        s_head = next;
    }

    // 任务里调用：清空
    void ringFlush()
    {
        s_tail = s_head;
    }

    // 任务里调用：取一个字节
    bool ringPop(uint8_t* out)
    {
        const uint16_t head = s_head;
        const uint16_t tail = s_tail;
        if (head == tail) return false;

        __DMB();
        *out = s_ring[tail];
        s_tail = static_cast<uint16_t>((tail + 1u) & kRingMask);
        return true;
    }

    // 清溢出标志并重新允许 RXNE 中断
    void enableRxInterrupt()
    {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    }

}

// ── USART1 中断：只搬字节，不做解析、不调用 FreeRTOS API ─────────────────
extern "C" void USART1_IRQHandler(void)
{
    const uint32_t sr = USART1->SR;

    // 有 RXNE / ORE / 帧错误 / 噪声 / 校验错误时读 DR（读一次清全部标志）
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE))
    {
        const uint8_t b = static_cast<uint8_t>(USART1->DR);
        if (sr & USART_SR_RXNE)
        {
            ringPushFromIsr(b);
        }
    }
}

// ── 对外接口 ────────────────────────────────────────────────────────────
void Esp01s_Init(void)
{
    s_head = 0;
    s_tail = 0;

    // 用不到的两个脚（PB6 / PA10）释放成输入，避免浮空
    //   ⚠️ PA9(TX) / PB7(RX) 已由 CubeMX 的 HAL_UART_MspInit 配好，这里不再重复配置
    GPIO_InitTypeDef gpio = {};
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin  = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin  = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA, &gpio);

    // RX 改成内部上拉（CubeMX 里配的是 NOPULL）：
    //   ESP-01S 没供电时 PB7 会悬空乱跳，触发大量接收中断把 CPU 吃满。
    //   （如果以后在 CubeMX 里把 PB7 的 Pull-up 勾上，这一段也可以删掉）
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    gpio.Pin       = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &gpio);

    // ⚠️ 这个 NVIC 必须手写：CubeMX 一旦勾上 USART1 中断，
    //    stm32f4xx_it.c 会生成同名 USART1_IRQHandler -> 和本文件的重复定义。
    // 优先级 6：数值上低于 configMAX_SYSCALL_INTERRUPT_PRIORITY(5)，
    //   会被 FreeRTOS 临界区屏蔽，且 ISR 内没有调用 RTOS API。
    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    enableRxInterrupt();   // 清 ORE + 开 RXNE
}

bool Esp01s_Write(const void* data, uint32_t len)
{
    if (data == nullptr || len == 0) return false;

    return HAL_UART_Transmit(&huart1,
                             static_cast<const uint8_t*>(data),
                             static_cast<uint16_t>(len), 200) == HAL_OK;
}

bool Esp01s_ReadByte(uint8_t* out)
{
    if (out == nullptr) return false;
    return ringPop(out);
}

void Esp01s_Flush(void)
{
    ringFlush();
}

void Esp01s_SetBaud(uint32_t baud)
{
    huart1.Init.BaudRate = baud;
    (void)HAL_UART_Init(&huart1);
    ringFlush();
    enableRxInterrupt();
}
