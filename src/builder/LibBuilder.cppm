export module makeDotCpp.builder:LibBuilder;
import :Targets;
import :BuilderContext;
import :ObjBuilder;
import :Export;
import :TargetProxy;
import std;
import makeDotCpp;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export struct LibTarget : public CachedTarget<>, public Deps<> {
 private:
  const std::string name;
  const bool isShared;

 public:
  LibTarget(const std::string &name, bool isShared = false)
      : name(name), isShared(isShared) {}

  static Path getOutput(const Context &ctx, const std::string &name,
                        bool isShared) {
    return ctx.output / ("lib" + name + (isShared ? SHLIB_POSTFIX : ".a"));
  }

  Path getOutput(const CtxWrapper &ctx) const override {
    return LibTarget::getOutput(ctx.ctx, name, isShared);
  }

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    const auto nodeList = Deps::buildNodeList(ctx);
    const auto depsOutput = Deps::getDepsOutput(ctx);
    const Path output = getOutput(ctx);
    if (ctx.needsUpdate(output, depsOutput)) {
      return isShared ? ctx.createSharedLib(depsOutput, output, nodeList)
                      : ctx.archive(depsOutput, output, nodeList);
    }
    return std::nullopt;
  }
};

export class LibBuilder : public ObjBuilder, public CachedExportFactory {
 protected:
  CHAIN_VAR(bool, isShared, false, setShared);

  TargetList onBuild(const Context &ctx, ModuleMap &map) const {
    TargetList list(std::in_place_type<LibTarget>, name, isShared);
    auto &target = list.getTarget<LibTarget>();
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

  LibBuilder(const std::string &name, bool isShared = false)
      : ObjBuilder(name), isShared(isShared) {}

  Path getOutput(const Context &ctx) const override {
    return LibTarget::getOutput(ctx, name, isShared);
  }

 protected:
  struct LibExport : public Export {
    const CompilerOptions compilerOptions;
    ModuleMap moduleMap;
    const TargetList targetList;
    const TargetProxy<> target;
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
    LibExport(const LibBuilder &builder, const Context &ctx)
        : compilerOptions(builder.getCompilerOptions()),
          targetList(builder.onBuild(ctx, moduleMap)),
          target(targetList.getTarget(), compilerOptions) {}

    std::optional<Ref<const ModuleTarget>> findModule(
        const std::string &moduleName) const override {
      const auto it = moduleMap.find(moduleName);
      return it == moduleMap.end()
                 ? std::nullopt
                 : std::make_optional(getFromCache(it->second));
    }

    std::optional<Ref<const Target>> getTarget() const override {
      return target;
    }
  };

 public:
  std::shared_ptr<Export> onCreate(const Context &ctx) const override {
    updateEverything(ctx);
    return std::make_shared<LibExport>(*this, ctx);
  }
};
}  // namespace makeDotCpp
