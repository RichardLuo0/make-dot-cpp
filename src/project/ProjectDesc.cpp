export struct ProjectDesc {
 public:
  std::string name;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  struct Dev {
    std::variant<Path, std::vector<Path>> buildFile = "build.cpp";
    std::string compiler = "clang++";
    bool debug = false;
    std::unordered_set<PackagePath, PackagePath::Hash> packages;

   private:
    BOOST_DESCRIBE_CLASS(Dev, (), (buildFile, compiler, debug, packages), (),
                         ())
  };
  Merge<Dev> dev;

  std::shared_ptr<Usage> usage;

  static ProjectDesc create(const Path& path, const Path& packagesPath) {
    const auto projectJsonPath =
        fs::canonical(fs::is_directory(path) ? path / "project.json" : path);
    return json::value_to<Merge<ProjectDesc>>(
        parseJson(projectJsonPath),
        PackageJsonContext{projectJsonPath.parent_path(), packagesPath});
  }

 private:
  BOOST_DESCRIBE_CLASS(ProjectDesc, (), (name, packages, dev, usage), (), ())
};

export std::shared_ptr<Usage> tag_invoke(
    const json::value_to_tag<std::shared_ptr<Usage>>&, const json::value& jv,
    const PackageJsonContext& ctx) {
  const auto* typePtr = jv.as_object().if_contains("type");
  const auto type = typePtr ? (*typePtr).as_string() : "";
  if (type == "custom")
    return json::value_to<std::shared_ptr<CustomUsage>>(jv, ctx);
  else
    return json::value_to<std::shared_ptr<Merge<DefaultUsage>>>(jv, ctx);
}
