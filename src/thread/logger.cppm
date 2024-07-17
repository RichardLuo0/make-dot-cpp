export module makeDotCpp.thread.logger;
import std;

#include "alias.hpp"

namespace makeDotCpp {
namespace logger {
thread_local std::string out;
std::mutex mutex;

export const std::string reset = "\033[0m";

export void flush() {
  std::lock_guard lock(mutex);
  std::cout << out;
  std::cout.flush();
  out.clear();
}

using EndlType = decltype(std::endl<char, std::char_traits<char>>);

struct Logger {
 protected:
  std::string content;
  bool isFlushed = false;

  std::string flushContent() {
    isFlushed = true;
    return content + reset + '\n';
  }

 public:
  Logger() {}
  Logger(const std::string& init) : content(init) {}

  template <class T>
  auto& operator<<(const T& msg) {
    content += std::string(msg);
    return *this;
  }

  auto& operator<<(const Path& path) {
    return operator<<(path.generic_string());
  }

  auto& operator<<(Logger& logger) {
    content += logger.flushContent();
    return *this;
  }

  void operator<<(EndlType&) {
    out += flushContent();
    flush();
  }

  ~Logger() {
    if (!isFlushed) out += flushContent();
  }
};

#define GENERATE_COLOR(NAME, CODE) \
  export Logger NAME() { return {"\033[0;" #CODE}; };

GENERATE_COLOR(defaultColor, 39m);
GENERATE_COLOR(black, 30m);
GENERATE_COLOR(red, 31m);
GENERATE_COLOR(green, 32m);
GENERATE_COLOR(yellow, 33m);
GENERATE_COLOR(blue, 34m);
GENERATE_COLOR(magenta, 35m);
GENERATE_COLOR(cyan, 36m);
GENERATE_COLOR(gray, 37m);
GENERATE_COLOR(darkGray, 90m);
GENERATE_COLOR(brightRed, 91m);
GENERATE_COLOR(brightGreen, 92m);
GENERATE_COLOR(brightYellow, 93m);
GENERATE_COLOR(brightBlue, 94m);
GENERATE_COLOR(brightMagenta, 95m);
GENERATE_COLOR(brightCyan, 96m);
GENERATE_COLOR(white, 97m);
#undef GENERATE_COLOR

export Logger info() { return {}; }
export auto success = green, warn = yellow, error = red;
}  // namespace logger
}  // namespace makeDotCpp
