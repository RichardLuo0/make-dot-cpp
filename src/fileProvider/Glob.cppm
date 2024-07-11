module;
#include "glob/glob.h"

export module makeDotCpp.fileProvider.Glob;
import std;
import makeDotCpp.fileProvider;

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
      fileSet.emplace(file);
    }
    return fileSet;
  }
};
}  // namespace makeDotCpp
