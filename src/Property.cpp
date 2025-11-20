#include <zlib.h>
#include "Property.hpp"
#include "Types.hpp"


// get Canvas node raw data (原始压缩数据，不解密不解压)
template <>
std::vector<u8> wz::Property<wz::WzCanvas>::get_raw_data()
{
    WzCanvas canvas = get();
    reader->set_position(canvas.offset);
    return reader->read_bytes(canvas.size);
}

// get Canvas node parsed data (解密并解压后的像素数据)
template <>
std::vector<u8> wz::Property<wz::WzCanvas>::get_parsed_data()
{
    WzCanvas canvas = get();

    std::vector<u8> compressed_data;
    reader->set_position(canvas.offset);
    size_t end_offset = reader->get_position() + canvas.size;
    unsigned long uncompressed_len = canvas.uncompressed_size;
    u8 *uncompressed = new u8[uncompressed_len];

    if (!canvas.is_encrypted)
    {
        // 未加密：直接读取压缩数据
        compressed_data = reader->read_bytes(canvas.size);
        uncompress(uncompressed, (unsigned long *)&uncompressed_len, compressed_data.data(), compressed_data.size());
    }
    else
    {
        // 已加密：边读取边解密
        auto wz_key = get_key();

        while (reader->get_position() < end_offset)
        {
            auto block_size = reader->read<i32>();
            for (size_t i = 0; i < block_size; ++i)
            {
                auto n = wz_key[i];
                compressed_data.push_back(
                    static_cast<u8>(reader->read_byte() ^ n));
            }
        }
        uncompress(uncompressed, (unsigned long *)&uncompressed_len, compressed_data.data(), compressed_data.size());
    }

    std::vector<u8> pixel_stream(uncompressed, uncompressed + uncompressed_len);
    delete[] uncompressed;
    return pixel_stream;
}

// get Sound node raw data (原始二进制数据，不做任何处理)
template <>
std::vector<u8> wz::Property<wz::WzSound>::get_raw_data()
{
    WzSound sound = get();
    reader->set_position(sound.offset);
    return reader->read_bytes(sound.size);
}

// get Sound node parsed data (可播放的音频数据: PCM 添加 WAV header，MP3 直接返回)
template <>
std::vector<u8> wz::Property<wz::WzSound>::get_parsed_data()
{
    WzSound sound = get();

    // 获取原始音频数据
    std::vector<u8> data_stream = get_raw_data();

    // 根据音频格式处理
    if (sound.format_tag == 1) {  // PCM 格式，需要添加 WAV header
        std::vector<u8> result;
        result.reserve(44 + data_stream.size());

        // RIFF header
        result.push_back('R'); result.push_back('I');
        result.push_back('F'); result.push_back('F');

        // 文件大小 - 8
        u32 chunk_size = 36 + data_stream.size();
        result.push_back(chunk_size & 0xFF);
        result.push_back((chunk_size >> 8) & 0xFF);
        result.push_back((chunk_size >> 16) & 0xFF);
        result.push_back((chunk_size >> 24) & 0xFF);

        // WAVE
        result.push_back('W'); result.push_back('A');
        result.push_back('V'); result.push_back('E');

        // fmt chunk
        result.push_back('f'); result.push_back('m');
        result.push_back('t'); result.push_back(' ');

        // fmt chunk size (16 for PCM)
        u32 fmt_size = 16;
        result.push_back(fmt_size & 0xFF);
        result.push_back((fmt_size >> 8) & 0xFF);
        result.push_back((fmt_size >> 16) & 0xFF);
        result.push_back((fmt_size >> 24) & 0xFF);

        // WAVEFORMATEX fields
        // wFormatTag
        result.push_back(sound.format_tag & 0xFF);
        result.push_back((sound.format_tag >> 8) & 0xFF);

        // nChannels
        result.push_back(sound.channels & 0xFF);
        result.push_back((sound.channels >> 8) & 0xFF);

        // nSamplesPerSec
        result.push_back(sound.frequency & 0xFF);
        result.push_back((sound.frequency >> 8) & 0xFF);
        result.push_back((sound.frequency >> 16) & 0xFF);
        result.push_back((sound.frequency >> 24) & 0xFF);

        // nAvgBytesPerSec
        result.push_back(sound.avg_bytes_per_sec & 0xFF);
        result.push_back((sound.avg_bytes_per_sec >> 8) & 0xFF);
        result.push_back((sound.avg_bytes_per_sec >> 16) & 0xFF);
        result.push_back((sound.avg_bytes_per_sec >> 24) & 0xFF);

        // nBlockAlign
        result.push_back(sound.block_align & 0xFF);
        result.push_back((sound.block_align >> 8) & 0xFF);

        // wBitsPerSample
        result.push_back(sound.bits_per_sample & 0xFF);
        result.push_back((sound.bits_per_sample >> 8) & 0xFF);

        // data chunk
        result.push_back('d'); result.push_back('a');
        result.push_back('t'); result.push_back('a');

        // data size
        u32 data_size = data_stream.size();
        result.push_back(data_size & 0xFF);
        result.push_back((data_size >> 8) & 0xFF);
        result.push_back((data_size >> 16) & 0xFF);
        result.push_back((data_size >> 24) & 0xFF);

        // 追加音频数据
        result.insert(result.end(), data_stream.begin(), data_stream.end());

        return result;
    } else {  // MP3 或其他格式，直接返回原始数据
        return data_stream;
    }
}

// get uol By uol node
template <>
wz::Node *wz::Property<wz::WzUOL>::get_uol()
{
    auto path = get().uol;
    auto uol_node = parent->find_from_path(path);
    while (uol_node->type == wz::Type::UOL)
    {
        path = dynamic_cast<wz::Property<wz::WzUOL> *>(uol_node)->get().uol;
        uol_node = uol_node->parent->find_from_path(path);
    }

    return uol_node;
}
