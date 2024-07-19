module;
#include <boost/describe.hpp>

export module makeDotCpp.builder:BuilderContext;
import :common;
import std;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.thread;
import makeDotCpp.thread.logger;
import makeDotCpp.thread.process;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
export DEF_EXCEPTION(FileNotFound, (const Path &file),
                     "file not found: " + file.generic_string());
export DEF_EXCEPTION(CompileError, (), "compile error");

export struct CompilerOptions {
  std::string compileOption;
  std::string linkOption;
};

export struct CtxWrapper {
 public:
  const Context &ctx;

  CtxWrapper(CLRef<Context> ctx) : ctx(ctx) {}

  Path outputPath() const { return ctx.output; }

  Path modulePath() const { return ctx.modulePath(); }

  Path objPath() const { return ctx.objPath(); }
};

export struct VFSContext : public CtxWrapper {
 protected:
  std::unordered_set<Path> vfs;

 public:
  using CtxWrapper::CtxWrapper;

  bool exists(Path path) const {
    return vfs.contains(path) ? true : fs::exists(path);
  }

  auto lastWriteTime(Path path) const {
    return vfs.contains(path) ? fs::file_time_type::max()
                              : fs::last_write_time(path);
  }

  bool needsUpdate(const Path &output,
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

  BuilderContext(CLRef<Context> ctx, const CompilerOptions &compilerOptions)
      : VFSContext(ctx),
        compiler(ctx.compiler),
        compilerOptions(compilerOptions) {}

  FutureList &&takeFutureList() { return std::move(futureList); }

  std::string compileCommand(
      const Path &input, const std::unordered_map<std::string, Path> &moduleMap,
      const Path &output) const {
    return compiler->compileCommand(input, output, ctx.debug, moduleMap,
                                    compilerOptions.compileOption);
  }

#define GENERATE_COMPILE_METHOD(NAME, INPUT, CAPTURE, LOGNAME, FUNC)        \
  template <ranges::range<Ref<Node>> Deps =                                 \
                std::ranges::empty_view<Ref<Node>>>                         \
  Node &NAME(UNPACK INPUT, const Path &output,                              \
             const Deps &deps = std::views::empty<Ref<Node>>) {             \
    addFile(output);                                                        \
    return collect(ctx.depGraph.addNode(                                    \
        [=, id = id++, verbose = this->ctx.verbose,                         \
         UNPACK CAPTURE](DepGraph &graph) {                                 \
          logger::blue() << std::format("[{}] " LOGNAME ": ", id) << output \
                         << std::endl;                                      \
          const auto &result = FUNC;                                        \
          if (verbose) logger::info() << result.command;                    \
          if (!result.output.empty()) logger::info() << result.output;      \
          logger::flush();                                                  \
          if (result.status != 0) {                                         \
            graph.terminate();                                              \
            throw CompileError();                                           \
          }                                                                 \
          return result.status;                                             \
        },                                                                  \
        deps));                                                             \
  }

  GENERATE_COMPILE_METHOD(
      compileModule,
      (const Path &input,
       const std::unordered_map<std::string, Path> &moduleMap),
      (compiler = this->compiler, compilerOptions = this->compilerOptions),
      "Compiling module",
      compiler->compileModule(input, output, moduleMap,
                              compilerOptions.compileOption));
  GENERATE_COMPILE_METHOD(
      compile,
      (const Path &input,
       const std::unordered_map<std::string, Path> &moduleMap),
      (debug = this->ctx.debug, compiler = this->compiler,
       compileOption = this->compilerOptions.compileOption),
      "Compiling obj",
      compiler->compile(input, output, debug, moduleMap, compileOption));
  GENERATE_COMPILE_METHOD(link, (ranges::range<Path> auto &&objList),
                          (debug = this->ctx.debug, compiler = this->compiler,
                           objList = objList | ranges::to<std::vector<Path>>(),
                           linkOption = this->compilerOptions.linkOption),
                          "Linking",
                          compiler->link(objList, output, debug, linkOption));
  GENERATE_COMPILE_METHOD(archive, (ranges::range<Path> auto &&objList),
                          (compiler = this->compiler,
                           objList = objList | ranges::to<std::vector<Path>>()),
                          "Archiving", compiler->archive(objList, output));
  GENERATE_COMPILE_METHOD(createSharedLib, (ranges::range<Path> auto &&objList),
                          (compiler = this->compiler,
                           objList = objList | ranges::to<std::vector<Path>>(),
                           linkOption = this->compilerOptions.linkOption),
                          "Archiving",
                          compiler->createSharedLib(objList, output,
                                                    linkOption));
#undef GENERATE_COMPILE_METHOD
};

export struct BuilderContextChild : public BuilderContext {
 private:
  BuilderContext &parent;

 public:
  BuilderContextChild(LRef<BuilderContext> parent, CLRef<Context> ctx,
                      const CompilerOptions &compilerOptions)
      : BuilderContext(ctx, compilerOptions), parent(parent) {
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
}  // namespace makeDotCpp
