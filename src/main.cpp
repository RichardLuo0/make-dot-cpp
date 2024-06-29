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

defException(PackageNotBuilt, (const std::string& name),
             name + " is not built");

class BuildFileProject {
 private:
  Context ctx{"build", fs::weakly_canonical(".build")};
  const Path packagesPath;
  std::shared_ptr<Compiler> compiler = std::make_shared<Clang>();
  ExeBuilder buildFileBuilder{"build"};
  std::unordered_set<std::string> exportPackageNames;

  std::unordered_map<Path, const ProjectDesc> projectDescCache;
  std::unordered_set<Path> builtExportPackageCache;
  using ExportSet = std::unordered_set<std::shared_ptr<Export>>;
  std::unordered_map<Path, const ExportSet> builtPackageCache;

 public:
  BuildFileProject(const Path& projectJsonPath, const Path& packagesPath)
      : packagesPath(packagesPath) {
    compiler->addOption("-march=native -std=c++20 -Wall");
#ifdef _WIN32
    compiler->addOption("-D _WIN32");
#endif

    const auto projectDesc = ProjectDesc::create(projectJsonPath, packagesPath);
    buildFileBuilder.setCompiler(compiler);
    std::visit(
        [&](auto&& buildFile) {
          using T = std::decay_t<decltype(buildFile)>;
          if constexpr (std::is_same_v<T, Path>)
            buildFileBuilder.addSrc(buildFile);
          else if constexpr (std::is_same_v<T, std::vector<Path>>) {
            for (auto& singleFile : buildFile)
              buildFileBuilder.addSrc(singleFile);
          }
        },
        projectDesc.dev.buildFile);
    ctx.debug = projectDesc.dev.debug;

    for (auto& path : projectDesc.packages) {
      buildExportPackage(path);
    }

    for (auto& path : projectDesc.dev.packages) {
      buildFileBuilder.dependOn(findBuiltPackage(path));
    }

    if (!exportPackageNames.empty()) {
      generateHeaderFromProjectJson(projectJsonPath, exportPackageNames);
      buildFileBuilder.include(ctx.output / "header");
    }
  }

  ~BuildFileProject() { ctx.threadPool.wait(); }

  auto build() { return buildFileBuilder.build(ctx); }

 protected:
  const ProjectDesc& getProjectDesc(const Path& projectJsonPath,
                                    const Path& packagesPath) {
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

    const auto& projectDesc = getProjectDesc(projectJsonPath, packagesPath);
    LibBuilder builder(projectDesc.name + "_export");
    builder.setCompiler(compiler)
        .define("NO_MAIN")
        .define("PROJECT_NAME=" + projectDesc.name)
        .define("PROJECT_JSON_PATH=" + projectJsonPath.generic_string())
        .define("PACKAGES_PATH=" + packagesPath.generic_string());
    const auto& usage = *projectDesc.usage;
    usage.populateBuilder(builder, packagesPath);
    for (auto& path : usage.getPackages(packagesPath)) {
      builder.dependOn(findBuiltPackage(path));
    }

    for (auto& path : projectDesc.packages) {
      buildExportPackage(path);
    }

    buildFileBuilder.dependOn(
        builder.createExport(ctx.output / "packages" / projectDesc.name));
    exportPackageNames.emplace(projectDesc.name);
  }

  const ExportSet& findBuiltPackage(const Path& path) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    auto it = builtPackageCache.find(projectJsonPath);
    if (it != builtPackageCache.end()) return it->second;
    ExportSet exSet;
    std::unordered_set<Path> visited;
    std::function<void(const Path& path)> findBuiltPackageRecursive =
        [&](const Path& path) {
          const auto projectJsonPath = fs::canonical(
              fs::is_directory(path) ? path / "project.json" : path);
          auto it = builtPackageCache.find(projectJsonPath);
          if (it != builtPackageCache.end()) {
            exSet.insert(it->second.begin(), it->second.end());
            return;
          }
          if (visited.contains(projectJsonPath)) return;
          visited.emplace(projectJsonPath);
          const auto& projectDesc =
              getProjectDesc(projectJsonPath, packagesPath);
          const auto& ex = std::dynamic_pointer_cast<Export>(projectDesc.usage);
          if (ex == nullptr) throw PackageNotBuilt(projectDesc.name);
          exSet.emplace(ex);
          for (auto& path : projectDesc.usage->getPackages(packagesPath))
            findBuiltPackageRecursive(path);
        };
    findBuiltPackageRecursive(projectJsonPath);
    return builtPackageCache.emplace(projectJsonPath, std::move(exSet))
        .first->second;
  }

  void generateHeaderFromProjectJson(
      const Path& projectJson,
      const ranges::range<std::string> auto& packageNames) {
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
  op.add_options()("no-build", "do not build the project.");
  op.parse(argc, argv);
  if (op.contains("help")) {
    op.printDesc();
    return 0;
  }

  BuildFileProject project(fs::canonical("project.json"), op.getPackagesPath());
  try {
    auto result = project.build();
    result.get();
    std::cout << "\033[0;32mBuilt " << result.output << "\033[0m" << std::endl;

    if (op.contains("no-build")) return 0;

    std::string args;
    for (std::size_t i = 1; i < argc; i++) {
      args += ' ' + std::string(argv[i]);
    }
    Process::runNoRedirect(result.output.generic_string() + args);
  } catch (std::exception& e) {
    std::cerr << "\033[0;31mError: " << e.what() << "\033[0m" << std::endl;
  }
  return 0;
}
