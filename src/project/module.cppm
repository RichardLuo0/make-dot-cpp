module;
#include <boost/json.hpp>
#include <boost/describe.hpp>
#include <boost/program_options.hpp>

export module makeDotCpp.project;

import std;
import makeDotCpp;
import makeDotCpp.builder;
import makeDotCpp.thread;
import makeDotCpp.utils;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
#include "Project.cpp"
#include "Usage.cpp"
#include "ProjectDesc.cpp"
}  // namespace makeDotCpp

namespace boost {
namespace json {
using namespace makeDotCpp;
template <>
struct is_sequence_like<FmtPath> : std::false_type {};
template <>
struct is_path_like<FmtPath> : std::true_type {};
template <>
struct is_described_class<Usage> : std::true_type {};
template <>
struct is_described_class<ProjectDesc> : std::true_type {};
template <>
struct is_described_class<ProjectDesc::Dev> : std::true_type {};
template <>
struct is_described_class<PackageLoc> : std::true_type {};
}  // namespace json
}  // namespace boost
