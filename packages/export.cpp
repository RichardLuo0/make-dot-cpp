import std;
import makeDotCpp;
import makeDotCpp.builder;

#define _STR(...) #__VA_ARGS__
#define STR(...) _STR(__VA_ARGS__)

using namespace makeDotCpp;

class : public ExportFactory {
  struct GlobExport : public Export {
    std::string getCompileOption() const override {
      return "-I " STR(PROJECT_PATH) "/glob/include";
    }

    std::string getLinkOption() const override {
      return STR(PROJECT_PATH) "/../build-cmake/packages/glob/libglob.a";
    }
  };

  std::shared_ptr<Export> create(const Context&) const override {
    return std::make_shared<GlobExport>();
  }
} globExportFactory;

extern "C" ExportFactory& exportFactory = globExportFactory;
