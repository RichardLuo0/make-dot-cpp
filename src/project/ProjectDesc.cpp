export struct PackagePath : public fs::path {};

export defException(PackageNotBuilt, (std::string name),
                    name + " is not built");

export struct ProjectDesc {
 public:
  std::string name;
  std::vector<PackagePath> packages;

  struct Dev {
    fs::path buildFile = "build.cpp";
    std::string compiler = "clang++";
    bool debug = false;
    std::vector<PackagePath> packages;

   private:
    BOOST_DESCRIBE_CLASS(Dev, (), (buildFile, compiler, debug, packages), (),
                         ())
  };
  std::optional<Merge<Dev>> dev;

  std::variant<std::shared_ptr<Merge<Usage>>, fs::path> usage;

  static ProjectDesc create(const fs::path& path, const fs::path packagesPath) {
    const auto projectPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    return json::value_to<Merge<ProjectDesc>>(
        parseJson(projectPath),
        PackageJsonContext{projectPath.parent_path(), packagesPath});
  }

  std::shared_ptr<Export> getExport() {
    if (std::holds_alternative<fs::path>(usage)) throw PackageNotBuilt(name);
    return std::get<0>(usage);
  }

 private:
  BOOST_DESCRIBE_CLASS(ProjectDesc, (), (name, packages, dev, usage), (), ())
};

struct PackageLoc {
 public:
  std::string name;
  fs::path path;

 private:
  BOOST_DESCRIBE_CLASS(PackageLoc, (), (name, path), (), ())
};

export PackagePath tag_invoke(const json::value_to_tag<PackagePath>&,
                              const json::value& jv,
                              const PackageJsonContext& ctx) {
  auto loc = json::value_to<std::variant<std::string, PackageLoc>>(jv);
  return PackagePath(std::visit(
      [&](auto&& loc) {
        using T = std::decay_t<decltype(loc)>;
        if constexpr (std::is_same_v<T, std::string>)
          return ctx.packagesPath / loc;
        else
          return loc.path;
      },
      loc));
}
