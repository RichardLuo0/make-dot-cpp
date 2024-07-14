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
  Path relativePCMPath = "pcm";
  Path relativeObjPath = "obj";
  std::shared_ptr<const Compiler> compiler;

  static inline ThreadPool threadPool{8};
  static inline DepGraph depGraph;

  Path pcmPath() const { return output / relativePCMPath; }
  Path objPath() const { return output / relativeObjPath; }

  void run() const { depGraph.runOn(threadPool); }
};
}  // namespace makeDotCpp
