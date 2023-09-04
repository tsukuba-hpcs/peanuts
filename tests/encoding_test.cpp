#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include "rpmbb/delta_encoding.hpp"
#include "rpmbb/varint_encoding.hpp"
using namespace rpmbb;

#include <iostream>
using namespace std;

TEST_CASE("Delta Encoding and Decoding Test") {
  delta_encoder encoder;
  delta_decoder decoder;

  std::vector<uint64_t> original_values = {0, 5, 5, 10, 20, 20, 40, 80};
  std::vector<uint64_t> encoded_values;
  std::vector<uint64_t> decoded_values;

  // Delta encoding
  for (const auto& val : original_values) {
    encoded_values.push_back(encoder.encode(val));
  }

  // Delta decoding
  for (const auto& delta : encoded_values) {
    decoded_values.push_back(decoder.decode(delta));
  }

  // Checking if the decoded values match the original values
  CHECK(original_values == decoded_values);
}

TEST_CASE("Delta Encoding and Decoding with Corner Cases") {
  delta_encoder encoder;
  delta_decoder decoder;

  std::vector<uint64_t> original_values = {
      0, std::numeric_limits<uint64_t>::max(),
      std::numeric_limits<uint64_t>::max(),
      std::numeric_limits<uint64_t>::max() - 1};
  std::vector<uint64_t> encoded_values;
  std::vector<uint64_t> decoded_values;

  // Delta encoding
  for (const auto& val : original_values) {
    encoded_values.push_back(encoder.encode(val));
  }

  // Delta decoding
  for (const auto& delta : encoded_values) {
    decoded_values.push_back(decoder.decode(delta));
  }

  // Checking if the decoded values match the original values
  CHECK(original_values == decoded_values);
}

TEST_CASE_TEMPLATE("Verint Encoing and Decoding using function",
                   T,
                   uint64_t,
                   uint32_t,
                   size_t) {
  std::string buffer;
  std::vector<T> original = {10, 100, 1000, 10000};
  std::vector<T> decoded;

  for (const auto& val : original) {
    rpmbb::encode_varint(val, buffer);
  }

  size_t index = 0;
  for (size_t i = 0; i < original.size(); ++i) {
    decoded.push_back(rpmbb::decode_varint<T>(buffer, index));
  }

  CHECK(original == decoded);
}

TEST_CASE_TEMPLATE("Varint Encoding and Decoding using varint_compressor",
                   T,
                   uint64_t,
                   uint32_t,
                   size_t) {
  rpmbb::varint_compressor<T> compressor;
  std::vector<T> original = {10, 100, 1000, 10000};
  std::vector<T> decoded;

  for (const auto& val : original) {
    compressor.encode(val);
  }

  for (const auto& val : compressor) {
    decoded.push_back(val);
  }

  CHECK(original == decoded);
}

TEST_CASE_TEMPLATE("Encoded data count", T, uint64_t, uint32_t, size_t) {
  rpmbb::varint_compressor<T> compressor;
  CHECK(compressor.size() == 0);  // Initial size should be 0

  compressor.encode(10);
  CHECK(compressor.size() == 1);  // One data point encoded

  compressor.encode(100);
  CHECK(compressor.size() == 2);  // Two data points encoded
}

TEST_CASE_TEMPLATE("Varint Encoding Corner and 7-bit Boundary Cases",
                   T,
                   uint64_t,
                   uint32_t,
                   size_t) {
  rpmbb::varint_compressor<T> compressor;
  std::vector<T> original = {
      0,
      std::numeric_limits<T>::max(),  // max for T
      1ull << 7,                      // 2^7
      1ull << 14,                     // 2^14
      1ull << 21,                     // 2^21
      1ull << 28                      // 2^28
  };
  std::vector<T> decoded;

  for (const auto& val : original) {
    compressor.encode(val);
  }

  for (const auto& val : compressor) {
    decoded.push_back(val);
  }

  CHECK(original == decoded);
}

TEST_CASE_TEMPLATE("Compression Ratio Test", T, uint64_t, uint32_t, size_t) {
  rpmbb::varint_compressor<T> compressor;
  std::vector<T> original = {
      0,                             // 0
      1,                             // 1
      127,                           // 127
      1ull << 7,                     // 2^7
      1ull << 14,                    // 2^14
      1ull << 21,                    // 2^21
      1ull << 28,                    // 2^28
      std::numeric_limits<T>::max()  // max for T
  };

  for (const auto& val : original) {
    compressor.encode(val);
  }

  size_t original_size = original.size() * sizeof(T);
  size_t compressed_size = compressor.compressed_size();
  double calculated_ratio =
      static_cast<double>(compressed_size) / original_size;

  CHECK(calculated_ratio ==
        doctest::Approx(compressor.compression_ratio()).epsilon(0.001));
}

TEST_CASE_TEMPLATE("Multiple calls to operator*",
                   T,
                   uint32_t,
                   uint64_t,
                   size_t) {
  varint_compressor<T> compressor;

  std::vector<T> original = {10, 100, 1000, 10000};
  std::vector<T> decoded;

  // Encode values
  for (const auto& val : original) {
    compressor.encode(val);
  }

  auto it = compressor.begin();
  auto end = compressor.end();
  while (it != end) {
    auto value = *it;  // First call to operator*()
    CHECK(value ==
          *it);  // Second call to operator*(), should be the same value
    CHECK(value ==
          *it);  // Third call to operator*(), should still be the same value

    decoded.push_back(value);
    ++it;
  }

  CHECK(original == decoded);
}
