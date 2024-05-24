import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.compiler;
import makeDotCpp.builder;
import makeDotCpp.thread;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

using namespace makeDotCpp;

defException(CyclicDependency, (const Path& path),
             "detected cyclic dependency: " + path.generic_string());

class BuildCppProject {
 private:
  Context ctx{"build", fs::weakly_canonical(".build")};
  const Path packagesPath;
  std::shared_ptr<Compiler> compiler = std::make_shared<Clang>();
  ExeBuilder buildCppBuilder{"build"};
  std::unordered_set<std::string> exportPackageNames;

  std::unordered_set<Path> builtExportPackageSet;
  using ExportSet = std::unordered_set<std::shared_ptr<Export>>;
  std::unordered_map<Path, ExportSet> builtPackageMap;
  std::unordered_set<Path> visitedBuiltPackageSet;

 public:
  BuildCppProject(const Path& projectJsonPath, const Path& packagesPath)
      : packagesPath(packagesPath) {
    compiler->addOption("-march=native -std=c++20 -Wall");
#ifdef _WIN32
    compiler->addOption("-D _WIN32");
#endif

    const auto projectDesc = ProjectDesc::create(projectJsonPath, packagesPath);
    buildCppBuilder.setCompiler(compiler).addSrc(projectDesc.dev.buildFile);
    ctx.debug = projectDesc.dev.debug;

    for (auto& path : projectDesc.packages) {
      buildExportPackage(path);
    }

    for (auto& path : projectDesc.dev.packages) {
      buildCppBuilder.dependOn(findBuiltPackage(path));
    }

    if (!exportPackageNames.empty()) {
      generateHeaderForProjectJson(projectJsonPath,
                                   std::move(exportPackageNames));
      buildCppBuilder.include(ctx.output / "header");
    }
  }

  ~BuildCppProject() { ctx.threadPool.wait(); }

  auto build() { return buildCppBuilder.build(ctx); }

 protected:
  void buildExportPackage(const Path& path) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    const auto projectPath = projectJsonPath.parent_path();
    if (builtExportPackageSet.contains(projectJsonPath)) return;
    builtExportPackageSet.emplace(projectJsonPath);

    auto projectDesc = ProjectDesc::create(projectJsonPath, packagesPath);
    LibBuilder builder(projectDesc.name + "_export");
    builder.setCompiler(compiler)
        .define("NO_MAIN")
        .define("PROJECT_NAME=" + projectDesc.name)
        .define("PROJECT_JSON_PATH=" + projectJsonPath.generic_string())
        .define("PACKAGES_PATH=" + packagesPath.generic_string());
    std::visit(
        [&](auto&& usage) {
          using T = std::decay_t<decltype(usage)>;
          if constexpr (std::is_same_v<T, Path>)
            builder.addSrc(usage);
          else if constexpr (std::is_same_v<T, std::vector<Path>>) {
            for (auto& path : usage) {
              builder.addSrc(path);
            }
          } else {
            builder.addSrc(packagesPath / "makeDotCpp/template/package.cppm");
            builder.define("\"USAGE=" + replace(usage->toJson(), "\"", "\\\"") +
                           '\"');
            projectDesc.dev.packages.emplace(
                fs::weakly_canonical(packagesPath / "std"));
            projectDesc.dev.packages.emplace(
                fs::weakly_canonical(packagesPath / "makeDotCpp"));
          }
        },
        projectDesc.usage);

    for (auto& path : projectDesc.packages) {
      buildExportPackage(path);
    }

    for (auto& path : projectDesc.dev.packages) {
      builder.dependOn(findBuiltPackage(path));
    }

    buildCppBuilder.dependOn(
        builder.createExport(ctx.output / "packages" / projectDesc.name));
    exportPackageNames.emplace(projectDesc.name);
  }

  const ExportSet& findBuiltPackage(const Path& path) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    auto it = builtPackageMap.find(projectJsonPath);
    if (it != builtPackageMap.end()) return it->second;
    if (visitedBuiltPackageSet.contains(projectJsonPath))
      throw CyclicDependency(projectJsonPath);
    visitedBuiltPackageSet.emplace(projectJsonPath);

    auto projectDesc = ProjectDesc::create(projectJsonPath, packagesPath);
    ExportSet exSet{projectDesc.getExport()};
    for (auto& path : projectDesc.packages) {
      auto& packageExSet = findBuiltPackage(path);
      exSet.insert(packageExSet.begin(), packageExSet.end());
    }

    visitedBuiltPackageSet.erase(projectJsonPath);
    return builtPackageMap.emplace(projectJsonPath, std::move(exSet))
        .first->second;
  }

  void generateHeaderForProjectJson(
      const Path& projectJson, ranges::range<std::string> auto&& packageNames) {
    const auto output =
        ctx.output / "header" / (projectJson.filename() += ".hpp");
    if (!fs::exists(output) ||
        fs::last_write_time(output) < fs::last_write_time(projectJson)) {
      fs::create_directories(output.parent_path());
      std::ofstream os(output);
      os.exceptions(std::ifstream::failbit);
      for (auto& name : packageNames) {
        os << "import " << name << "_export;\n";
      }
      os << "void populatePackages(auto&& packages) {\n";
      for (auto& name : packageNames) {
        os << "  packages.emplace_back(" << name << "_export::getExport());\n";
      }
      os << "}\n";
    }
  }
};

int main(int argc, const char** argv) {
  Project::OptionParser op;
  op.add_options()("no-build", "do not build the project");
  op.parse(argc, argv);
  BuildCppProject project(fs::canonical("project.json"), op.getPackagesPath());

  try {
    auto result = project.build();
    result.get();
    std::cout << "\033[0;32mBuilt " << result.output << "\033[0m" << std::endl;

    if (op.contains("no-build")) return 0;

    std::string args;
    for (std::size_t i = 1; i < argc; i++) {
      args += std::to_string(' ') + argv[i];
    }
    Process::runNoRedirect(result.output.generic_string() + args);
  } catch (std::exception& e) {
    std::cerr << "\033[0;31mError: " << e.what() << "\033[0m" << std::endl;
  }
  return 0;
}
