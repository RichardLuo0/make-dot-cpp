export module makeDotCpp;

import std;
import makeDotCpp.thread;

#include "alias.hpp"

namespace makeDotCpp {
export struct Context {
  std::string name = "Project";
  fs::path output = fs::weakly_canonical("build");
  bool debug = false;
  fs::path relativePCMPath = "pcm";
  fs::path relativeObjPath = "obj";

  static inline ThreadPool threadPool{8};
  static inline DepGraph depGraph;

  fs::path pcmPath() const { return output / relativePCMPath; }
  fs::path objPath() const { return output / relativeObjPath; }

  void run() const { depGraph.runOn(threadPool); }
};
}  // namespace makeDotCpp
