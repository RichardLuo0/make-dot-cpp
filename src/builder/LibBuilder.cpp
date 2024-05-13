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
    BuilderContext ctx(
        this->ctx.has_value() ? this->ctx.value() : parentCtx.ctx,
        parentCtx.compiler, compilerOptions);
    NodeList nodeList;
    for (auto &dep : deps) {
      const auto &nodeOpt = dep.get().build(ctx);
      if (nodeOpt.has_value()) nodeList.emplace_back(nodeOpt.value());
    }
    auto objView = deps | std::views::transform([&](const Target &target) {
                     return target.getOutput(ctx);
                   });
    std::vector<Path> objList{objView.begin(), objView.end()};
    const Path output = getOutput(ctx);
    if (!objView.empty() && ctx.isNeedUpdate(output, objView)) {
      return ctx.archive(objList, output, nodeList);
    }
    parentCtx.merge(ctx);
    return std::nullopt;
  }
};

export class LibBuilder : public Builder {
 protected:
  struct LibExport : public Export {
   private:
    LibTarget target;
    ModuleMap moduleMap;
    const std::deque<ObjTarget> objList;

   public:
    LibExport(const LibBuilder &parent,
              const std::optional<Context> &ctx = std::nullopt)
        : target("lib" + parent.name + ".a", ctx, parent.compilerOptions),
          objList(
              parent.buildObjTargetList(parent.buildUnitList(), moduleMap)) {
      target.dependOn(objList);
      target.dependOn(parent.buildExportLibList());
    }

    virtual std::optional<Ref<const Target>> findPCM(
        const std::string &moduleName) const override {
      const auto it = moduleMap.find(moduleName);
      return it == moduleMap.end() ? std::nullopt
                                   : std::make_optional(it->second);
    };

    std::optional<Ref<const Target>> getLibrary() const override {
      return target;
    };
  };

  void createExportIfNull() const {
    if (ex != nullptr) return;
    prepareCompilerOptions();
    ex = std::make_shared<LibExport>(*this);
  }

 public:
  mutable std::shared_ptr<LibExport> ex;

  std::shared_ptr<Export> getExport() const {
    createExportIfNull();
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

  BuildResult build(const Context &ctx) const override {
    createExportIfNull();
    const auto &target = ex->getLibrary().value().get();
    BuilderContext builderCtx{ctx, compiler, compilerOptions};
    ctx.run();
    return {target.getOutput(builderCtx), builderCtx.takeFutureList()};
  };
};
