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

  const CtxWrapper *ctxW = nullptr;
  const CompilerOption &compilerOptions;

 public:
  TargetProxy(const T *target, const CompilerOption *compilerOptions)
      : target(*target), compilerOptions(*compilerOptions) {}

  TargetProxy(const T *target, const CtxWrapper *ctxW,
              const CompilerOption *compilerOptions)
      : target(*target), ctxW(ctxW), compilerOptions(*compilerOptions) {}

  void build(BuilderContext &parent) const override {
    BuilderContext::Child child(&parent, &compilerOptions, ctxW);
    target.build(child);
  }

  Path getOutput(const CtxWrapper &parent) const override {
    return target.getOutput(ctxW != nullptr ? *ctxW : parent);
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
    return target.getModuleMap(ctxW != nullptr ? *ctxW : parent);
  }
};
}  // namespace makeDotCpp
