import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.project.api;
import makeDotCpp.compiler.Clang;
import makeDotCpp.fileProvider.Glob;
import makeDotCpp.builder;

#include "src/utils/alias.hpp"

using namespace makeDotCpp;

extern "C" int build(const PackageExports &packageExports, int argc,
                     const char **argv) {
  auto compiler = std::make_shared<Clang>();
  compiler->addOption("-march=native -std=c++20 -O3 -Wall")
      .addOption("-I src/utils");

  LibBuilder libBuilder("makeDotCpp");
  libBuilder.setShared(true).setCompiler(compiler).addSrc(
      Glob("src/**/*.cppm"));

  ExeBuilder builder("make.cpp");
  builder.setCompiler(compiler).addSrc("src/main.cpp");

  for (auto &package : packageExports | std::views::values) {
    libBuilder.dependOn(package);
    builder.dependOn(package);
  }

  Project()
      .setName("make-dot-cpp")
      .setBuild([&](const Context &ctx) {
        builder.dependOn(libBuilder.getExport(ctx));

        auto future = builder.build(ctx);
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
          Project::updateFile(builder.getOutput(ctx), binPath);
          Project::updateFile(libBuilder.getOutput(ctx), binPath);
        }

        {
          const Path libPath = ctx.install / "lib";
          Project::ensureDirExists(libPath);
          Project::updateAllFiles(ctx.pcmPath(), libPath);
        }

        { Project::updateFile("project.json", ctx.install); }
      })
      .to("build-make-dot-cpp")
      .run(argc, argv);
  return 0;
}
