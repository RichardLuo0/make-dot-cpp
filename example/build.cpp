#include <iostream>

import makeDotCpp;
import makeDotCpp.Compiler;
import makeDotCpp.FileProvider;
import makeDotCpp.Builder;

using namespace makeDotCpp;

int main(int argc, const char **argv) {
  Clang clang;
  clang.addOption("-march=native -std=c++20");

  Project()
      .setName("Example")
      .setDebug(true)
      .setBuild([&](const Context &ctx) {
        auto future = ExeBuilder()
                          .setName("example")
                          .setCompiler(clang)
                          .addSrc(Glob("src/**/*.cpp*"))
                          .build(ctx);
        try {
          future.get();
          std::cout << "\033[0;32mDone\033[0m" << std::endl;
        } catch (const CompileError &e) {
          std::cout << "\033[0;31merror: " << e.what() << "\033[0m"
                    << std::endl;
        }
      })
      .parseRun(argc, argv);
  return 0;
}
