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

export class Builder {
 protected:
  friend struct BuilderContext;

  struct TargetList {
   private:
    std::unique_ptr<Target> target;
    std::deque<std::unique_ptr<const Target>> list;

   public:
    template <class T>
    TargetList(std::in_place_type_t<T>, auto &&...args)
        : target(std::make_unique<T>(std::forward<decltype(args)>(args)...)) {}

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
    T &getTarget() const {
      return *static_cast<T *>(target.get());
    }
  };

 protected:
  std::string name = "builder";

 protected:
  chainVarSet(Path, srcSet, addSrc, src) { srcSet.emplace(src); };

 protected:
  chainVarSet(std::shared_ptr<const Export>, exSet, dependOn, ex) {
    exSet.emplace(ex);
    coOpt.reset();
    isExTargetListOutdated = true;
  }

 protected:
  const Path cache = "cache/" + name;

 private:
  struct OptionsCache {
   private:
    std::unordered_map<const Context *, std::optional<Path>> map;

   public:
    std::optional<Path> &at(const Context &ctx) {
      const auto it = map.find(&ctx);
      return it != map.end() ? it->second
                             : map.emplace(&ctx, std::nullopt).first->second;
    }

    void setOutdated() {
      for (auto &pair : map) {
        pair.second.reset();
      }
    }
  };

  CompilerOptions compilerOptions;

  mutable OptionsCache compileOptionsCache;
  mutable OptionsCache linkOptionsCache;

  void updateCacheFile(const Path &path, const std::string &content) const {
    if (!fs::exists(path) || readAsStr(path) != content) {
      std::ofstream os(path);
      os << content;
    }
  }

 protected:
#define GENERATE_GET_OPTIONS_JSON_METHOD(NAME, OPTIONS)                       \
  Path NAME(const Context &ctx) const {                                       \
    auto &pathOpt = OPTIONS##Cache.at(ctx);                                   \
    if (!pathOpt.has_value()) {                                               \
      auto &json = pathOpt.emplace(ctx.output / cache / STR(OPTIONS) ".txt"); \
      updateCacheFile(json, compilerOptions.OPTIONS);                         \
    }                                                                         \
    return pathOpt.value();                                                   \
  }

  GENERATE_GET_OPTIONS_JSON_METHOD(getCompileOptionsJson, compileOptions);
  GENERATE_GET_OPTIONS_JSON_METHOD(getLinkOptionsJson, linkOptions);
#undef GENERATE_GET_OPTIONS_JSON_METHOD

  chainMethod(addSrc, FileProvider, p) { srcSet.merge(p.list()); }
  chainMethod(define, std::string, d) {
    compilerOptions.compileOptions += " -D " + d;
    compileOptionsCache.setOutdated();
    coOpt.reset();
  }
  chainMethod(include, Path, path) {
    compilerOptions.compileOptions += " -I " + path.generic_string();
    compileOptionsCache.setOutdated();
    coOpt.reset();
  }

 public:
  Builder(const std::string &name) : name(name) {}
  virtual ~Builder() = default;

  template <class E, class... Args>
    requires std::is_base_of_v<Export, E>
  inline auto &dependOn(Args &&...args) {
    dependOn(std::make_shared<const E>(std::forward<Args>(args)...));
    return *this;
  }

  template <class E>
    requires(std::is_base_of_v<Export, E> && !std::is_const_v<E>)
  inline auto &dependOn(E &&ex) {
    return dependOn(std::move(ex));
  }

  chainMethod(dependOn, ranges::range<const std::shared_ptr<const Export>> auto,
              exs) {
    for (auto &ex : exs) dependOn(ex);
  }

 protected:
  // A pair consists of a set of input src file and a common base
  using InputInfo = std::pair<std::unordered_set<Path>, Path>;

  InputInfo buildInputInfo() const {
    std::unordered_set<Path> inputSet;
    for (const auto &src : srcSet) {
      inputSet.emplace(fs::canonical(src));
    }
    return std::make_pair(std::move(inputSet), commonBase(inputSet));
  }

  auto buildUnitList(const Context &ctx, const InputInfo &inputInfo) const {
    const auto &[inputSet, base] = inputInfo;
    std::vector<Unit> unitList;
    unitList.reserve(inputSet.size());
    const auto cachePath = ctx.output / cache / "units";
    for (auto &input : inputSet) {
      const auto depJsonPath =
          cachePath / (fs::proximate(input, base) += ".json");
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
        const auto info = ctx.compiler->getModuleInfo(input, compileOptions);
        auto &unit = unitList.emplace_back(
            input, info.exported, info.name,
            ctx.compiler->getIncludeDeps(input, compileOptions), info.deps);
        fs::create_directories(depJsonPath.parent_path());
        std::ofstream os(depJsonPath);
        os.exceptions(std::ifstream::failbit);
        os << json::value_from(unit);
      }
    }
    return unitList;
  }

  auto buildUnitList(const Context &ctx) const {
    const auto inputInfo = buildInputInfo();
    return buildUnitList(ctx, inputInfo);
  }

 private:
  mutable bool isExTargetListOutdated = true;
  mutable std::deque<Ref<const Target>> _exTargetList;

 public:
  auto buildExTargetList() const {
    if (isExTargetListOutdated) {
      _exTargetList.clear();
      for (auto &ex : exSet) {
        const auto target = ex->getTarget();
        if (target.has_value()) _exTargetList.emplace_back(target.value());
      }
      isExTargetListOutdated = false;
    }
    return _exTargetList;
  }

 private:
  mutable std::optional<CompilerOptions> coOpt;

 protected:
  CompilerOptions getCompilerOptions() const {
    if (!coOpt.has_value()) {
      auto &co = coOpt.emplace();
      for (auto &ex : exSet) {
        co.compileOptions += ' ' + ex->getCompileOption();
        co.linkOptions += ' ' + ex->getLinkOption();
      }
      co.compileOptions += ' ' + compilerOptions.compileOptions;
      co.linkOptions += ' ' + compilerOptions.linkOptions;
    }
    return coOpt.value();
  }

  virtual TargetList onBuild(const Context &ctx) const = 0;

 public:
  // Do not call build() on same ctx sequentially.
  // This will cause race condition.
  virtual FutureList build(const Context &ctx) const {
    const auto targetList = onBuild(ctx);
    const auto &target = targetList.getTarget();
    BuilderContext builderCtx{ctx, getCompilerOptions()};
    target.build(builderCtx);
    ctx.run();
    return builderCtx.takeFutureList();
  };

  virtual Path getOutput(const Context &ctx) const = 0;
};
