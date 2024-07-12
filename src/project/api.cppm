export module makeDotCpp.project.api;
import std;
import makeDotCpp.builder;

namespace makeDotCpp {
export using PackageExports =
    std::unordered_map<std::string, std::shared_ptr<Export>>;
}  // namespace makeDotCpp
