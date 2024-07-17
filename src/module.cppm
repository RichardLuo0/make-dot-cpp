export module makeDotCpp;

import std;
import makeDotCpp.thread;

#include "alias.hpp"

namespace makeDotCpp {
struct Compiler;

export struct Context {
  std::string name = "Project";
  Path output = fs::weakly_canonical("build");
  Path install;
  bool debug = false;
  Path relativeModulePath = "module";
  Path relativeObjPath = "obj";
  std::shared_ptr<const Compiler> compiler;

  bool verbose = false;

  static inline ThreadPool threadPool{8};
  static inline DepGraph depGraph;

  Path modulePath() const { return output / relativeModulePath; }
  Path objPath() const { return output / relativeObjPath; }

  void run() const { depGraph.runOn(threadPool); }
};
}  // namespace makeDotCpp
