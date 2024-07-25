module;
#include <boost/process.hpp>

module makeDotCpp.thread.process;
import std;
import makeDotCpp.thread.logger;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
namespace process {
namespace bp = boost::process;

std::unordered_map<std::string, std::string> exeCache;

const std::string findExecutable(const std::string &name) {
  if (!exeCache.contains(name))
    exeCache.emplace(name, bp::search_path(name).generic_string());
  return exeCache.at(name);
}

const Result run(const std::string &command) {
  Result result{command};
  bp::ipstream is;
  bp::child child(command, (bp::std_out & bp::std_err) > is);
  std::error_code ec;
  std::string line;
  while (child.running(ec) && std::getline(is, line)) {
    result.output += line + '\n';
    if (ec) {
      logger::error() << result.output << std::endl;
      throw ProcessError(ec);
    }
  }
  child.wait();
  while (std::getline(is, line)) result.output += line + '\n';
  result.status = child.exit_code();
  return result;
}

int runNoRedirect(const std::string &command) {
  std::error_code ec;
  auto status = bp::system(command, ec);
  if (ec) throw ProcessError(ec);
  return status;
}
}  // namespace process
}  // namespace makeDotCpp
