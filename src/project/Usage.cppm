module;
#include <boost/describe.hpp>

export module makeDotCpp.project.desc:Usage;
import :common;
import std;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.builder;
import makeDotCpp.fileProvider.Glob;
import makeDotCpp.utils;
import boost.json;
import boost.dll;

#include "alias.hpp"

namespace makeDotCpp {
export using BuildPackage =
    std::function<std::shared_ptr<const ExportFactory>(const Path&)>;

export struct Usage {
 public:
  virtual ~Usage() = default;

  virtual std::shared_ptr<const ExportFactory> getExportFactory(
      const Context& ctx, const std::string& name, const Path& projectPath,
      const BuildPackage& buildPackage) const = 0;

  virtual const std::unordered_set<PackagePath, PackagePath::Hash>&
  getPackages() const = 0;
};

export struct CustomUsage : public Usage {
 public:
  std::unordered_set<PackagePath, PackagePath::Hash> devPackages;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;
  Required<std::variant<ProjectFmtPath, std::vector<ProjectFmtPath>>> setupFile;

  std::shared_ptr<const ExportFactory> getExportFactory(
      const Context& ctx, const std::string& name, const Path& projectPath,
      const BuildPackage& buildPackage) const override {
    LibBuilder builder(name + "_export");
    builder.setShared(true)
        .define("NO_MAIN")
        .define("PROJECT_NAME=" + name)
        .define("PROJECT_PATH=" + projectPath.generic_string());
    std::visit(
        [&](auto&& setupFile) {
          using T = std::decay_t<decltype(setupFile)>;
          if constexpr (std::is_same_v<T, ProjectFmtPath>)
            builder.addSrc(setupFile);
          else if constexpr (std::is_same_v<T, std::vector<ProjectFmtPath>>) {
            for (auto& singleFile : setupFile) builder.addSrc(singleFile);
          }
        },
        setupFile);
    for (auto& path : devPackages) {
      builder.dependOn(buildPackage(path));
    }
    auto result = builder.build(ctx);
    result.get();
    auto dll = std::make_shared<boost::dll::shared_library>(
        builder.getOutput(ctx).generic_string());
    return {dll, &dll->get<ExportFactory&>("exportFactory")};
  }

  const std::unordered_set<PackagePath, PackagePath::Hash>& getPackages()
      const override {
    return packages;
  }

 private:
  BOOST_DESCRIBE_CLASS(CustomUsage, (), (devPackages, packages, setupFile), (),
                       ())
};

export struct DefaultUsage : public Usage {
 protected:
  struct DefaultExport : public Export {
   private:
    const std::string compileOption;
    const std::string linkOption;

   public:
    DefaultExport(const std::string& compileOption,
                  const std::string& linkOption)
        : compileOption(compileOption), linkOption(linkOption) {}

    std::string getCompileOption() const override { return compileOption; }

    std::string getLinkOption() const override { return linkOption; }
  };

  struct DefaultExportFactory : public ExportFactory {
   private:
    const std::string name;
    const std::shared_ptr<Export> ex;

   public:
    DefaultExportFactory(const std::string& name,
                         const std::shared_ptr<Export>& ex)
        : name(name), ex(ex) {}

    const std::string& getName() const override { return name; };

    std::shared_ptr<Export> create(const Context&) const override { return ex; }
  };

  struct ModuleExport : public DefaultExport {
   private:
    const std::shared_ptr<Export> ex;

   public:
    ModuleExport(const std::shared_ptr<Export>& ex,
                 const std::string& compileOption,
                 const std::string& linkOption)
        : DefaultExport(compileOption, linkOption), ex(ex) {}

    std::optional<Ref<const ModuleTarget>> findModule(
        const std::string& moduleName) const override {
      return ex->findModule(moduleName);
    }

    virtual std::optional<Ref<const Target>> getTarget() const override {
      return ex->getTarget();
    }
  };

  template <class Exf>
    requires std::is_base_of_v<ExportFactory, Exf>
  struct ModuleExportFactory : public CachedExportFactory {
   private:
    const std::string name;
    const Exf exf;
    const std::string compileOption;
    const std::string linkOption;

   public:
    ModuleExportFactory(const std::string& name, Exf&& exf,
                        const std::string& compileOption,
                        const std::string& linkOption)
        : name(name),
          exf(exf),
          compileOption(compileOption),
          linkOption(linkOption) {}

    const std::string& getName() const override { return name; };

    std::shared_ptr<Export> onCreate(const Context& ctx) const override {
      return std::make_shared<ModuleExport>(exf.create(ctx), compileOption,
                                            linkOption);
    }
  };

 public:
  std::optional<ProjectFmtStr> moduleFileGlob;
  std::vector<ProjectFmtPath> moduleIncludePath;
  bool needsCompile = false;
  std::unordered_set<PackagePath, PackagePath::Hash> devPackages;

  ProjectFmtStr compileOption;
  ProjectFmtStr linkOption;
  std::vector<std::string> libs;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  std::shared_ptr<const ExportFactory> getExportFactory(
      const Context&, const std::string& name, const Path& projectPath,
      const BuildPackage& buildPackage) const override {
    if (moduleFileGlob != std::nullopt) {
      auto createFactory =
          [&]<class B>() -> std::shared_ptr<const ExportFactory> {
        B builder(name);
        builder.setBase(projectPath);
        builder.addSrc(Glob(moduleFileGlob.value()));
        for (auto& path : moduleIncludePath) builder.include(path);
        for (auto& path : devPackages) builder.dependOn(buildPackage(path));
        return std::make_shared<ModuleExportFactory<B>>(
            name, std::move(builder), getCompileOption(), getLinkOption());
      };
      return needsCompile ? createFactory.template operator()<LibBuilder>()
                          : createFactory.template operator()<ModuleBuilder>();
    } else {
      return std::make_shared<DefaultExportFactory>(
          name,
          std::make_shared<DefaultExport>(getCompileOption(), getLinkOption()));
    }
  }

  std::string getCompileOption() const { return compileOption; }

  std::string getLinkOption() const {
    std::string lo = linkOption;
    for (auto& lib : libs) {
      lo += " -l" + lib;
    }
    return lo;
  }

  const std::unordered_set<PackagePath, PackagePath::Hash>& getPackages()
      const override {
    return packages;
  }

 private:
  BOOST_DESCRIBE_CLASS(DefaultUsage, (),
                       (moduleFileGlob, moduleIncludePath, needsCompile,
                        compileOption, linkOption, libs, devPackages, packages),
                       (), ())
};

export struct LibUsage : public Usage {
 public:
  bool shared = false;
  std::vector<ProjectFmtStr> srcFileGlob;
  std::vector<ProjectFmtPath> includePath;
  std::unordered_set<PackagePath, PackagePath::Hash> devPackages;
  std::unordered_set<PackagePath, PackagePath::Hash> packages;

  std::shared_ptr<const ExportFactory> getExportFactory(
      const Context&, const std::string& name, const Path& projectPath,
      const BuildPackage& buildPackage) const override {
    auto builder = std::make_shared<LibBuilder>(name, shared);
    builder->setBase(projectPath);
    for (auto& glob : srcFileGlob) builder->addSrc(Glob(glob));
    for (auto& path : includePath) builder->include(path);
    for (auto& path : devPackages) builder->dependOn(buildPackage(path));
    return builder;
  }

  const std::unordered_set<PackagePath, PackagePath::Hash>& getPackages()
      const override {
    return packages;
  }

 private:
  BOOST_DESCRIBE_CLASS(LibUsage, (),
                       (shared, srcFileGlob, includePath, devPackages,
                        packages),
                       (), ())
};
}  // namespace makeDotCpp
