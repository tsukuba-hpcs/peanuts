#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace rpmbb::utils {

[[noreturn]] __attribute__((noinline)) inline void die(const char* fmt, ...) {
  constexpr int slen = 256;
  static char msg[slen];

  std::va_list args;
  va_start(args, fmt);
  std::vsnprintf(msg, slen, fmt, args);
  va_end(args);

  std::fprintf(stderr, "\x1b[31m%s\x1b[39m\n", msg);
  std::fflush(stderr);

  std::abort();
}

template <typename T>
inline T getenv_with_default(const char* env_var, T default_val) {
  if (const char* val_str = std::getenv(env_var)) {
    T val;
    std::stringstream ss(val_str);
    ss >> val;
    if (ss.fail()) {
      die("Environment variable '%s' is invalid.\n", env_var);
    }
    return val;
  } else {
    return default_val;
  }
}

}  // namespace rpmbb::utils
