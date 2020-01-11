#pragma once

#include <stdexcept>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <math.h>
#include <bitset>

// 2 to the power of x
#define XIL_BIT(x) (1 << (x))

// set first x bits to 1
#define XIL_BITS(x) (XIL_BIT(x) - 1)

#define XIL_UNUSED(x) ((void)x)

#define XIL_U16_SWAP(x) ((x >> 8) | (x << 8))

#define XIL_U32_SWAP(x) (((x >> 24)  & 0x000000ff) | \
                         ((x >> 8)   & 0x0000ff00) | \
                         ((x << 8)   & 0x00ff0000) | \
                         ((x << 24)  & 0xff000000))

#ifdef _WIN32
    #define XIL_MEMCPY(dst, dst_size, src, src_size) memcpy_s(dst, dst_size, src, src_size)
    #define XIL_READ(bytes, dst, dst_size, file) fread_s(dst, dst_size, sizeof(uint8_t), bytes, file)
    #define XIL_OPEN_FILE(file, path) fopen_s(&file, path.c_str(), "rb")
#else
    #define XIL_MEMCPY(dst, dst_size, src, src_size) memcpy(dst, src, src_size)
    #define XIL_READ(bytes, dst, dst_size, file) fread(dst, sizeof(uint8_t), bytes, file)
    #define XIL_OPEN_FILE(file, path) file = fopen(path.c_str(), "rb")
#endif

#define XIL_READ_EXACTLY(bytes, dst, dst_size, file) (bytes == XIL_READ(bytes, dst, dst_size, file))



// the only valid pre c++20 compile time endianness detection?
#define XIL_IS_LITTLE_ENDIAN ('ABCD' == 0x41424344UL)
#define XIL_IS_BIG_ENDIAN    ('ABCD' == 0x44434241UL)

#if !XIL_IS_LITTLE_ENDIAN && !XIL_IS_BIG_ENDIAN
#error Either your system is middle endian or my code is broken, sorry!
#endif

namespace XIL {

    enum class byte_order
    {
        UNDEFINED = 0,
        LITTLE = 1,
        BIG = 2
    };

    inline constexpr byte_order host_endiannes()
    {
        return XIL_IS_LITTLE_ENDIAN ? byte_order::LITTLE : byte_order::BIG;
    }

    inline static uint8_t highest_set_bit(uint32_t x)
    {
        uint8_t set = 0;

        if (!x)
            throw std::runtime_error("Cannot extract highest set bit for a 0");

        if (x >= 0x10000) { set += 16; x >>= 16; }
        if (x >= 0x00100) { set += 8;  x >>= 8; }
        if (x >= 0x00010) { set += 4;  x >>= 4; }
        if (x >= 0x00004) { set += 2;  x >>= 2; }
        if (x >= 0x00002) { set += 1; }

        return set;
    }

    inline static uint8_t count_bits(uint32_t x)
    {
        return static_cast<uint8_t>(std::bitset<32>(x).count());
    }
}
