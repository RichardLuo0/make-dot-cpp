export class Glob : public FileProvider {
  const std::string pattern;

 public:
  Glob(const std::string& pattern) : pattern(pattern) {}

  std::unordered_set<fs::path> list() const override {
    std::unordered_set<fs::path> fileSet;
    auto fileList = glob::rglob(pattern);
    for (auto& file : fileList) {
      fileSet.emplace(file);
    }
    return fileSet;
  }
};
