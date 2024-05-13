module;
#include <boost/process.hpp>

export module makeDotCpp.thread;

import std;

#include "macro.hpp"
#include "alias.hpp"

namespace makeDotCpp {
#include "ThreadPool.cpp"
#include "DepGraph.cpp"
#include "FutureList.cpp"
#include "Logger.cpp"
#include "Process.cpp"
}  // namespace makeDotCpp
