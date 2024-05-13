module;
#include <boost/json.hpp>

export module makeDotCpp.compiler;

import std;
import makeDotCpp.thread;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
#include "Compiler.cpp"
#include "Clang.cpp"
}  // namespace makeDotCpp
