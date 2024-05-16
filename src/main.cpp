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
defException(CyclicDependency, (Path path),
             "detected cyclic dependency: " + path.generic_string());

Path output = fs::weakly_canonical(".build");
std::shared_ptr<Compiler> compiler = std::make_shared<Clang>();

std::unordered_map<Path, ProjectDesc> descCache;

void populateDepends(std::unordered_set<Path>& currentDeps,
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

std::shared_ptr<Export> buildPackage(std::unordered_set<Path>& currentDeps,
                                     Path path,
                                     const Path& packagesPath) {
  auto& projectDesc = findBuiltPackage(path, packagesPath);
  LibBuilder builder;
  builder.setName(projectDesc.name)
      .setCompiler(compiler)
      .define("NO_MAIN")
      .define("MODULE_NAME=" + projectDesc.name)
      .define("PROJECT_PATH=" + path.generic_string())
      .define("PACKAGES_PATH=" + packagesPath.generic_string());
  if (std::holds_alternative<Path>(projectDesc.usage)) {
    builder.addSrc(std::get<Path>(projectDesc.usage));
  } else {
    builder.addSrc(packagesPath / "makeDotCpp/template/package.cppm");
    auto& dev = projectDesc.dev.emplace();
    dev.packages.emplace(fs::weakly_canonical(packagesPath / "std"));
    dev.packages.emplace(fs::weakly_canonical(packagesPath / "makeDotCpp"));
  }
  populateDepends(currentDeps, projectDesc, builder, packagesPath);
  return builder.createExport(
      fs::is_directory(path) ? path : path.parent_path(),
      output / "packages" / projectDesc.name);
}

void populateDepends(std::unordered_set<Path>& currentDeps,
                     const ProjectDesc& desc, Builder& builder,
                     const Path& packagesPath) {
  if (desc.dev.has_value())
    for (auto& packagePath : desc.dev.value().packages) {
      builder.addDepend(
          findBuiltPackage(packagePath, packagesPath).getExport());
    }

  for (auto& packagePath : desc.packages) {
    if (currentDeps.contains(packagePath)) throw CyclicDependency(packagePath);
    currentDeps.emplace(packagePath);
    builder.addDepend(buildPackage(currentDeps, packagePath, packagesPath));
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
  const auto& dev = projectDesc.dev.value();

  ExeBuilder builder;
  builder.setName("build").addSrc(dev.buildFile);
  std::unordered_set<Path> currentDeps{projectPath};
  populateDepends(currentDeps, projectDesc, builder, packagesPath);

  Context ctx{"build", output, dev.debug};
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
  } catch (std::exception& e) {
    std::cerr << "\033[0;31mError: " << e.what() << "\033[0m" << std::endl;
  }
}
