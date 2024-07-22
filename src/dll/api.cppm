export module makeDotCpp.dll.api;
import std;

#include "alias.hpp"

namespace makeDotCpp {
struct ExportFactory;
struct Compiler;

namespace api {
export using Packages =
    std::unordered_set<std::shared_ptr<const ExportFactory>>;

export struct ProjectContext {
  const std::string name;
  const Packages &packages;
  const std::shared_ptr<Compiler> compiler;
  int argc;
  const char **argv;
};

export struct CompileCommand {
  Path input;
  Path output;
  std::string command;
};

export std::deque<CompileCommand> compileCommands;

// In build.cpp
export using Build = int(const ProjectContext &pCtx);

// In library export
// SomeExportFactory someExportFactory;
// extern "C" ExportFactory& exportFactory = someExportFactory;

// In compiler dll
// SomeCompiler someCompiler;
// extern "C" Compiler& compiler = someCompiler;
}  // namespace api
}  // namespace makeDotCpp
