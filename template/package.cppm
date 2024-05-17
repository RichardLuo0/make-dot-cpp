#define _STR(X) #X
#define STR(X) _STR(X)

#define _CONCAT(X, Y) X##Y
#define CONCAT(X, Y) _CONCAT(X, Y)

export module CONCAT(MODULE_NAME, _export);

import std;
import makeDotCpp;
import makeDotCpp.builder;
import makeDotCpp.project;

using namespace makeDotCpp;
namespace fs = std::filesystem;

class PackageNotUsable : public std::exception {
 private:
  std::string msg;

 public:
  PackageNotUsable(std::string name)
      : msg(name + " is not usable now, requires further building") {}

 public:
  const char *what() const noexcept override { return msg.c_str(); };
};

export std::shared_ptr<Export> CONCAT(create_, MODULE_NAME)() {
  auto projectDesc =
      ProjectDesc::create(STR(PROJECT_JSON_PATH), STR(PACKAGES_PATH));
  if (std::holds_alternative<fs::path>(projectDesc.usage))
    throw PackageNotUsable(projectDesc.name);
  return std::get<0>(projectDesc.usage);
}
