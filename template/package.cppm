#define _STR(...) #__VA_ARGS__
#define STR(...) _STR(__VA_ARGS__)

#define _CONCAT(X, Y) X##Y
#define CONCAT(X, Y) _CONCAT(X, Y)

export module CONCAT(PROJECT_NAME, _export);

import std;
import makeDotCpp;
import makeDotCpp.builder;
import makeDotCpp.project;

using namespace makeDotCpp;

export std::shared_ptr<Export> CONCAT(create_, PROJECT_NAME)() {
  return std::make_shared<Usage>(Usage::create(STR(USAGE)));
}
