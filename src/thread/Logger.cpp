namespace Logger {
thread_local std::string out;
std::mutex mutex;

export void flush() {
  std::lock_guard lock(mutex);
  std::cout << out;
  std::cout.flush();
  out.clear();
}

export void info(const std::string& msg) { out += msg + '\n'; }
}  // namespace Logger
