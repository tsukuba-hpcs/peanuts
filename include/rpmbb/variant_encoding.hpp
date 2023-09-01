#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rpmbb {

class variant_compressor {
 private:
  std::vector<uint8_t> encoded_;
  size_t data_count_ = 0;

 public:
  // Encoding
  auto encode(uint64_t value) -> void {
    ++data_count_;
    while(value >= 0x80) {
      encoded_.push_back((value & 0x7F) | 0x80);
      value >>= 7;
    }
    encoded_.push_back(value);
  }

  // Decoding
  class iterator {
   private:
    const std::vector<uint8_t>& encoded_;
    size_t decode_index_ = 0;
    size_t count_;

   public:
    iterator(const std::vector<uint8_t>& encoded, size_t count)
        : encoded_(encoded), count_(count) {}

    auto operator*() -> uint64_t {
      uint64_t value = 0;
      int shift = 0;
      while (decode_index_ < encoded_.size()) {
        uint8_t byte = encoded_[decode_index_++];
        value |= (uint64_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
          return value;
        }
        shift += 7;
      }
      throw std::runtime_error("Incomplete varint sequence");
    }

    auto operator++() -> iterator& {
      --count_;
      return *this;
    }
    auto operator!=(const iterator& other) const -> bool {
      return count_ != other.count_;
    }
  };

  auto begin() const -> iterator { return {encoded_, data_count_}; }
  auto end() const -> iterator { return {encoded_, 0}; }
  auto size() const -> size_t { return data_count_; }
  auto encoded_buffer() const -> const std::vector<uint8_t>& { return encoded_; }
  auto compressed_size() const -> size_t { return encoded_.size(); }
  double compression_ratio() const {
    size_t original_size = data_count_ * sizeof(uint64_t);
    if (original_size == 0) return 0.0;
    return static_cast<double>(encoded_.size()) / original_size;
  }
};

}  // namespace rpmbb
