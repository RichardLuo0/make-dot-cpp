export struct Export {
  virtual ~Export() = default;

  // This is used for user build file to change behaviours in export.
  virtual void set(const std::string& key, const std::string& value) {}

  virtual std::string getCompileOption() const { return std::string(); }

  virtual std::string getLinkOption() const { return std::string(); }

  virtual std::optional<Ref<const ModuleTarget>> findPCM(
      const std::string& moduleName) const {
    return std::nullopt;
  }

  virtual std::optional<Ref<const Target>> getTarget() const {
    return std::nullopt;
  }
};

export template <class T = Target>
struct TargetProxy : public T {
 protected:
  const T& target;
  const std::optional<Context>& ctx;
  const std::optional<CompilerOptions>& compilerOptions;

  auto createNewContext(BuilderContext& parent) const {
    const auto co = compilerOptions.has_value() ? compilerOptions.value()
                                                : parent.compilerOptions;
    const auto& _ctx = ctx.has_value() ? ctx.value() : parent.ctx;
    return BuilderContextChild(parent, _ctx, co);
  }

 public:
  TargetProxy(
      CLRef<T> target, CLRef<const std::optional<Context>> ctx = std::nullopt,
      CLRef<std::optional<CompilerOptions>> compilerOptions = std::nullopt)
      : target(target), ctx(ctx), compilerOptions(compilerOptions) {}

  Path getOutput(BuilderContext& parent) const override {
    auto ctx = createNewContext(parent);
    return target.getOutput(ctx);
  };

  std::optional<Ref<Node>> build(BuilderContext& parent) const override {
    auto ctx = createNewContext(parent);
    return target.build(ctx);
  }

  struct EqualTo {
    using is_transparent = void;

    constexpr bool operator()(const TargetProxy<T>& lhs,
                              const Target& rhs) const {
      return &lhs.target == &rhs;
    }
  };

  struct Hash : public std::hash<const Target*> {
    using Base = std::hash<const Target*>;
    using is_transparent = void;

    std::size_t operator()(const TargetProxy<T>& proxy) const {
      return Base::operator()(&proxy.target);
    }

    std::size_t operator()(const Target& target) const {
      return Base::operator()(&target);
    }
  };
};

export struct ModuleTargetProxy : public TargetProxy<ModuleTarget> {
  using TargetProxy<ModuleTarget>::TargetProxy;

  const std::string& getName() const override { return target.getName(); };

  std::unordered_map<std::string, Path> getModuleMap(
      BuilderContext& parent) const override {
    auto ctx = createNewContext(parent);
    return target.getModuleMap(ctx);
  }
};
