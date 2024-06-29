export class Clang : public Compiler {
 public:
  defException(ScanDepsError, (const Path &input),
               "scanning deps: " + input.generic_string());

 private:
  std::string compileOptions;
  std::string linkOptions;

 protected:
  std::string getModuleMapStr(
      std::unordered_map<std::string, Path> moduleMap) const {
    std::string moduleMapStr;
    for (auto &pair : moduleMap) {
      moduleMapStr +=
          " -fmodule-file=" + pair.first + '=' + pair.second.generic_string();
    }
    return moduleMapStr;
  }

 public:
  Clang &addOption(std::string option) override {
    compileOptions += ' ' + option;
    return *this;
  };

  Clang &addLinkOption(std::string option) override {
    linkOptions += ' ' + option;
    return *this;
  };

#define GENERATE_COMPILE_METHOD(NAME, ARGS, PASS_ARGS) \
  Process::Result NAME ARGS const override {           \
    this->ensureParentExists(output);                  \
    return Process::run(NAME##Command PASS_ARGS);      \
  }                                                    \
  std::string NAME##Command ARGS const override

  GENERATE_COMPILE_METHOD(
      compilePCM,
      (const Path &input, const Path &output,
       const std::unordered_map<std::string, Path> &moduleMap = {},
       const std::string &extraOptions = ""),
      (input, output, moduleMap, extraOptions)) {
    return std::format(
        "{} -fansi-escape-codes -fcolor-diagnostics "
        "-std=c++20 --precompile -c {} {} {} {} -o {}",
        Process::findExecutable("clang++"), compileOptions,
        getModuleMapStr(moduleMap), extraOptions, input.generic_string(),
        output.generic_string());
  }

  GENERATE_COMPILE_METHOD(
      compile,
      (const Path &input, const Path &output, bool isDebug = false,
       const std::unordered_map<std::string, Path> &moduleMap = {},
       const std::string &extraOptions = ""),
      (input, output, isDebug, moduleMap, extraOptions)) {
    return std::format(
        "{} -fansi-escape-codes -fcolor-diagnostics -c {} {} {} {} {} -o {}",
        Process::findExecutable("clang++"), (isDebug ? "-g" : ""),
        compileOptions, getModuleMapStr(moduleMap), extraOptions,
        input.generic_string(), output.generic_string());
  }

  GENERATE_COMPILE_METHOD(link,
                          (const std::vector<Path> &input, const Path &output,
                           bool isDebug = false,
                           const std::string &extraOptions = ""),
                          (input, output, isDebug, extraOptions)) {
    std::string objList;
    for (auto &obj : input) {
      objList += obj.generic_string() + ' ';
    }
    return std::format(
        "{} -fansi-escape-codes -fcolor-diagnostics {} {} {} {} -o {}",
        Process::findExecutable("clang++"), (isDebug ? "-g" : ""), linkOptions,
        extraOptions, objList, output.generic_string());
  }

  GENERATE_COMPILE_METHOD(archive,
                          (const std::vector<Path> &input, const Path &output),
                          (input, output)) {
    std::string objList;
    for (auto &obj : input) {
      objList += obj.generic_string() + ' ';
    }
    return std::format("{0} r {2} {1}", Process::findExecutable("ar"), objList,
                       output.generic_string());
  }

  GENERATE_COMPILE_METHOD(createSharedLib,
                          (const std::vector<Path> &input, const Path &output,
                           const std::string &extraOptions = ""),
                          (input, output, extraOptions)) {
    std::string objList;
    for (auto &obj : input) {
      objList += obj.generic_string() + ' ';
    }
    return std::format("{} -shared -Wl,-export-all-symbols {} {} {} -o {}",
                       Process::findExecutable("clang++"), linkOptions,
                       extraOptions, objList, output.generic_string());
  }
#undef GENERATE_COMPILE_METHOD

  std::deque<Path> getIncludeDeps(
      const Path &input, const std::string &extraOptions = "") const override {
    const auto result = Process::run(
        std::format("{} {} {} -MM {}", Process::findExecutable("clang++"),
                    compileOptions, extraOptions, input.generic_string()));
    if (result.status != 0) throw ScanDepsError(input);
    const auto &output = result.output;
    std::deque<Path> deps;
    bool isStart = false;
    bool isPreSpace = false;
    std::string temp;
    for (auto &ch : output) {
      if (ch == ':' && !isStart) {
        isStart = true;
      } else if (isStart) {
        if (ch == '\\' && isPreSpace) {
          isPreSpace = false;
          continue;
        }
        if (ch == ' ') {
          if (!temp.empty()) {
            deps.emplace_back(temp);
            temp.clear();
          }
          isPreSpace = true;
          continue;
        } else
          isPreSpace = false;
        if (ch == '\n' || ch == '\r') continue;
        temp += ch;
      }
    }
    if (!temp.empty()) deps.emplace_back(temp);
    return deps;
  }

  ModuleInfo getModuleInfo(
      const Path &input, const std::string &extraOptions = "") const override {
    ModuleInfo info;
    const auto result = Process::run(
        Process::findExecutable("clang-scan-deps") + " -format=p1689 -- " +
        compileCommand(input, "test", false, {}, extraOptions));
    if (result.status != 0) throw ScanDepsError(input);
    const json::object rule = json::parse(result.output)
                                  .as_object()
                                  .at("rules")
                                  .as_array()
                                  .at(0)
                                  .as_object();
    if (rule.contains("provides")) {
      info.exported = true;
      info.name = rule.at("provides")
                      .as_array()
                      .at(0)
                      .as_object()
                      .at("logical-name")
                      .as_string();
    } else {
      info.exported = false;
    }
    if (rule.contains("requires")) {
      const auto &deps = rule.at("requires").as_array();
      for (auto &require : deps) {
        info.deps.emplace_back(
            require.as_object().at("logical-name").as_string());
      }
    }
    return info;
  }
};
