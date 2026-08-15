#include <cassert>
#include <vector>
#include <codecvt>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include "Reader.hpp"
#include "Keys.hpp"

#ifdef __EMSCRIPTEN__
#include "Emscripten.hpp"

wz::Reader::Reader(wz::MutableKey &new_key, const char *file_path)
    : url(file_path), key(new_key), cursor(0)
{
}

wz::Reader::Reader(wz::MutableKey &new_key, const unsigned char *data, size_t size)
    : buffer_data(data, data + size), key(new_key), cursor(0)
{
}

mio::mmap_source::size_type wz::Reader::size() const
{
    return buffer_data.size();
}
#else
wz::Reader::Reader(wz::MutableKey &new_key, const char *file_path)
    : key(new_key), cursor(0)
{
    std::error_code error_code;
    mmap = mio::make_mmap_source<decltype(file_path)>(file_path, error_code);
    if (error_code)
        throw std::system_error(error_code, file_path);
}

mio::mmap_source::size_type wz::Reader::size() const
{
    return mmap.size();
}
#endif

u8 wz::Reader::read_byte()
{
    return read<u8>();
}

[[maybe_unused]] std::vector<u8> wz::Reader::read_bytes(const size_t &len)
{
    ensure_available(len);
    std::vector<u8> result(len);

#ifdef __EMSCRIPTEN__
    // Emscripten 环境：从 buffer_data 批量复制
    std::memcpy(result.data(), buffer_data.data() + cursor, len);
    cursor += len;
#else
    // 非 Emscripten 环境：从 mmap 批量复制
    std::memcpy(result.data(), &mmap[cursor], len);
    cursor += len;
#endif

    return result;
}

wz::wzstring wz::Reader::read_string()
{
    wz::wzstring result{};

    while (true)
    {
        auto c = static_cast<char>(read_byte());
        if (!c)
            break;
        result.push_back(c);
    }

    return result;
}

wz::wzstring wz::Reader::read_string(const size_t &len)
{
    wz::wzstring result{};

    for (size_t i = 0; i < len; ++i)
    {
        result.push_back(read_byte());
    }

    return result;
}

void wz::Reader::set_position(const size_t &size)
{
    if (size > this->size())
        throw std::out_of_range("reader position is outside the input");
    cursor = size;
}

size_t wz::Reader::get_position() const
{
    return cursor;
}

void wz::Reader::skip(const size_t &size)
{
    ensure_available(size);
    cursor += size;
}

i32 wz::Reader::read_compressed_int()
{
    i32 result = static_cast<i32>(read<i8>());
    if (result == INT8_MIN)
        return read<i32>();
    return result;
}

i16 wz::Reader::read_i16()
{
    i16 result = static_cast<i16>(read<i16>());
    return result;
}

wz::wzstring wz::Reader::read_wz_string()
{
    auto len8 = read<i8>();

    if (len8 == 0)
        return {};

    i32 len;

    if (len8 > 0)
    {
        u16 mask = 0xAAAA;

        len = len8 == 127 ? read<i32>() : len8;

        if (len <= 0)
        {
            return {};
        }
        if (static_cast<size_t>(len) > (size() - cursor) / sizeof(u16))
            throw std::out_of_range("WZ string exceeds the remaining input");

        wz::wzstring result{};

        for (int i = 0; i < len; ++i)
        {
            auto encrypted_char = read<u16>();
            encrypted_char ^= mask;
            const auto key_word = static_cast<u16>(key[2 * i]) |
                                  (static_cast<u16>(key[2 * i + 1]) << 8u);
            encrypted_char ^= key_word;
            result.push_back(encrypted_char);
            mask++;
        }

        return result;
    }

    u8 mask = 0xAA;

    if (len8 == -128)
    {
        len = read<i32>();
    }
    else
    {
        len = -len8;
    }

    if (len <= 0)
    {
        return {};
    }
    if (static_cast<size_t>(len) > size() - cursor)
        throw std::out_of_range("WZ string exceeds the remaining input");

    wz::wzstring result{};

    for (int n = 0; n < len; ++n)
    {
        u8 encrypted_char = read_byte();
        encrypted_char ^= mask;
        encrypted_char ^= key[n];
        result.push_back(static_cast<u16>(encrypted_char));
        mask++;
    }

    return result;
}

bool wz::Reader::is_wz_image()
{
    if (read<u8>() != 0x73)
        return false;
    if (read_wz_string() != u"Property")
        return false;
    if (read<u16>() != 0)
        return false;
    return true;
}

wz::wzstring wz::Reader::read_string_block(const size_t &offset)
{
    switch (read<u8>())
    {
    case 0:
        [[fallthrough]];
    case 0x73:
        return read_wz_string();
    case 1:
        [[fallthrough]];
    case 0x1B:
        return read_wz_string_from_offset(offset + read<u32>());
    default:
    {
        throw std::runtime_error("invalid WZ string block type");
    }
    }
    return {};
}

wz::wzstring wz::Reader::read_wz_string_from_offset(const size_t &offset)
{
    auto prev = get_position();
    set_position(offset);
    try
    {
        auto result = read_wz_string();
        set_position(prev);
        return result;
    }
    catch (...)
    {
        set_position(prev);
        throw;
    }
}

void wz::Reader::ensure_available(size_t length) const
{
    if (cursor > size() || length > size() - cursor)
        throw std::out_of_range("unexpected end of WZ data");
}
