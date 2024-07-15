module;
#include <boost/describe.hpp>

export module makeDotCpp.builder:Builder;
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
  std::string name = "builder";

 protected:
  CHAIN_VAR_SET(Path, srcSet, addSrc, src) { srcSet.emplace(src); };

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
  const Path cache = "cache/" + name;

 private:
  CompilerOptions compilerOptions;

 protected:
#define GENERATE_OPTIONS_JSON_METHOD(NAME, OPTIONS)        \
  void update##NAME(const Context &ctx) const {            \
    const Path path = get##NAME(ctx);                      \
    const auto &content = compilerOptions.OPTIONS;         \
    if (!fs::exists(path) || readAsStr(path) != content) { \
      std::ofstream os(path);                              \
      os.exceptions(std::ifstream::failbit);               \
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

  CHAIN_METHOD(addSrc, FileProvider, p) { srcSet.merge(p.list()); }
  CHAIN_METHOD(define, std::string, d) {
    compilerOptions.compileOption += " -D " + d;
  }
  CHAIN_METHOD(include, Path, path) {
    compilerOptions.compileOption += " -I " + path.generic_string();
  }

 public:
  Builder(const std::string &name) : name(name) {}
  virtual ~Builder() = default;

  CHAIN_METHOD(dependOn,
               ranges::range<const std::shared_ptr<const ExportFactory>> auto,
               exFactories) {
    for (auto &exFactory : exFactories) dependOn(exFactory);
  }

 protected:
  // A pair consists of a set of input src file and a common base
  using InputInfo = std::pair<std::unordered_set<Path>, Path>;

  InputInfo buildInputInfo() const {
    std::unordered_set<Path> inputSet;
    for (const auto &src : srcSet) {
      inputSet.emplace(fs::canonical(src));
    }
    return std::make_pair(std::move(inputSet), commonBase(inputSet));
  }

  auto buildUnitList(const Context &ctx, const InputInfo &inputInfo) const {
    const auto &[inputSet, base] = inputInfo;
    std::vector<Unit> unitList;
    unitList.reserve(inputSet.size());
    const auto cachePath = ctx.output / cache / "units";
    for (auto &input : inputSet) {
      const auto depJsonPath =
          cachePath / (fs::proximate(input, base) += ".json");
      const auto compileOptionsJson = getCompileOptionsJson(ctx);
      if (fs::exists(depJsonPath) &&
          fs::last_write_time(depJsonPath) > fs::last_write_time(input) &&
          fs::last_write_time(depJsonPath) >
              fs::last_write_time(compileOptionsJson)) {
        std::ifstream is(depJsonPath);
        const auto depJson = parseJson(depJsonPath);
        unitList.emplace_back(json::value_to<Unit>(depJson));
      } else {
        const auto compileOption = getCompilerOptions().compileOption;
        const auto info = ctx.compiler->getModuleInfo(input, compileOption);
        auto &unit = unitList.emplace_back(
            input, info.exported, info.name,
            ctx.compiler->getIncludeDeps(input, compileOption), info.deps);
        fs::create_directories(depJsonPath.parent_path());
        std::ofstream os(depJsonPath);
        os.exceptions(std::ifstream::failbit);
        os << json::value_from(unit);
      }
    }
    return unitList;
  }

  auto buildUnitList(const Context &ctx) const {
    const auto inputInfo = buildInputInfo();
    return buildUnitList(ctx, inputInfo);
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
  GENERATE_UPDATE_GET(CompilerOptions, co, CompilerOptions) {
    co = {};
    for (auto &ex : exSet) {
      co.compileOption += ' ' + ex->getCompileOption();
      co.linkOption += ' ' + ex->getLinkOption();
    }
    co.compileOption += ' ' + compilerOptions.compileOption;
    co.linkOption += ' ' + compilerOptions.linkOption;
  }
#undef GENERATE_UPDATE_GET

  void updateEverything(const Context &ctx) const {
    updateExportSet(ctx);
    updateExportTargetList();
    updateCompilerOptions();
    updateCompileOptionsJson(ctx);
    updateLinkOptionsJson(ctx);
  }

  virtual TargetList onBuild(const Context &ctx) const = 0;

 public:
  // Do not call build() on same ctx sequentially.
  // This will cause race condition.
  virtual FutureList build(const Context &ctx) const {
    updateEverything(ctx);
    const auto targetList = onBuild(ctx);
    const auto &target = targetList.getTarget();
    BuilderContext builderCtx{ctx, getCompilerOptions()};
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