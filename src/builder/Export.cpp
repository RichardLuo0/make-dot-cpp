export struct Export {
  virtual ~Export() = default;

  virtual std::string getCompileOption() const { return std::string(); };

  virtual std::string getLinkOption() const { return std::string(); };

  virtual std::optional<Ref<const Target>> findPCM(
      const std::string &moduleName) const {
    return std::nullopt;
  };

  virtual std::optional<Ref<const Target>> getLibrary() const {
    return std::nullopt;
  };
};
