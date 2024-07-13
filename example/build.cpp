import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.project.api;
import makeDotCpp.compiler.Clang;
import makeDotCpp.fileProvider.Glob;
import makeDotCpp.builder;

using namespace makeDotCpp;
using namespace api;

extern "C" int build(const ProjectContext &ctx) {
  Clang clang;
  clang.addOption("-march=native -std=c++20 -Wall -Wextra");

  ExeBuilder builder("example");
  builder.addSrc(Glob("src/**/*.cpp*"))
      .dependOn(ctx.packageExports | std::views::values);

  Project()
      .setName(ctx.name)
      .setCompiler(clang)
      .setDebug(true)
      .setBuild([&](const Context &ctx) {
        try {
          builder.build(ctx).get();
          std::cout << "\033[0;32mDone\033[0m" << std::endl;
        } catch (const std::exception &e) {
          std::cout << "\033[0;31merror: " << e.what() << "\033[0m"
                    << std::endl;
          throw e;
        }
      })
      .run(ctx.argc, ctx.argv);
  return 0;
}
