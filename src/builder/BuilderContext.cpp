export struct CompilerOptions {
  std::string compileOptions;
  std::string linkOptions;
};

export struct BuilderContext {
  friend struct BuilderContextChild;

 protected:
  int id = 1;
  FutureList futureList;
  std::unordered_set<Path> vfs;

  Node &collect(Node &node) {
    futureList.emplace_back(node.takeFuture());
    return node;
  }

 public:
  const Context &ctx;
  const std::shared_ptr<const Compiler> &compiler;
  const CompilerOptions &compilerOptions;

  BuilderContext(const Context &ctx,
                 const std::shared_ptr<const Compiler> &compiler,
                 const CompilerOptions &compilerOptions)
      : ctx(ctx), compiler(compiler), compilerOptions(compilerOptions) {}

  FutureList &&takeFutureList() { return std::move(futureList); }

  bool exists(Path path) const {
    return vfs.contains(path) ? true : fs::exists(path);
  }

  auto lastWriteTime(Path path) const {
    return vfs.contains(path) ? fs::file_time_type::max()
                              : fs::last_write_time(path);
  }

  bool isNeedUpdate(const Path &output,
                    const std::ranges::range auto &deps) const {
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

  Path outputPath() const { return ctx.output; }

  Path pcmPath() const { return ctx.pcmPath(); }

  Path objPath() const { return ctx.objPath(); }

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

  template <std::ranges::range Deps = std::ranges::empty_view<Ref<Node>>>
  Node &compilePCM(const Path &input, const Path &output,
                   const Deps &deps = std::views::empty<Ref<Node>>) {
    vfs.emplace(output);
    return collect(ctx.depGraph.addNode(
        [=, &ctx = this->ctx, compiler = this->compiler,
         compilerOptions = this->compilerOptions, id = id++](DepGraph &graph) {
          UNUSED(compilerOptions);
          Logger::info(std::format("\033[0;34m[{}] Compiling pcm: {}\033[0m",
                                   id, output.generic_string()));
          Logger::flush();
          return handleResult(
              compiler->compilePCM(input, output, ctx.pcmPath(),
                                   compilerOptions.compileOptions),
              graph);
        },
        deps));
  }

  template <std::ranges::range Deps = std::ranges::empty_view<Ref<Node>>>
  Node &compile(const Path &input, const Path &output,
                const Deps &deps = std::views::empty<Ref<Node>>) {
    vfs.emplace(output);
    return collect(ctx.depGraph.addNode(
        [=, &ctx = this->ctx, compiler = this->compiler,
         compilerOptions = this->compilerOptions, id = id++](DepGraph &graph) {
          UNUSED(compilerOptions);
          Logger::info(std::format("\033[0;34m[{}] Compiling obj: {}\033[0m",
                                   id, output.generic_string()));
          Logger::flush();
          return handleResult(
              compiler->compile(input, output, ctx.debug, ctx.pcmPath(),
                                compilerOptions.compileOptions),
              graph);
        },
        deps));
  }

#define GENERATE_COMPILE_METHOD(NAME, INPUT, LOGNAME, FUNC)                  \
  template <std::ranges::range Deps = std::ranges::empty_view<Ref<Node>>>    \
  Node &NAME(INPUT, const Path &output,                                      \
             const Deps &deps = std::views::empty<Ref<Node>>) {              \
    vfs.emplace(output);                                                     \
    return collect(ctx.depGraph.addNode(                                     \
        [=, &ctx = this->ctx, compiler = this->compiler,                     \
         compilerOptions = this->compilerOptions,                            \
         id = id++](DepGraph &graph) {                                       \
          UNUSED(compilerOptions);                                           \
          Logger::info(std::format("\033[0;34m[{}] " #LOGNAME ": {}\033[0m", \
                                   id, output.generic_string()));            \
          Logger::flush();                                                   \
          return handleResult(FUNC, graph);                                  \
        },                                                                   \
        deps));                                                              \
  }

  // GENERATE_COMPILE_METHOD(compilePCM, const Path &input, Compiling pcm,
  //                         compiler->compilePCM(input, output, ctx.pcmPath(),
  //                                              compilerOptions.compileOptions));
  // GENERATE_COMPILE_METHOD(compile, const Path &input, Compiling obj,
  //                         compiler->compile(input, output, ctx.debug,
  //                                           ctx.pcmPath(),
  //                                           compilerOptions.compileOptions));
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
  BuilderContextChild(BuilderContext &parent, const Context &ctx,
                      const std::shared_ptr<const Compiler> &compiler,
                      const CompilerOptions &compilerOptions)
      : BuilderContext(ctx, compiler, compilerOptions), parent(parent) {
    id = parent.id;
  }

  ~BuilderContextChild() {
    parent.vfs.merge(vfs);
    parent.id = id;
    std::move(futureList.begin(), futureList.end(),
              std::back_inserter(parent.futureList));
  }
};
