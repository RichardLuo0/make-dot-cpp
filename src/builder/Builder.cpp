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

  using UnitList = std::vector<Unit>;

  struct TargetList {
   private:
    std::unique_ptr<Target> target;
    std::deque<std::unique_ptr<const Target>> list;

   public:
    template <class T>
    TargetList(std::in_place_type_t<T>, auto &&...args) {
      target = std::make_unique<T>(std::forward<decltype(args)>(args)...);
    }

    auto append(std::ranges::range auto &&anotherList) {
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
  chainVar(std::string, name, "defaultName", setName);

 protected:
  chainVar(std::shared_ptr<const Compiler>, compiler, setCompiler);

 protected:
  chainVar(Path, cachePath, ".cache/make.cpp", setCache);

 protected:
  chainVarSet(Path, srcSet, addSrc, src);

 protected:
  chainVarSet(std::shared_ptr<const Export>, exSet, addDepend, ex);

 protected:
  mutable bool isPrepared = false;
  mutable CompilerOptions compilerOptions;

 private:
  chainMethod(addSrc, FileProvider, p) { srcSet.merge(p.list()); }
  chainMethod(define, std::string, d) {
    compilerOptions.compileOptions += " -D " + d;
  }

 public:
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
    return *this;
  }

  template <class E>
    requires(std::is_base_of_v<Export, E> && !std::is_const_v<E>)
  inline auto &addDepend(E &&ex) {
    this->exSet.emplace(std::make_shared<const E>(std::move(ex)));
    return *this;
  }

 protected:
  std::unordered_set<Path> buildInputSet() const {
    std::unordered_set<Path> inputSet;
    for (const auto &src : this->srcSet) {
      const auto input = fs::canonical(src);
      inputSet.emplace(input);
    }
    return inputSet;
  }

  UnitList buildUnitList() const {
    UnitList unitList;
    auto inputSet = buildInputSet();
    unitList.reserve(inputSet.size());
    for (auto &input : inputSet) {
      const auto depJsonPath =
          cachePath / (absoluteProximate(input) += ".dep.json");
      if (fs::exists(depJsonPath) &&
          fs::last_write_time(depJsonPath) > fs::last_write_time(input)) {
        std::ifstream is(depJsonPath);
        const auto depJson =
            json::parse(std::string(std::istreambuf_iterator<char>(is), {}));
        unitList.emplace_back(json::value_to<Unit>(depJson));
      } else {
        auto &unit = unitList.emplace_back();
        unit.input = input;
        unit.includeDeps = this->compiler->getIncludeDeps(input);
        auto info = this->compiler->getModuleInfo(input);
        unit.moduleName = info.name;
        unit.exported = info.exported;
        unit.moduleDeps = info.deps;
        fs::create_directories(depJsonPath.parent_path());
        std::ofstream os(depJsonPath);
        os << json::value_from(unit);
        if (os.fail()) throw FileNotFound(depJsonPath);
      }
    }
    return unitList;
  }

  auto buildExportLibList() const {
    std::deque<Ref<const Target>> libList;
    for (auto &ex : exSet) {
      const auto lib = ex->getLibrary();
      if (lib.has_value()) libList.emplace_back(lib.value());
    }
    return libList;
  }

  void prepareCompilerOptions() const {
    if (!isPrepared) {
      isPrepared = true;
      for (auto &ex : exSet) {
        compilerOptions.compileOptions += ' ' + ex->getCompileOption();
        compilerOptions.linkOptions += ' ' + ex->getLinkOption();
      }
    }
  }

  virtual TargetList onBuild() const = 0;

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
    prepareCompilerOptions();
    const auto targetList = onBuild();
    const auto &target = targetList.getTarget();
    BuilderContext builderCtx{ctx, compiler, compilerOptions};
    target.build(builderCtx);
    ctx.run();
    return {target.getOutput(builderCtx), builderCtx.takeFutureList()};
  };
};
