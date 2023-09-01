#include <rpmbb/version.h>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include <chrono>
#include <cxxopts.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

using ordered_json = nlohmann::ordered_json;

auto main(int argc, char const* argv[]) -> int {
  cxxopts::Options options("devcpp", "devcpp - a template for c++ projects");
  // clang-format off
  options.add_options()
    ("h,help", "Print usage")
    ("V,version", "Print version")
    ("prettify", "Prettify output")
  ;
  // clang-format on

  auto parsed = options.parse(argc, argv);
  if (parsed.count("help") != 0U) {
    fmt::print("{}\n", options.help());
    return 0;
  }

  if (parsed.count("version") != 0U) {
    fmt::print("{}\n", RPMBB_VERSION);
    return 0;
  }

  ordered_json tscc_result = {
      {"version", RPMBB_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
  };

  if (parsed.count("prettify") != 0U) {
    std::cout << std::setw(4);
  }
  std::cout << tscc_result << std::endl;

  return 0;
}
