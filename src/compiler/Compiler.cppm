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

  virtual Compiler &addOption(const std::string &option) = 0;
  virtual Compiler &addLinkOption(const std::string &option) = 0;

  virtual std::string getModuleSuffix() const = 0;

#define GENERATE_COMPILE_METHOD(NAME, ARGS, PASS_ARGS) \
  virtual process::Result NAME ARGS const {            \
    ensureParentExists(output);                        \
    return process::run(NAME##Command PASS_ARGS);      \
  }                                                    \
                                                       \
  virtual std::string NAME##Command ARGS const = 0;

  GENERATE_COMPILE_METHOD(
      compileModule,
      (const Path &input, const Path &output,
       const std::unordered_map<std::string, Path> &moduleMap = {},
       const std::string &extraOptions = ""),
      (input, output, moduleMap, extraOptions));
  GENERATE_COMPILE_METHOD(
      compile,
      (const Path &input, const Path &output, bool isDebug = false,
       const std::unordered_map<std::string, Path> &moduleMap = {},
       const std::string &extraOptions = ""),
      (input, output, isDebug, moduleMap, extraOptions));
  GENERATE_COMPILE_METHOD(link,
                          (const std::vector<Path> &input, const Path &output,
                           bool isDebug = false,
                           const std::string &extraOptions = ""),
                          (input, output, isDebug, extraOptions));
  GENERATE_COMPILE_METHOD(archive,
                          (const std::vector<Path> &input, const Path &output),
                          (input, output));
  GENERATE_COMPILE_METHOD(createSharedLib,
                          (const std::vector<Path> &input, const Path &output,
                           const std::string &extraOptions = ""),
                          (input, output, extraOptions));
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
