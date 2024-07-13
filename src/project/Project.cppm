export module makeDotCpp.project;
import std;
import makeDotCpp;
import makeDotCpp.compiler;
import boost.program_options;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
namespace po = boost::program_options;

export class Project {
 public:
  struct OptionParser {
   private:
    po::options_description od;
    po::variables_map vm;

   public:
    OptionParser() {
      od.add_options()
          .operator()("help,h", "Display help message.")
          .operator()("build", "Build the project.")
          .operator()("install", "Build and install the project.")
          .operator()("clean", "Clean the output directory.")
          .operator()("output,o", po::value<Path>(),
                      "Output directory. Default to `build`.")
          .operator()("installPath,i", po::value<Path>(),
                      "Installation directory.")
          .operator()("packages,p", po::value<Path>(), "Packages directory.")
          .operator()("debug,g", "Enable debug.");
    }

    void add(const std::string &name, const std::string &desc) {
      od.add_options()(name.c_str(), desc.c_str());
    }

    template <class T>
    void add(const std::string &name, const std::string &desc) {
      od.add_options()(name.c_str(), po::value<T>(), desc.c_str());
    }

    void parse(int argc, const char **argv) {
      po::store(po::command_line_parser(argc, argv)
                    .options(od)
                    .allow_unregistered()
                    .run(),
                vm);
      po::notify(vm);
    }

    Path getPackagesPath() {
      const auto vv = vm["packages"];
      return vv.empty() ? fs::weakly_canonical(std::getenv("CXX_PACKAGES"))
                        : vv.as<Path>();
    }

    void printHelp() const { std::cout << od << std::endl; }

    const po::variable_value &operator[](const std::string &key) const {
      return vm[key];
    }

    bool contains(const std::string &key) const { return vm.contains(key); }
  };

  using BuildFunc = std::function<void(const Context &)>;
  using InstallFunc = std::function<void(const Context &)>;

 private:
  Context ctx;

  chainVar(BuildFunc, buildFunc, [](const Context &) {}, setBuild);
  chainVar(
      InstallFunc, installFunc,
      [](const Context &) {
        // TODO default install location
      },
      setInstall);

  chainMethod(setName, std::string, name) { ctx.name = name; }
  chainMethod(to, Path, path) { ctx.output = fs::weakly_canonical(path); }
  chainMethod(installTo, Path, path) {
    ctx.install = fs::weakly_canonical(path);
  }
  chainMethod(setDebug, bool, debug) { ctx.debug = debug; }
  chainMethod(setThreadPoolSize, std::size_t, size) {
    ctx.threadPool.setSize(size);
  }
  chainMethod(setRelativePCMPath, std::string, path) {
    ctx.relativePCMPath = path;
  }
  chainMethod(setRelativeObjPath, std::string, path) {
    ctx.relativeObjPath = path;
  }
  chainMethod(setCompiler, std::shared_ptr<const Compiler>, compiler) {
    ctx.compiler = compiler;
  }

 public:
  template <class C>
    requires std::is_base_of_v<Compiler, C>
  auto &setCompiler(const C &compiler) {
    return setCompiler(std::make_shared<C>(compiler));
  }

 public:
  ~Project() { ctx.threadPool.wait(); }

  void build() {
    ensureDirExists(ctx.output);
    this->buildFunc(ctx);
  }

  void install() {
    ctx.threadPool.wait();
    this->installFunc(ctx);
  }

  void watch() {
    // TODO
  }

  void clean() { fs::remove_all(ctx.output); }

  void setUpWith(const OptionParser &op) {
#define APPLY_IF_HAS(KEY, TYPE, FUNC) \
  {                                   \
    auto vv = op[KEY];                \
    if (!vv.empty()) {                \
      auto value = vv.as<TYPE>();     \
      FUNC;                           \
    }                                 \
  }

    APPLY_IF_HAS("output", Path, to(value));
    APPLY_IF_HAS("installPath", Path, installTo(value));
#undef APPLY_IF_HAS

    if (op.contains("debug")) {
      setDebug(true);
    }
  }

  void run(const OptionParser &op) {
    setUpWith(op);
    if (op.contains("help")) {
      op.printHelp();
    } else if (op.contains("install")) {
      build();
      install();
    } else if (op.contains("clean")) {
      clean();
    } else {
      build();
    }
  }

  void run(int argc, const char **argv) {
    Project::OptionParser op;
    op.parse(argc, argv);
    return run(op);
  }

  static void ensureDirExists(const Path &path) {
    fs::create_directories(path);
  }

  static void updateFile(const Path &from, const Path &to) {
    fs::copy(from, to, fs::copy_options::update_existing);
    std::cout << "Updating " << to << std::endl;
  }

  static void updateAllFiles(const Path &from, const Path &to) {
    fs::copy(from, to,
             fs::copy_options::update_existing | fs::copy_options::recursive);
    std::cout << "Updating " << to << std::endl;
  }
};
}  // namespace makeDotCpp
