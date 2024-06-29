#ifdef _WIN32
#define POSTFIX ".exe"
#else
#define POSTFIX ""
#endif

import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.compiler;
import makeDotCpp.fileProvider;
import makeDotCpp.builder;

#include "project.json.hpp"
#include "src/utils/alias.hpp"

using namespace makeDotCpp;

int main(int argc, const char **argv) {
  std::deque<std::shared_ptr<Export>> packages;
  populatePackages(packages);

  Project::OptionParser op;
  op.parse(argc, argv);

  auto compiler = std::make_shared<Clang>();
  compiler->addOption("-march=native -std=c++20 -O3 -Wall")
      .addOption("-I src/utils");

  LibBuilder libBuilder("makeDotCpp");
  libBuilder.setShared(true).setCompiler(compiler).addSrc(
      Glob("src/**/module.cppm"));

  ExeBuilder builder("make.cpp");
  builder.setCompiler(compiler).addSrc("src/main.cpp");

  for (auto &package : packages) {
    libBuilder.dependOn(package);
    builder.dependOn(package);
  }

  Project()
      .setName("make-dot-cpp")
      .setBuild([&](const Context &ctx) {
        builder.dependOn(libBuilder.getExport(ctx));

        auto future = builder.build(ctx);
        try {
          future.get();
          std::cout << "\033[0;32mDone\033[0m" << std::endl;
        } catch (const std::exception &e) {
          std::cout << "\033[0;31mError: " << e.what() << "\033[0m"
                    << std::endl;
          throw e;
        }
      })
      .setInstall([](const Context &ctx) {
        if (ctx.install.empty()) {
          std::cerr << "Install path is not set" << std::endl;
          return;
        }

        {
          const Path binPath = ctx.install / "bin";
          fs::create_directory(binPath);
          fs::copy(ctx.output / (std::string("make.cpp") + POSTFIX), binPath,
                   fs::copy_options::update_existing);
          fs::copy(ctx.output / "libmakeDotCpp.dll", binPath,
                   fs::copy_options::update_existing);
        }

        {
          const Path libPath = ctx.install / "lib";
          fs::create_directory(libPath);
          for (const auto &pcmFile : fs::directory_iterator(ctx.pcmPath()))
            fs::copy(pcmFile, libPath, fs::copy_options::update_existing);
        }

        {
          fs::copy(
              "template", ctx.install / "template",
              fs::copy_options::update_existing | fs::copy_options::recursive);
        }

        {
          fs::copy("project.json", ctx.install,
                   fs::copy_options::update_existing);
        }
      })
      .to("build-make-dot-cpp")
      .run(op);
  return 0;
}
