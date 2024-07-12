module;
#include <boost/describe.hpp>

export module makeDotCpp.project.desc:Usage;
import std;
import :common;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.builder;
import makeDotCpp.thread;
import makeDotCpp.utils;
import boost.json;
import boost.dll;

#include "alias.hpp"

namespace makeDotCpp {
export using ExportSet = std::unordered_set<std::shared_ptr<Export>>;

export struct Usage {
 public:
  virtual ~Usage() = default;

  // This is used if Usage does not inherit from Export.
  virtual std::shared_ptr<Export> getExport(
      const Context& ctx, const std::string& name,
      const std::shared_ptr<Compiler>& compiler,
      std::function<const ExportSet&(const Path&)> findBuiltPackage) const {
    return nullptr;
  };

  virtual const std::unordered_set<PackagePath, PackagePath::Hash>&
  getPackages() const = 0;
};

export struct CustomUsage : public Usage {
 public:
  std::unordered_set<PackagePath, PackagePath::Hash> devPackages;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;
  std::variant<Path, std::vector<Path>> setupFile;

  std::shared_ptr<Export> getExport(const Context& ctx, const std::string& name,
                                    const std::shared_ptr<Compiler>& compiler,
                                    std::function<const ExportSet&(const Path&)>
                                        findBuiltPackage) const override {
    LibBuilder builder(name + "_export");
    builder.setShared(true).setCompiler(compiler).define("NO_MAIN").define(
        "PROJECT_NAME=" + name);
    std::visit(
        [&](auto&& setupFile) {
          using T = std::decay_t<decltype(setupFile)>;
          if constexpr (std::is_same_v<T, Path>)
            builder.addSrc(setupFile);
          else if constexpr (std::is_same_v<T, std::vector<Path>>) {
            for (auto& singleFile : setupFile) builder.addSrc(singleFile);
          }
        },
        setupFile);
    for (auto& path : devPackages) {
      builder.dependOn(findBuiltPackage(path));
    }
    Context pCtx{name, ctx.output / "packages" / name};
    auto result = builder.build(pCtx);
    result.get();
    boost::dll::shared_library lib(builder.getOutput(pCtx).generic_string());
    auto getExport = lib.get<std::shared_ptr<Export>()>("getExport");
    return getExport();
  }

  const std::unordered_set<PackagePath, PackagePath::Hash>& getPackages()
      const override {
    return packages;
  }

 private:
  BOOST_DESCRIBE_CLASS(CustomUsage, (), (devPackages, packages, setupFile), (),
                       ())
};

export struct DefaultUsage : public Export, public Usage {
 protected:
  struct BuiltTarget : public ModuleTarget {
   private:
    const std::string name;
    const Path output;

   public:
    BuiltTarget(const std::string& name, const Path& output)
        : name(name), output(output) {}

    const std::string& getName() const override { return name; }

    Path getOutput(BuilderContext& ctx) const override { return output; };

   protected:
    std::optional<Ref<DepGraph::Node>> build(
        BuilderContext& ctx) const override {
      return std::nullopt;
    }

    std::unordered_map<std::string, Path> getModuleMap(
        BuilderContext& ctx) const override {
      return std::unordered_map<std::string, Path>();
    }
  };

 public:
  std::optional<ProjectFmtPath> pcmPath;
  ProjectFmtStr compileOption;
  ProjectFmtStr linkOption;
  std::vector<std::string> libs;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  std::string getCompileOption() const override {
    if (pcmPath.has_value())
      return "-fprebuilt-module-path=" + pcmPath.value().generic_string() +
             ' ' + compileOption;
    else
      return compileOption;
  }

  std::string getLinkOption() const override {
    std::string lo = linkOption;
    for (auto& lib : libs) {
      lo += " -l" + lib;
    }
    return lo;
  }

 private:
  mutable std::unordered_map<std::string, BuiltTarget> cache;

 public:
  std::optional<Ref<const ModuleTarget>> findPCM(
      const std::string& moduleName) const override {
    if (!pcmPath.has_value()) return std::nullopt;
    const auto it = cache.find(moduleName);
    if (it != cache.end())
      return it->second;
    else {
      auto modulePath =
          pcmPath.value() / (replace(moduleName, ':', '-') + ".pcm");
      if (!fs::exists(modulePath)) return std::nullopt;
      return cache
          .emplace(std::piecewise_construct, std::forward_as_tuple(moduleName),
                   std::forward_as_tuple(moduleName, modulePath))
          .first->second;
    }
  };

  const std::unordered_set<PackagePath, PackagePath::Hash>& getPackages()
      const override {
    return packages;
  }

 private:
  BOOST_DESCRIBE_CLASS(DefaultUsage, (),
                       (pcmPath, compileOption, linkOption, libs, packages), (),
                       ())
};
}  // namespace makeDotCpp

namespace boost {
namespace json {
using namespace makeDotCpp;
template <>
struct is_described_class<CustomUsage> : std::true_type {};
template <>
struct is_described_class<DefaultUsage> : std::true_type {};
}  // namespace json
}  // namespace boost
