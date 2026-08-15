#include "Property.hpp"
#include "Types.hpp"
#include <zlib.h>
#include <unordered_set>
#include <stdexcept>

namespace
{
    class PositionGuard
    {
    public:
        explicit PositionGuard(wz::Reader *reader)
            : reader_(reader), position_(reader->get_position()) {}

        ~PositionGuard() { reader_->set_position(position_); }

    private:
        wz::Reader *reader_;
        size_t position_;
    };
}

// get Canvas node raw data (原始压缩数据，不解密不解压)
template <> std::vector<u8> wz::Property<wz::WzCanvas>::get_raw_data() {
  WzCanvas canvas = get();
  auto *reader = get_reader();
  PositionGuard guard(reader);
  reader->set_position(canvas.offset);
  return reader->read_bytes(canvas.size);
}

// get Canvas node parsed data (解密并解压后的像素数据)
template <> std::vector<u8> wz::Property<wz::WzCanvas>::get_parsed_data() {
  WzCanvas canvas = get();
  auto *reader = get_reader();
  PositionGuard guard(reader);

  std::vector<u8> compressed_data;
  reader->set_position(canvas.offset);
  size_t end_offset = reader->get_position() + canvas.size;
  if (canvas.uncompressed_size <= 0)
    throw std::runtime_error("invalid WZ canvas output size");
  uLongf uncompressed_len = static_cast<uLongf>(canvas.uncompressed_size);
  std::vector<u8> pixel_stream(uncompressed_len);

  if (!canvas.is_encrypted) {
    // 未加密：直接读取压缩数据
    compressed_data = reader->read_bytes(canvas.size);
  } else {
    // 已加密：边读取边解密
    auto &wz_key = get_key();

    while (reader->get_position() < end_offset) {
      if (end_offset - reader->get_position() < sizeof(i32))
        throw std::runtime_error("truncated encrypted WZ canvas block");
      auto block_size = reader->read<i32>();
      if (block_size < 0 || static_cast<size_t>(block_size) > end_offset - reader->get_position())
        throw std::runtime_error("invalid encrypted WZ canvas block size");
      for (i32 i = 0; i < block_size; ++i) {
        auto n = wz_key[i];
        compressed_data.push_back(static_cast<u8>(reader->read_byte() ^ n));
      }
    }
  }

  const auto result = uncompress(pixel_stream.data(), &uncompressed_len,
                                 compressed_data.data(), compressed_data.size());
  // Some WZ variants contain more decoded bytes than the declared canvas
  // dimensions. Keep the declared surface and accept a completely filled
  // output buffer, matching the format's original truncation behavior.
  if (result != Z_OK &&
      !(result == Z_BUF_ERROR && uncompressed_len == pixel_stream.size()))
    throw std::runtime_error(std::string("failed to decompress WZ canvas data: ") +
                             zError(result));
  pixel_stream.resize(uncompressed_len);
  return pixel_stream;
}

// get Sound node raw data (原始二进制数据，不做任何处理)
template <> std::vector<u8> wz::Property<wz::WzSound>::get_raw_data() {
  WzSound sound = get();
  auto *reader = get_reader();
  PositionGuard guard(reader);
  reader->set_position(sound.offset);
  return reader->read_bytes(sound.size);
}

// get Sound node parsed data (可播放的音频数据: PCM 添加 WAV header，MP3
// 直接返回)
template <> std::vector<u8> wz::Property<wz::WzSound>::get_parsed_data() {
  WzSound sound = get();

  // 获取原始音频数据
  std::vector<u8> data_stream = get_raw_data();

  // 根据音频格式处理
  if (sound.format_tag == 1) { // PCM 格式，需要添加 WAV header
    std::vector<u8> result;
    result.reserve(44 + data_stream.size());

    // RIFF header
    result.push_back('R');
    result.push_back('I');
    result.push_back('F');
    result.push_back('F');

    // 文件大小 - 8
    u32 chunk_size = 36 + data_stream.size();
    result.push_back(chunk_size & 0xFF);
    result.push_back((chunk_size >> 8) & 0xFF);
    result.push_back((chunk_size >> 16) & 0xFF);
    result.push_back((chunk_size >> 24) & 0xFF);

    // WAVE
    result.push_back('W');
    result.push_back('A');
    result.push_back('V');
    result.push_back('E');

    // fmt chunk
    result.push_back('f');
    result.push_back('m');
    result.push_back('t');
    result.push_back(' ');

    // fmt chunk size (16 for PCM)
    u32 fmt_size = 16;
    result.push_back(fmt_size & 0xFF);
    result.push_back((fmt_size >> 8) & 0xFF);
    result.push_back((fmt_size >> 16) & 0xFF);
    result.push_back((fmt_size >> 24) & 0xFF);

    // WAVEFORMATEX fields
    // format_tag
    result.push_back(sound.format_tag & 0xFF);
    result.push_back((sound.format_tag >> 8) & 0xFF);

    // channels
    result.push_back(sound.channels & 0xFF);
    result.push_back((sound.channels >> 8) & 0xFF);

    // samples_per_sec
    result.push_back(sound.frequency & 0xFF);
    result.push_back((sound.frequency >> 8) & 0xFF);
    result.push_back((sound.frequency >> 16) & 0xFF);
    result.push_back((sound.frequency >> 24) & 0xFF);

    // avg_bytes_per_sec
    result.push_back(sound.avg_bytes_per_sec & 0xFF);
    result.push_back((sound.avg_bytes_per_sec >> 8) & 0xFF);
    result.push_back((sound.avg_bytes_per_sec >> 16) & 0xFF);
    result.push_back((sound.avg_bytes_per_sec >> 24) & 0xFF);

    // block_align
    result.push_back(sound.block_align & 0xFF);
    result.push_back((sound.block_align >> 8) & 0xFF);

    // bits_per_sample
    result.push_back(sound.bits_per_sample & 0xFF);
    result.push_back((sound.bits_per_sample >> 8) & 0xFF);

    // data chunk
    result.push_back('d');
    result.push_back('a');
    result.push_back('t');
    result.push_back('a');

    // data size
    u32 data_size = data_stream.size();
    result.push_back(data_size & 0xFF);
    result.push_back((data_size >> 8) & 0xFF);
    result.push_back((data_size >> 16) & 0xFF);
    result.push_back((data_size >> 24) & 0xFF);

    // 追加音频数据
    result.insert(result.end(), data_stream.begin(), data_stream.end());

    return result;
  } else { // MP3 或其他格式，直接返回原始数据
    return data_stream;
  }
}

// get uol By uol node
template <> wz::Node *wz::Property<wz::WzUOL>::get_uol() {
  static thread_local std::unordered_set<const Node *> resolving;
  if (!resolving.insert(this).second)
    return nullptr;
  struct ResolutionGuard {
    std::unordered_set<const Node *> &nodes;
    const Node *node;
    ~ResolutionGuard() { nodes.erase(node); }
  } guard{resolving, this};

  auto path = get().uol;
  auto *parent = get_parent();
  if (parent == nullptr)
    return nullptr;
  auto uol_node = parent->find_from_path(path);

  if (!uol_node) {
    return nullptr;
  }

  return uol_node;
}
