module;
#include <boost/describe.hpp>

export module makeDotCpp.builder:Builder;
import :Exceptions;
import :BuilderContext;
import :Targets;
import :Export;
import std;
import makeDotCpp;
import makeDotCpp.thread;
import makeDotCpp.fileProvider;
import makeDotCpp.utils;
import boost.json;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export struct Unit {
  Path input;
  bool exported = false;
  std::string moduleName;
  std::deque<Path> includeDeps;
  std::deque<std::string> moduleDeps;

 private:
  BOOST_DESCRIBE_CLASS(Unit, (),
                       (input, exported, moduleName, includeDeps, moduleDeps),
                       (), ())
};

export DEF_EXCEPTION(SrcNotInBase, (const Path &src, const Path &base),
                     src.generic_string() + " is not in " +
                         base.generic_string());

export class Builder {
 protected:
  struct TargetList {
   private:
    std::unique_ptr<Target> target;
    std::deque<std::unique_ptr<const Target>> list;

   public:
    template <class T>
    TargetList(std::in_place_type_t<T>, auto &&...args)
        : target(std::make_unique<T>(std::forward<decltype(args)>(args)...)) {}

    auto append(ranges::range<std::unique_ptr<Target>> auto &&anotherList) {
      std::vector<Ref<const Target>> targetRefList;
      targetRefList.reserve(anotherList.size());
      for (auto &t : anotherList) {
        targetRefList.emplace_back(*t);
      }
      std::move(anotherList.begin(), anotherList.end(),
                std::back_inserter(list));
      return targetRefList;
    }

    const Target &at(std::size_t i) const { return *list.at(i); }

    template <class T = Target>
    T &getTarget() const {
      return *static_cast<T *>(target.get());
    }
  };

 protected:
  const std::string name = "builder";

  CHAIN_VAR_SET(Path, srcSet, addSrc, src) { srcSet.emplace(src); }

 protected:
  Path base = fs::current_path();

 public:
  CHAIN_METHOD(addSrc, FileProvider, p) { srcSet.merge(p.list()); }
  CHAIN_METHOD(setBase, Path, base) {
    this->base = fs::absolute(base);
    if (!fs::exists(this->base)) throw DirNotFound(this->base);
  }

 protected:
  CHAIN_VAR_SET(std::shared_ptr<const ExportFactory>, exFactorySet, dependOn,
                exFactory) {
    exFactorySet.emplace(exFactory);
  }

 protected:
  mutable std::unordered_set<std::shared_ptr<const Export>> exSet;

  void updateExportSet(const Context &ctx) const {
    exSet.clear();
    for (auto &exFactory : exFactorySet) exSet.emplace(exFactory->create(ctx));
  }

 protected:
  const Path cache = Path(name) / "cache";

 private:
  CompilerOption compilerOption;

 protected:
#define GENERATE_OPTIONS_JSON_METHOD(NAME, OPTIONS)        \
  void update##NAME(const Context &ctx) const {            \
    const Path path = get##NAME(ctx);                      \
    const auto &content = compilerOption.OPTIONS;          \
    if (!fs::exists(path) || readAsStr(path) != content) { \
      fs::create_directory(path.parent_path());            \
      std::ofstream os(path);                              \
      os.exceptions(std::ofstream::failbit);               \
      os << content;                                       \
    }                                                      \
  }                                                        \
                                                           \
  Path get##NAME(const Context &ctx) const {               \
    return ctx.output / cache / STR(OPTIONS) ".txt";       \
  }

  GENERATE_OPTIONS_JSON_METHOD(CompileOptionsJson, compileOption);
  GENERATE_OPTIONS_JSON_METHOD(LinkOptionsJson, linkOption);
#undef GENERATE_OPTIONS_JSON_METHOD

  CHAIN_METHOD(define, std::string, d) {
    compilerOption.compileOption += " -D " + d;
  }
  CHAIN_METHOD(include, Path, path) {
    compilerOption.compileOption += " -I " + path.generic_string();
  }

 public:
  Builder(const std::string &name) : name(name) {
    // TODO validate the name
  }
  virtual ~Builder() = default;

  CHAIN_METHOD(dependOn,
               ranges::range<const std::shared_ptr<const ExportFactory>> auto,
               exFactories) {
    for (auto &exFactory : exFactories) dependOn(exFactory);
  }

 protected:
  std::unordered_set<Path> buildInputSet() const {
    std::unordered_set<Path> inputSet;
    for (const auto &src : srcSet) {
      const auto srcPath = fs::absolute(src);
      if (!fs::exists(srcPath)) throw FileNotFound(srcPath);
      const auto [baseEnd, _] =
          std::mismatch(base.begin(), base.end(), srcPath.begin());
      if (baseEnd != base.end()) throw SrcNotInBase(srcPath, base);
      inputSet.emplace(srcPath);
    }
    return inputSet;
  }

  auto buildUnitList(const Context &ctx,
                     const std::unordered_set<Path> &inputSet) const {
    std::vector<Unit> unitList(inputSet.size());
    FutureList futureList;
    futureList.reserve(inputSet.size());
    const auto unitsPath = ctx.output / cache / "units";
    const auto compileOptionsJson = getCompileOptionsJson(ctx);
    const auto &compileOption = getCompilerOption().compileOption;
    int i = 0;
    for (auto &input : inputSet) {
      auto future = ctx.threadPool.post([&, i = i++](ThreadPool &) {
        const auto depJsonPath =
            unitsPath / (input.lexically_proximate(base) += ".json");
        if (fs::exists(depJsonPath) &&
            fs::last_write_time(depJsonPath) > fs::last_write_time(input) &&
            fs::last_write_time(depJsonPath) >
                fs::last_write_time(compileOptionsJson)) {
          unitList[i] = json::value_to<Unit>(parseJson(depJsonPath));
          // FIXME https://github.com/llvm/llvm-project/pull/99780
          unitList[i].input.make_preferred();
        } else {
          const auto info = ctx.compiler->getModuleInfo(input, compileOption);
          unitList[i] = Unit(input, info.exported, info.name,
                             ctx.compiler->getIncludeDeps(input, compileOption),
                             info.deps);
          fs::create_directories(depJsonPath.parent_path());
          std::ofstream os(depJsonPath);
          os.exceptions(std::ofstream::failbit);
          os << json::value_from(unitList[i]);
        }
        return 0;
      });
      futureList.emplace_back(std::move(future));
    }
    futureList.get();
    return unitList;
  }

  auto buildUnitList(const Context &ctx) const {
    return buildUnitList(ctx, buildInputSet());
  }

#define GENERATE_UPDATE_GET(TYPE, NAME, FUNC_NAME)    \
 private:                                             \
  mutable TYPE NAME;                                  \
                                                      \
 protected:                                           \
  const TYPE &get##FUNC_NAME() const { return NAME; } \
                                                      \
  void update##FUNC_NAME() const

  GENERATE_UPDATE_GET(std::deque<Ref<const Target>>, exTargetList,
                      ExportTargetList) {
    exTargetList.clear();
    for (auto &ex : exSet) {
      const auto target = ex->getTarget();
      if (target.has_value()) exTargetList.emplace_back(target.value());
    }
  }
  GENERATE_UPDATE_GET(CompilerOption, co, CompilerOption) {
    co = {};
    for (auto &ex : exSet) {
      co.compileOption += ' ' + ex->getCompileOption();
      co.linkOption += ' ' + ex->getLinkOption();
    }
    co.compileOption += ' ' + compilerOption.compileOption;
    co.linkOption += ' ' + compilerOption.linkOption;
  }
#undef GENERATE_UPDATE_GET

  void updateEverything(const Context &ctx) const {
    fs::create_directories(ctx.output / name);
    updateExportSet(ctx);
    // exSet is prepared here
    updateExportTargetList();
    updateCompilerOption();
    updateCompileOptionsJson(ctx);
    updateLinkOptionsJson(ctx);
  }

  virtual TargetList onBuild(const Context &ctx) const = 0;

 public:
  // Do not call build() on same ctx sequentially. May cause race condition.
  virtual FutureList build(const Context &ctx) const {
    updateEverything(ctx);
    const auto targetList = onBuild(ctx);
    const auto &target = targetList.getTarget();
    BuilderContext builderCtx{&ctx, name, &getCompilerOption()};
    target.build(builderCtx);
    ctx.run();
    return builderCtx.takeFutureList();
  };

  virtual Path getOutput(const Context &ctx) const = 0;
};
}  // namespace makeDotCpp

namespace boost {
namespace json {
template <>
struct is_described_class<makeDotCpp::Unit> : std::true_type {};
}  // namespace json
}  // namespace boost
