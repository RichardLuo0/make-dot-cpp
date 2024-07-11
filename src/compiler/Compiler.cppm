export module makeDotCpp.compiler;
import std;
import makeDotCpp.thread.process;

#include "alias.hpp"

namespace makeDotCpp {
export struct ModuleInfo {
  bool exported = false;
  std::string name;
  std::deque<std::string> deps;
};

export class Compiler {
 public:
  virtual ~Compiler() = default;

  virtual Compiler &addOption(std::string option) = 0;
  virtual Compiler &addLinkOption(std::string option) = 0;

#define GENERATE_COMPILE_METHOD(NAME, ARGS)    \
  virtual process::Result NAME ARGS const = 0; \
  virtual std::string NAME##Command ARGS const = 0;

  GENERATE_COMPILE_METHOD(
      compilePCM, (const Path &input, const Path &output,
                   const std::unordered_map<std::string, Path> &moduleMap = {},
                   const std::string &extraOptions = ""));
  GENERATE_COMPILE_METHOD(
      compile, (const Path &input, const Path &output, bool isDebug = false,
                const std::unordered_map<std::string, Path> &moduleMap = {},
                const std::string &extraOptions = ""));
  GENERATE_COMPILE_METHOD(link, (const std::vector<Path> &input,
                                 const Path &output, bool isDebug = false,
                                 const std::string &extraOptions = ""));
  GENERATE_COMPILE_METHOD(archive,
                          (const std::vector<Path> &input, const Path &output));
  GENERATE_COMPILE_METHOD(createSharedLib,
                          (const std::vector<Path> &input, const Path &output,
                           const std::string &extraOptions = ""));
#undef GENERATE_COMPILE_METHOD

  virtual std::deque<Path> getIncludeDeps(
      const Path &input, const std::string &extraOptions = "") const = 0;

  virtual ModuleInfo getModuleInfo(
      const Path &input, const std::string &extraOptions = "") const = 0;

 protected:
  void ensureParentExists(const Path &path) const {
    fs::create_directories(path.parent_path());
  }
};
}  // namespace makeDotCpp
