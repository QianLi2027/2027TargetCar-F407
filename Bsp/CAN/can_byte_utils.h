//
// Created by 20852 on 2026/7/8.
//

#ifndef F407_STANDARDTEMPLATE_CAN_BYTE_UTILS_H
#define F407_STANDARDTEMPLATE_CAN_BYTE_UTILS_H

#pragma once

#include <cstdint>

static inline int16_t CanReadInt16BE(const uint8_t* data)
{
    return static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) |
                                 static_cast<uint16_t>(data[1]));
}

static inline uint16_t CanReadUint16BE(const uint8_t* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                                  static_cast<uint16_t>(data[1]));
}

static inline int16_t CanReadInt16LE(const uint8_t* data)
{
    return static_cast<int16_t>((static_cast<uint16_t>(data[1]) << 8) |
                                 static_cast<uint16_t>(data[0]));
}

static inline uint16_t CanReadUint16LE(const uint8_t* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[1]) << 8) |
                                  static_cast<uint16_t>(data[0]));
}

static inline void CanWriteInt16BE(uint8_t* data, int16_t value)
{
    data[0] = static_cast<uint8_t>((static_cast<uint16_t>(value) >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>( static_cast<uint16_t>(value)       & 0xFF);
}

static inline void CanWriteInt16LE(uint8_t* data, int16_t value)
{
    data[0] = static_cast<uint8_t>( static_cast<uint16_t>(value)       & 0xFF);
    data[1] = static_cast<uint8_t>((static_cast<uint16_t>(value) >> 8) & 0xFF);
}

static inline void CanWriteInt32LE(uint8_t* data, int32_t value)
{
    data[0] = static_cast<uint8_t>( static_cast<uint32_t>(value)        & 0xFF);
    data[1] = static_cast<uint8_t>((static_cast<uint32_t>(value) >> 8)  & 0xFF);
    data[2] = static_cast<uint8_t>((static_cast<uint32_t>(value) >> 16) & 0xFF);
    data[3] = static_cast<uint8_t>((static_cast<uint32_t>(value) >> 24) & 0xFF);
}

#endif //F407_STANDARDTEMPLATE_CAN_BYTE_UTILS_H
