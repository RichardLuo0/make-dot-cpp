export module makeDotCpp.builder:ModuleBuilder;
import :Builder;
import :Targets;
import :BuilderContext;
import :ObjBuilder;
import :Export;
import :TargetProxy;
import std;
import makeDotCpp;
import makeDotCpp.utils;

namespace makeDotCpp {
export struct AllModulesTarget : public Target, public TargetDeps<> {
  Path getOutput(const CtxWrapper &) const override { return Path(); }

  void build(BuilderContext &ctx) const override { TargetDeps::build(ctx); }
};

export class ModuleBuilder : public ObjBuilder, public CachedExportFactory {
 protected:
  virtual TargetList onBuild(const Context &ctx, ModuleMap &map) const {
    TargetList list(std::in_place_type<AllModulesTarget>);
    auto &target = list.getTarget<AllModulesTarget>();
    target.dependOn(list.append(buildObjTargetList(ctx, map)));
    target.dependOn(getExportTargetList());
    return list;
  }

  TargetList onBuild(const Context &ctx) const override {
    ModuleMap map;
    return onBuild(ctx, map);
  }

 public:
  using ObjBuilder::ObjBuilder;

  Path getOutput(const Context &) const override { return Path(); }

 protected:
  struct ModuleExport : public Export {
   protected:
    const Context ctx;
    const CtxWrapper ctxW;
    const CompilerOption compilerOptions;
    ModuleMap moduleMap;
    const TargetList targetList;
    mutable std::unordered_set<ModuleTargetProxy, ModuleTargetProxy::Hash,
                               ModuleTargetProxy::EqualTo>
        proxyCache;

    auto getFromCache(const Ref<const ModuleTarget> &target) const {
      const auto it = proxyCache.find(target.get());
      return std::ref(
          it != proxyCache.end()
              ? *it
              : *proxyCache.emplace(&target.get(), &ctxW, &compilerOptions)
                     .first);
    }

   public:
    ModuleExport(const ModuleBuilder &builder, const Context &ctx,
                 const Path &bOutput)
        : ctx(ctx),
          ctxW(&this->ctx, bOutput),
          compilerOptions(builder.getCompilerOption()),
          targetList(builder.onBuild(ctx, moduleMap)) {}

    std::optional<Ref<const ModuleTarget>> findModule(
        const std::string &moduleName) const override {
      const auto it = moduleMap.find(moduleName);
      return it == moduleMap.end()
                 ? std::nullopt
                 : std::make_optional(getFromCache(it->second));
    }
  };

 public:
  const std::string &getName() const override { return name; }

  std::shared_ptr<Export> onCreate(const Context &ctx) const override {
    updateEverything(ctx);
    return std::make_shared<ModuleExport>(*this, ctx, name);
  }
};
}  // namespace makeDotCpp
