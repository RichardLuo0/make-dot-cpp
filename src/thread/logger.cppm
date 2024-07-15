export module makeDotCpp.thread.logger;
import std;

namespace makeDotCpp {
namespace logger {
thread_local std::string out;
std::mutex mutex;

export void flush() {
  std::lock_guard lock(mutex);
  std::cout << out;
  std::cout.flush();
  out.clear();
}

export void info(const std::string& msg) { out += msg + '\n'; }

export const std::string reset = "\033[0m";

#define GENERATE_COLOR(NAME, CODE) \
  export const std::string NAME = "\033[0;" #CODE;

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
}  // namespace logger
}  // namespace makeDotCpp
