export class FileProvider {
 public:
  virtual ~FileProvider() = default;

  virtual std::unordered_set<fs::path> list() const = 0;
};
