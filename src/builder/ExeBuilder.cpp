export struct ExeTarget : public CachedTarget<>,
                          public Deps<>,
                          public FilesDeps {
 public:
  ExeTarget(const Path &output) : CachedTarget(output) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.outputPath() / _output;
  };

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

 protected:
  TargetList onBuild(const Context &ctx) const override {
    TargetList list(std::in_place_type<ExeTarget>, name + EXE_POSTFIX);
    auto &target = list.getTarget<ExeTarget>();
    target.dependOn(list.append(buildObjTargetList(ctx)));
    target.dependOn(buildExTargetList());
    target.dependOn(getLinkOptionsJson(ctx));
    return list;
  }
};
