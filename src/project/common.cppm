module;
#include <boost/describe.hpp>

export module makeDotCpp.project.desc:common;
import std;
import boost.json;

#include "alias.hpp"

namespace makeDotCpp {
struct PackageJsonContext {
  const Path& projectPath;
  const Path& packagesPath;
};

export struct PackagePath : public Path {
  using Hash = std::hash<Path>;
};

struct ProjectFmtStr : public std::string {};
struct ProjectFmtPath : public Path {};

struct PackageLoc {
 public:
  Path path;

 private:
  BOOST_DESCRIBE_CLASS(PackageLoc, (), (path), (), ())
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
}  // namespace makeDotCpp

namespace boost {
namespace json {
using namespace makeDotCpp;
template <>
struct is_sequence_like<ProjectFmtPath> : std::false_type {};
template <>
struct is_path_like<ProjectFmtPath> : std::true_type {};
template <>
struct is_described_class<PackageLoc> : std::true_type {};
}  // namespace json
}  // namespace boost
