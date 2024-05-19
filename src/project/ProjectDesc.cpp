export struct PackagePath : public Path {
  using Hash = std::hash<Path>;
};

export defException(PackageNotBuilt, (std::string name),
                    name + " is not built");

export struct ProjectDesc {
 public:
  std::string name;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  struct Dev {
    Path buildFile = "build.cpp";
    std::string compiler = "clang++";
    bool debug = false;
    std::unordered_set<PackagePath, PackagePath::Hash> packages;

   private:
    BOOST_DESCRIBE_CLASS(Dev, (), (buildFile, compiler, debug, packages), (),
                         ())
  };
  std::optional<Merge<Dev>> dev;

  std::variant<std::shared_ptr<Merge<Usage>>, Path, std::vector<Path>> usage;

  static ProjectDesc create(const Path& path, const Path& packagesPath) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    return json::value_to<Merge<ProjectDesc>>(
        parseJson(projectJsonPath),
        PackageJsonContext{projectJsonPath.parent_path(), packagesPath});
  }

  std::shared_ptr<Export> getExport() {
    if (std::holds_alternative<Path>(usage)) throw PackageNotBuilt(name);
    return std::get<0>(usage);
  }

 private:
  BOOST_DESCRIBE_CLASS(ProjectDesc, (), (name, packages, dev, usage), (), ())
};

struct PackageLoc {
 public:
  Path path;

 private:
  BOOST_DESCRIBE_CLASS(PackageLoc, (), (path), (), ())
};

export PackagePath tag_invoke(const json::value_to_tag<PackagePath>&,
                              const json::value& jv,
                              const PackageJsonContext& ctx) {
  auto loc = json::value_to<std::variant<std::string, PackageLoc>>(jv);
  return PackagePath(fs::weakly_canonical(std::visit(
      [&](auto&& loc) {
        using T = std::decay_t<decltype(loc)>;
        if constexpr (std::is_same_v<T, std::string>)
          return ctx.packagesPath / loc;
        else
          return loc.path;
      },
      loc)));
}
