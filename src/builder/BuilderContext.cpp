export struct CompilerOptions {
  std::string compileOptions;
  std::string linkOptions;
};

export struct VFSContext {
 protected:
  std::unordered_set<Path> vfs;

 public:
  const Context &ctx;

  VFSContext(CLRef<Context> ctx) : ctx(ctx) {}

  bool exists(Path path) const {
    return vfs.contains(path) ? true : fs::exists(path);
  }

  auto lastWriteTime(Path path) const {
    return vfs.contains(path) ? fs::file_time_type::max()
                              : fs::last_write_time(path);
  }

  bool isNeedUpdate(const Path &output, std::ranges::range auto &&deps) const {
    if (!exists(output))
      return true;
    else {
      auto outputWriteTime = lastWriteTime(output);
      for (const auto &dep : deps) {
        if (!exists(dep)) throw FileNotFound(dep.generic_string());
        if (lastWriteTime(dep) > outputWriteTime) return true;
      }
    }
    return false;
  }

  void addFile(const Path &path) { vfs.emplace(path); }

  Path outputPath() const { return ctx.output; }

  Path pcmPath() const { return ctx.pcmPath(); }

  Path objPath() const { return ctx.objPath(); }
};

export struct BuilderContext : public VFSContext {
  friend struct BuilderContextChild;

 protected:
  int id = 1;
  FutureList futureList;

  Node &collect(Node &node) {
    futureList.emplace_back(node.takeFuture());
    return node;
  }

 public:
  const std::shared_ptr<const Compiler> &compiler;
  const CompilerOptions compilerOptions;

  BuilderContext(CLRef<Context> ctx,
                 CLRef<std::shared_ptr<const Compiler>> compiler,
                 const CompilerOptions &compilerOptions)
      : VFSContext(ctx), compiler(compiler), compilerOptions(compilerOptions) {}

  FutureList &&takeFutureList() { return std::move(futureList); }

  static int handleResult(const Process::Result &&result, DepGraph &graph) {
    Logger::info(result.command);
    if (!result.output.empty()) Logger::info(result.output);
    Logger::flush();
    if (result.status != 0) {
      graph.terminate();
      throw CompileError();
    }
    return 0;
  }

#define GENERATE_COMPILE_METHOD(NAME, INPUT, LOGNAME, FUNC)                  \
  template <std::ranges::range Deps = std::ranges::empty_view<Ref<Node>>>    \
  Node &NAME(INPUT, const Path &output,                                      \
             const Deps &deps = std::views::empty<Ref<Node>>) {              \
    addFile(output);                                                         \
    return collect(ctx.depGraph.addNode(                                     \
        [=, &ctx = this->ctx, compiler = this->compiler,                     \
         compilerOptions = this->compilerOptions,                            \
         id = id++](DepGraph &graph) {                                       \
          UNUSED(ctx);                                                       \
          UNUSED(compilerOptions);                                           \
          Logger::info(std::format("\033[0;34m[{}] " #LOGNAME ": {}\033[0m", \
                                   id, output.generic_string()));            \
          Logger::flush();                                                   \
          return handleResult(FUNC, graph);                                  \
        },                                                                   \
        deps));                                                              \
  }

  GENERATE_COMPILE_METHOD(
      compilePCM,
      const Path &input
          COMMA const std::unordered_map<std::string COMMA Path> &moduleMap,
      Compiling pcm,
      compiler->compilePCM(input, output, moduleMap,
                           compilerOptions.compileOptions));
  GENERATE_COMPILE_METHOD(
      compile,
      const Path &input
          COMMA const std::unordered_map<std::string COMMA Path> &moduleMap,
      Compiling obj,
      compiler->compile(input, output, ctx.debug, moduleMap,
                        compilerOptions.compileOptions));
  GENERATE_COMPILE_METHOD(link, const std::vector<Path> &objList, Linking,
                          compiler->link(objList, output, ctx.debug,
                                         compilerOptions.linkOptions));
  GENERATE_COMPILE_METHOD(archive, const std::vector<Path> &objList, Archiving,
                          compiler->archive(objList, output, ctx.debug));
#undef GENERATE_COMPILE_METHOD
};

export struct BuilderContextChild : public BuilderContext {
 private:
  BuilderContext &parent;

 public:
  BuilderContextChild(LRef<BuilderContext> parent, CLRef<Context> ctx,
                      const CompilerOptions &compilerOptions)
      : BuilderContext(ctx, parent.compiler, compilerOptions), parent(parent) {
    vfs.merge(parent.vfs);
    id = parent.id;
  }

  ~BuilderContextChild() {
    parent.vfs.merge(vfs);
    parent.id = id;
    std::move(futureList.begin(), futureList.end(),
              std::back_inserter(parent.futureList));
  }
};
