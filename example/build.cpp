import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.dll.api;
import makeDotCpp.compiler;
import makeDotCpp.fileProvider.Glob;
import makeDotCpp.builder;
import makeDotCpp.thread.logger;

using namespace makeDotCpp;
using namespace api;
using namespace logger;

extern "C" int build(const ProjectContext &ctx) {
  ctx.compiler->addOption("-march=native -std=c++20 -Wall -Wextra");

  ExeBuilder builder("example");
  builder.addSrc(Glob("src/**/*.cpp*"))
      .dependOn(ctx.packageExports | std::views::values);

  Project(ctx.name)
      .setCompiler(ctx.compiler)
      .setBuild([&](const Context &ctx) {
        builder.build(ctx).get();
        std::cout << green << "Done" << reset << std::endl;
      })
      .run(ctx.argc, ctx.argv);
  return 0;
}
