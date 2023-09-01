#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include "rpmbb/delta_encoding.hpp"
#include "rpmbb/variant_encoding.hpp"
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

TEST_CASE("Variant Encoding and Decoding") {
  variant_compressor compressor;
  std::vector<uint64_t> original = {10, 100, 1000, 10000};
  std::vector<uint64_t> decoded;

  for (const auto& val : original) {
    compressor.encode(val);
  }

  for (const auto& val : compressor) {
    decoded.push_back(val);
  }

  CHECK(original == decoded);
}

TEST_CASE("Encoded data count") {
  variant_compressor compressor;
  CHECK(compressor.size() == 0);  // Initial size should be 0

  compressor.encode(10);
  CHECK(compressor.size() == 1);  // One data point encoded

  compressor.encode(100);
  CHECK(compressor.size() == 2);  // Two data points encoded
}

TEST_CASE("Variant Encoding Corner and 7-bit Boundary Cases") {
  variant_compressor compressor;
  std::vector<uint64_t> original = {
      0,
      std::numeric_limits<uint64_t>::max(),  // UINT64_MAX
      1ull << 7,                             // 2^7
      1ull << 14,                            // 2^14
      1ull << 21,                            // 2^21
      1ull << 28                             // 2^28
  };
  std::vector<uint64_t> decoded;

  for (const auto& val : original) {
    compressor.encode(val);
  }

  for (const auto& val : compressor) {
    decoded.push_back(val);
  }

  CHECK(original == decoded);
}

TEST_CASE("Compression Ratio Test") {
  variant_compressor compressor;
  std::vector<uint64_t> original = {
      0,          1,          127,        1ull << 7,
      1ull << 14, 1ull << 21, 1ull << 28, std::numeric_limits<uint64_t>::max()};

  for (const auto& val : original) {
    compressor.encode(val);
  }

  size_t original_size = original.size() * sizeof(uint64_t);
  size_t compressed_size = compressor.compressed_size();
  double calculated_ratio =
      static_cast<double>(compressed_size) / original_size;

  // MESSAGE("Original size: ", original_size, " bytes");
  // MESSAGE("Compressed size: ", compressed_size, " bytes");
  // MESSAGE("Calculated ratio: ", calculated_ratio);
  // MESSAGE("Actual ratio: ", compressor.compression_ratio());

  CHECK(calculated_ratio ==
        doctest::Approx(compressor.compression_ratio()).epsilon(0.001));
}
