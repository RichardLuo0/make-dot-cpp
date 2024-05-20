export struct Unit {
  Path input;
  bool exported = false;
  std::string moduleName;
  std::deque<Path> includeDeps;
  std::deque<std::string> moduleDeps;

 private:
  BOOST_DESCRIBE_CLASS(Unit, (),
                       (input, exported, moduleName, includeDeps, moduleDeps),
                       (), ())
};

struct BuilderState {
  Path compileOptionsJson, linkOptionsJson;
  std::unordered_set<Path> inputSet;
  std::vector<Unit> unitList;
};

Path absoluteProximate(const Path &x, const Path &y = fs::current_path()) {
  Path common;
  auto it = y.begin();
  for (auto folder : x) {
    if (folder == *it) {
      common /= folder;
    } else
      break;
    it++;
  }
  return fs::proximate(x, common);
}

export struct BuildResult {
  Path output;
  mutable FutureList futureList;

  void wait() const { return futureList.wait(); }

  auto get() const { return futureList.get(); }
};

export class Builder {
 protected:
  friend struct BuilderContext;

  struct TargetList {
   private:
    std::unique_ptr<Target> target;
    std::deque<std::unique_ptr<const Target>> list;

   public:
    template <class T>
    TargetList(std::in_place_type_t<T>, auto &&...args) {
      target = std::make_unique<T>(std::forward<decltype(args)>(args)...);
    }

    auto append(ranges::range<std::unique_ptr<Target>> auto &&anotherList) {
      std::vector<Ref<const Target>> targetRefList;
      targetRefList.reserve(anotherList.size());
      for (auto &t : anotherList) {
        targetRefList.emplace_back(*t);
      }
      std::move(anotherList.begin(), anotherList.end(),
                std::back_inserter(list));
      return targetRefList;
    }

    const Target &at(std::size_t i) const { return *list.at(i); }

    template <class T = Target>
    auto &getTarget() const {
      return static_cast<T &>(*target);
    }
  };

 protected:
  std::string name = "builder";

 protected:
  chainVar(std::shared_ptr<const Compiler>, compiler, setCompiler);

 protected:
  chainVarSet(Path, srcSet, addSrc, src);

 protected:
  chainVarSet(std::shared_ptr<const Export>, exSet, addDepend, ex);

 protected:
  const Path cache = "cache/" + name;

 private:
  struct IsOptionsOutdated {
   private:
    std::unordered_map<const Context *, bool> map;

   public:
    bool &at(const Context &ctx) {
      const auto it = map.find(&ctx);
      return it != map.end() ? it->second
                             : map.emplace(&ctx, true).first->second;
    }

    void setOutdated(bool isOutdated = true) {
      for (auto &pair : map) {
        pair.second = isOutdated;
      }
    }
  };

  mutable IsOptionsOutdated isCompileOptionsJsonOutdated;
  mutable IsOptionsOutdated isLinkOptionsJsonOutdated;

  CompilerOptions compilerOptions;

  const Path _compileOptionsJson{cache / "compileOptions.txt"};
  const Path _linkOptionsJson{cache / "linkOptions.txt"};

  void updateCacheFile(const Path &path, const std::string &content) const {
    if (!fs::exists(path) || readAsStr(path) != content) {
      std::ofstream os(path);
      os << content;
    }
  }

 protected:
  Path getCompileOptionsJson(const Context &ctx) const {
    const auto json = ctx.output / _compileOptionsJson;
    bool &isOutdated = isCompileOptionsJsonOutdated.at(ctx);
    if (isOutdated) {
      updateCacheFile(json, compilerOptions.compileOptions);
      isOutdated = false;
    }
    return json;
  }

  Path getLinkOptionsJson(const Context &ctx) const {
    const auto json = ctx.output / _linkOptionsJson;
    bool &isOutdated = isLinkOptionsJsonOutdated.at(ctx);
    if (isOutdated) {
      updateCacheFile(json, compilerOptions.linkOptions);
      isOutdated = false;
    }
    return json;
  }

  chainMethod(addSrc, FileProvider, p) { srcSet.merge(p.list()); }
  chainMethod(define, std::string, d) {
    compilerOptions.compileOptions += " -D " + d;
    isCompileOptionsJsonOutdated.setOutdated();
    isCompilerOptionsOutdated = true;
  }
  chainMethod(include, Path, path) {
    compilerOptions.compileOptions += " -I " + path.generic_string();
    isCompileOptionsJsonOutdated.setOutdated();
    isCompilerOptionsOutdated = true;
  }

 public:
  Builder(const std::string &name) : name(name) {}
  virtual ~Builder() = default;

  template <class C>
    requires std::is_base_of_v<Compiler, C>
  inline auto &setCompiler(const C &compiler) {
    this->compiler = std::make_shared<const C>(compiler);
    return *this;
  }

  template <class E, class... Args>
    requires std::is_base_of_v<Export, E>
  inline auto &addDepend(Args &&...args) {
    this->exSet.emplace(std::make_shared<const E>(std::forward<Args>(args)...));
    isCompilerOptionsOutdated = true;
    isExportLibListOutdated = true;
    return *this;
  }

  template <class E>
    requires(std::is_base_of_v<Export, E> && !std::is_const_v<E>)
  inline auto &addDepend(E &&ex) {
    this->exSet.emplace(std::make_shared<const E>(std::move(ex)));
    isCompilerOptionsOutdated = true;
    isExportLibListOutdated = true;
    return *this;
  }

 protected:
  auto buildInputSet() const {
    std::unordered_set<Path> inputSet;
    for (const auto &src : this->srcSet) {
      const auto input = fs::canonical(src);
      inputSet.emplace(input);
    }
    return inputSet;
  }

  auto buildUnitList(const Context &ctx) const {
    std::vector<Unit> unitList;
    auto inputSet = buildInputSet();
    unitList.reserve(inputSet.size());
    const auto cachePath = ctx.output / cache / "units";
    for (auto &input : inputSet) {
      const auto depJsonPath =
          cachePath / (absoluteProximate(input) += ".json");
      const auto compileOptionsJson = getCompileOptionsJson(ctx);
      if (fs::exists(depJsonPath) &&
          fs::last_write_time(depJsonPath) > fs::last_write_time(input) &&
          fs::last_write_time(depJsonPath) >
              fs::last_write_time(compileOptionsJson)) {
        std::ifstream is(depJsonPath);
        const auto depJson = parseJson(depJsonPath);
        unitList.emplace_back(json::value_to<Unit>(depJson));
      } else {
        const auto compileOptions = getCompilerOptions().compileOptions;
        const auto info = compiler->getModuleInfo(input, compileOptions);
        auto &unit = unitList.emplace_back(
            input, info.exported, info.name,
            compiler->getIncludeDeps(input, compileOptions), info.deps);
        fs::create_directories(depJsonPath.parent_path());
        std::ofstream os(depJsonPath);
        os.exceptions(std::ifstream::failbit);
        os << json::value_from(unit);
      }
    }
    return unitList;
  }

 private:
  mutable bool isExportLibListOutdated = true;
  mutable std::deque<Ref<const Target>> _libList;

 public:
  auto buildExportLibList() const {
    if (isExportLibListOutdated) {
      _libList.clear();
      for (auto &ex : exSet) {
        const auto lib = ex->getLibrary();
        if (lib.has_value()) _libList.emplace_back(lib.value());
      }
      isExportLibListOutdated = false;
    }
    return _libList;
  }

 private:
  mutable bool isCompilerOptionsOutdated = true;
  mutable CompilerOptions _co;

 protected:
  CompilerOptions getCompilerOptions() const {
    if (isCompilerOptionsOutdated) {
      _co = CompilerOptions();
      for (auto &ex : exSet) {
        _co.compileOptions += ' ' + ex->getCompileOption();
        _co.linkOptions += ' ' + ex->getLinkOption();
      }
      _co.compileOptions += ' ' + compilerOptions.compileOptions;
      _co.linkOptions += ' ' + compilerOptions.linkOptions;
      isCompilerOptionsOutdated = false;
    }
    return _co;
  }

  virtual TargetList onBuild(const Context &ctx) const = 0;

 public:
  void buildCompileCommands(const Context &ctx) const {
    json::array compileCommands;
    for (const auto &input : buildInputSet()) {
      const auto output = ctx.objPath() / (absoluteProximate(input) += ".obj");
      json::object commandObject;
      commandObject["directory"] = ctx.output.generic_string();
      commandObject["command"] =
          this->compiler->compileCommand(input, output, ctx.debug);
      commandObject["file"] = input.generic_string();
      commandObject["output"] = output.generic_string();
      compileCommands.emplace_back(commandObject);
    }
    std::ofstream os((ctx.output / "compile_commands.json").generic_string());
    os << compileCommands;
  }

  virtual BuildResult build(const Context &ctx) const {
    updateCacheFile(getCompileOptionsJson(ctx), compilerOptions.compileOptions);
    updateCacheFile(getLinkOptionsJson(ctx), compilerOptions.linkOptions);
    const auto targetList = onBuild(ctx);
    const auto &target = targetList.getTarget();
    BuilderContext builderCtx{ctx, compiler, getCompilerOptions()};
    target.build(builderCtx);
    ctx.run();
    return {target.getOutput(builderCtx), builderCtx.takeFutureList()};
  };
};
