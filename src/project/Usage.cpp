export struct Usage {
 public:
  virtual ~Usage() = default;

  virtual void populateBuilder(Builder& builder,
                               const Path& packagesPath) const = 0;

  virtual std::unordered_set<Path> getPackages(
      const Path& packagesPath) const = 0;
};

export struct CustomUsage : public Usage {
 public:
  std::unordered_set<PackagePath, PackagePath::Hash> packages;
  std::variant<Path, std::vector<Path>> setupFile;

  void populateBuilder(Builder& builder, const Path&) const override {
    std::visit(
        [&](auto&& setupFile) {
          using T = std::decay_t<decltype(setupFile)>;
          if constexpr (std::is_same_v<T, Path>)
            builder.addSrc(setupFile);
          else if constexpr (std::is_same_v<T, std::vector<Path>>) {
            for (auto& singleFile : setupFile) builder.addSrc(singleFile);
          }
        },
        setupFile);
  }

  std::unordered_set<Path> getPackages(
      const Path& packagesPath) const override {
    return packages | ranges::to<std::unordered_set<Path>>();
  }

 private:
  BOOST_DESCRIBE_CLASS(CustomUsage, (), (packages, setupFile), (), ())
};

export struct DefaultUsage : public Export, public Usage {
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
  std::optional<ProjectFmtPath> pcmPath;
  ProjectFmtStr compileOption;
  ProjectFmtStr linkOption;
  std::vector<std::string> libs;

  static DefaultUsage create(const std::string& jsonStr) {
    return json::value_to<DefaultUsage>(json::parse(jsonStr));
  }

  std::string toJson() const {
    return json::serialize(json::value_from(*this));
  }

  std::string getCompileOption() const override {
    if (pcmPath.has_value())
      return "-fprebuilt-module-path=" + pcmPath.value().generic_string() +
             ' ' + compileOption;
    else
      return compileOption;
  }

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

  void populateBuilder(Builder& builder,
                       const Path& packagesPath) const override {
    builder.addSrc(packagesPath / "makeDotCpp/template/package.cppm");
    builder.define("\"USAGE=" + replace(toJson(), "\"", "\\\"") + '\"');
  }

  std::unordered_set<Path> getPackages(
      const Path& packagesPath) const override {
    return {packagesPath / "std", packagesPath / "makeDotCpp",
            packagesPath / "boost"};
  }

 private:
  BOOST_DESCRIBE_CLASS(DefaultUsage, (),
                       (pcmPath, compileOption, linkOption, libs), (), ())
};
