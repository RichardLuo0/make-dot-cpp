module;
#include <string>
#include <filesystem>
#include <iostream>
export module Print;

namespace fs = std::filesystem;

export void print() { std::cout << "Hello world!" << '\n'; }

namespace makeDotCpp {
export struct Context {
  std::string name = "Project";
  fs::path output;
  bool debug = false;
  fs::path relativeModulePath = "pcm";
  fs::path relativeObjPath = "obj";
  inline static int i = 1;

  fs::path modulePath() const { i+=1; return output / relativeModulePath; }
  fs::path objPath() const { return output / relativeObjPath; }
};
}
