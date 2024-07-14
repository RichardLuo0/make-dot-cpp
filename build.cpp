import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.project.api;
import makeDotCpp.compiler.Clang;
import makeDotCpp.fileProvider.Glob;
import makeDotCpp.builder;

#include "src/utils/alias.hpp"

using namespace makeDotCpp;
using namespace api;

extern "C" int build(const ProjectContext &ctx) {
  auto compiler = std::make_shared<Clang>();
  compiler->addOption("-march=native -O3 -std=c++20 -Wall -Wextra")
      .addOption("-Wno-missing-field-initializers")
      // https://github.com/llvm/llvm-project/issues/75057;
      .addOption("-Wno-deprecated-declarations");

  auto libBuilder = std::make_shared<LibBuilder>("makeDotCpp");
  libBuilder->setShared(true)
      .addSrc(Glob("src/**/*.cppm"))
      .include("src/utils");

  auto builder = std::make_shared<ExeBuilder>("make.cpp");
  builder->addSrc("src/main.cpp").include("src/utils");

  for (auto &package : ctx.packageExports | std::views::values) {
    libBuilder->dependOn(package);
    builder->dependOn(package);
  }
  builder->dependOn(libBuilder);

  Project(ctx.name)
      .setCompiler(compiler)
      .setBuild([&](const Context &ctx) {
        auto future = builder->build(ctx);
        future.get();
        std::cout << "\033[0;32mDone\033[0m" << std::endl;
      })
      .setInstall([&](const Context &ctx) {
        if (ctx.install.empty()) {
          std::cerr << "Install path is not set" << std::endl;
          return;
        }

        {
          const Path binPath = ctx.install / "bin";
          Project::ensureDirExists(binPath);
          Project::updateFile(builder->getOutput(ctx), binPath);
          Project::updateFile(libBuilder->getOutput(ctx), binPath);
        }

        {
          const Path libPath = ctx.install / "lib";
          Project::ensureDirExists(libPath);
          Project::updateAllFiles(ctx.pcmPath(), libPath);
        }

        { Project::updateFile("project.json", ctx.install); }
      })
      .to("build-make-dot-cpp")
      .run(ctx.argc, ctx.argv);
  return 0;
}
