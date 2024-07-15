module;
#include <boost/describe.hpp>

export module makeDotCpp.project.desc:Usage;
import :common;
import std;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.builder;
import makeDotCpp.thread;
import makeDotCpp.utils;
import boost.json;
import boost.dll;

#include "alias.hpp"

namespace makeDotCpp {
export using ExFSet = std::unordered_set<std::shared_ptr<ExportFactory>>;

export struct Usage {
 public:
  virtual ~Usage() = default;

  // This is used if Usage does not inherit from ExportFactory.
  virtual std::shared_ptr<ExportFactory> getExport(
      const Context& ctx, const std::string& name,
      std::function<const ExFSet&(const Path&)> findBuiltPackage) const {
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

  std::shared_ptr<ExportFactory> getExport(
      const Context& ctx, const std::string& name,
      std::function<const ExFSet&(const Path&)> findBuiltPackage)
      const override {
    LibBuilder builder(name + "_export");
    builder.setShared(true).define("NO_MAIN").define("PROJECT_NAME=" + name);
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
    auto lib = std::make_shared<boost::dll::shared_library>(
        builder.getOutput(pCtx).generic_string());
    return {lib, &lib->get<ExportFactory&>("exportFactory")};
  }

  const std::unordered_set<PackagePath, PackagePath::Hash>& getPackages()
      const override {
    return packages;
  }

 private:
  BOOST_DESCRIBE_CLASS(CustomUsage, (), (devPackages, packages, setupFile), (),
                       ())
};

export struct DefaultUsage : public ExportFactory, public Usage {
 protected:
  struct BuiltTarget : public ModuleTarget {
   private:
    const std::string name;
    const Path output;

   public:
    BuiltTarget(const std::string& name, const Path& output)
        : name(name), output(output) {}

    const std::string& getName() const override { return name; }

    Path getOutput(const CtxWrapper& ctx) const override { return output; };

   protected:
    std::optional<Ref<DepGraph::Node>> build(
        BuilderContext& ctx) const override {
      return std::nullopt;
    }

    std::unordered_map<std::string, Path> getModuleMap(
        const CtxWrapper& ctx) const override {
      return std::unordered_map<std::string, Path>();
    }
  };

  struct UsageExport : public Export {
   private:
    std::optional<ProjectFmtPath> pcmPath;
    std::string compileOption;
    std::string linkOption;
    mutable std::unordered_map<std::string, BuiltTarget> cache;

   public:
    UsageExport(std::optional<ProjectFmtPath> pcmPath,
                std::string compileOption, std::string linkOption)
        : pcmPath(pcmPath),
          compileOption(compileOption),
          linkOption(linkOption) {}

    std::string getCompileOption() const override { return compileOption; }

    std::string getLinkOption() const override { return linkOption; }

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
            .emplace(std::piecewise_construct,
                     std::forward_as_tuple(moduleName),
                     std::forward_as_tuple(moduleName, modulePath))
            .first->second;
      }
    };
  };

  std::shared_ptr<Export> onCreate(const Context& ctx) const override {
    return nullptr;
  };

 public:
  std::optional<ProjectFmtPath> pcmPath;
  ProjectFmtStr compileOption;
  ProjectFmtStr linkOption;
  std::vector<std::string> libs;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  std::shared_ptr<Export> create(const Context& ctx) const override {
    return std::make_shared<UsageExport>(pcmPath, getCompileOption(),
                                         getLinkOption());
  }

  std::string getCompileOption() const {
    if (pcmPath.has_value())
      return "-fprebuilt-module-path=" + pcmPath.value().generic_string() +
             ' ' + compileOption;
    else
      return compileOption;
  }

  std::string getLinkOption() const {
    std::string lo = linkOption;
    for (auto& lib : libs) {
      lo += " -l" + lib;
    }
    return lo;
  }

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
