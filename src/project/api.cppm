export module makeDotCpp.project.api;
import std;

#include "alias.hpp"

namespace makeDotCpp {
struct Export;

namespace api {
export using PackageExports =
    std::unordered_map<std::string, std::shared_ptr<Export>>;

export struct ProjectContext {
  const std::string name;
  const PackageExports &packageExports;
  int argc;
  const char **argv;
};

export using Build = int(const ProjectContext &pCtx);

export struct CompileCommand {
  Path input;
  Path output;
  std::string command;
};

export std::deque<CompileCommand> compileCommands;
}  // namespace api
}  // namespace makeDotCpp
