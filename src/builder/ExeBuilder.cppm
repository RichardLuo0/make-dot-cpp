export module makeDotCpp.builder:ExeBuilder;
import :common;
import :Targets;
import :Builder;
import :BuilderContext;
import :ObjBuilder;
import std;
import makeDotCpp;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export struct ExeTarget : public CachedTarget<>,
                          public Deps<>,
                          public FilesDeps {
 private:
  const std::string name;

 public:
  ExeTarget(const std::string &name) : name(name) {}

  static Path getOutput(const CtxWrapper &ctx, const Path &name) {
    return ctx.outputPath() / name += EXE_POSTFIX;
  }

  Path getOutput(const CtxWrapper &ctx) const override {
    return getOutput(ctx, name);
  }

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx,
                                   const Path &output) const override {
    const auto nodeList = Deps::buildNodeList(ctx);
    const auto depsOutput = Deps::getDepsOutput(ctx);
    if (ctx.needsUpdate(output, depsOutput)) {
      return ctx.link(depsOutput, output, nodeList);
    }
    return std::nullopt;
  }

  using Deps<>::dependOn;
  using FilesDeps::dependOn;
};

export class ExeBuilder : public ObjBuilder {
 public:
  using ObjBuilder::ObjBuilder;

  Path getOutput(const Context &ctx) const override {
    return ExeTarget::getOutput(CtxWrapper(&ctx, name), name);
  }

 protected:
  TargetList onBuild(const Context &ctx) const override {
    TargetList list(std::in_place_type<ExeTarget>, name);
    auto &target = list.getTarget<ExeTarget>();
    target.dependOn(list.append(buildObjTargetList(ctx)));
    target.dependOn(getExportTargetList());
    target.dependOn(getLinkOptionsJson(ctx));
    return list;
  }
};
}  // namespace makeDotCpp
