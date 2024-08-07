import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.dll.api;
import makeDotCpp.compiler;
import makeDotCpp.fileProvider.Glob;
import makeDotCpp.builder;
import makeDotCpp.thread.logger;

#include "src/utils/alias.hpp"

using namespace makeDotCpp;
using namespace api;

extern "C" int build(const ProjectContext &ctx) {
  Project::OptionParser op;
  op.parse(ctx.argc, ctx.argv);

  if (!op.contains("debug")) ctx.compiler->addOption("-O3");
  ctx.compiler->addOption("-std=c++20 -Wall -Wextra")
      .addOption("-Wno-missing-field-initializers")
      // https://github.com/llvm/llvm-project/issues/75057;
      .addOption("-Wno-deprecated-declarations");

  auto libBuilder = std::make_shared<LibBuilder>("makeDotCpp");
  libBuilder->setShared(true)
      .setBase("src")
      .addSrc(Glob("src/**/*.cppm"))
      .addSrc(Glob("src/?*/*.cpp"))
      .include("src/utils");

  auto builder = std::make_shared<ExeBuilder>("make.cpp");
  builder->setBase("src").addSrc("src/main.cpp").include("src/utils");

  for (auto &package : ctx.packages | std::views::values) {
    libBuilder->dependOn(package);
    builder->dependOn(package);
  }
  builder->dependOn(libBuilder);

  Project(ctx.name)
      .setCompiler(ctx.compiler)
      .setBuild([&](const Context &ctx) {
        auto future = builder->build(ctx);
        future.get();
        logger::success() << "Done" << std::endl;
      })
      .setInstall([&](const Context &ctx) {
        if (ctx.install.empty()) {
          logger::error() << "Install path is not set" << std::endl;
          return;
        }

        {
          const Path binPath = ctx.install / "bin";
          Project::ensureDirExists(binPath);
          Project::updateFile(builder->getOutput(ctx), binPath);
          Project::updateFile(libBuilder->getOutput(ctx), binPath);
        }

        { Project::updateFile("project.json", ctx.install); }

        logger::success() << "Installed " << ctx.name << std::endl;
      })
      .to("build-{1}")
      .run(op);
  return 0;
}
