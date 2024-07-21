module makeDotCpp.fileProvider.Glob;
import std;
import glob;

#include "alias.hpp"

namespace makeDotCpp {
std::unordered_set<Path> Glob::list() const {
  std::unordered_set<Path> fileSet;
  auto fileList = glob::rglob(pattern);
  for (auto& file : fileList) {
    // FIXME https://github.com/llvm/llvm-project/pull/99780
    fileSet.emplace(file.make_preferred());
  }
  return fileSet;
}
}  // namespace makeDotCpp
