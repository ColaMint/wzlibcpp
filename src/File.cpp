#include <cassert>
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "File.hpp"
#include "Wz.hpp"
#include "Directory.hpp"
#ifdef __EMSCRIPTEN__
#include "Emscripten.hpp"
#include <ranges>

bool wz::File::parse(const wzstring &name)
{
    root = std::make_unique<Node>(Type::NotSet, this);
    reader.url = std::string{name.begin(), name.end()};
    root->path = name;
    return parse_directories(root.get());
}

bool wz::File::parse_directories(wz::Node *node)
{
    auto path = reader.url;
    auto url = "Img/" + reader.url + "/Directory.txt";

    Emscripten::load_file(url);

    std::string data(reinterpret_cast<const char *>(Emscripten::data()), Emscripten::size());
    std::replace(data.begin(), data.end(), '\r', '\n');

    for (const auto &s : std::views::split(data, u'\n') | std::views::common)
    {
        if (s.size() == 0)
        {
            continue;
        }
        auto file_name = std::u16string{s.begin(), s.end()};
        const bool is_image = file_name.find(u".img") != wzstring::npos;
        auto dir = std::make_unique<Directory>(this, is_image, 0, 0, 0);
        auto *dir_ptr = dir.get();
        node->append_child(file_name, std::move(dir));
        if (!is_image)
        {
            reader.url = path + "/" + std::string{file_name.begin(), file_name.end()};
            if (!parse_directories(dir_ptr))
                return false;
        }
        reader.url = path;
    }
    return true;
}
#else
bool wz::File::parse(const wzstring &name)
{
    root = std::make_unique<Node>(Type::NotSet, this);
    reader.set_position(0);
    auto magic = reader.read_string(4);
    if (magic != u"PKG1")
        return false;

    [[maybe_unused]] auto file_size = reader.read<u64>();
    auto start_at = reader.read<u32>();

    [[maybe_unused]] auto copyright = reader.read_string();

    reader.set_position(start_at);

    auto encrypted_version = reader.read<i16>();

    for (int i = 0; i < 0x7FFF; ++i)
    {
        i16 file_version = static_cast<decltype(file_version)>(i);
        u32 version_hash = wz::get_version_hash(encrypted_version, file_version);

        if (version_hash != 0)
        {
            desc.start = start_at;
            desc.hash = version_hash;
            desc.version = file_version;

            auto prev_position = reader.get_position();

            bool valid = false;
            try
            {
                valid = parse_directories(nullptr);
            }
            catch (const std::exception &)
            {
                valid = false;
            }
            if (!valid)
            {
                reader.set_position(prev_position);
                continue;
            }
            else
            {
                if (root)
                {
                    root->path = name;
                    reader.set_position(prev_position);
                    if (!parse_directories(root.get()))
                        return false;
                }
                return true;
            }
        }
    }

    return false;
}

bool wz::File::parse_directories(wz::Node *node)
{
    auto entry_count = reader.read_compressed_int();
    if (entry_count < 0)
        return false;

    for (int i = 0; i < entry_count; ++i)
    {
        auto type = reader.read_byte();
        size_t previous_position = 0;
        wzstring name;

        if (type == 1)
        {
            reader.skip(sizeof(i32) + sizeof(u16));

            get_wz_offset();
            continue;
        }
        else if (type == 2)
        {
            i32 string_offset = reader.read<i32>();
            type = reader.read_wz_string_from_offset<u8>(desc.start + string_offset, name);
        }
        else if (type == 3 || type == 4)
        {
            name = reader.read_wz_string();
        }
        else
        {
            return false;
        }

        i32 size = reader.read_compressed_int();
        i32 checksum = reader.read_compressed_int();
        u32 offset = get_wz_offset();

        if (node == nullptr && offset >= reader.size())
            return false;

        if (type == 3)
        {
            if (node != nullptr)
            {
                auto dir = std::make_unique<Directory>(this, false, size, checksum, offset);
                node->append_child(name, std::move(dir));
            }
        }
        else
        {
            if (node != nullptr)
            {
                auto dir = std::make_unique<Directory>(this, true, size, checksum, offset);
                node->append_child(name, std::move(dir));
            }
            else
            {
                previous_position = reader.get_position();
                reader.set_position(offset);

                if (!reader.is_wz_image())
                    return false;

                reader.set_position(previous_position);
            }
        }
    }

    if (node != nullptr)
    {
        for (auto *child : *node)
        {
            auto *dir = dynamic_cast<Directory *>(child);

            if (dir != nullptr)
            {
                if (!dir->is_image())
                {
                    reader.set_position(dir->get_offset());
                    if (!parse_directories(dir))
                        return false;
                }
            }
        }
    }

    return true;
}
#endif
[[maybe_unused]] wz::File::File(const std::initializer_list<u8> &new_iv, const char *path)
    : key(), reader(key, path), root(std::make_unique<Node>(Type::NotSet, this))
{
    if (new_iv.size() != 4)
        throw std::invalid_argument("WZ IV must contain exactly four bytes");
    std::copy(new_iv.begin(), new_iv.end(), iv.begin());
    init_key();
}

[[maybe_unused]] wz::File::File(const u8 *new_iv, const char *path)
    : key(), reader(key, path), root(std::make_unique<Node>(Type::NotSet, this))
{
    if (new_iv == nullptr)
        throw std::invalid_argument("WZ IV must not be null");
    std::copy_n(new_iv, iv.size(), iv.begin());
    init_key();
}

wz::File::~File()
{
}

u32 wz::File::get_wz_offset()
{
    u32 offset = static_cast<u32>(reader.get_position());
    offset = ~(offset - desc.start);
    offset *= desc.hash;
    offset -= wz::offset_key;
    offset = std::rotl(offset, static_cast<int>(offset & 0x1Fu));
    u32 encrypted_offset = reader.read<u32>();
    offset ^= encrypted_offset;
    offset += desc.start * 2;
    return offset;
}

wz::Node *wz::File::get_root() const
{
    return root.get();
}

void wz::File::init_key()
{
    std::vector<u8> aes_key_v(32);
    memcpy(aes_key_v.data(), wz::aes_key_2, 32);
    key = MutableKey(iv, aes_key_v);
}

wz::Node &wz::File::get_child(const wzstring &name)
{
    return (*root)[name];
}
