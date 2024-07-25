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
export struct ExeTarget : public Cached<>,
                          public TargetDeps<>,
                          public FileDeps {
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

  using TargetDeps<>::dependOn;
  using FileDeps::dependOn;

 protected:
  void onBuild(BuilderContext &ctx, const Path &output) const override {
    TargetDeps::build(ctx);
    const auto depsOutput = TargetDeps::getOutput(ctx);
    ctx.link(depsOutput, output, depsOutput);
  }
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
