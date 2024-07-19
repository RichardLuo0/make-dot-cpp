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
export struct AllModulesTarget : public CachedTarget<>, public Deps<> {
  Path getOutput(const CtxWrapper &) const override { return Path(); }

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx,
                                   const Path &) const override {
    Deps::buildNodeList(ctx);
    return std::nullopt;
  }
};

export class ModuleBuilder : public ObjBuilder, public CachedExportFactory {
 protected:
  TargetList onBuild(const Context &ctx, ModuleMap &map) const {
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

  struct ModuleExport : public Export {
    const CompilerOptions compilerOptions;
    ModuleMap moduleMap;
    const TargetList targetList;
    mutable std::unordered_set<ModuleTargetProxy, ModuleTargetProxy::Hash,
                               ModuleTargetProxy::EqualTo>
        proxyCache;

   protected:
    auto getFromCache(const Ref<const ModuleTarget> &target) const {
      const auto it = proxyCache.find(target.get());
      return std::ref(it != proxyCache.end()
                          ? *it
                          : *proxyCache.emplace(target, compilerOptions).first);
    }

   public:
    ModuleExport(const ModuleBuilder &builder, const Context &ctx)
        : compilerOptions(builder.getCompilerOptions()),
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
  using ObjBuilder::ObjBuilder;

  const std::string &getName() const override { return name; }

  std::shared_ptr<Export> onCreate(const Context &ctx) const override {
    updateEverything(ctx);
    return std::make_shared<ModuleExport>(*this, ctx);
  }

  Path getOutput(const Context &) const override { return Path(); }
};
}  // namespace makeDotCpp
