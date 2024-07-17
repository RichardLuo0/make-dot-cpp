export module makeDotCpp.builder:TargetProxy;
import :Targets;
import :BuilderContext;
import std;
import makeDotCpp.utils;

namespace makeDotCpp {
export template <class T = Target>
struct TargetProxy : public T {
 protected:
  const T &target;
  const CompilerOptions &compilerOptions;

 public:
  TargetProxy(CLRef<T> target, CLRef<CompilerOptions> compilerOptions)
      : target(target), compilerOptions(compilerOptions) {}

  std::optional<Ref<Node>> build(BuilderContext &parent) const override {
    BuilderContextChild child(parent, compilerOptions);
    return target.build(child);
  }

  Path getOutput(const CtxWrapper &ctx) const override {
    return target.getOutput(ctx);
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

  ModuleMap getModuleMap(const CtxWrapper &ctx) const override {
    return target.getModuleMap(ctx);
  }
};
}  // namespace makeDotCpp
