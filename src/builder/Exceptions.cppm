export module makeDotCpp.builder:Exceptions;
import std;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export DEF_EXCEPTION(FileNotFound, (const Path &file),
                     "file not found: " + file.generic_string());
export DEF_EXCEPTION(DirNotFound, (const Path &dir),
                     "Directory not found: " + dir.generic_string());
}  // namespace makeDotCpp
