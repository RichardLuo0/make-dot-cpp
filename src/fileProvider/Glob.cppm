export module makeDotCpp.fileProvider.Glob;
import std;
import makeDotCpp.fileProvider;

#include "alias.hpp"

namespace makeDotCpp {
export class Glob : public FileProvider {
  const std::string pattern;

 public:
  Glob(const std::string& pattern) : pattern(pattern) {}

  std::unordered_set<Path> list() const override;
};
}  // namespace makeDotCpp
