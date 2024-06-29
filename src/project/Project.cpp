namespace po = boost::program_options;

export class Project {
 public:
  struct OptionParser : public boost::program_options::options_description {
   private:
    po::variables_map vm;

   public:
    OptionParser() {
      add_options()
          .operator()("help,h", "display help message.")
          .operator()("build", "build the project.")
          .operator()("install", "build and install the project.")
          .operator()("clean", "clean the output directory.")
          .operator()("output,o", po::value<Path>(),
                      "output directory. Default to `build`.")
          .operator()("installPath,i", po::value<Path>(), "install directory.")
          .operator()("packages,p", po::value<Path>(), "packages directory.")
          .operator()("debug,g", "enable debug.");
    }

    void parse(int argc, const char **argv) {
      po::store(po::parse_command_line(argc, argv, *this), vm);
      po::notify(vm);
    }

    Path getPackagesPath() {
      const auto vv = vm["packages"];
      return vv.empty() ? fs::weakly_canonical(std::getenv("CXX_PACKAGES"))
                        : vv.as<Path>();
    }

    void printDesc() const { std::cout << *this << std::endl; }

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

 public:
  ~Project() { ctx.threadPool.wait(); }

  void build() {
    ensureOutputExists();
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
      op.printDesc();
    } else if (op.contains("install")) {
      build();
      install();
    } else if (op.contains("clean")) {
      clean();
    } else {
      build();
    }
  }

 private:
  void ensureOutputExists() { fs::create_directories(ctx.output); }
};
