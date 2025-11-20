#pragma once

#include <string>
#include "NumTypes.hpp"

namespace wz {

    using wzstring = std::u16string;

    struct WzNull {
    };

    struct WzSubProp {
    };

    struct WzConvex {
    };

    struct WzUOL {
        [[maybe_unused]]
        wzstring uol;
    };

    struct WzCanvas {
        i32 width;
        i32 height;
        i32 format;
        i32 format2;
        bool is_encrypted;
        i32 size;
        i32 uncompressed_size;
        size_t offset;

        WzCanvas()
                : width(0), height(0),
                  format(0), format2(0),
                  is_encrypted(false), size(0), uncompressed_size(0),
                  offset(0) {}
    };

    struct WzSound {
        i32 length;              // 播放时长（毫秒）
        i32 size;                // 音频数据大小
        size_t offset;           // 音频数据偏移

        // WAVEFORMATEX 字段
        u16 format_tag;          // 格式标签：1=PCM, 0x55=MP3
        u16 channels;            // 声道数
        i32 frequency;           // 采样率
        i32 avg_bytes_per_sec;   // 平均字节率
        u16 block_align;         // 块对齐
        u16 bits_per_sample;     // 采样位数

        WzSound() : length(0), size(0), offset(0),
                    format_tag(0), channels(0), frequency(0),
                    avg_bytes_per_sec(0), block_align(0), bits_per_sample(0) {}
    };

    struct WzVec2D {

        i32 x;
        i32 y;

        WzVec2D() : x(0), y(0) {}
        WzVec2D(int new_x, int new_y) : x(new_x), y(new_y) {}
    };
}