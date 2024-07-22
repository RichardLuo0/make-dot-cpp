export module makeDotCpp.project;
import std;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.thread.logger;
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
          .operator()("output,o", po::value<std::string>(),
                      "Output directory. Default to `build`.")
          .operator()("installPath,i", po::value<Path>(),
                      "Installation directory.")
          .operator()("debug,g", "Enable debug.")
          .operator()("verbose,v", "Enable verbose output.");
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

  bool polished = false;
  std::string fmtOutput;

  CHAIN_VAR(BuildFunc, buildFunc, [](const Context &) {}, setBuild);
  CHAIN_VAR(
      InstallFunc, installFunc,
      [](const Context &) {
        // TODO default install location
      },
      setInstall);

  CHAIN_METHOD(to, std::string, fmtOutput) {
    this->fmtOutput = fmtOutput;
    polished = false;
  }
  CHAIN_METHOD(installTo, Path, path) { ctx.install = fs::absolute(path); }
  CHAIN_METHOD(setDebug, bool, debug) { ctx.debug = debug; }
  CHAIN_METHOD(setThreadPoolSize, std::size_t, size) {
    ctx.threadPool.setSize(size);
  }
  CHAIN_METHOD(setCompiler, std::shared_ptr<const Compiler>, compiler) {
    ctx.compiler = compiler;
  }
  CHAIN_METHOD(setVerbose, bool, verbose) { ctx.verbose = verbose; }

  void polish() {
    if (polished) return;
    polished = true;
    if (!fmtOutput.empty()) {
      const auto buildType = ctx.debug ? "debug" : "release";
      ctx.output = fs::absolute(std::vformat(
          fmtOutput,
          std::make_format_args(ctx.name, buildType, ctx.compiler->getName())));
    }
  }

 public:
  Project(const std::string &name) : ctx{name} {}

 public:
  ~Project() { ctx.threadPool.wait(); }

  void build() {
    polish();
    ensureDirExists(ctx.output);
    this->buildFunc(ctx);
  }

  void install() {
    polish();
    ctx.threadPool.wait();
    this->installFunc(ctx);
  }

  void watch() {
    polish();
    // TODO watch for changes
  }

  void clean() { fs::remove_all(ctx.output); }

  void setUpWith(const OptionParser &op) {
#define APPLY_IF_HAS_VALUE(KEY, TYPE, FUNC) \
  {                                         \
    auto vv = op[KEY];                      \
    if (!vv.empty()) {                      \
      auto value = vv.as<TYPE>();           \
      FUNC;                                 \
    }                                       \
  }

    APPLY_IF_HAS_VALUE("output", std::string, to(value));
    APPLY_IF_HAS_VALUE("installPath", Path, installTo(value));
#undef APPLY_IF_HAS_VALUE

#define APPLY_IF_CONTAINS(KEY, FUNC) \
  {                                  \
    if (op.contains(KEY)) {          \
      FUNC;                          \
    }                                \
  }

    APPLY_IF_CONTAINS("debug", setDebug(true));
    APPLY_IF_CONTAINS("verbose", setVerbose(true));
#undef APPLY_IF_CONTAINS
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
    logger::success() << "Updated " << to << std::endl;
  }

  static void updateAllFiles(const Path &from, const Path &to) {
    fs::copy(from, to,
             fs::copy_options::update_existing | fs::copy_options::recursive);
    logger::success() << "Updated " << to << std::endl;
  }
};
}  // namespace makeDotCpp
