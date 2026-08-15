#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/fetch.h>
#include <string>
#include <iostream>
#include <vector>

namespace wz
{
    class Emscripten
    {
    public:
        static void load_file(std::string &url);
        static void load_file(std::u16string &url);

        [[nodiscard]] static const unsigned char *data() noexcept { return file_data.data(); }
        [[nodiscard]] static size_t size() noexcept { return file_data.size(); }

    private:
        static inline std::vector<unsigned char> file_data;
        static inline volatile bool fetch_done = false;

        static void fetch_file_success(emscripten_fetch_t *fetch);
        static void fetch_file_failure(emscripten_fetch_t *fetch);
    };
}
#endif
