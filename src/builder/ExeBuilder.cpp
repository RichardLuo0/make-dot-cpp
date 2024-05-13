#ifdef _WIN32
#define POSTFIX ".exe"
#else
#define POSTFIX ""
#endif

export struct ExeTarget : public Target {
 private:
  std::deque<Ref<const Target>> deps;

 public:
  ExeTarget(const Path &output) : Target(output) {}

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
    auto objView = deps | std::views::transform([&](const Target &target) {
                     return target.getOutput(ctx);
                   });
    std::vector<Path> objList{objView.begin(), objView.end()};
    const Path output = getOutput(ctx);
    if (!objView.empty() && ctx.isNeedUpdate(output, objView)) {
      return ctx.link(objList, output, nodeList);
    }
    return std::nullopt;
  }
};

export class ExeBuilder : public Builder {
 protected:
  mutable std::unique_ptr<ExeTarget> target;

  const Target &onBuild(const Context &ctx,
                        const std::deque<ObjTarget> &objList) const override {
    target = std::make_unique<ExeTarget>(name + POSTFIX);
    target->dependOn(objList);
    target->dependOn(buildExportLibList());
    return *target;
  }
};
