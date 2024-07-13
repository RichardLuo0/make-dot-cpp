export module makeDotCpp.builder:common;
import std;
import makeDotCpp.thread;

#include "alias.hpp"

namespace makeDotCpp {
using Node = DepGraph::Node;
using NodeList = std::deque<Ref<Node>>;
}  // namespace makeDotCpp
