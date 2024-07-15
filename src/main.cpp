import std;
import makeDotCpp;
import makeDotCpp.dll.api;
import makeDotCpp.project.desc;
import makeDotCpp.compiler;
import makeDotCpp.compiler.Clang;
import makeDotCpp.builder;
import makeDotCpp.utils;
import boost.dll;
import boost.program_options;
import boost.json;

#include "alias.hpp"
#include "macro.hpp"

using namespace makeDotCpp;

DEF_EXCEPTION(CyclicPackageDependency, (ranges::range<Path> auto&& visited),
              "detected cyclic package dependency: " +
                  (visited |
                   std::views::transform([](auto& path) -> std::string {
                     return path.generic_string() + ' ';
                   }) |
                   std::views::join | ranges::to<std::string>()));
DEF_EXCEPTION(PackageNotBuilt, (const std::string& name),
              name + " is not built");

class BuildFileProject {
 private:
  const ProjectDesc projectDesc;
  Context ctx;
  const Path packagesPath;
  std::shared_ptr<Compiler> compiler;
  LibBuilder builder;
  api::Packages packageExports;

  std::unordered_map<Path, const ProjectDesc> projectDescCache;
  std::unordered_set<Path> builtExportPackageCache;
  std::unordered_map<Path, const ExFSet> builtPackageCache;

 public:
  BuildFileProject(const Path& projectJsonPath, const Path& packagesPath,
                   const std::shared_ptr<Compiler>& compiler)
      : projectDesc(ProjectDesc::create(projectJsonPath, packagesPath)),
        ctx(projectDesc.name + "_build", fs::weakly_canonical(".build")),
        packagesPath(packagesPath),
        compiler(compiler),
        builder(projectDesc.name + "_build") {
    compiler->addOption("-march=native -std=c++20 -Wall");
#ifdef _WIN32
    compiler->addOption("-D _WIN32");
#endif
    ctx.compiler = compiler;
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
      buildExportPackage(path);
    }

    for (auto& path : projectDesc.dev.packages) {
      builder.dependOn(findBuiltPackage(path));
    }
  }

  ~BuildFileProject() { ctx.threadPool.wait(); }

  const std::string& getName() const { return projectDesc.name; }

  const auto& getContext() const { return ctx; }

  auto build() { return builder.build(ctx); }

  auto getOutput() const { return builder.getOutput(ctx); }

  const auto& getPackageExports() const { return packageExports; }

 protected:
  const ProjectDesc& getProjectDesc(const Path& projectJsonPath) {
    auto it = projectDescCache.find(projectJsonPath);
    if (it != projectDescCache.end()) return it->second;
    const auto projectDesc = ProjectDesc::create(projectJsonPath, packagesPath);
    return projectDescCache.emplace(projectJsonPath, projectDesc).first->second;
  }

  void buildExportPackage(const Path& path) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    const auto projectPath = projectJsonPath.parent_path();
    if (builtExportPackageCache.contains(projectJsonPath)) return;
    builtExportPackageCache.emplace(projectJsonPath);

    const auto& projectDesc = getProjectDesc(projectJsonPath);
    for (auto& path : projectDesc.getUsagePackages()) {
      buildExportPackage(path);
    }
    packageExports.emplace(
        projectDesc.name,
        projectDesc.getUsageExport(
            ctx, std::bind(&BuildFileProject::findBuiltPackage, this,
                           std::placeholders::_1)));
  }

  const ExFSet& findBuiltPackage(const Path& path) {
    std::unordered_set<Path> visited;
    std::function<const ExFSet&(const Path& path)> findBuiltPackageR =
        [&](const Path& path) -> const ExFSet& {
      const auto projectJsonPath =
          fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
      auto it = builtPackageCache.find(projectJsonPath);
      if (it != builtPackageCache.end()) return it->second;
      if (visited.contains(projectJsonPath))
        throw CyclicPackageDependency(visited);
      visited.emplace(projectJsonPath);
      const auto& projectDesc = getProjectDesc(projectJsonPath);
      const auto ex =
          std::dynamic_pointer_cast<ExportFactory>(projectDesc.usage);
      if (ex == nullptr) throw PackageNotBuilt(projectDesc.name);
      ExFSet exSet{std::move(ex)};
      for (auto& path : projectDesc.getUsagePackages()) {
        auto& packages = findBuiltPackageR(path);
        exSet.insert(packages.begin(), packages.end());
      }
      visited.erase(projectJsonPath);
      return builtPackageCache.emplace(projectJsonPath, std::move(exSet))
          .first->second;
    };
    return findBuiltPackageR(path);
  }
};

namespace po = boost::program_options;

Path getPackagesPath(const po::variables_map& vm) {
  const auto vv = vm["packages"];
  return fs::canonical(vv.empty() ? Path(std::getenv("CXX_PACKAGES"))
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
  std::cout << "\033[0;32mBuilt " << ccPath << "\033[0m" << std::endl;
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
  if (vm.contains("help")) {
    std::cout << od << std::endl;
  }

  const Path packagesPath = getPackagesPath(vm);
  auto& vv = vm["compiler"];
  const auto compiler =
      getCompiler(vv.empty() ? "clang" : vv.as<std::string>(), packagesPath);
  BuildFileProject project(fs::canonical("project.json"), packagesPath,
                           compiler);
  try {
    auto future = project.build();
    future.get();
    const auto output = project.getOutput();
    std::cout << "\033[0;32mBuilt " << output << "\033[0m" << std::endl;

    if (vm.contains("no-build")) return 0;

    boost::dll::shared_library lib(output.generic_string());
    auto build = lib.get<api::Build>("build");
    int ret = build(
        {project.getName(), project.getPackageExports(), compiler, argc, argv});
    if (vm.contains("compile-commands"))
      generateCompileCommands(project.getContext(), api::compileCommands);
    return ret;
  } catch (const std::exception& e) {
    std::cerr << "\033[0;31mError: " << e.what() << "\033[0m" << std::endl;
    return 1;
  }
}
