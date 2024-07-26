export module makeDotCpp.builder:Targets;
import :common;
import :BuilderContext;
import std;
import makeDotCpp;
import makeDotCpp.dll.api;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export struct Target {
  Target() = default;
  Target(const Target &) = delete;
  Target(Target &&) = delete;
  virtual ~Target() = default;

  virtual Path getOutput(const CtxWrapper &ctx) const = 0;
  virtual void build(BuilderContext &ctx) const = 0;
};

export template <class T = Target>
struct Cached : public T {
  void build(BuilderContext &ctx) const override {
    const Path output = this->getOutput(ctx);
    if (!ctx.isBuilt(output)) onBuild(ctx, output);
  }

 protected:
  virtual void onBuild(BuilderContext &ctx, const Path &output) const = 0;
};

export const struct : public Target {
  Path getOutput(const CtxWrapper &) const override { return Path(); }
  void build(BuilderContext &) const override {}
} emptyTarget;

using ModuleMap = std::unordered_map<std::string, Path>;

export struct ModuleTarget : public Target {
  virtual const std::string &getName() const = 0;
  virtual ModuleMap getModuleMap(const CtxWrapper &ctx) const = 0;
};

export template <class T = Target>
struct TargetDeps {
 protected:
  std::deque<Ref<const T>> targetDeps;

  void build(BuilderContext &ctx) const {
    for (auto &dep : targetDeps) {
      dep.get().build(ctx);
    }
  }

  auto getOutput(BuilderContext &ctx) const {
    return targetDeps | std::views::transform([&](const auto &target) {
             return target.get().getOutput(ctx);
           }) |
           ranges::to<std::vector<Path>>();
  }

 public:
  void dependOn(const ranges::range<Ref<const T>> auto &targets) {
    for (auto &target : targets) {
      targetDeps.emplace_back(target);
    }
  }

  void dependOn(Ref<const T> target) { targetDeps.emplace_back(target); }
};

export using ModuleTargetDeps = TargetDeps<ModuleTarget>;

export struct FileDeps {
 protected:
  std::deque<Path> files;

 public:
  FileDeps() = default;
  FileDeps(const std::deque<Path> &files) : files(files) {}

  void dependOn(const ranges::range<Path> auto &paths) {
    for (auto &path : paths) {
      files.emplace_back(path);
    }
  }

  void dependOn(const Path &path) { files.emplace_back(path); }
};

export DEF_EXCEPTION(
    CyclicModuleDependency,
    (ranges::range<const ModuleTarget *> auto &&visited),
    "detected cyclic module dependency: " +
        (visited | std::views::transform([](auto *target) -> std::string {
           return target->getName() + ' ';
         }) |
         std::views::join | ranges::to<std::string>()));

export struct UnitDeps : public ModuleTargetDeps, public FileDeps {
 protected:
  using FileDeps::FileDeps;

  ModuleMap getModuleMap(std::unordered_set<const ModuleTarget *> visited,
                         const CtxWrapper &ctx) const {
    std::unordered_map<std::string, Path> map;
    for (auto &modRef : targetDeps) {
      const auto &mod = modRef.get();
      if (visited.contains(&mod)) throw CyclicModuleDependency(visited);
      visited.emplace(&mod);
      map.emplace(mod.getName(), mod.getOutput(ctx));
      map.merge(mod.getModuleMap(ctx));
      visited.erase(&mod);
    }
    return map;
  }

 public:
  ModuleMap getModuleMap(const CtxWrapper &ctx) const {
    std::unordered_set<const ModuleTarget *> visited;
    return getModuleMap(visited, ctx);
  }

  using FileDeps::dependOn;
  using TargetDeps::build;
  using TargetDeps::dependOn;
};

export struct DepsModTarget : public UnitDeps, public Cached<ModuleTarget> {
 private:
  const std::string name;
  const Path input;
  const Path output;

 public:
  DepsModTarget(const std::string &name, const Path &input, const Path &output,
                const std::deque<Path> &includeFiles)
      : UnitDeps(includeFiles), name(name), input(input), output(output) {}

  const std::string &getName() const override { return name; }

  Path getOutput(const CtxWrapper &ctx) const override {
    return ctx.modulePath() / output;
  };

  ModuleMap getModuleMap(const CtxWrapper &ctx) const override {
    return UnitDeps::getModuleMap(ctx);
  }

  using Cached<ModuleTarget>::build;

 protected:
  void onBuild(BuilderContext &ctx, const Path &output) const override {
    UnitDeps::build(ctx);
    ctx.compileModule(
        input, getModuleMap(ctx), output,
        ranges::concat(std::ranges::ref_view(files), UnitDeps::getOutput(ctx)));
  }
};

export struct ObjTarget : public Cached<> {
 protected:
  using IsModule = DepsModTarget;

  struct NotModule : public UnitDeps, public Target {
    const ObjTarget &parent;

    NotModule(const ObjTarget &parent, const std::deque<Path> &includeFiles)
        : UnitDeps(includeFiles), parent(parent) {}

    Path getOutput(const CtxWrapper &ctx) const override {
      return parent.getOutput(ctx);
    };

    void build(BuilderContext &ctx) const override {
      UnitDeps::build(ctx);
      ctx.compile(parent.input, getModuleMap(ctx), getOutput(ctx),
                  ranges::concat(std::ranges::ref_view(files),
                                 UnitDeps::getOutput(ctx)));
    }
  };

  std::variant<IsModule, NotModule> internal;
  const Path input;
  const Path output;

 public:
  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output, const std::string &name, const Path &mod)
      : internal(std::in_place_type<IsModule>, name, input, mod, includeDeps),
        input(input),
        output(output) {}

  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output)
      : internal(std::in_place_type<NotModule>, *this, includeDeps),
        input(input),
        output(output) {}

  Path getOutput(const CtxWrapper &ctx) const override {
    return ctx.objPath() / output;
  };

  const ModuleTarget &getModule() { return std::get<IsModule>(internal); }

  void dependOn(const ModuleTarget &target) {
    std::visit([&](auto &&internal) { internal.dependOn(target); }, internal);
  }

  void dependOn(const Path &path) {
    std::visit([&](auto &&internal) { internal.dependOn(path); }, internal);
  }

 protected:
  void onBuild(BuilderContext &ctx, const Path &output) const override {
    std::visit(
        [&](auto &&internal) {
          // Record compile command to generate compile_commands.json
          api::compileCommands.emplace_back(
              input, output,
              ctx.compileCommand(input, internal.getModuleMap(ctx), output));

          internal.build(ctx);
          using T = std::decay_t<decltype(internal)>;
          if constexpr (std::is_same_v<T, IsModule>) {
            const Path mod = internal.getOutput(ctx);
            ctx.compile(mod, internal.getModuleMap(ctx), output,
                        std::ranges::single_view(mod));
          }
        },
        internal);
  }
};
}  // namespace makeDotCpp
