export struct LibTarget : public CachedTarget<>, public Deps<> {
 private:
  bool isShared;

 public:
  LibTarget(const Path &output, bool isShared = false)
      : CachedTarget(output), isShared(isShared) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.outputPath() / _output;
  }

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    const auto [nodeList, objView] = Deps::buildNodeList(ctx);
    const Path output = getOutput(ctx);
    if (!objView.empty() && ctx.isNeedUpdate(output, objView)) {
      return isShared ? ctx.createSharedLib(objView, output, nodeList)
                      : ctx.archive(objView, output, nodeList);
    }
    return std::nullopt;
  }
};

export class LibBuilder : public ObjBuilder {
 protected:
  struct LibExport : public Export {
   protected:
    ModuleMap moduleMap;
    const TargetList targetList;

   public:
    LibExport(const LibBuilder &builder, const Context &ctx)
        : targetList(builder.onBuild(ctx, moduleMap)) {}

    virtual std::optional<Ref<const ModuleTarget>> findPCM(
        const std::string &moduleName) const override {
      const auto it = moduleMap.find(moduleName);
      return it == moduleMap.end() ? std::nullopt
                                   : std::make_optional(it->second);
    };

    std::optional<Ref<const Target>> getTarget() const override {
      return targetList.getTarget();
    };
  };

  struct ExternalLibExport : public LibExport {
   private:
    const std::optional<Context> ctx;
    const std::optional<CompilerOptions> compilerOptions;
    const TargetProxy<> target;
    mutable std::unordered_set<ModuleTargetProxy, ModuleTargetProxy::Hash,
                               ModuleTargetProxy::EqualTo>
        pcmCache;

    auto getFromCache(const ModuleTarget &target) const {
      const auto it = pcmCache.find(target);
      return std::ref(
          it != pcmCache.end()
              ? *it
              : *pcmCache.emplace(target, ctx, compilerOptions).first);
    }

   public:
    ExternalLibExport(const LibBuilder &builder, const Context &ctx)
        : LibExport(builder, ctx),
          ctx(ctx),
          compilerOptions(builder.getCompilerOptions()),
          target(targetList.getTarget(), this->ctx, this->compilerOptions) {}

    virtual std::optional<Ref<const ModuleTarget>> findPCM(
        const std::string &moduleName) const override {
      const auto targetOpt = LibExport::findPCM(moduleName);
      return targetOpt.has_value()
                 ? std::make_optional(getFromCache(targetOpt.value().get()))
                 : std::nullopt;
    }

    std::optional<Ref<const Target>> getTarget() const override {
      return target;
    };
  };

  mutable std::shared_ptr<LibExport> ex;

  TargetList onBuild(const Context &ctx, ModuleMap &map) const {
    TargetList list(std::in_place_type<LibTarget>,
                    "lib" + name + (isShared ? SHLIB_POSTFIX : ".a"), isShared);
    auto &target = list.getTarget<LibTarget>();
    target.dependOn(list.append(buildObjTargetList(ctx, map)));
    target.dependOn(buildExTargetList());
    return list;
  }

  TargetList onBuild(const Context &ctx) const override {
    ModuleMap map;
    return onBuild(ctx, map);
  }

  chainVar(bool, isShared, false, setShared);

 public:
  using ObjBuilder::ObjBuilder;

  LibBuilder(const std::string &name, bool isShared = false)
      : ObjBuilder(name), isShared(isShared) {}

  std::shared_ptr<Export> getExport(const Context &ctx) const {
    if (ex == nullptr) ex = std::make_shared<LibExport>(*this, ctx);
    return ex;
  }

  std::shared_ptr<Export> createExport(const Path &outputPath) const {
    auto ex =
        std::make_shared<ExternalLibExport>(*this, Context{name, outputPath});
    return ex;
  }
};
