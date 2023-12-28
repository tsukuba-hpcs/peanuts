#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace rpmbb {

template <typename T>
auto encode_varint(T value, std::string& buffer) -> void {
  static_assert(std::is_unsigned_v<T>,
                "Unsigned type required for varint encoding");
  while (value >= 0x80) {
    buffer.push_back(static_cast<char>((value & 0x7F) | 0x80));
    value >>= 7;
  }
  buffer.push_back(static_cast<char>(value));
}

template <typename T>
auto decode_varint(const std::string& buffer, size_t& index) -> T {
  static_assert(std::is_unsigned_v<T>,
                "Unsigned type required for varint decoding");
  T value = 0;
  int shift = 0;
  while (index < buffer.size()) {
    char byte = buffer[index++];
    value |= static_cast<T>(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) {
      return value;
    }
    shift += 7;
  }
  throw std::runtime_error("Incomplete varint sequence");
}

template <typename T>
class varint_compressor {
 private:
  std::string buffer_;
  size_t data_count_ = 0;

 public:
  // Encoding
  auto encode(T value) -> void {
    ++data_count_;
    encode_varint(value, buffer_);
  }

  // Decoding
  class iterator {
   private:
    const std::string& buffer_;
    size_t decode_index_ = 0;
    size_t count_;
    mutable size_t next_decode_index_ = 0;

   public:
    iterator(const std::string& buffer, size_t count)
        : buffer_(buffer), count_(count) {}

    auto operator*() const -> T {
      next_decode_index_ = decode_index_;
      return decode_varint<T>(buffer_, next_decode_index_);
    }

    auto operator++() -> iterator& {
      decode_index_ = next_decode_index_;
      --count_;
      return *this;
    }

    auto operator!=(const iterator& other) const -> bool {
      return count_ != other.count_;
    }
  };

  auto begin() const -> iterator { return {buffer_, data_count_}; }
  auto end() const -> iterator { return {buffer_, 0}; }
  auto size() const -> size_t { return data_count_; }
  auto empty() const -> bool { return data_count_ == 0; }
  auto encoded_buffer() const -> const std::string& { return buffer_; }
  auto compressed_size() const -> size_t { return buffer_.size(); }

  double compression_ratio() const {
    size_t original_size = data_count_ * sizeof(T);
    if (original_size == 0)
      return 0.0;
    return static_cast<double>(buffer_.size()) / original_size;
  }
};

}  // namespace rpmbb
