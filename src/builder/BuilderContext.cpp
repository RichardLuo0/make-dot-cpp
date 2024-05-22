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

  bool isNeedUpdate(const Path &output,
                    ranges::range<const Path> auto &&deps) const {
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

#define GENERATE_COMPILE_METHOD(NAME, INPUT, CAPTURE, LOGNAME, FUNC)        \
  template <ranges::range<Ref<Node>> Deps =                                 \
                std::ranges::empty_view<Ref<Node>>>                         \
  Node &NAME(UNPACK INPUT, const Path &output,                              \
             const Deps &deps = std::views::empty<Ref<Node>>) {             \
    addFile(output);                                                        \
    return collect(ctx.depGraph.addNode(                                    \
        [=, id = id++, UNPACK CAPTURE](DepGraph &graph) {                   \
          Logger::info(std::format("\033[0;34m[{}] " LOGNAME ": {}\033[0m", \
                                   id, output.generic_string()));           \
          Logger::flush();                                                  \
          return handleResult(FUNC, graph);                                 \
        },                                                                  \
        deps));                                                             \
  }

  GENERATE_COMPILE_METHOD(
      compilePCM,
      (const Path &input,
       const std::unordered_map<std::string, Path> &moduleMap),
      (compiler = this->compiler, compilerOptions = this->compilerOptions),
      "Compiling pcm",
      compiler->compilePCM(input, output, moduleMap,
                           compilerOptions.compileOptions));
  GENERATE_COMPILE_METHOD(
      compile,
      (const Path &input,
       const std::unordered_map<std::string, Path> &moduleMap),
      (&ctx = this->ctx, compiler = this->compiler,
       compileOptions = this->compilerOptions.compileOptions),
      "Compiling obj",
      compiler->compile(input, output, ctx.debug, moduleMap, compileOptions));
  GENERATE_COMPILE_METHOD(link, (ranges::range<Path> auto &&objList),
                          (&ctx = this->ctx, compiler = this->compiler,
                           objList = objList | ranges::to<std::vector<Path>>(),
                           linkOptions = this->compilerOptions.linkOptions),
                          "Linking",
                          compiler->link(objList, output, ctx.debug,
                                         linkOptions));
  GENERATE_COMPILE_METHOD(archive, (ranges::range<Path> auto &&objList),
                          (compiler = this->compiler,
                           objList = objList | ranges::to<std::vector<Path>>()),
                          "Archiving",
                          compiler->archive({objList.begin(), objList.end()},
                                            output));
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
