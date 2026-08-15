#include "Node.hpp"
#include "Directory.hpp"
#include "File.hpp"
#include "Property.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <ranges>
#include <stdexcept>

wz::Node::Node() : type(Type::NotSet), parent(nullptr), file(nullptr) {}

wz::Node::Node(const Type &new_type, File *root_file)
    : type(new_type), parent(nullptr), file(root_file) {
  if (file == nullptr)
    throw std::invalid_argument("root file must not be null");
  reader = &file->reader;
}

wz::Node::~Node() {
  for (auto *node : children) {
    delete node;
  }
}

void wz::Node::append_child(const wzstring &name, Node *node) {
  if (node == nullptr || node == this || node->parent != nullptr)
    throw std::invalid_argument("child must be a non-null, unowned node");
  auto child_path = this->path + u"/" + name;
  node->name = name;
  auto [name_it, inserted] = children_by_name.try_emplace(name);
  bool child_added = false;
  try {
    children.push_back(node);
    child_added = true;
    name_it->second.push_back(node);
  } catch (...) {
    if (child_added)
      children.pop_back();
    if (inserted)
      children_by_name.erase(name_it);
    throw;
  }
  node->parent = this;
  node->path = std::move(child_path);
}

void wz::Node::append_child(const wzstring &name, std::unique_ptr<Node> node) {
  auto *child = node.get();
  append_child(name, child);
  node.release();
}

const wz::WzList &wz::Node::get_children() const noexcept { return children; }

wz::Node *wz::Node::get_parent() const noexcept { return parent; }

wz::WzList::iterator wz::Node::begin() noexcept { return children.begin(); }

wz::WzList::iterator wz::Node::end() noexcept { return children.end(); }

wz::WzList::const_iterator wz::Node::begin() const noexcept { return children.begin(); }

wz::WzList::const_iterator wz::Node::end() const noexcept { return children.end(); }

size_t wz::Node::children_count() const noexcept { return children.size(); }

bool wz::Node::parse_property_list(Node *target, size_t offset) {
  auto entry_count = reader->read_compressed_int();
  if (entry_count < 0)
    throw std::runtime_error("invalid WZ property count");

  for (i32 i = 0; i < entry_count; i++) {
    auto name = reader->read_string_block(offset);

    auto prop_type = reader->read<u8>();
    switch (prop_type) {
    case 0: {
      auto prop = std::make_unique<wz::Property<WzNull>>(Type::Null, file);
      prop->path = target->path + u"/" + name;

      target->append_child(name, std::move(prop));
    } break;
    case 0x0B:
      [[fallthrough]];
    case 2: {
      auto prop = std::make_unique<wz::Property<u16>>(
          Type::UnsignedShort, file, reader->read<u16>());
      prop->path = target->path + u"/" + name;

      target->append_child(name, std::move(prop));
    } break;
    case 3: {
      auto prop = std::make_unique<wz::Property<i32>>(
          Type::Int, file, reader->read_compressed_int());
      prop->path = target->path + u"/" + name;

      target->append_child(name, std::move(prop));
    } break;
    case 4: {
      auto float_type = reader->read<u8>();
      if (float_type == 0x80) {
        auto prop = std::make_unique<wz::Property<f32>>(
            Type::Float, file, reader->read<f32>());
        prop->path = target->path + u"/" + name;

        target->append_child(name, std::move(prop));
      } else if (float_type == 0) {
        auto prop = std::make_unique<wz::Property<f32>>(Type::Float, file, 0.f);
        prop->path = target->path + u"/" + name;

        target->append_child(name, std::move(prop));
      } else
        throw std::runtime_error("invalid WZ float encoding");
    } break;
    case 5: {
      auto prop = std::make_unique<wz::Property<f64>>(
          Type::Double, file, reader->read<f64>());
      prop->path = target->path + u"/" + name;

      target->append_child(name, std::move(prop));
    } break;
    case 8: {
      auto prop = std::make_unique<wz::Property<wzstring>>(Type::String, file);
      prop->path = target->path + u"/" + name;

      auto str = reader->read_string_block(offset);
      prop->set(str);
      target->append_child(name, std::move(prop));
    } break;
    case 9: {
      auto ofs = reader->read<u32>();
      auto eob = reader->get_position() + ofs;
      parse_extended_prop(name, target, offset);
      if (reader->get_position() != eob)
        reader->set_position(eob);
    } break;
    case 0x14: {
      auto prop = std::make_unique<wz::Property<i64>>(
          Type::Int, file, reader->read_compressed_int());
      prop->path = target->path + u"/" + name;

      target->append_child(name, std::move(prop));
    } break;
    default: {
      throw std::runtime_error("unsupported WZ property type");
    }
    }
  }
  return true;
}

void wz::Node::parse_extended_prop(const wzstring &name, wz::Node *target,
                                   const size_t &offset) {
  auto property_type_name = reader->read_string_block(offset);

  if (property_type_name == u"Property") {
    auto prop = std::make_unique<Property<WzSubProp>>(Type::SubProperty, file);
    prop->path = target->path + u"/" + name;
    reader->skip(sizeof(u16));
    parse_property_list(prop.get(), offset);
    target->append_child(name, std::move(prop));
  } else if (property_type_name == u"Canvas") {
    auto prop = std::make_unique<Property<WzCanvas>>(Type::Canvas, file);
#ifdef __EMSCRIPTEN__
    prop->reader = this->reader;
#endif
    prop->path = target->path + u"/" + name;
    reader->skip(sizeof(u8));
    if (reader->read<u8>() == 1) {
      reader->skip(sizeof(u16));
      parse_property_list(prop.get(), offset);
    }

    prop->set(parse_canvas_property());

    target->append_child(name, std::move(prop));
  } else if (property_type_name == u"Shape2D#Vector2D") {
    auto prop = std::make_unique<Property<WzVec2D>>(Type::Vector2D, file);
    prop->path = target->path + u"/" + name;

    auto x = reader->read_compressed_int();
    auto y = reader->read_compressed_int();
    prop->set({x, y});

    target->append_child(name, std::move(prop));
  } else if (property_type_name == u"Shape2D#Convex2D") {
    auto prop = std::make_unique<Property<WzConvex>>(Type::Convex2D, file);
    prop->path = target->path + u"/" + name;

    int convex_entry_count = reader->read_compressed_int();
    if (convex_entry_count < 0)
      throw std::runtime_error("invalid WZ convex property count");
    for (int i = 0; i < convex_entry_count; i++) {
      parse_extended_prop(name, prop.get(), offset);
    }

    target->append_child(name, std::move(prop));
  } else if (property_type_name == u"Sound_DX8") {
    auto prop = std::make_unique<Property<WzSound>>(Type::Sound, file);
#ifdef __EMSCRIPTEN__
    prop->reader = this->reader;
#endif
    prop->path = target->path + u"/" + name;

    prop->set(parse_sound_property());

    target->append_child(name, std::move(prop));
  } else if (property_type_name == u"UOL") {
    reader->skip(sizeof(u8));
    auto prop = std::make_unique<Property<WzUOL>>(Type::UOL, file);
#ifdef __EMSCRIPTEN__
    prop->reader = this->reader;
#endif
    prop->path = target->path + u"/" + name;

    prop->set({reader->read_string_block(offset)});
    target->append_child(name, std::move(prop));
  } else {
    throw std::runtime_error("unsupported WZ extended property type");
  }
}

wz::WzCanvas wz::Node::parse_canvas_property() {
  WzCanvas canvas;
  canvas.width = reader->read_compressed_int();
  canvas.height = reader->read_compressed_int();
  canvas.format = reader->read_compressed_int();
  canvas.format2 = reader->read<u8>();
  reader->skip(sizeof(u32));
  canvas.size = reader->read<i32>() - 1;
  if (canvas.width < 0 || canvas.height < 0 || canvas.size < 0)
    throw std::runtime_error("invalid WZ canvas dimensions or data size");
  reader->skip(sizeof(u8));

  canvas.offset = reader->get_position();

  auto header = reader->read<u16>();

  if (header != 0x9C78 && header != 0xDA78) {
    canvas.is_encrypted = true;
  }

  switch (canvas.format + canvas.format2) {
  case 1: {
    canvas.uncompressed_size = canvas.width * canvas.height * 2;
  } break;
  case 2: {
    canvas.uncompressed_size = canvas.width * canvas.height * 4;
  } break;
  case 513: // Format16bppRgb565
  {
    canvas.uncompressed_size = canvas.width * canvas.height * 2;
  } break;
  case 517: {
    canvas.uncompressed_size = canvas.width * canvas.height / 128;
  } break;
  }

  reader->set_position(canvas.offset + canvas.size);

  return canvas;
}

wz::WzSound wz::Node::parse_sound_property() {
  WzSound sound;

  // 跳过 sound_dx8_ver (1字节)
  reader->skip(sizeof(u8));

  // 读取音频基本信息
  sound.size = reader->read_compressed_int();   // 数据长度
  sound.length = reader->read_compressed_int(); // 播放时长（毫秒）

  // 读取 sound_decl 类型
  auto sound_decl = reader->read<u8>();

  // 跳过 media_type 结构 (50字节: 16+16+1+1+16)
  reader->skip(50);

  // 如果 sound_decl == 2，读取并解析 WAVEFORMATEX
  if (sound_decl == 2) {
    auto fmt_ext_len = reader->read_compressed_int();

    if (fmt_ext_len > 0) {
      // 读取格式扩展数据
      std::vector<u8> fmt_data(fmt_ext_len);
      for (i32 i = 0; i < fmt_ext_len; i++) {
        fmt_data[i] = reader->read<u8>();
      }

      // 解析 WAVEFORMATEX 结构（至少需要 18 字节）
      if (fmt_ext_len >= 18) {
        size_t pos = 0;
        std::memcpy(&sound.format_tag, &fmt_data[pos], sizeof(sound.format_tag));
        pos += 2;
        std::memcpy(&sound.channels, &fmt_data[pos], sizeof(sound.channels));
        pos += 2;
        std::memcpy(&sound.frequency, &fmt_data[pos], sizeof(sound.frequency));
        pos += 4;
        std::memcpy(&sound.avg_bytes_per_sec, &fmt_data[pos], sizeof(sound.avg_bytes_per_sec));
        pos += 4;
        std::memcpy(&sound.block_align, &fmt_data[pos], sizeof(sound.block_align));
        pos += 2;
        std::memcpy(&sound.bits_per_sample, &fmt_data[pos], sizeof(sound.bits_per_sample));
        pos += 2;
        // cb_size 在 pos + 2，但我们不需要它
      }
    }
  }

  // 记录音频数据的起始位置
  sound.offset = reader->get_position();

  if (sound.size < 0)
    throw std::runtime_error("invalid WZ sound data size");

  // 跳过音频数据
  reader->set_position(sound.offset + sound.size);

  return sound;
}

wz::Type wz::Node::get_type() const { return type; }

const wz::wzstring &wz::Node::get_name() const noexcept { return name; }

wz::Reader *wz::Node::get_reader() const noexcept { return reader; }

bool wz::Node::is_property() const {
  return (bit(type) & bit(Type::Property)) == bit(Type::Property);
}

wz::MutableKey &wz::Node::get_key() const { return file->key; }

const u8 *wz::Node::get_iv() const { return file->iv.data(); }

wz::Node *wz::Node::get_child(const wz::wzstring &name) {
  if (auto it = children_by_name.find(name); it != children_by_name.end()) {
    return it->second[0];
  }
  return nullptr;
}

wz::Node *wz::Node::get_child(const std::string &name) {
  return get_child(std::u16string{name.begin(), name.end()});
}

wz::Node &wz::Node::operator[](const wz::wzstring &name) {
  auto *child = get_child(name);
  if (child == nullptr)
    throw std::out_of_range("WZ child does not exist");
  return *child;
}

wz::Node *wz::Node::find_from_path(const std::u16string &path) {
  auto next = std::views::split(path, u'/') | std::views::common;
  wz::Node *node = this;
  for (const auto &s : next) {
    auto str = std::u16string{s.begin(), s.end()};
    if (str.empty() || str == u".")
      continue;
    if (str == u"..") {
      if (node == nullptr || node->parent == nullptr)
        return nullptr;
      node = node->parent;
      continue;
    } else {
      if (node == nullptr)
        return nullptr;
      node = node->get_child(str);
      if (node != nullptr) {
        if (node->type == wz::Type::UOL) {
          node = dynamic_cast<wz::Property<wz::WzUOL> *>(node)->get_uol();
          if (!node) {
            return nullptr;
          }
        }
        if (node->type == wz::Type::Image) {
          auto *dir = dynamic_cast<wz::Directory *>(node);
          node = dir != nullptr ? dir->get_image() : nullptr;
          if (node == nullptr)
            return nullptr;
          continue;
        }
      } else {
        return nullptr;
      }
    }
  }
  return node;
}

wz::Node *wz::Node::find_from_path(const std::string &path) {
  return find_from_path(std::u16string{path.begin(), path.end()});
}
