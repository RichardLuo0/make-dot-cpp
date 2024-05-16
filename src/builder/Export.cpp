export struct Export {
  virtual ~Export() = default;

  virtual std::string getCompileOption() const { return std::string(); };

  virtual std::string getLinkOption() const { return std::string(); };

  virtual std::optional<Ref<const NamedTarget>> findPCM(
      const std::string& moduleName) const {
    return std::nullopt;
  };

  virtual std::optional<Ref<const Target>> getLibrary() const {
    return std::nullopt;
  };
};

export template <class T = Target>
struct TargetProxy : public T {
 protected:
  const T& target;
  const std::optional<Context>& ctx;
  const CompilerOptions& compilerOptions;

 public:
  TargetProxy(CLRef<T> target, CLRef<std::optional<Context>> ctx,
              CLRef<CompilerOptions> compilerOptions)
      : target(target), ctx(ctx), compilerOptions(compilerOptions) {}

  Path getOutput(BuilderContext& parent) const override {
    BuilderContextChild bCtx(parent, ctx.has_value() ? ctx.value() : parent.ctx,
                             compilerOptions);
    return target.getOutput(bCtx);
  };

  std::optional<Ref<Node>> build(BuilderContext& parent) const override {
    BuilderContextChild bCtx(parent, ctx.has_value() ? ctx.value() : parent.ctx,
                             compilerOptions);
    return target.build(bCtx);
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

export struct NamedTargetProxy : public TargetProxy<NamedTarget> {
  NamedTargetProxy(CLRef<NamedTarget> target, CLRef<std::optional<Context>> ctx,
                   CLRef<CompilerOptions> compilerOptions)
      : TargetProxy(target, ctx, compilerOptions) {}

  const std::string& getName() const override { return target.getName(); };

  std::unordered_map<std::string, Path> getModuleMap(
      BuilderContext& parent) const override {
    BuilderContextChild bCtx(parent, ctx.has_value() ? ctx.value() : parent.ctx,
                             compilerOptions);
    return target.getModuleMap(bCtx);
  }
};
