import std;
import makeDotCpp;
import makeDotCpp.project;
import makeDotCpp.compiler.Clang;
import makeDotCpp.fileProvider.Glob;
import makeDotCpp.builder;

using namespace makeDotCpp;

extern "C" int build(const PackageExports &packageExports, int argc,
                     const char **argv) {
  Project::OptionParser op;
  op.parse(argc, argv);

  Clang clang;
  clang.addOption("-march=native -std=c++20");

  ExeBuilder builder("example");
  builder.setCompiler(clang)
      .addSrc(Glob("src/**/*.cpp*"))
      .dependOn(packageExports | std::views::values);

  Project()
      .setName("Example")
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
      .run(op);
  return 0;
}
