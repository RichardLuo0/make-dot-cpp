import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.compiler;
import makeDotCpp.builder;
import makeDotCpp.thread;

#include "alias.hpp"
#include "macro.hpp"

using namespace makeDotCpp;

defException(ProjectNotBuildable, (std::string name),
             name + " is not buildable");

defException(CyclicDependency, (fs::path path),
             "detected cyclic dependency: " + path.generic_string());

Context ctx{"build", ".build"};
std::shared_ptr<Compiler> compiler = std::make_shared<Clang>();

void populateDepends(std::unordered_set<fs::path>& currentDeps,
                     const ProjectDesc& desc, Builder& builder,
                     const fs::path& packagesPath);

std::unordered_map<fs::path, ProjectDesc> cacheDesc;

std::optional<std::shared_ptr<Export>> buildPackage(
    std::unordered_set<fs::path>& currentDeps, fs::path path,
    const fs::path& packagesPath) {
  path = fs::weakly_canonical(path);
  const auto it = cacheDesc.find(path);
  const auto& projectDesc =
      it != cacheDesc.end()
          ? it->second
          : cacheDesc.emplace(path, ProjectDesc::create(path, packagesPath))
                .first->second;

  if (!std::holds_alternative<fs::path>(projectDesc.usage)) return std::nullopt;
  const auto buildFile = std::get<fs::path>(projectDesc.usage);

  LibBuilder builder;
  builder.setName(projectDesc.name)
      .setCompiler(compiler)
      .define("NO_MAIN")
      .define("MODULE_NAME=" + projectDesc.name)
      .define("PROJECT_PATH=" + path.generic_string())
      .define("PACKAGES_PATH=" + packagesPath.generic_string())
      .addSrc(buildFile);
  populateDepends(currentDeps, projectDesc, builder, packagesPath);
  return builder.createExport(
      fs::is_directory(path) ? path : path.parent_path(),
      ctx.output / projectDesc.name);
}

ProjectDesc findBuiltPackage(fs::path path, const fs::path& packagesPath) {
  path = fs::weakly_canonical(path);
  const auto it = cacheDesc.find(path);
  if (it != cacheDesc.end()) return it->second;
  const auto& projectDesc =
      cacheDesc.emplace(path, ProjectDesc::create(path, packagesPath))
          .first->second;
  return projectDesc;
}

void populateDepends(std::unordered_set<fs::path>& currentDeps,
                     const ProjectDesc& desc, Builder& builder,
                     const fs::path& packagesPath) {
  if (desc.dev.has_value())
    for (auto& packagePath : desc.dev.value().packages) {
      builder.addDepend(
          findBuiltPackage(packagePath, packagesPath).getExport());
    }

  for (auto& packagePath : desc.packages) {
    if (currentDeps.contains(packagePath)) throw CyclicDependency(packagePath);
    currentDeps.emplace(packagePath);
    const auto ex = buildPackage(currentDeps, packagePath, packagesPath);
    if (ex.has_value()) builder.addDepend(ex.value());
    currentDeps.erase(packagePath);
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
  const auto dev = projectDesc.dev.value();

  ExeBuilder builder;
  builder.setName("build").addSrc(dev.buildFile);
  std::unordered_set<fs::path> currentDeps{projectPath};
  populateDepends(currentDeps, projectDesc, builder, packagesPath);

  const auto result = builder.setCompiler(compiler).build(ctx);

  try {
    result.get();
    ctx.threadPool.wait();
    std::cout << "\033[0;32mBuilt " << result.output << "\033[0m" << std::endl;

    if (op.contains("no-build")) return 0;

    std::string args;
    for (std::size_t i = 1; i < argc; i++) {
      args += std::string(" ") + argv[i];
    }
    Process::runNoRedirect(result.output.generic_string() + args);
  } catch (CompileError& e) {
    std::cerr << "\033[0;31mError: " << e.what() << "\033[0m" << std::endl;
  }
}
