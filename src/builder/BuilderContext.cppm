module;
#include <boost/describe.hpp>

export module makeDotCpp.builder:BuilderContext;
import :Exceptions;
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
export DEF_EXCEPTION(CompileError, (), "compile error");

export struct CompilerOption {
  std::string compileOption;
  std::string linkOption;
};

export struct CtxWrapper {
 protected:
  const Context &ctx;
  const Path bOutput;

 public:
  CtxWrapper(const Context *ctx, const Path &bOutput)
      : ctx(*ctx), bOutput(bOutput) {}

  Path outputPath() const { return ctx.output / bOutput; }

  Path modulePath() const { return ctx.output / bOutput / "module"; }

  Path objPath() const { return ctx.output / bOutput / "obj"; }
};

export struct VFS {
 protected:
  std::unordered_map<Path, Ref<Node>> vfs;
  std::unordered_set<Path> builtCache;

 public:
  bool exists(const Path &path) const {
    return vfs.contains(path) ? true : fs::exists(path);
  }

  auto lastWriteTime(const Path &path) const {
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
        if (dep.empty()) continue;
        if (!exists(dep)) throw FileNotFound(dep.generic_string());
        if (lastWriteTime(dep) > outputWriteTime) return true;
      }
    }
    return false;
  }

  void add(const Path &path, Node &node) { vfs.emplace(path, node); }

  Node *get(const Path &path) {
    const auto it = vfs.find(path);
    return it != vfs.end() ? &it->second.get() : nullptr;
  }

  void addToCache(const Path &path) { builtCache.emplace(path); }

  bool isCached(const Path &path) { return builtCache.contains(path); }

  void clear() { vfs.clear(); }
};

export struct BuilderContext : public CtxWrapper {
 protected:
  inline static VFS vfs;

  int id = 1;
  FutureList futureList;

 public:
  const CompilerOption &compilerOptions;

  BuilderContext(const Context *ctx, const Path &bOutput,
                 const CompilerOption *compilerOptions)
      : CtxWrapper(ctx, bOutput), compilerOptions(*compilerOptions) {}
  BuilderContext(const CtxWrapper &ctxW, const CompilerOption *compilerOptions)
      : CtxWrapper(ctxW), compilerOptions(*compilerOptions) {}

  FutureList &&takeFutureList() { return std::move(futureList); }

  std::string compileCommand(
      const Path &input, const std::unordered_map<std::string, Path> &moduleMap,
      const Path &output) const {
    return ctx.compiler->compileCommand(input, output, ctx.debug, moduleMap,
                                        compilerOptions.compileOption);
  }

  auto exists(auto &&...args) const {
    return vfs.exists(std::forward<decltype(args)>(args)...);
  }

  auto lastWriteTime(auto &&...args) const {
    return vfs.lastWriteTime(std::forward<decltype(args)>(args)...);
  }

  auto isCached(auto &&...args) const {
    return vfs.isCached(std::forward<decltype(args)>(args)...);
  }

#define GENERATE_COMPILE_METHOD(NAME, INPUT, CAPTURE, LOGNAME, FUNC)        \
  bool NAME(UNPACK INPUT, const Path &output,                               \
            const ranges::range<Path> auto &deps) {                         \
    vfs.addToCache(output);                                                 \
    if (!vfs.needsUpdate(output, deps)) return false;                       \
    std::deque<Ref<Node>> nodeList;                                         \
    for (const auto &dep : deps) {                                          \
      Node *node = vfs.get(dep);                                            \
      if (node != nullptr) nodeList.emplace_back(*node);                    \
    }                                                                       \
    auto &node = ctx.depGraph.addNode(                                      \
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
        nodeList);                                                          \
    vfs.add(output, node);                                                  \
    futureList.emplace_back(node.takeFuture());                             \
    return true;                                                            \
  }

  GENERATE_COMPILE_METHOD(
      compileModule,
      (const Path &input,
       const std::unordered_map<std::string, Path> &moduleMap),
      (compiler = ctx.compiler, compilerOptions = compilerOptions),
      "Compiling module",
      compiler->compileModule(input, output, moduleMap,
                              compilerOptions.compileOption));
  GENERATE_COMPILE_METHOD(
      compile,
      (const Path &input,
       const std::unordered_map<std::string, Path> &moduleMap),
      (compiler = ctx.compiler, debug = ctx.debug,
       compileOption = compilerOptions.compileOption),
      "Compiling obj",
      compiler->compile(input, output, debug, moduleMap, compileOption));
  GENERATE_COMPILE_METHOD(link, (const ranges::range<Path> auto &objList),
                          (compiler = ctx.compiler, debug = ctx.debug,
                           objList = objList | ranges::to<std::vector<Path>>(),
                           linkOption = compilerOptions.linkOption),
                          "Linking",
                          compiler->link(objList, output, debug, linkOption));
  GENERATE_COMPILE_METHOD(archive, (const ranges::range<Path> auto &objList),
                          (compiler = ctx.compiler,
                           objList = objList | ranges::to<std::vector<Path>>()),
                          "Archiving", compiler->archive(objList, output));
  GENERATE_COMPILE_METHOD(
      createSharedLib, (const ranges::range<Path> auto &objList),
      (compiler = ctx.compiler,
       objList = objList | ranges::to<std::vector<Path>>(),
       linkOption = compilerOptions.linkOption),
      "Archiving", compiler->createSharedLib(objList, output, linkOption));
#undef GENERATE_COMPILE_METHOD

  struct Child;
};

struct BuilderContext::Child : public BuilderContext {
 private:
  BuilderContext &parent;

 public:
  Child(BuilderContext *parent, const CompilerOption *compilerOptions,
        const CtxWrapper *ctxW = nullptr)
      : BuilderContext(ctxW != nullptr ? *ctxW : *parent, compilerOptions),
        parent(*parent) {
    id = this->parent.id;
  }

  ~Child() {
    parent.id = id;
    std::move(futureList.begin(), futureList.end(),
              std::back_inserter(parent.futureList));
  }
};
}  // namespace makeDotCpp
