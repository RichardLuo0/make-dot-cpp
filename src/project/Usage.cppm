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
export using ExFSet = std::unordered_set<std::shared_ptr<const ExportFactory>>;

export struct Usage {
 public:
  virtual ~Usage() = default;

  virtual std::shared_ptr<const ExportFactory> getExportFactory(
      [[maybe_unused]] const Context& ctx,
      [[maybe_unused]] const std::string& name,
      [[maybe_unused]] const Path& projectPath,
      [[maybe_unused]] std::function<const ExFSet&(const Path&)> buildPackage)
      const = 0;

  virtual const std::unordered_set<PackagePath, PackagePath::Hash>&
  getPackages() const = 0;
};

export struct CustomUsage : public Usage {
 public:
  std::unordered_set<PackagePath, PackagePath::Hash> devPackages;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;
  Required<std::variant<ProjectFmtPath, std::vector<ProjectFmtPath>>> setupFile;

  std::shared_ptr<const ExportFactory> getExportFactory(
      const Context& ctx, const std::string& name, const Path& projectPath,
      std::function<const ExFSet&(const Path&)> buildPackage) const override {
    LibBuilder builder(name);
    builder.setShared(true)
        .define("NO_MAIN")
        .define("PROJECT_NAME=" + name)
        .define("PROJECT_PATH=" + projectPath.generic_string());
    std::visit(
        [&](auto&& setupFile) {
          using T = std::decay_t<decltype(setupFile)>;
          if constexpr (std::is_same_v<T, ProjectFmtPath>)
            builder.addSrc(setupFile);
          else if constexpr (std::is_same_v<T, std::vector<ProjectFmtPath>>) {
            for (auto& singleFile : setupFile) builder.addSrc(singleFile);
          }
        },
        setupFile);
    for (auto& path : devPackages) {
      builder.dependOn(buildPackage(path));
    }
    fs::create_directories(ctx.output);
    auto result = builder.build(ctx);
    result.get();
    auto lib = std::make_shared<boost::dll::shared_library>(
        builder.getOutput(ctx).generic_string());
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

export struct DefaultUsage : public ExportFactory,
                             public Usage,
                             public std::enable_shared_from_this<DefaultUsage> {
 protected:
  struct DefaultUsageExport : public Export {
   private:
    std::string compileOption;
    std::string linkOption;

   public:
    DefaultUsageExport(const std::string& compileOption,
                       const std::string& linkOption)
        : compileOption(compileOption), linkOption(linkOption) {}

    std::string getCompileOption() const override { return compileOption; }

    std::string getLinkOption() const override { return linkOption; }
  };

 public:
  ProjectFmtStr compileOption;
  ProjectFmtStr linkOption;
  std::vector<std::string> libs;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  std::shared_ptr<const ExportFactory> getExportFactory(
      const Context&, const std::string&, const Path&,
      std::function<const ExFSet&(const Path&)>) const override {
    return shared_from_this();
  }

  std::shared_ptr<Export> create(const Context&) const override {
    return std::make_shared<DefaultUsageExport>(getCompileOption(),
                                                getLinkOption());
  }

  std::string getCompileOption() const { return compileOption; }

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
                       (compileOption, linkOption, libs, packages), (), ())
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
