export module makeDotCpp.thread.process;
import std;

#include "macro.hpp"

namespace makeDotCpp {
namespace process {
export DEF_EXCEPTION(ProcessError, (const std::error_code &ec),
                     "process error: " + ec.message());

export struct Result {
  std::string command;
  std::string output;
  int status;
};

export const std::string findExecutable(const std::string &name);

export const Result run(const std::string &command);

export int runNoRedirect(const std::string &command);
}  // namespace process
}  // namespace makeDotCpp
