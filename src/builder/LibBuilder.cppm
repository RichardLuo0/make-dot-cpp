export module makeDotCpp.builder:LibBuilder;
import :Targets;
import :BuilderContext;
import :ModuleBuilder;
import :Export;
import :TargetProxy;
import std;
import makeDotCpp;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export struct LibTarget : public Cached<>, public TargetDeps<> {
 private:
  const std::string name;
  const bool isShared;

 public:
  LibTarget(const std::string &name, bool isShared = false)
      : name(name), isShared(isShared) {}

  static Path getOutput(const CtxWrapper &ctx, const std::string &name,
                        bool isShared) {
    return ctx.outputPath() /
           ("lib" + name + (isShared ? SHLIB_POSTFIX : ".a"));
  }

  Path getOutput(const CtxWrapper &ctx) const override {
    return getOutput(ctx, name, isShared);
  }

 protected:
  void onBuild(BuilderContext &ctx, const Path &output) const override {
    TargetDeps::build(ctx);
    const auto depsOutput = TargetDeps::getOutput(ctx);
    isShared ? ctx.createSharedLib(depsOutput, output, depsOutput)
             : ctx.archive(depsOutput, output, depsOutput);
  }
};

export class LibBuilder : public ModuleBuilder {
 protected:
  CHAIN_VAR(bool, isShared, false, setShared);

  TargetList onBuild(const Context &ctx, ModuleMap &map) const override {
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
  using ModuleBuilder::ModuleBuilder;

  LibBuilder(const std::string &name, bool isShared = false)
      : ModuleBuilder(name), isShared(isShared) {}

  Path getOutput(const Context &ctx) const override {
    return LibTarget::getOutput(CtxWrapper(&ctx, name), name, isShared);
  }

 protected:
  struct LibExport : public ModuleExport {
   protected:
    const TargetProxy<> target;

   public:
    LibExport(const LibBuilder &builder, const Context &ctx,
              const Path &bOutput)
        : ModuleExport(builder, ctx, bOutput),
          target(&targetList.getTarget(), &ctxW, &compilerOptions) {}

    std::optional<Ref<const Target>> getTarget() const override {
      return target;
    }
  };

 public:
  std::shared_ptr<Export> onCreate(const Context &ctx) const override {
    updateEverything(ctx);
    return std::make_shared<LibExport>(*this, ctx, name);
  }
};
}  // namespace makeDotCpp
