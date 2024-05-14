struct PackageJsonContext {
  const fs::path& projectPath;
  const fs::path& packagesPath;
};

export struct FmtString : public std::string {};
export struct FmtPath : public fs::path {};

struct Usage : public Export {
 protected:
  struct BuiltTarget : public Target {
    BuiltTarget(const fs::path& output) : Target(output) {}

    fs::path getOutput(BuilderContext& ctx) const override { return _output; };

   protected:
    std::optional<Ref<DepGraph::Node>> onBuild(
        BuilderContext& ctx) const override {
      return std::nullopt;
    }
  };

 public:
  std::optional<FmtPath> pcmPath;
  FmtString compileOption;
  FmtString linkOption;
  std::vector<std::string> libs;

  std::string getCompileOption() const override { return compileOption; }

  std::string getLinkOption() const override {
    std::string lo = linkOption;
    for (auto& lib : libs) {
      lo += " -l" + lib;
    }
    return lo;
  }

  mutable std::unordered_map<std::string, BuiltTarget> cache;

  std::optional<Ref<const Target>> findPCM(
      const std::string& moduleName) const override {
    if (!pcmPath.has_value()) return std::nullopt;
    const auto it = cache.find(moduleName);
    if (it != cache.end())
      return it->second;
    else {
      auto modulePath =
          pcmPath.value() / (replace(moduleName, ':', '-') + ".pcm");
      if (!fs::exists(modulePath)) return std::nullopt;
      return cache.emplace(moduleName, modulePath).first->second;
    }
  };

 private:
  BOOST_DESCRIBE_CLASS(Usage, (), (pcmPath, compileOption, linkOption, libs),
                       (), ())
};

export FmtString tag_invoke(const json::value_to_tag<FmtString>&,
                            const json::value& jv,
                            const PackageJsonContext& ctx) {
  const auto projectPathStr = ctx.projectPath.generic_string();
  return FmtString(
      std::vformat(jv.as_string(), std::make_format_args(projectPathStr)));
}

export FmtPath tag_invoke(const json::value_to_tag<FmtPath>&,
                          const json::value& jv,
                          const PackageJsonContext& ctx) {
  const auto projectPathStr = ctx.projectPath.generic_string();
  return FmtPath(
      std::vformat(jv.as_string(), std::make_format_args(projectPathStr)));
}
