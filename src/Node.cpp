#include <cassert>
#include <ranges>
#include <memory>
#include <filesystem>
#include "Node.hpp"
#include "Property.hpp"
#include "File.hpp"
#include "Directory.hpp"
#include "Img.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/fetch.h>
#endif

wz::Node::Node()
    : parent(nullptr), file(nullptr), type(Type::NotSet)
{
}

wz::Node::Node(const wzstring &name)
    : path(name), parent(nullptr), file(nullptr), type(Type::NotSet)
{
}

wz::Node::Node(const Type &new_type, File *root_file)
    : parent(nullptr), file(root_file), type(new_type)
{
    reader = &file->reader;
}

wz::Node::Node(const Type &new_type, Img *img, const wzstring &name)
    : parent(nullptr), img(img), type(new_type), path(name)
{
    reader = &img->reader;
}

wz::Node::~Node()
{
    for (auto &[_, nodes] : children)
    {
        for (auto *node : nodes)
        {
            delete node;
        }
    }
}

void wz::Node::appendChild(const wzstring &name, Node *node)
{
    assert(node);
    children[name].push_back(node);
    node->parent = this;
    node->path = this->path + u"/" + name;
    return;
}

const wz::WzMap &wz::Node::get_children() const
{
    return children;
}

wz::Node *wz::Node::get_parent() const
{
    return parent;
}

wz::WzMap::iterator wz::Node::begin()
{
    return children.begin();
}

wz::WzMap::iterator wz::Node::end()
{
    return children.end();
}

size_t wz::Node::children_count() const
{
    return children.size();
}

bool wz::Node::parse_property_list(Node *target, size_t offset)
{
    auto entryCount = reader->read_compressed_int();

    for (i32 i = 0; i < entryCount; i++)
    {
        auto name = reader->read_string_block(offset);

        auto prop_type = reader->read<u8>();
        switch (prop_type)
        {
        case 0:
        {
            auto *prop = new wz::Property<WzNull>(Type::Null, file);
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 0x0B:
            [[fallthrough]];
        case 2:
        {
            auto *prop = new wz::Property<u16>(Type::UnsignedShort, file, reader->read<u16>());
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 3:
        {
            auto *prop = new wz::Property<i32>(Type::Int, file, reader->read_compressed_int());
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 4:
        {
            auto float_type = reader->read<u8>();
            if (float_type == 0x80)
            {
                auto *prop = new wz::Property<f32>(Type::Float, file, reader->read<f32>());
                prop->path = target->path + u"/" + name;

                target->appendChild(name, prop);
            }
            else if (float_type == 0)
            {
                auto *pProp = new wz::Property<f32>(Type::Float, file, 0.f);
                pProp->path = target->path + u"/" + name;

                target->appendChild(name, pProp);
            }
        }
        break;
        case 5:
        {
            auto *prop = new wz::Property<f64>(Type::Double, file, reader->read<f64>());
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 8:
        {
            auto *prop = new wz::Property<wzstring>(Type::String, file);
            prop->path = target->path + u"/" + name;

            auto str = reader->read_string_block(offset);
            prop->set(str);
            target->appendChild(name, prop);
        }
        break;
        case 9:
        {
            auto ofs = reader->read<u32>();
            auto eob = reader->get_position() + ofs;
            parse_extended_prop(name, target, offset);
            if (reader->get_position() != eob)
                reader->set_position(eob);
        }
        break;
        default:
        {
            assert(0);
        }
        }
    }
    return true;
}

bool wz::Node::parse_property_list2(Node *target, size_t offset)
{
    auto entryCount = reader->read_compressed_int();

    for (i32 i = 0; i < entryCount; i++)
    {
        auto name = reader->read_string_block(offset);

        auto prop_type = reader->read<u8>();
        switch (prop_type)
        {
        case 0:
        {
            auto *prop = new wz::Property<WzNull>(Type::Null, img);
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 0x0B:
            [[fallthrough]];
        case 2:
        {
            auto *prop = new wz::Property<u16>(Type::UnsignedShort, img, reader->read<u16>());
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 3:
        {
            auto *prop = new wz::Property<i32>(Type::Int, img, reader->read_compressed_int());
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 4:
        {
            auto float_type = reader->read<u8>();
            if (float_type == 0x80)
            {
                auto *prop = new wz::Property<f32>(Type::Float, img, reader->read<f32>());
                prop->path = target->path + u"/" + name;

                target->appendChild(name, prop);
            }
            else if (float_type == 0)
            {
                auto *pProp = new wz::Property<f32>(Type::Float, img, 0.f);
                pProp->path = target->path + u"/" + name;

                target->appendChild(name, pProp);
            }
        }
        break;
        case 5:
        {
            auto *prop = new wz::Property<f64>(Type::Double, img, reader->read<f64>());
            prop->path = target->path + u"/" + name;

            target->appendChild(name, prop);
        }
        break;
        case 8:
        {
            auto *prop = new wz::Property<wzstring>(Type::String, img);
            prop->path = target->path + u"/" + name;

            auto str = reader->read_string_block(offset);
            prop->set(str);
            target->appendChild(name, prop);
        }
        break;
        case 9:
        {
            auto ofs = reader->read<u32>();
            auto eob = reader->get_position() + ofs;
            parse_extended_prop2(name, target, offset);
            if (reader->get_position() != eob)
                reader->set_position(eob);
        }
        break;
        default:
        {
            assert(0);
        }
        }
    }
    return true;
}

void wz::Node::parse_extended_prop(const wzstring &name, wz::Node *target, const size_t &offset)
{
    auto strPropName = reader->read_string_block(offset);

    if (strPropName == u"Property")
    {
        auto *prop = new Property<WzSubProp>(Type::SubProperty, file);
        prop->path = target->path + u"/" + name;
        reader->skip(sizeof(u16));
        parse_property_list(prop, offset);
        target->appendChild(name, prop);
    }
    else if (strPropName == u"Canvas")
    {
        auto *prop = new Property<WzCanvas>(Type::Canvas, file);
        prop->path = target->path + u"/" + name;
        reader->skip(sizeof(u8));
        if (reader->read<u8>() == 1)
        {
            reader->skip(sizeof(u16));
            parse_property_list(prop, offset);
        }

        prop->set(parse_canvas_property());

        target->appendChild(name, prop);
    }
    else if (strPropName == u"Shape2D#Vector2D")
    {
        auto *prop = new Property<WzVec2D>(Type::Vector2D, file);
        prop->path = target->path + u"/" + name;

        auto x = reader->read_compressed_int();
        auto y = reader->read_compressed_int();
        prop->set({x, y});

        target->appendChild(name, prop);
    }
    else if (strPropName == u"Shape2D#Convex2D")
    {
        auto *prop = new Property<WzConvex>(Type::Convex2D, file);
        prop->path = target->path + u"/" + name;

        int convexEntryCount = reader->read_compressed_int();
        for (int i = 0; i < convexEntryCount; i++)
        {
            parse_extended_prop(name, prop, offset);
        }

        target->appendChild(name, prop);
    }
    else if (strPropName == u"Sound_DX8")
    {
        auto *prop = new Property<WzSound>(Type::Sound, file);
        prop->path = target->path + u"/" + name;

        prop->set(parse_sound_property());

        target->appendChild(name, prop);
    }
    else if (strPropName == u"UOL")
    {
        reader->skip(sizeof(u8));
        auto *prop = new Property<WzUOL>(Type::UOL, file);
        prop->path = target->path + u"/" + name;

        prop->set({reader->read_string_block(offset)});
        target->appendChild(name, prop);
    }
    else
    {
        assert(0);
    }
}

void wz::Node::parse_extended_prop2(const wzstring &name, Node *target, const size_t &offset)
{

    auto strPropName = reader->read_string_block(offset);

    if (strPropName == u"Property")
    {
        auto *prop = new Property<WzSubProp>(Type::SubProperty, img);
        prop->path = target->path + u"/" + name;
        reader->skip(sizeof(u16));
        parse_property_list2(prop, offset);
        target->appendChild(name, prop);
    }
    else if (strPropName == u"Canvas")
    {
        auto *prop = new Property<WzCanvas>(Type::Canvas, img);
        prop->path = target->path + u"/" + name;
        reader->skip(sizeof(u8));
        if (reader->read<u8>() == 1)
        {
            reader->skip(sizeof(u16));
            parse_property_list2(prop, offset);
        }

        prop->set(parse_canvas_property());

        target->appendChild(name, prop);
    }
    else if (strPropName == u"Shape2D#Vector2D")
    {
        auto *prop = new Property<WzVec2D>(Type::Vector2D, img);
        prop->path = target->path + u"/" + name;

        auto x = reader->read_compressed_int();
        auto y = reader->read_compressed_int();
        prop->set({x, y});

        target->appendChild(name, prop);
    }
    else if (strPropName == u"Shape2D#Convex2D")
    {
        auto *prop = new Property<WzConvex>(Type::Convex2D, img);
        prop->path = target->path + u"/" + name;

        int convexEntryCount = reader->read_compressed_int();
        for (int i = 0; i < convexEntryCount; i++)
        {
            parse_extended_prop2(name, prop, offset);
        }

        target->appendChild(name, prop);
    }
    else if (strPropName == u"Sound_DX8")
    {
        auto *prop = new Property<WzSound>(Type::Sound, img);
        prop->path = target->path + u"/" + name;

        prop->set(parse_sound_property());

        target->appendChild(name, prop);
    }
    else if (strPropName == u"UOL")
    {
        reader->skip(sizeof(u8));
        auto *prop = new Property<WzUOL>(Type::UOL, img);
        prop->path = target->path + u"/" + name;

        prop->set({reader->read_string_block(offset)});
        target->appendChild(name, prop);
    }
    else
    {
        assert(0);
    }
}

wz::WzCanvas wz::Node::parse_canvas_property()
{
    WzCanvas canvas;
    canvas.width = reader->read_compressed_int();
    canvas.height = reader->read_compressed_int();
    canvas.format = reader->read_compressed_int();
    canvas.format2 = reader->read<u8>();
    reader->skip(sizeof(u32));
    canvas.size = reader->read<i32>() - 1;
    reader->skip(sizeof(u8));

    canvas.offset = reader->get_position();

    auto header = reader->read<u16>();

    if (header != 0x9C78 && header != 0xDA78)
    {
        canvas.is_encrypted = true;
    }

    switch (canvas.format + canvas.format2)
    {
    case 1:
    {
        canvas.uncompressed_size = canvas.width * canvas.height * 2;
    }
    break;
    case 2:
    {
        canvas.uncompressed_size = canvas.width * canvas.height * 4;
    }
    break;
    case 513: // Format16bppRgb565
    {
        canvas.uncompressed_size = canvas.width * canvas.height * 2;
    }
    break;
    case 517:
    {
        canvas.uncompressed_size = canvas.width * canvas.height / 128;
    }
    break;
    }

    reader->set_position(canvas.offset + canvas.size);

    return canvas;
}

wz::WzSound wz::Node::parse_sound_property()
{
    WzSound sound;
    // reader->ReadUInt8();
    reader->skip(sizeof(u8));
    sound.size = reader->read_compressed_int();
    sound.length = reader->read_compressed_int();
    reader->set_position(reader->get_position() + 56);
    sound.frequency = reader->read<i32>();
    reader->set_position(reader->get_position() + 22);

    sound.offset = reader->get_position();

    reader->set_position(sound.offset + sound.size);

    return sound;
}

wz::Type wz::Node::get_type() const
{
    return type;
}

bool wz::Node::is_property() const
{
    return (bit(type) & bit(Type::Property)) == bit(Type::Property);
}

wz::MutableKey &wz::Node::get_key() const
{
    if (file != nullptr)
    {
        return file->key;
    }
    else
    {
        return img->key;
    }
}

u8 *wz::Node::get_iv() const
{
    if (file != nullptr)
    {
        return file->iv;
    }
    else
    {
        return img->iv;
    }
}

wz::Node *wz::Node::get_child(const wz::wzstring &name)
{
    if (auto it = children.find(name); it != children.end())
    {
        return it->second[0];
    }
    return nullptr;
}

wz::Node *wz::Node::get_child(const std::string &name)
{
    return get_child(std::u16string{name.begin(), name.end()});
}

wz::Node &wz::Node::operator[](const wz::wzstring &name)
{
    return *get_child(name);
}

#ifdef __EMSCRIPTEN__
// 请求成功时的回调函数
unsigned char *file_data = nullptr;
unsigned int file_size = 0;
volatile bool fetch_done = false; // 用于控制是否完成请求
void fetch_file_success(emscripten_fetch_t *fetch)
{
    file_size = fetch->numBytes;
    file_data = (unsigned char *)malloc(file_size);
    memcpy(file_data, fetch->data, fetch->numBytes);
    emscripten_fetch_close(fetch); // 关闭 fetch 对象，释放资源
    fetch_done = true;             // 请求完成
}

// 请求失败时的回调函数
void fetch_file_failure(emscripten_fetch_t *fetch)
{
    emscripten_fetch_close(fetch); // 关闭 fetch 对象，释放资源
    fetch_done = true;             // 请求完成
}

void load_file(const std::string &url)
{
    emscripten_fetch_attr_t fetchAttr;
    emscripten_fetch_attr_init(&fetchAttr);

    // 设置请求方法为 "GET"（注意：直接赋值字符串）
    strcpy(fetchAttr.requestMethod, "GET");

    // 设置成功和失败的回调函数
    fetchAttr.onsuccess = fetch_file_success;
    fetchAttr.onerror = fetch_file_failure;

    fetchAttr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    // 启动文件下载
    emscripten_fetch(&fetchAttr, url.c_str());
    // 模拟同步：等待请求完成
    while (!fetch_done)
    {
        emscripten_sleep(100); // 每100毫秒检查一次是否完成
    }
    fetch_done = false;
}

wz::Node *wz::Node::find_from_path(const std::u16string &path)
{
    std::u16string url = u"";
    // 首先去掉路径的..
    for (const auto &s : std::views::split(this->path + u"/" + path, u'/') | std::views::common)
    {
        auto str = std::u16string{s.begin(), s.end()};
        if (str == u"..")
        {
            size_t pos = url.find_last_of('/');
            url.resize(pos);
            continue;
        }
        else
        {
            if (url != u"")
            {
                url += u"/";
            }
            url += str;
        }
    }

    static std::unordered_map<std::u16string, wz::Node *> cache;
    // 获取img路径
    auto pos = url.find(u".img");
    wz::Node *cache_img_node = nullptr;
    if (pos != std::string::npos)
    {
        std::u16string img_path = url.substr(0, pos + 4);
        if (cache.contains(img_path))
        {
            cache_img_node = cache[img_path];
        }
        else
        {
            std ::string p = "Img/" + std::string{img_path.begin(), img_path.end()};
            load_file(p);
            // auto file = fopen(p.c_str(), "rb"); // 使用"rb"模式以二进制读取方式打开文件
            // if (file == NULL)
            // {
            //     perror("Error opening file");
            // }
            // // 获取文件大小
            // fseek(file, 0, SEEK_END);     // 移动到文件末尾
            // auto file_size = ftell(file); // 获取当前位置，即文件大小
            // rewind(file);

            // // 分配内存以存储文件内容
            // unsigned char *buffer = (unsigned char *)malloc(file_size);
            // if (buffer == NULL)
            // {
            //     perror("Memory allocation failed");
            //     fclose(file);
            // }

            // // 读取文件内容到buffer
            // fread(buffer, file_size, 1, file); // fread的参数依次是：目标地址，元素大小，元素数量，文件指针

            const auto iv = IV4(0xb9, 0x7d, 0x63, 0xe9);
            // auto img = new wz::Img(iv, buffer, file_size);
            auto img = new wz::Img(iv, file_data, file_size);
            auto *img_node = new wz::Node(Type::Image, img, img_path);
            img->parse_img(img_node);
            cache[img_path] = img_node;
            cache_img_node = img_node;
        }
    }
    else
    {
        if (cache.contains(url))
        {
            cache_img_node = cache[url];
        }
        else
        {
            std ::string p = "Img/" + std::string{url.begin(), url.end()} + "/Directory.txt";
            load_file(p);
            // auto file = fopen(p.c_str(), "rb"); // 使用"rb"模式以二进制读取方式打开文件
            // if (file == NULL)
            // {
            //     perror("Error opening file");
            // }
            // // 获取文件大小
            // fseek(file, 0, SEEK_END);     // 移动到文件末尾
            // auto file_size = ftell(file); // 获取当前位置，即文件大小
            // rewind(file);

            // // 分配内存以存储文件内容
            // unsigned char *buffer = (unsigned char *)malloc(file_size); // +1 为字符串结尾的'\0'
            // if (buffer == NULL)
            // {
            //     perror("Memory allocation failed");
            //     fclose(file);
            // }

            // // 读取文件内容到buffer
            // fread(buffer, file_size, 1, file); // fread的参数依次是：目标地址，元素大小，元素数量，文件指针

            auto *img_node = new wz::Node(url);
            // std::string data((const char *)buffer, file_size - 1);

            std::string data((const char *)file_data, file_size - 1);

            std::replace(data.begin(), data.end(), '\r', '\n');

            for (const auto &s : std::views::split(data, u'\n') | std::views::common)
            {
                if (s.size() == 0)
                {
                    continue;
                }
                auto file_name = std::u16string{s.begin(), s.end()};
                auto *prop = new wz::Property<wzstring>(Type::String, this->img);
                prop->set(file_name);
                img_node->appendChild(file_name, prop);
            }
            cache[url] = img_node;
            cache_img_node = img_node;
        }
        return cache_img_node;
    }

    std::u16string child_path = url.substr(pos + 4);
    wz::Node *node = cache_img_node;
    for (const auto &s : std::views::split(child_path, u'/') | std::views::common)
    {
        auto str = std::u16string{s.begin(), s.end()};
        if (str == u"")
        {
            continue;
        }
        if (str == u"..")
        {
            node = node->parent;
            continue;
        }
        else if (node != nullptr)
        {
            node = node->get_child(str);
            if (node != nullptr)
            {
                // 处理UOL
                if (node->type == wz::Type::UOL)
                {
                    node = dynamic_cast<wz::Property<wz::WzUOL> *>(node)->get_uol();
                }
            }
        }
    }
    return node;
}
#else
wz::Node *wz::Node::find_from_path(const std::u16string &path)
{
    auto next = std::views::split(path, u'/') | std::views::common;
    wz::Node *node = this;
    for (const auto &s : next)
    {
        auto str = std::u16string{s.begin(), s.end()};
        if (str == u"..")
        {
            node = node->parent;
            continue;
        }
        else
        {
            node = node->get_child(str);
            if (node != nullptr)
            {
                // 处理UOL
                if (node->type == wz::Type::UOL)
                {
                    node = dynamic_cast<wz::Property<wz::WzUOL> *>(node)->get_uol();
                }
                if (node->type == wz::Type::Image)
                {
                    static std::unordered_map<wz::Node *, wz::Node *> cache;
                    if (cache.contains(node))
                    {
                        node = cache[node];
                    }
                    else
                    {
                        auto *image = new wz::Node();
                        auto *dir = dynamic_cast<wz::Directory *>(node);
                        dir->parse_image(image);
                        cache[node] = image;
                        node = image;
                    }
                    continue;
                }
            }
            else
            {
                return nullptr;
            }
        }
    }
    return node;
}
#endif

wz::Node *wz::Node::find_from_path(const std::string &path)
{
    return find_from_path(std::u16string{path.begin(), path.end()});
}