module;
#include <boost/json.hpp>
#include <boost/describe.hpp>
#include <boost/algorithm/string.hpp>

export module makeDotCpp.utils;

import std;

#include "alias.hpp"

namespace makeDotCpp {
export template <typename T>
using LRef = std::add_lvalue_reference_t<T>;
export template <typename T>
using CLRef = std::add_lvalue_reference_t<std::add_const_t<T>>;

namespace ranges {
export template <std::ranges::range C, std::size_t N>
class concatView : public std::ranges::view_interface<concatView<C, N>> {
 private:
  const std::array<std::ranges::ref_view<C>, N> nested;
  const decltype(nested | std::ranges::views::join) joined;

 public:
  concatView(auto&&... cs)
      : nested{cs...}, joined(nested | std::ranges::views::join) {}

  auto begin() const { return joined.begin(); }

  auto end() const { return joined.end(); }
};

export template <std::ranges::range C>
inline auto concat(C&& first, auto&&... cs) noexcept {
  return concatView<C, 1 + sizeof...(cs)>{std::forward<C>(first), cs...};
}

export template <class C>
struct to {};

export template <class C>
C operator|(std::ranges::range auto&& range, to<C> to) {
  C result;
  std::ranges::copy(range.begin(), range.end(), std::back_inserter(result));
  return result;
}

export template <class C, class Item>
concept range = std::ranges::range<C> &&
                std::is_convertible_v<typename C::value_type, Item>;
}  // namespace ranges

export template <std::ranges::range C>
inline C replace(const C& c, auto&& item, auto&& newItem) {
  C cCopy(c);
  std::ranges::replace(cCopy, item, newItem);
  return cCopy;
}

export inline std::string replace(const std::string& str, const char* toReplace,
                                  const char* newStr) {
  std::string strCopy(str);
  boost::replace_all(strCopy, toReplace, newStr);
  return strCopy;
}

export std::string readAsStr(const Path& path) {
  std::ifstream is(path);
  return std::string(std::istreambuf_iterator<char>(is), {});
}

export json::value parseJson(const Path& path) {
  const auto input = readAsStr(path);
  return json::parse(input);
}

export template <class T, class Ctx>
std::shared_ptr<T> tag_invoke(const json::value_to_tag<std::shared_ptr<T>>&,
                              const json::value& jv, const Ctx& ctx) {
  return std::make_shared<T>(json::value_to<T>(jv, ctx));
}

export template <class T>
struct Merge : public T {};

export template <class T, class Ctx>
Merge<T> tag_invoke(const json::value_to_tag<Merge<T>>&, const json::value& jv,
                    Ctx&& ctx) {
  Merge<T> t;
  const auto obj = jv.as_object();
  using namespace boost::describe;
  boost::mp11::mp_for_each<describe_members<T, mod_public>>([&](auto&& m) {
    const auto* value = obj.if_contains(m.name);
    if (value) {
      auto& member = t.*m.pointer;
      using memberType = std::remove_reference_t<decltype(member)>;
      auto cValue =
          json::try_value_to<memberType>(*value, std::forward<Ctx>(ctx));
      if (cValue) member = std::move(*cValue);
    }
  });
  return t;
}

export template <class T>
void tag_invoke(const json::value_from_tag&, json::value& jv,
                const Merge<T>& merge) {
  jv = json::value_from(static_cast<T>(merge));
}
}  // namespace makeDotCpp
