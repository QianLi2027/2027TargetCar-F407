//
// ESP-01S 的板级支持（BSP 层）
// ============================================================================
//
// 【这一层的职责】只保证"能把字节发出去、能把字节收进来"。
//   不含任何 AT 命令、不含 WiFi/TCP、不含协议解析。
//   那些属于业务，放在 Task/EspLink.cpp。
//
// 【提供什么】
//   Esp01s_Init()          上电初始化：GPIO + USART1 + NVIC
//   Esp01s_Write()         发一段原始字节
//   Esp01s_ReadByte()      从接收缓冲取 1 字节（没有就返回 false）
//   Esp01s_Flush()         清空接收缓冲
//   Esp01s_SetBaud()       改波特率（重新初始化 USART1 + 清缓冲）
//
// 【接收怎么做的】
//   USART1 中断 -> 无锁环形缓冲（SPSC：中断写 head，任务读 tail）
//   ISR 里只搬字节，不调用任何 FreeRTOS API。
//
// 【接线】C 板实测：TX = PA9，RX = PB7（混搭，不是标准的 PA9/PA10）
//
// ⚠️ USART1_IRQHandler 定义在本文件。不要在 CubeMX 里再勾选 USART1 的 NVIC，
//    否则 stm32f4xx_it.c 会生成同名函数导致重复定义。
//
// ============================================================================

#ifndef F407_ESP01S_H
#define F407_ESP01S_H

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 GPIO / USART1 / NVIC，并打开接收中断。
void Esp01s_Init(void);

// 发一段原始字节（阻塞，超时 200ms）。返回 true 表示发送成功。
bool Esp01s_Write(const void* data, uint32_t len);

// 从接收缓冲里取一个字节。取到了返回 true。
bool Esp01s_ReadByte(uint8_t* out);

// 丢弃接收缓冲里还没读的数据。
void Esp01s_Flush(void);

// 改波特率并重新打开接收中断（会顺带清空接收缓冲）。
void Esp01s_SetBaud(uint32_t baud);

#ifdef __cplusplus
}
#endif

#endif // F407_ESP01S_H
