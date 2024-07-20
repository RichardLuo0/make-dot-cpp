export module makeDotCpp.fileProvider.Glob;
import std;
import makeDotCpp.fileProvider;
import glob;

#include "alias.hpp"

namespace makeDotCpp {
export class Glob : public FileProvider {
  const std::string pattern;

 public:
  Glob(const std::string& pattern) : pattern(pattern) {}

  std::unordered_set<Path> list() const override {
    std::unordered_set<Path> fileSet;
    auto fileList = glob::rglob(pattern);
    for (auto& file : fileList) {
      // FIXME https://github.com/llvm/llvm-project/pull/99780
      fileSet.emplace(file.make_preferred());
    }
    return fileSet;
  }
};
}  // namespace makeDotCpp
