module;
#include <boost/describe.hpp>

export module makeDotCpp.project.desc:ProjectDesc;
import :common;
import :Usage;
import std;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.builder;
import makeDotCpp.utils;
import boost.json;

#include "macro.hpp"

namespace makeDotCpp {
DEF_EXCEPTION(UsageNotDefined, (const std::string& name),
              "no usage is defined in package: " + name);

export struct ProjectDesc {
 public:
  Required<std::string> name;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  struct Dev {
    std::variant<Path, std::vector<Path>> buildFile = "build.cpp";
    std::string compiler = "clang++";
    bool debug = false;
    std::unordered_set<PackagePath, PackagePath::Hash> packages;

   private:
    BOOST_DESCRIBE_CLASS(Dev, (), (buildFile, compiler, debug, packages), (),
                         ())
  };
  Merge<Dev> dev;
  std::shared_ptr<Usage> usage;

  static ProjectDesc create(const Path& path, const Path& packagesPath) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    return json::value_to<Merge<ProjectDesc>>(
        parseJson(projectJsonPath),
        PackageJsonContext{projectJsonPath.parent_path(), packagesPath});
  }

  const std::unordered_set<PackagePath, PackagePath::Hash>& getUsagePackages()
      const {
    if (usage == nullptr) throw UsageNotDefined(name);
    return usage->getPackages();
  }

  std::shared_ptr<const ExportFactory> getExportFactory(
      const Context& ctx, const Path& projectPath,
      std::function<const ExFSet&(const Path&)> buildPackage) const {
    if (usage == nullptr) throw UsageNotDefined(name);
    return usage->getExportFactory(ctx, name, projectPath, buildPackage);
  }

 private:
  BOOST_DESCRIBE_CLASS(ProjectDesc, (), (name, packages, dev, usage), (), ())
};

export std::shared_ptr<Usage> tag_invoke(
    const json::value_to_tag<std::shared_ptr<Usage>>&, const json::value& jv,
    const PackageJsonContext& ctx) {
  const auto* typePtr = jv.as_object().if_contains("type");
  const auto type = typePtr ? (*typePtr).as_string() : "";
  if (type == "custom")
    return json::value_to<std::shared_ptr<Merge<CustomUsage>>>(jv, ctx);
  else
    return json::value_to<std::shared_ptr<Merge<DefaultUsage>>>(jv, ctx);
}
}  // namespace makeDotCpp

namespace boost {
namespace json {
using namespace makeDotCpp;
template <>
struct is_described_class<ProjectDesc> : std::true_type {};
template <>
struct is_described_class<ProjectDesc::Dev> : std::true_type {};
}  // namespace json
}  // namespace boost
