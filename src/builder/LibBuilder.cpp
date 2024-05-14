export struct LibTarget : public Target {
 private:
  std::deque<Ref<const Target>> deps;
  const std::optional<Context> ctx;
  const CompilerOptions compilerOptions;

 public:
  LibTarget(const Path &output, const std::optional<Context> &ctx,
            CompilerOptions &compilerOptions)
      : Target(output), ctx(ctx), compilerOptions(compilerOptions) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.outputPath() / _output;
  }

  void dependOn(const std::ranges::range auto &targets) {
    for (auto &target : targets) {
      deps.emplace_back(target);
    }
  }

  std::optional<Ref<Node>> onBuild(BuilderContext &parentCtx) const override {
    BuilderContextChild ctx(
        parentCtx, this->ctx.has_value() ? this->ctx.value() : parentCtx.ctx,
        parentCtx.compiler, compilerOptions);
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
    TargetList targetList;

   public:
    LibExport(const LibBuilder &builder,
              const std::optional<Context> &ctx = std::nullopt)
        : targetList(builder.onBuild(moduleMap, ctx)) {}

    virtual std::optional<Ref<const Target>> findPCM(
        const std::string &moduleName) const override {
      const auto it = moduleMap.find(moduleName);
      return it == moduleMap.end() ? std::nullopt
                                   : std::make_optional(it->second);
    };

    std::optional<Ref<const Target>> getLibrary() const override {
      return targetList.getTarget();
    };
  };

 public:
  mutable std::shared_ptr<LibExport> ex;

  std::shared_ptr<Export> getExport() const {
    if (ex == nullptr) ex = std::make_shared<LibExport>(*this);
    return ex;
  }

  std::shared_ptr<Export> createExport(const Path &projectPath,
                                       const Path &outputPath) const {
    auto currentPath = fs::current_path();
    fs::current_path(projectPath);
    auto ex = std::make_shared<LibExport>(*this, Context{name, outputPath});
    fs::current_path(currentPath);
    return ex;
  }

  TargetList onBuild(ModuleMap &map,
                     const std::optional<Context> &ctx = std::nullopt) const {
    prepareCompilerOptions();
    TargetList list(std::in_place_type<LibTarget>, "lib" + name + ".a", ctx,
                    compilerOptions);
    auto &target = list.getTarget<ExeTarget>();
    target.dependOn(list.append(buildObjTargetList(map)));
    target.dependOn(buildExportLibList());
    return list;
  }

  TargetList onBuild() const override {
    ModuleMap map;
    return onBuild(map);
  }
};
