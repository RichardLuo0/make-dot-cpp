#ifdef _WIN32
#define POSTFIX ".exe"
#else
#define POSTFIX ""
#endif

export struct ExeTarget : public CachedTarget<> {
 private:
  std::deque<Ref<const Target>> deps;

 public:
  ExeTarget(const Path &output) : CachedTarget(output) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.outputPath() / _output;
  };

  void dependOn(const std::ranges::range auto &targets) {
    for (auto &target : targets) {
      deps.emplace_back(target);
    }
  }

  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    NodeList nodeList;
    for (auto &dep : deps) {
      const auto &nodeOpt = dep.get().build(ctx);
      if (nodeOpt.has_value()) nodeList.emplace_back(nodeOpt.value());
    }
    auto objView = deps | std::views::transform([&](auto &&target) {
                     return target.get().getOutput(ctx);
                   });
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
