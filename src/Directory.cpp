#include "Directory.hpp"

#ifdef __EMSCRIPTEN__
#include "Emscripten.hpp"
#include <ranges>

bool wz::Directory::parse_image(Node *node)
{
    auto url = "Img/" + std::string{this->path.begin(), this->path.end()};
    Emscripten::load_file(url);
    node->path = this->path;
    image_reader = std::make_unique<Reader>(get_key(), Emscripten::data(), Emscripten::size());
    node->reader = image_reader.get();
    this->reader = node->reader;
    // parse img
    if (reader->is_wz_image())
    {
        return parse_property_list(node, 0);
    }
    return true;
}
#else
bool wz::Directory::parse_image(Node *node)
{
    if (is_image())
    {
        struct PositionGuard
        {
            Reader *reader;
            size_t position;
            ~PositionGuard() { reader->set_position(position); }
        } guard{reader, reader->get_position()};

        node->reader = reader;
        node->path = this->path;
        const auto current_offset = get_offset();
        reader->set_position(current_offset);
        if (reader->is_wz_image())
        {
            return parse_property_list(node, current_offset);
        }
    }
    return false;
}
#endif
wz::Directory::Directory(File *root_file, bool is_image_node, int new_size, int new_checksum, unsigned int new_offset)
    : Node(is_image_node ? Type::Image : Type::Directory, root_file), image_node(is_image_node),
      size(new_size), checksum(new_checksum), offset(new_offset)
{
}

u32 wz::Directory::get_offset() const
{
    return offset;
}

bool wz::Directory::is_image() const
{
    return image_node;
}

wz::Node *wz::Directory::get_image()
{
    if (!is_image())
        return nullptr;
    if (!parsed_image)
    {
        auto image_node = std::make_unique<Node>(Type::NotSet, file);
        if (!parse_image(image_node.get()))
            return nullptr;
        parsed_image = std::move(image_node);
    }
    return parsed_image.get();
}
