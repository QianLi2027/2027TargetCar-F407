//
// 小工具（与平台无关，C / C++ 都能用）
//
//   clampf / clampi   C 接口：浮点 / 整数
//   clamp             C++ 接口：模板，任意类型
//

#ifndef F407_MAGICTOOLS_H
#define F407_MAGICTOOLS_H

#pragma once

// ── C 接口（C 和 C++ 都能调）────────────────────────────────
//   static inline：每个编译单元各有一份，不产生外部符号，不会链接冲突。
static inline float clampf(float value, float min_val, float max_val)
{
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

static inline int clampi(int value, int min_val, int max_val)
{
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

#ifdef __cplusplus
// ── C++ 接口：模板，任意类型（int / float / double …）────────
template <typename T>
inline T clamp(T value, T min_val, T max_val)
{
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}
#endif

#endif // F407_MAGICTOOLS_H
