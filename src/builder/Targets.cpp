export struct Target {
 protected:
  // Relative path
  const Path _output;

 public:
  Target(const Path &output) : _output(output) {}
  Target(const Target &) = delete;
  Target(Target &&) = delete;
  virtual ~Target() = default;

  virtual Path getOutput(BuilderContext &ctx) const = 0;

 private:
  mutable bool isBuilt = false;
  mutable std::optional<Ref<Node>> node;

 public:
  virtual std::optional<Ref<Node>> build(BuilderContext &ctx) const {
    if (!isBuilt) {
      node = onBuild(ctx);
      isBuilt = true;
    }
    return node;
  };

 protected:
  // Make sure target has been built.
  // 1. Build target.
  // 2. Return corresponding node
  virtual std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const {
    return std::nullopt;
  };
};

struct EmptyTarget : public Target {
  EmptyTarget() : Target(Path()) {}

  Path getOutput(BuilderContext &ctx) const override { return Path(); };
};
export EmptyTarget emptyTarget;

export struct DepsTarget {
 protected:
  const std::deque<Path> includeDeps;
  std::deque<Ref<const Target>> targetDeps;

  DepsTarget(const std::deque<Path> &includeDeps) : includeDeps(includeDeps) {}
  DepsTarget(std::deque<Path> &&includeDeps) = delete;

  std::optional<NodeList> buildNodeList(BuilderContext &ctx,
                                        const Path &output) const {
    NodeList nodeList;
    for (auto &dep : targetDeps) {
      const auto &nodeOpt = dep.get().build(ctx);
      if (nodeOpt.has_value()) nodeList.emplace_back(nodeOpt.value());
    }
    auto targetOutputList =
        targetDeps | std::views::transform([&](const auto &target) {
          return target.get().getOutput(ctx);
        });
    return ctx.isNeedUpdate(output, includeDeps) ||
                   ctx.isNeedUpdate(output, targetOutputList)
               ? std::make_optional(nodeList)
               : std::nullopt;
  }

 public:
  void dependOn(const Target &target) { targetDeps.emplace_back(target); }
};

export struct PCMTarget : public DepsTarget, public Target {
 private:
  const Path input;

 public:
  PCMTarget(const Path &input, const Path &output,
            const std::deque<Path> &includeDeps)
      : DepsTarget(includeDeps), Target(output), input(input) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.pcmPath() / _output;
  };

 protected:
  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    const Path output = getOutput(ctx);
    auto nodeListOpt = DepsTarget::buildNodeList(ctx, output);
    if (nodeListOpt.has_value())
      return ctx.compilePCM(input, output, nodeListOpt.value());
    else
      return std::nullopt;
  }
};

export struct ObjTarget : public Target {
 private:
  using IsModule = PCMTarget;

  struct NotModule : public DepsTarget {
    const ObjTarget &parent;
    const Path input;

    NotModule(const Path &input, const ObjTarget &parent,
              const std::deque<Path> &includeDeps)
        : DepsTarget(includeDeps), parent(parent), input(input) {}

    std::optional<Ref<Node>> build(BuilderContext &ctx) const {
      const Path output = parent.getOutput(ctx);
      auto nodeListOpt = DepsTarget::buildNodeList(ctx, output);
      if (nodeListOpt.has_value())
        return ctx.compile(input, output, nodeListOpt.value());
      else
        return std::nullopt;
    }
  };

  std::variant<IsModule, NotModule> internal;

 public:
  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output, const Path &pcm)
      : Target(output),
        internal(std::in_place_type<IsModule>, input, pcm, includeDeps) {}

  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output)
      : Target(output),
        internal(std::in_place_type<NotModule>, input, *this, includeDeps) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.objPath() / _output;
  };

  const PCMTarget &getPCM() { return std::get<IsModule>(internal); }

  void dependOn(const Target &target) {
    std::visit([&](auto &&internal) { internal.dependOn(target); }, internal);
  }

 protected:
  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    return std::visit(
        [&](auto &&internal) -> std::optional<Ref<Node>> {
          auto nodeOpt = internal.build(ctx);
          using T = std::decay_t<decltype(internal)>;
          if constexpr (std::is_same_v<T, IsModule>) {
            const auto output = getOutput(ctx);
            const auto pcm = internal.getOutput(ctx);
            if (nodeOpt.has_value())
              return ctx.compile(pcm, output,
                                 std::views::single(nodeOpt.value()));
            else if (!ctx.exists(output))
              return ctx.compile(pcm, output);
            else
              return std::nullopt;
          } else
            return nodeOpt;
        },
        internal);
  }
};
