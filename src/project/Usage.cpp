struct PackageJsonContext {
  const Path& projectPath;
  const Path& packagesPath;
};

struct FmtString : public std::string {};
struct FmtPath : public Path {
  using Path::path;
};

export struct Usage : public Export {
 protected:
  struct BuiltTarget : public ModuleTarget {
   private:
    const std::string name;
    const Path output;

   public:
    BuiltTarget(const std::string& name, const Path& output)
        : name(name), output(output) {}

    const std::string& getName() const override { return name; }

    Path getOutput(BuilderContext& ctx) const override { return output; };

   protected:
    std::optional<Ref<DepGraph::Node>> build(
        BuilderContext& ctx) const override {
      return std::nullopt;
    }

    std::unordered_map<std::string, Path> getModuleMap(
        BuilderContext& ctx) const override {
      return std::unordered_map<std::string, Path>();
    }
  };

 public:
  std::optional<FmtPath> pcmPath;
  FmtString compileOption;
  FmtString linkOption;
  std::vector<std::string> libs;

  static Usage create(const std::string& jsonStr) {
    return json::value_to<Usage>(json::parse(jsonStr));
  }

  std::string toJson() { return json::serialize(json::value_from(*this)); }

  std::string getCompileOption() const override { return compileOption; }

  std::string getLinkOption() const override {
    std::string lo = linkOption;
    for (auto& lib : libs) {
      lo += " -l" + lib;
    }
    return lo;
  }

 private:
  mutable std::unordered_map<std::string, BuiltTarget> cache;

 public:
  std::optional<Ref<const ModuleTarget>> findPCM(
      const std::string& moduleName) const override {
    if (!pcmPath.has_value()) return std::nullopt;
    const auto it = cache.find(moduleName);
    if (it != cache.end())
      return it->second;
    else {
      auto modulePath =
          pcmPath.value() / (replace(moduleName, ':', '-') + ".pcm");
      if (!fs::exists(modulePath)) return std::nullopt;
      return cache
          .emplace(std::piecewise_construct, std::forward_as_tuple(moduleName),
                   std::forward_as_tuple(moduleName, modulePath))
          .first->second;
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
