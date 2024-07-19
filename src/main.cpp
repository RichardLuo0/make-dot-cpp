import std;
import makeDotCpp;
import makeDotCpp.dll.api;
import makeDotCpp.project.desc;
import makeDotCpp.compiler;
import makeDotCpp.compiler.Clang;
import makeDotCpp.builder;
import makeDotCpp.utils;
import makeDotCpp.thread.logger;
import boost.dll;
import boost.program_options;
import boost.json;

#include "alias.hpp"
#include "macro.hpp"

using namespace makeDotCpp;

DEF_EXCEPTION(ProjectJsonNotFound, (),
              "project.json is not found in current location");
DEF_EXCEPTION(CyclicPackageDependency, (ranges::range<Path> auto&& visited),
              "detected cyclic package dependency: " +
                  (visited |
                   std::views::transform([](auto& path) -> std::string {
                     return path.generic_string() + ' ';
                   }) |
                   std::views::join | ranges::to<std::string>()));

class BuildFileProject {
 private:
  const ProjectDesc projectDesc;
  Context ctx;
  const Path packagesPath;
  LibBuilder builder;
  api::Packages packageExports;

  std::unordered_map<Path, const ProjectDesc> projectDescCache;
  std::unordered_set<Path> localPackageCache;
  std::unordered_map<Path, const ExFSet> globalPackageCache;

 public:
  BuildFileProject(const Path& projectJsonPath, const Path& packagesPath,
                   const std::shared_ptr<Compiler>& compiler)
      : projectDesc(ProjectDesc::create(projectJsonPath, packagesPath)),
        ctx{.name = projectDesc.name + "_build",
            .output = fs::weakly_canonical(".build"),
            .compiler = compiler},
        packagesPath(packagesPath),
        builder(projectDesc.name + "_build") {
    compiler->addOption("-march=native -std=c++20 -Wall");
#ifdef _WIN32
    compiler->addOption("-D _WIN32");
#endif
    ctx.debug = projectDesc.dev.debug;

    builder.setShared(true);
    std::visit(
        [&](auto&& buildFile) {
          using T = std::decay_t<decltype(buildFile)>;
          if constexpr (std::is_same_v<T, Path>)
            builder.addSrc(buildFile);
          else if constexpr (std::is_same_v<T, std::vector<Path>>) {
            for (auto& singleFile : buildFile) builder.addSrc(singleFile);
          }
        },
        projectDesc.dev.buildFile);

    for (auto& path : projectDesc.packages) {
      buildLocalPackage(path);
    }

    for (auto& path : projectDesc.dev.packages) {
      builder.dependOn(buildGlobalPackage(path));
    }
  }

  ~BuildFileProject() { ctx.threadPool.wait(); }

  const std::string& getName() const { return projectDesc.name; }

  const auto& getContext() const { return ctx; }

  auto build() {
    fs::create_directories(ctx.output);
    return builder.build(ctx);
  }

  auto getOutput() const { return builder.getOutput(ctx); }

  const auto& getPackageExports() const { return packageExports; }

 protected:
  const ProjectDesc& getProjectDesc(const Path& projectJsonPath) {
    auto it = projectDescCache.find(projectJsonPath);
    if (it != projectDescCache.end()) return it->second;
    auto projectDesc = ProjectDesc::create(projectJsonPath, packagesPath);
    return projectDescCache.emplace(projectJsonPath, std::move(projectDesc))
        .first->second;
  }

  void buildLocalPackage(const Path& path) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    if (localPackageCache.contains(projectJsonPath)) return;
    localPackageCache.emplace(projectJsonPath);

    const auto& projectDesc = getProjectDesc(projectJsonPath);
    for (auto& path : projectDesc.getUsagePackages()) {
      buildLocalPackage(path);
    }
    const std::string& name = projectDesc.name;
    packageExports.emplace(
        name, buildPackage(projectDesc, ctx.output / "packages" / name,
                           projectJsonPath, false));
  }

  const ExFSet& buildGlobalPackage(const Path& path) {
    std::unordered_set<Path> visited;
    std::function<const ExFSet&(const Path& path)> buildGlobalPackageR =
        [&](const Path& path) -> const ExFSet& {
      const auto projectJsonPath =
          fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
      auto it = globalPackageCache.find(projectJsonPath);
      if (it != globalPackageCache.end()) return it->second;

      if (visited.contains(projectJsonPath))
        throw CyclicPackageDependency(visited);
      visited.emplace(projectJsonPath);

      const auto& projectDesc = getProjectDesc(projectJsonPath);
      ExFSet exSet{buildPackage(projectDesc,
                                projectJsonPath.parent_path() / ".build",
                                projectJsonPath, true)};
      for (auto& path : projectDesc.getUsagePackages()) {
        auto& packages = buildGlobalPackageR(path);
        exSet.insert(packages.begin(), packages.end());
      }
      visited.erase(projectJsonPath);
      return globalPackageCache.emplace(projectJsonPath, std::move(exSet))
          .first->second;
    };
    return buildGlobalPackageR(path);
  }

  std::shared_ptr<const ExportFactory> buildPackage(
      const ProjectDesc& projectDesc, const Path& output,
      const Path& projectJsonPath, bool isGlobal) {
    Context pCtx{
        .name = projectDesc.name, .output = output, .compiler = ctx.compiler};
    return projectDesc.getExportFactory(
        pCtx, isGlobal, projectJsonPath.parent_path(),
        std::bind(&BuildFileProject::buildGlobalPackage, this,
                  std::placeholders::_1));
  }
};

namespace po = boost::program_options;

Path getPackagesPath(const po::variables_map& vm) {
  const auto* fromEnv = std::getenv("CXX_PACKAGES");
  const auto vv = vm["packages"];
  return fs::canonical(vv.empty() ? (fromEnv ? Path(fromEnv) : "packages")
                                  : vv.as<Path>());
}

void generateCompileCommands(
    const Context& ctx, const ranges::range<api::CompileCommand> auto& ccs) {
  json::array array;
  for (const auto& cc : ccs) {
    json::object cco;
    cco["directory"] = ctx.output.generic_string();
    cco["command"] = cc.command;
    cco["file"] = cc.input.generic_string();
    cco["output"] = cc.output.generic_string();
    array.emplace_back(cco);
  }
  const auto ccPath = ctx.output / "compile_commands.json";
  std::ofstream os(ccPath);
  os.exceptions(std::ifstream::failbit);
  os << array;
  logger::success() << "Built " << ccPath << std::endl;
}

DEF_EXCEPTION(UnknownCompiler, (const std::string& name),
              "unknown compiler: " + name);

std::shared_ptr<Compiler> getCompiler(const std::string& name,
                                      const Path& packagesPath) {
  if (name == "clang") return std::make_shared<Clang>();
  try {
    auto lib = std::make_shared<boost::dll::shared_library>(
        (packagesPath / ".global" / "compiler" / name += SHLIB_POSTFIX)
            .generic_string());
    return {lib, &lib->get<Compiler&>("compiler")};
  } catch (const std::exception& e) {
    throw UnknownCompiler(name);
  }
}

int main(int argc, const char** argv) {
  po::options_description od;
  po::variables_map vm;
  od.add_options()
      .operator()("help,h", "Display help message.")
      .operator()("no-build", "Do not build the project.")
      .operator()("compile-commands", "Generate compile_commands.json")
      .operator()("compiler", po::value<std::string>(), "The compiler to use.");
  po::store(po::command_line_parser(argc, argv)
                .options(od)
                .allow_unregistered()
                .run(),
            vm);
  po::notify(vm);

  Path projectJsonPath = fs::exists("project.json") ? "project.json" : Path();

  if (vm.contains("help")) {
    std::cout << od << std::endl;
    if (projectJsonPath.empty()) return 0;
  }

  try {
    if (projectJsonPath.empty()) throw ProjectJsonNotFound();
    const Path packagesPath = getPackagesPath(vm);
    const auto& vv = vm["compiler"];
    const auto compiler =
        getCompiler(vv.empty() ? "clang" : vv.as<std::string>(), packagesPath);
    BuildFileProject project(projectJsonPath, packagesPath, compiler);
    auto future = project.build();
    future.get();
    const auto output = project.getOutput();
    logger::success() << "Built " << output << std::endl;

    if (vm.contains("no-build")) return 0;

    boost::dll::shared_library lib(output.generic_string());
    auto build = lib.get<api::Build>("build");
    int ret = build(
        {project.getName(), project.getPackageExports(), compiler, argc, argv});
    if (vm.contains("compile-commands"))
      generateCompileCommands(project.getContext(), api::compileCommands);
    return ret;
  } catch (const std::exception& e) {
    logger::error() << "error: " << e.what() << std::endl;
    return 1;
  }
}
