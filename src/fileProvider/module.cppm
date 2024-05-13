module;
#include "glob/glob.h"

export module makeDotCpp.fileProvider;

import std;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
#include "FileProvider.cpp"
#include "Glob.cpp"
}  // namespace makeDotCpp
