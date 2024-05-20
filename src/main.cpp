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

defException(ProjectNotBuildable, (const std::string& name),
             name + " is not buildable");
defException(CyclicDependency, (const Path& path),
             "detected cyclic dependency: " + path.generic_string());

Context ctx{"build", fs::weakly_canonical(".build")};
std::shared_ptr<Compiler> compiler = std::make_shared<Clang>();

std::unordered_map<Path, ProjectDesc> descCache;

void populateDepends(std::unordered_set<Path>& visited, const Path& path,
                     const ProjectDesc& desc, Builder& builder,
                     const Path& packagesPath);

ProjectDesc& findBuiltPackage(Path path, const Path& packagesPath) {
  path = fs::weakly_canonical(path);
  const auto it = descCache.find(path);
  return it != descCache.end()
             ? it->second
             : descCache.emplace(path, ProjectDesc::create(path, packagesPath))
                   .first->second;
}

std::pair<std::shared_ptr<Export>, std::string> buildPackage(
    std::unordered_set<Path>& visited, const Path& path,
    const Path& packagesPath) {
  const auto projectJsonPath =
      fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
  const auto projectPath = projectJsonPath.parent_path();

  const auto currentPath = fs::current_path();
  fs::current_path(projectPath);
  auto& projectDesc = findBuiltPackage(projectJsonPath, packagesPath);
  LibBuilder builder;
  builder.setName(projectDesc.name + "_export")
      .setCompiler(compiler)
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
          auto& dev = projectDesc.dev.emplace();
          dev.packages.emplace(fs::weakly_canonical(packagesPath / "std"));
          dev.packages.emplace(
              fs::weakly_canonical(packagesPath / "makeDotCpp"));
        }
      },
      projectDesc.usage);
  fs::current_path(currentPath);

  populateDepends(visited, path, projectDesc, builder, packagesPath);
  return {builder.createExport(projectPath,
                               ctx.output / "packages" / projectDesc.name),
          projectDesc.name};
}

void generateHeaderForProjectJson(
    const Path& projectJson, const std::vector<std::string>& packageNames) {
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
      os << "  packages.emplace_back(create_" << name << "());\n";
    }
    os << "}\n";
  }
}

struct ProjectJsonExport : public Export {
  std::string getCompileOption() const override {
    return "-I " + (ctx.output / "header").generic_string();
  }
};

void populateDepends(std::unordered_set<Path>& visited, const Path& path,
                     const ProjectDesc& desc, Builder& builder,
                     const Path& packagesPath) {
  if (desc.dev.has_value())
    for (auto& packagePath : desc.dev.value().packages) {
      builder.addDepend(
          findBuiltPackage(packagePath, packagesPath).getExport());
    }

  std::vector<std::string> packageNames;
  packageNames.reserve(desc.packages.size());
  for (auto& packagePath : desc.packages) {
    if (visited.contains(packagePath)) throw CyclicDependency(packagePath);
    visited.emplace(packagePath);
    auto pair = buildPackage(visited, packagePath, packagesPath);
    builder.addDepend(pair.first);
    packageNames.emplace_back(pair.second);
    visited.erase(packagePath);
  }
  if (!packageNames.empty()) {
    generateHeaderForProjectJson(path, std::move(packageNames));
    builder.addDepend<ProjectJsonExport>();
  }
}

int main(int argc, const char** argv) {
  Project::OptionParser op;
  op.add_options()("no-build", "do not build the project");
  op.parse(argc, argv);
  const auto packagesPath = op.getPackagesPath();

  compiler->addOption("-march=native -std=c++20 -Wall");
#ifdef _WIN32
  compiler->addOption("-D _WIN32");
#endif

  const auto projectPath = fs::weakly_canonical("project.json");
  const auto projectDesc = ProjectDesc::create(projectPath, packagesPath);

  if (!projectDesc.dev.has_value()) throw ProjectNotBuildable(projectDesc.name);
  const auto& dev = projectDesc.dev.value();

  ctx.debug = dev.debug;

  ExeBuilder builder;
  builder.setName("build").addSrc(dev.buildFile);
  std::unordered_set<Path> visited{projectPath};
  populateDepends(visited, projectPath, projectDesc, builder, packagesPath);

  const auto result = builder.setCompiler(compiler).build(ctx);

  try {
    result.get();
    ctx.threadPool.wait();
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
}
