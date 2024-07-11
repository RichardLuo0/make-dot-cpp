export module makeDotCpp.fileProvider;
import std;

#include "alias.hpp"

namespace makeDotCpp {
export class FileProvider {
 public:
  virtual ~FileProvider() = default;

  virtual std::unordered_set<Path> list() const = 0;
};
}  // namespace makeDotCpp
