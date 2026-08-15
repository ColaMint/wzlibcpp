#include "Keys.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

wz::MutableKey::MutableKey(const std::array<u8, 4> &new_iv,
                           std::vector<u8> new_aes_key)
    : iv(new_iv), aes_key(std::move(new_aes_key)) {}

u8 &wz::MutableKey::operator[](size_t index) {
  if (keys.empty() || keys.size() <= index) {
    ensure_key_size(index + 1);
  }
  return keys[index];
}

void wz::MutableKey::ensure_key_size(size_t size) {
  if (size > std::numeric_limits<size_t>::max() - (batch_size - 1))
    throw std::length_error("requested WZ key stream is too large");
  size = ((size + batch_size - 1) / batch_size) * batch_size;
  decltype(keys) new_keys;
  new_keys.reserve(size);
  if (std::all_of(iv.begin(), iv.end(), [](u8 byte) { return byte == 0; })) {
    new_keys.resize(size, 0);
    keys = std::move(new_keys);
    return;
  }
  if (aes_key.size() < 32)
    throw std::logic_error("WZ AES key is not initialized");

  size_t start_index = 0;

  if (!keys.empty()) {
    // std::copy(keys.begin(), keys.end(), new_keys.begin());
    new_keys.assign(keys.begin(), keys.end());
    start_index = keys.size();
  }

  AES aes(256, 128);

  for (size_t i = start_index; i < size; i += 16) {
    if (i == 0) {
      u8 block[16];
      for (int n = 0; n < 16; ++n) {
        block[n] = iv[n % 4];
      }
      u32 out_len;
      auto *eb = aes.EncryptECB(&block[0], 16, aes_key.data(), out_len);
      u8 buf[16];
      memcpy(buf, eb, 16);
      delete[] eb;
      for (unsigned char &n : buf) {
        new_keys.emplace_back(n);
      }
    } else {
      const size_t len = 16;
      u32 out_len;
      auto *eb = aes.EncryptECB(new_keys.data() + i - 16, len, aes_key.data(),
                                out_len);
      u8 buf[len];
      memcpy(buf, eb, 16);
      delete[] eb;
      for (unsigned char &n : buf) {
        new_keys.emplace_back(n);
      }
    }
  }

  keys = std::move(new_keys);
}
