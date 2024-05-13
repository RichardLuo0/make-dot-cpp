import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.compiler;
import makeDotCpp.fileProvider;
import makeDotCpp.builder;

using namespace makeDotCpp;

namespace fs = std::filesystem;

int main(int argc, const char **argv) {
  Project::OptionParser op;
  op.parse(argc, argv);
  const auto packagesPath = op.getPackagesPath();
  const auto projectDesc = ProjectDesc::create("project.json", packagesPath);

  Clang clang;
  clang.addOption("-march=native -std=c++20 -O3 -Wall")
      .addOption("-I src/utils");

  LibBuilder libBuilder;
  libBuilder.setName("makeDotCpp")
      .setCompiler(clang)
      .addSrc(Glob("src/**/module.cppm"));

  ExeBuilder builder;
  builder.setName("make.cpp").setCompiler(clang).addSrc("src/main.cpp");

  for (auto &packagePath : projectDesc.packages) {
    const auto package =
        ProjectDesc::create(packagePath, packagesPath).getExport();
    libBuilder.addDepend(package);
    builder.addDepend(package);
  }

  builder.addDepend(libBuilder.getExport());

  Project()
      .setName("make-dot-cpp")
      .setDebug(false)
      .setBuild([&](const Context &ctx) {
        auto future = builder.build(ctx);
        try {
          future.get();
          std::cout << "\033[0;32mDone\033[0m" << std::endl;
        } catch (const CompileError &e) {
          std::cout << "\033[0;31mError: " << e.what() << "\033[0m"
                    << std::endl;
          throw e;
        }
      })
      .setRelease([](const Context &) {})
      .to("build-make-dot-cpp")
      .run(op);
  return 0;
}
