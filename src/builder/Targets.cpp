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

export template <class T = Target>
struct Deps {
 protected:
  std::deque<Ref<const T>> targetDeps;

  auto buildNodeList(BuilderContext &ctx) const {
    NodeList nodeList;
    for (auto &dep : targetDeps) {
      const auto &nodeOpt = dep.get().build(ctx);
      if (nodeOpt.has_value()) nodeList.emplace_back(nodeOpt.value());
    }
    auto outputList =
        targetDeps | std::views::transform([&](const auto &target) {
          return target.get().getOutput(ctx);
        });
    return std::pair{nodeList, outputList};
  }

 public:
  void dependOn(const ranges::range<Ref<const T>> auto &targets) {
    for (auto &target : targets) {
      targetDeps.emplace_back(target);
    }
  }

  void dependOn(CLRef<T> target) { targetDeps.emplace_back(target); }
};

export struct FilesDeps {
 protected:
  std::deque<Path> filesDeps;

 public:
  FilesDeps() = default;
  FilesDeps(const std::deque<Path> &filesDeps) : filesDeps(filesDeps) {}

  void dependOn(const ranges::range<Ref<Path>> auto &paths) {
    for (auto &path : paths) {
      filesDeps.emplace_back(path);
    }
  }

  void dependOn(const Path &path) { filesDeps.emplace_back(path); }
};

export struct UnitDeps : public Deps<ModuleTarget>, public FilesDeps {
 public:
  defException(CyclicModuleDependency, (std::ranges::range auto &&visited),
               "detected cyclic module dependency: " +
                   (visited |
                    std::views::transform([](auto *target) -> std::string {
                      return target->getName() + ' ';
                    }) |
                    std::views::join | ranges::to<std::string>()));

 protected:
  using FilesDeps::FilesDeps;

  std::optional<NodeList> buildNodeList(BuilderContext &ctx,
                                        const Path &output) const {
    const auto [nodeList, outputList] = Deps::buildNodeList(ctx);
    return ctx.isNeedUpdate(output, filesDeps) ||
                   ctx.isNeedUpdate(output, outputList)
               ? std::make_optional(nodeList)
               : std::nullopt;
  }

  std::unordered_map<std::string, Path> getModuleMap(
      std::unordered_set<const ModuleTarget *> visited,
      BuilderContext &ctx) const {
    std::unordered_map<std::string, Path> map;
    for (auto &mod : targetDeps) {
      const auto &pcm = mod.get();
      if (visited.contains(&pcm)) throw CyclicModuleDependency(visited);
      visited.emplace(&pcm);
      map.emplace(pcm.getName(), pcm.getOutput(ctx));
      map.merge(pcm.getModuleMap(ctx));
      visited.erase(&pcm);
    }
    return map;
  }

  std::unordered_map<std::string, Path> getModuleMap(
      BuilderContext &ctx) const {
    std::unordered_set<const ModuleTarget *> visited;
    return getModuleMap(visited, ctx);
  }

 public:
  using Deps<ModuleTarget>::dependOn;
  using FilesDeps::dependOn;
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

  void dependOn(const Path &path) {
    std::visit([&](auto &&internal) { internal.dependOn(path); }, internal);
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
