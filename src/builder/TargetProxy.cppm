export module makeDotCpp.builder:TargetProxy;
import :Targets;
import :BuilderContext;
import std;
import makeDotCpp;
import makeDotCpp.utils;

namespace makeDotCpp {
export template <class T = Target>
struct TargetProxy : public T {
 protected:
  const T &target;

  const Context *ctx = nullptr;
  const CompilerOption &compilerOptions;

 public:
  TargetProxy(CLRef<T> target, CLRef<CompilerOption> compilerOptions)
      : target(target), compilerOptions(compilerOptions) {}

  TargetProxy(CLRef<T> target, CLRef<Context> ctx,
              CLRef<CompilerOption> compilerOptions)
      : target(target), ctx(&ctx), compilerOptions(compilerOptions) {}

  std::optional<Ref<Node>> build(BuilderContext &parent) const override {
    BuilderContextChild child(parent, ctx != nullptr ? *ctx : parent.ctx,
                              compilerOptions);
    return target.build(child);
  }

  Path getOutput(const CtxWrapper &parent) const override {
    return target.getOutput(ctx != nullptr ? CtxWrapper(*ctx) : parent);
  }

  struct EqualTo {
    using is_transparent = void;

    constexpr bool operator()(const TargetProxy<T> &lhs,
                              const Target &rhs) const {
      return &lhs.target == &rhs;
    }
  };

  struct Hash : public std::hash<const Target *> {
    using Base = std::hash<const Target *>;
    using is_transparent = void;

    std::size_t operator()(const TargetProxy<T> &proxy) const {
      return Base::operator()(&proxy.target);
    }

    std::size_t operator()(const Target &target) const {
      return Base::operator()(&target);
    }
  };
};

export struct ModuleTargetProxy : public TargetProxy<ModuleTarget> {
  using TargetProxy<ModuleTarget>::TargetProxy;

  const std::string &getName() const override { return target.getName(); };

  ModuleMap getModuleMap(const CtxWrapper &parent) const override {
    return target.getModuleMap(ctx != nullptr ? CtxWrapper(*ctx) : parent);
  }
};
}  // namespace makeDotCpp
