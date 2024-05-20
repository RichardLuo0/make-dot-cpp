#ifdef _WIN32
#define POSTFIX ".exe"
#else
#define POSTFIX ""
#endif

export struct ExeTarget : public CachedTarget<>, public Deps<> {
 public:
  ExeTarget(const Path &output) : CachedTarget(output) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.outputPath() / _output;
  };

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    const auto [nodeList, objView] = Deps::buildNodeList(ctx);
    const Path output = getOutput(ctx);
    if (!objView.empty() && ctx.isNeedUpdate(output, objView)) {
      return ctx.link({objView.begin(), objView.end()}, output, nodeList);
    }
    return std::nullopt;
  }
};

export class ExeBuilder : public ObjBuilder {
 protected:
  TargetList onBuild(const Context &ctx) const override {
    TargetList list(std::in_place_type<ExeTarget>, name + POSTFIX);
    auto &target = list.getTarget<ExeTarget>();
    target.dependOn(list.append(buildObjTargetList(ctx)));
    target.dependOn(buildExportLibList());
    return list;
  }
};
