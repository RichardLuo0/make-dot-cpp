module;
#include <boost/describe.hpp>

export module makeDotCpp.project;

import std;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.builder;
import makeDotCpp.thread;
import makeDotCpp.utils;
import boost.json;
import boost.program_options;
import boost.dll;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
struct PackageJsonContext {
  const Path& projectPath;
  const Path& packagesPath;
};

struct PackageLoc {
 public:
  Path path;

 private:
  BOOST_DESCRIBE_CLASS(PackageLoc, (), (path), (), ())
};

export struct PackagePath : public Path {
  using Hash = std::hash<Path>;
};

struct ProjectFmtStr : public std::string {};
struct ProjectFmtPath : public Path {
  using Path::path;
};

export PackagePath tag_invoke(const json::value_to_tag<PackagePath>&,
                              const json::value& jv,
                              const PackageJsonContext& ctx) {
  auto loc = json::value_to<std::variant<std::string, PackageLoc>>(jv);
  return PackagePath(fs::weakly_canonical(std::visit(
      [&](auto&& loc) {
        using T = std::decay_t<decltype(loc)>;
        if constexpr (std::is_same_v<T, std::string>)
          return ctx.packagesPath / loc;
        else
          return loc.path;
      },
      loc)));
}

export ProjectFmtStr tag_invoke(const json::value_to_tag<ProjectFmtStr>&,
                                const json::value& jv,
                                const PackageJsonContext& ctx) {
  const auto projectPathStr = ctx.projectPath.generic_string();
  return ProjectFmtStr(
      std::vformat(jv.as_string(), std::make_format_args(projectPathStr)));
}

export ProjectFmtPath tag_invoke(const json::value_to_tag<ProjectFmtPath>&,
                                 const json::value& jv,
                                 const PackageJsonContext& ctx) {
  const auto projectPathStr = ctx.projectPath.generic_string();
  return ProjectFmtPath(
      std::vformat(jv.as_string(), std::make_format_args(projectPathStr)));
}

#include "Project.cpp"
#include "Usage.cpp"
#include "ProjectDesc.cpp"
}  // namespace makeDotCpp

namespace boost {
namespace json {
using namespace makeDotCpp;
template <>
struct is_sequence_like<ProjectFmtPath> : std::false_type {};
template <>
struct is_path_like<ProjectFmtPath> : std::true_type {};
template <>
struct is_described_class<CustomUsage> : std::true_type {};
template <>
struct is_described_class<DefaultUsage> : std::true_type {};
template <>
struct is_described_class<ProjectDesc> : std::true_type {};
template <>
struct is_described_class<ProjectDesc::Dev> : std::true_type {};
template <>
struct is_described_class<PackageLoc> : std::true_type {};
}  // namespace json
}  // namespace boost
