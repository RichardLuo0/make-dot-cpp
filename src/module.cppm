export module makeDotCpp;

import std;
import makeDotCpp.thread;

#include "alias.hpp"

namespace makeDotCpp {
struct Compiler;

export struct Context {
  std::string name = "Project";
  Path output = fs::absolute("build");
  Path install;
  bool debug = false;
  std::shared_ptr<const Compiler> compiler;

  bool verbose = false;

  static inline ThreadPool threadPool{8};
  static inline DepGraph depGraph;

  void run() const { depGraph.runOn(threadPool); }
};
}  // namespace makeDotCpp
