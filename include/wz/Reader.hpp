#pragma once

#include <mio/mmap.hpp>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include "NumTypes.hpp"
#include "Keys.hpp"
#ifdef __EMSCRIPTEN__
#include "Emscripten.hpp"
#endif

namespace wz
{
    using wzstring = std::u16string;

    class Reader final
    {
    public:
        explicit Reader(wz::MutableKey &new_key, const char *file_path);

#ifdef __EMSCRIPTEN__
        explicit Reader(wz::MutableKey &new_key, const unsigned char *data, size_t size);

        template <typename T>
        [[nodiscard]] T read()
        {
            static_assert(std::is_trivially_copyable_v<T>);
            ensure_available(sizeof(T));
            T result;
            std::memcpy(&result, buffer_data.data() + cursor, sizeof(T));
            cursor += sizeof(T);
            return result;
        }
#else
        template <typename T>
        [[nodiscard]] T read()
        {
            static_assert(std::is_trivially_copyable_v<T>);
            ensure_available(sizeof(T));
            T result;
            std::memcpy(&result, mmap.data() + cursor, sizeof(T));
            cursor += sizeof(T);
            return result;
        }
#endif

        void skip(const size_t &size);

        [[nodiscard]] u8 read_byte();

        [[maybe_unused]] [[nodiscard]] std::vector<u8> read_bytes(const size_t &len);

        /*
         * read string until **null terminated**
         */
        [[nodiscard]] wzstring read_string();

        [[nodiscard]] wzstring read_string(const size_t &len);

        [[nodiscard]] i32 read_compressed_int();

        i16 read_i16();

        [[nodiscard]] wzstring read_wz_string();

        wzstring read_string_block(const size_t &offset);

        template <typename T>
        [[nodiscard]] T read_wz_string_from_offset(const size_t &offset, wzstring &out)
        {
            auto prev = cursor;
            set_position(offset);
            try
            {
                auto result = read<T>();
                out = read_wz_string();
                set_position(prev);
                return result;
            }
            catch (...)
            {
                set_position(prev);
                throw;
            }
        }

        wzstring read_wz_string_from_offset(const size_t &offset);

        [[nodiscard]] size_t get_position() const;

        void set_position(const size_t &size);

        [[nodiscard]] mio::mmap_source::size_type size() const;

        [[nodiscard]] bool is_wz_image();

    private:
#ifdef __EMSCRIPTEN__
        std::string url;
        std::vector<u8> buffer_data;
#endif
        MutableKey &key;

        size_t cursor = 0;

        mio::mmap_source mmap;

        void ensure_available(size_t length) const;

        explicit Reader() = delete;

        friend class Node;
        friend class File;
    };
}
