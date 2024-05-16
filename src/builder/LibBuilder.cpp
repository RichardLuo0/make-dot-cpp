export struct LibTarget : public CachedTarget<> {
 private:
  std::deque<Ref<const Target>> deps;

 public:
  LibTarget(const Path &output) : CachedTarget(output) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.outputPath() / _output;
  }

  void dependOn(const std::ranges::range auto &targets) {
    for (auto &target : targets) {
      deps.emplace_back(target);
    }
  }

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    NodeList nodeList;
    for (auto &dep : deps) {
      const auto &nodeOpt = dep.get().build(ctx);
      if (nodeOpt.has_value()) nodeList.emplace_back(nodeOpt.value());
    }
    auto objView = deps | std::views::transform([&](auto &&target) {
                     return target.get().getOutput(ctx);
                   });
    const Path output = getOutput(ctx);
    if (!objView.empty() && ctx.isNeedUpdate(output, objView)) {
      return ctx.archive({objView.begin(), objView.end()}, output, nodeList);
    }
    return std::nullopt;
  }
};

export class LibBuilder : public ObjBuilder {
 protected:
  struct LibExport : public Export {
   private:
    ModuleMap moduleMap;
    const TargetList targetList;

    mutable std::unordered_set<NamedTargetProxy, NamedTargetProxy::Hash,
                               NamedTargetProxy::EqualTo>
        pcmCache;
    const std::optional<Context> ctx;
    const CompilerOptions compilerOptions;
    const TargetProxy<> target;

    auto getFromCache(const NamedTarget &target) const {
      const auto it = pcmCache.find(target);
      return std::ref(
          it != pcmCache.end()
              ? *it
              : *pcmCache.emplace(target, ctx, compilerOptions).first);
    }

   public:
    LibExport(const LibBuilder &builder,
              const std::optional<Context> &ctx = std::nullopt,
              const CompilerOptions compilerOptions = {})
        : targetList(builder.onBuild(moduleMap)),
          ctx(ctx),
          compilerOptions(compilerOptions),
          target(targetList.getTarget(), this->ctx, this->compilerOptions) {}

    virtual std::optional<Ref<const NamedTarget>> findPCM(
        const std::string &moduleName) const override {
      const auto it = moduleMap.find(moduleName);
      return it == moduleMap.end()
                 ? std::nullopt
                 : std::make_optional(getFromCache(it->second));
    };

    std::optional<Ref<const Target>> getLibrary() const override {
      return target;
    };
  };

 public:
  mutable std::shared_ptr<LibExport> ex;

  std::shared_ptr<Export> getExport() const {
    if (ex == nullptr)
      ex = std::make_shared<LibExport>(*this, std::nullopt,
                                       getCompilerOptions());
    return ex;
  }

  std::shared_ptr<Export> createExport(const Path &projectPath,
                                       const Path &outputPath) const {
    auto currentPath = fs::current_path();
    fs::current_path(projectPath);
    auto ex = std::make_shared<LibExport>(*this, Context{name, outputPath},
                                          getCompilerOptions());
    fs::current_path(currentPath);
    return ex;
  }

  TargetList onBuild(ModuleMap &map) const {
    TargetList list(std::in_place_type<LibTarget>, "lib" + name + ".a");
    auto &target = list.getTarget<LibTarget>();
    target.dependOn(list.append(buildObjTargetList(map)));
    target.dependOn(buildExportLibList());
    return list;
  }

  TargetList onBuild() const override {
    ModuleMap map;
    return onBuild(map);
  }
};
