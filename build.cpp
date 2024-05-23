import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.compiler;
import makeDotCpp.fileProvider;
import makeDotCpp.builder;

#include "project.json.hpp"

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
  libBuilder.setCompiler(compiler).addSrc(Glob("src/**/module.cppm"));

  ExeBuilder builder("make.cpp");
  builder.setCompiler(compiler).addSrc("src/main.cpp");

  for (auto &package : packages) {
    libBuilder.dependOn(package);
    builder.dependOn(package);
  }

  Project()
      .setName("make-dot-cpp")
      .setDebug(false)
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
      .setRelease([](const Context &) {})
      .to("build-make-dot-cpp")
      .run(op);
  return 0;
}
