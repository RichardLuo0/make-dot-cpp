import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.compiler;
import makeDotCpp.compiler.Clang;
import makeDotCpp.builder;
import makeDotCpp.utils;
import boost.dll;

#include "alias.hpp"
#include "macro.hpp"

using namespace makeDotCpp;

defException(CyclicPackageDependency, (ranges::range<Path> auto&& visited),
             "detected cyclic package dependency: " +
                 (visited |
                  std::views::transform([](auto& path) -> std::string {
                    return path.generic_string() + ' ';
                  }) |
                  std::views::join | ranges::to<std::string>()));
defException(PackageNotBuilt, (const std::string& name),
             name + " is not built");

class BuildFileProject {
 private:
  Context ctx{"build", fs::weakly_canonical(".build")};
  const Path packagesPath;
  std::shared_ptr<Compiler> compiler = std::make_shared<Clang>();
  LibBuilder builder{"build"};
  PackageExports packageExports;

  std::unordered_map<Path, const ProjectDesc> projectDescCache;
  std::unordered_set<Path> builtExportPackageCache;
  std::unordered_map<Path, const ExportSet> builtPackageCache;

 public:
  BuildFileProject(const Path& projectJsonPath, const Path& packagesPath)
      : packagesPath(packagesPath) {
    compiler->addOption("-march=native -std=c++20 -Wall");
#ifdef _WIN32
    compiler->addOption("-D _WIN32");
#endif

    const auto projectDesc = ProjectDesc::create(projectJsonPath, packagesPath);
    builder.setShared(true).setCompiler(compiler);
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
    ctx.debug = projectDesc.dev.debug;

    for (auto& path : projectDesc.packages) {
      buildExportPackage(path);
    }

    for (auto& path : projectDesc.dev.packages) {
      builder.dependOn(findBuiltPackage(path));
    }
  }

  ~BuildFileProject() { ctx.threadPool.wait(); }

  auto build() { return builder.build(ctx); }

  auto getOutput() { return builder.getOutput(ctx); }

  const auto& getPackageExports() { return packageExports; }

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
    packageExports.emplace(projectDesc.name,
                           projectDesc.getUsageExport(
                               ctx, compiler,
                               std::bind(&BuildFileProject::findBuiltPackage,
                                         this, std::placeholders::_1)));
  }

  const ExportSet& findBuiltPackage(const Path& path) {
    std::unordered_set<Path> visited;
    std::function<const ExportSet&(const Path& path)> findBuiltPackageR =
        [&](const Path& path) {
          const auto projectJsonPath = fs::canonical(
              fs::is_directory(path) ? path / "project.json" : path);
          auto it = builtPackageCache.find(projectJsonPath);
          if (it != builtPackageCache.end()) return it->second;
          if (visited.contains(projectJsonPath))
            throw CyclicPackageDependency(visited);
          visited.emplace(projectJsonPath);
          const auto& projectDesc = getProjectDesc(projectJsonPath);
          const auto ex = std::dynamic_pointer_cast<Export>(projectDesc.usage);
          if (ex == nullptr) throw PackageNotBuilt(projectDesc.name);
          ExportSet exSet{ex};
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

int main(int argc, const char** argv) {
  Project::OptionParser op;
  op.add("no-build", "do not build the project.");
  op.parse(argc, argv);
  if (op.contains("help")) {
    op.printHelp();
    return 0;
  }

  BuildFileProject project(fs::canonical("project.json"), op.getPackagesPath());
  try {
    auto future = project.build();
    future.get();
    const auto output = project.getOutput();
    std::cout << "\033[0;32mBuilt " << output << "\033[0m" << std::endl;

    if (op.contains("no-build")) return 0;
    boost::dll::shared_library lib(output.generic_string());
    auto build =
        lib.get<int(const PackageExports&, int, const char**)>("build");
    return build(project.getPackageExports(), argc, argv);
  } catch (std::exception& e) {
    std::cerr << "\033[0;31mError: " << e.what() << "\033[0m" << std::endl;
  }
  return 0;
}
