#ifndef F407_ESPLINK_H
#define F407_ESPLINK_H

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 方向
enum
{
    ESLINK_DIR_STOP    = 0,
    ESLINK_DIR_FORWARD = 1,
    ESLINK_DIR_REVERSE = 2,
};

typedef struct
{
    uint8_t  valid;           // 是否收到过合法指令
    uint8_t  dir;             // ESLINK_DIR_xxx
    uint8_t  gear;            // 1 / 2 / 3 / 4
    uint32_t last_rx_tick;    // 最近一条合法指令的 HAL_GetTick()
} EspLinkCommand;

void EspLink_Task_Init(void);
    
void EspLink_GetCommand(EspLinkCommand* out);

#ifdef __cplusplus
}
#endif

#endif // F407_ESPLINK_H
