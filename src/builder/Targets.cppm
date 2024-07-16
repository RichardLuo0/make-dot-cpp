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
  virtual std::optional<Ref<Node>> build(BuilderContext &ctx) const = 0;
};

export template <class T = Target>
struct CachedTarget : public T {
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
  Path getOutput(const CtxWrapper &) const override { return Path(); }
  std::optional<Ref<Node>> build(BuilderContext &) const override {
    return std::nullopt;
  }
} emptyTarget;

using ModuleMap = std::unordered_map<std::string, Path>;

export struct ModuleTarget : public Target {
  virtual const std::string &getName() const = 0;
  virtual ModuleMap getModuleMap(const CtxWrapper &ctx) const = 0;
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
    return nodeList;
  }

  auto getDepsOutput(BuilderContext &ctx) const {
    return targetDeps | std::views::transform([&](const auto &target) {
             return target.get().getOutput(ctx);
           });
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

export DEF_EXCEPTION(
    CyclicModuleDependency,
    (ranges::range<const ModuleTarget *> auto &&visited),
    "detected cyclic module dependency: " +
        (visited | std::views::transform([](auto *target) -> std::string {
           return target->getName() + ' ';
         }) |
         std::views::join | ranges::to<std::string>()));

export struct UnitDeps : public Deps<ModuleTarget>, public FilesDeps {
 public:
 protected:
  using FilesDeps::FilesDeps;

  ModuleMap getModuleMap(std::unordered_set<const ModuleTarget *> visited,
                         const CtxWrapper &ctx) const {
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

 public:
  ModuleMap getModuleMap(const CtxWrapper &ctx) const {
    std::unordered_set<const ModuleTarget *> visited;
    return getModuleMap(visited, ctx);
  }

  std::optional<NodeList> buildNodeList(BuilderContext &ctx,
                                        const Path &output) const {
    const auto nodeList = Deps::buildNodeList(ctx);
    const auto depsOutput = Deps::getDepsOutput(ctx);
    return ctx.isNeedUpdate(output, concat<Path>(filesDeps, depsOutput))
               ? std::make_optional(nodeList)
               : std::nullopt;
  }

  using Deps<ModuleTarget>::dependOn;
  using FilesDeps::dependOn;
};

export struct PCMTarget : public UnitDeps, public CachedTarget<ModuleTarget> {
 private:
  const std::string name;
  const Path input;
  const Path output;

 public:
  PCMTarget(const std::string &name, const Path &input, const Path &output,
            const std::deque<Path> &includeDeps)
      : UnitDeps(includeDeps), name(name), input(input), output(output) {}

  const std::string &getName() const override { return name; }

  const Path &getInput() const { return input; }

  Path getOutput(const CtxWrapper &ctx) const override {
    return ctx.pcmPath() / output;
  };

  ModuleMap getModuleMap(const CtxWrapper &ctx) const override {
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

    const Path &getInput() const { return input; }

    Path getOutput(const CtxWrapper &ctx) const override {
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
  const Path output;

 public:
  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output, const std::string &name, const Path &pcm)
      : internal(std::in_place_type<IsModule>, name, input, pcm, includeDeps),
        output(output) {}

  ObjTarget(const Path &input, const std::deque<Path> &includeDeps,
            const Path &output)
      : internal(std::in_place_type<NotModule>, input, *this, includeDeps),
        output(output) {}

  Path getOutput(const CtxWrapper &ctx) const override {
    return ctx.objPath() / output;
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
    // Record compile command for generating compile_commands.json
    std::visit(
        [&](auto &&internal) {
          const Path input = internal.getInput();
          api::compileCommands.emplace_back(
              input, output,
              ctx.compileCommand(input, internal.getModuleMap(ctx), output));
        },
        internal);

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
}  // namespace makeDotCpp
