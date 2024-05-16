export class FileProvider {
 public:
  virtual ~FileProvider() = default;

  virtual std::unordered_set<Path> list() const = 0;
};
