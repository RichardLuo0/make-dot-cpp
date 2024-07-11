export struct ExeTarget : public CachedTarget<>,
                          public Deps<>,
                          public FilesDeps {
 private:
  const std::string name;

 public:
  ExeTarget(const std::string &name) : name(name) {}

  static Path getOutput(const Context &ctx, const Path &name) {
    return ctx.output / name += EXE_POSTFIX;
  }

  Path getOutput(BuilderContext &ctx) const override {
    return ExeTarget::getOutput(ctx.ctx, name);
  }

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    const auto [nodeList, objView] = Deps::buildNodeList(ctx);
    const Path output = getOutput(ctx);
    if (!objView.empty() && ctx.isNeedUpdate(output, objView)) {
      return ctx.link(objView, output, nodeList);
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
    return ExeTarget::getOutput(ctx, name);
  }

 protected:
  TargetList onBuild(const Context &ctx) const override {
    TargetList list(std::in_place_type<ExeTarget>, name);
    auto &target = list.getTarget<ExeTarget>();
    target.dependOn(list.append(buildObjTargetList(ctx)));
    target.dependOn(buildExTargetList());
    target.dependOn(getLinkOptionsJson(ctx));
    return list;
  }
};
