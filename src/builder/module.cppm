module;
#include <boost/describe.hpp>
#include <boost/json.hpp>

export module makeDotCpp.builder;

import std;
import makeDotCpp;
import makeDotCpp.compiler;
import makeDotCpp.fileProvider;
import makeDotCpp.thread;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
using Node = DepGraph::Node;
using NodeList = std::deque<Ref<Node>>;

export defException(FileNotFound, (const Path &file),
                    "file not found: " + file.generic_string());
export defException(ModuleNotFound,
                    (const Path input, const std::string &moduleName),
                    input.generic_string() +
                        ": module not found: " + moduleName);
export defException(CompileError, (), "compile error");

#include "BuilderContext.cpp"
#include "Targets.cpp"
#include "Export.cpp"
#include "Builder.cpp"
#include "ObjBuilder.cpp"
#include "ExeBuilder.cpp"
#include "LibBuilder.cpp"
}  // namespace makeDotCpp

namespace boost {
namespace json {
template <>
struct is_described_class<makeDotCpp::Unit> : std::true_type {};
}  // namespace json
}  // namespace boost
