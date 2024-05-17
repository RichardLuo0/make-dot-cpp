export struct Target {
  Target() = default;
  Target(const Target &) = delete;
  Target(Target &&) = delete;
  virtual ~Target() = default;

  virtual Path getOutput(BuilderContext &ctx) const = 0;
  virtual std::optional<Ref<Node>> build(BuilderContext &ctx) const = 0;
};

export template <class T = Target>
struct CachedTarget : public T {
 protected:
  // Relative path
  const Path _output;

 public:
  CachedTarget(const Path &output) : _output(output) {}

 private:
  mutable bool isBuilt = false;
  mutable std::optional<Ref<Node>> node;

 public:
  std::optional<Ref<Node>> build(BuilderContext &ctx) const override {
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
  virtual std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const = 0;
};

export struct : public Target {
  Path getOutput(BuilderContext &ctx) const override { return Path(); }
  std::optional<Ref<Node>> build(BuilderContext &ctx) const override {
    return std::nullopt;
  }
} emptyTarget;

export struct CustomTarget : public Target {
  Path getOutput(BuilderContext &ctx) const override {
    return getOutput(static_cast<VFSContext &>(ctx));
  }

  std::optional<Ref<Node>> build(BuilderContext &ctx) const override {
    return build(static_cast<VFSContext &>(ctx));
  }

  virtual Path getOutput(VFSContext &ctx) const = 0;
  virtual std::optional<Ref<Node>> build(VFSContext &ctx) const = 0;
};

export struct ModuleTarget : public Target {
  virtual const std::string &getName() const = 0;
  virtual std::unordered_map<std::string, Path> getModuleMap(
      BuilderContext &ctx) const = 0;
};

export struct UnitDeps {
 private:
  const std::deque<Path> includeDeps;
  std::deque<Ref<const ModuleTarget>> targetDeps;

 protected:
  UnitDeps(const std::deque<Path> &includeDeps) : includeDeps(includeDeps) {}

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

  // TODO Cycle detection
  std::unordered_map<std::string, Path> getModuleMap(
      BuilderContext &ctx) const {
    std::unordered_map<std::string, Path> map;
    for (auto &mod : targetDeps) {
      const auto &pcm = mod.get();
      map.emplace(pcm.getName(), pcm.getOutput(ctx));
      map.merge(pcm.getModuleMap(ctx));
    }
    return map;
  }

 public:
  void dependOn(const ModuleTarget &target) { targetDeps.emplace_back(target); }
};

export struct PCMTarget : public UnitDeps, public CachedTarget<ModuleTarget> {
 private:
  const std::string name;
  const Path input;

 public:
  PCMTarget(const std::string &name, const Path &input, const Path &output,
            const std::deque<Path> &includeDeps)
      : UnitDeps(includeDeps), CachedTarget(output), name(name), input(input) {}

  const std::string &getName() const override { return name; }

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.pcmPath() / _output;
  };

  std::unordered_map<std::string, Path> getModuleMap(
      BuilderContext &ctx) const override {
    return UnitDeps::getModuleMap(ctx);
  }

 protected:
  std::optional<Ref<Node>> onBuild(BuilderContext &ctx) const override {
    const Path output = getOutput(ctx);
    auto nodeListOpt = UnitDeps::buildNodeList(ctx, output);
    if (nodeListOpt.has_value())
      return ctx.compilePCM(input, getModuleMap(ctx), output,
                            nodeListOpt.value());
    else
      return std::nullopt;
  }
};

export struct ObjTarget : public CachedTarget<> {
 protected:
  using IsModule = PCMTarget;

  struct NotModule : public UnitDeps, public Target {
    const ObjTarget &parent;
    const Path input;

    NotModule(const Path &input, const ObjTarget &parent,
              const std::deque<Path> &includeDeps)
        : UnitDeps(includeDeps), parent(parent), input(input) {}

    Path getOutput(BuilderContext &ctx) const override {
      return parent.getOutput(ctx);
    };

    std::optional<Ref<Node>> build(BuilderContext &ctx) const override {
      const Path output = getOutput(ctx);
      auto nodeListOpt = UnitDeps::buildNodeList(ctx, output);
      if (nodeListOpt.has_value())
        return ctx.compile(input, getModuleMap(ctx), output,
                           nodeListOpt.value());
      else
        return std::nullopt;
    }
  };

  std::variant<IsModule, NotModule> internal;

 public:
  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output, const std::string &name, const Path &pcm)
      : CachedTarget(output),
        internal(std::in_place_type<IsModule>, name, input, pcm, includeDeps) {}

  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output)
      : CachedTarget(output),
        internal(std::in_place_type<NotModule>, input, *this, includeDeps) {}

  Path getOutput(BuilderContext &ctx) const override {
    return ctx.objPath() / _output;
  };

  const PCMTarget &getPCM() { return std::get<IsModule>(internal); }

  void dependOn(const ModuleTarget &target) {
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
            if (ctx.isNeedUpdate(output, std::views::single(pcm)))
              if (nodeOpt.has_value())
                return ctx.compile(pcm, internal.getModuleMap(ctx), output,
                                   std::views::single(nodeOpt.value()));
              else
                return ctx.compile(pcm, internal.getModuleMap(ctx), output);
            else
              return std::nullopt;
          } else
            return nodeOpt;
        },
        internal);
  }
};
