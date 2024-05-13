module;
#include <boost/json.hpp>
#include <boost/describe.hpp>

export module makeDotCpp.utils;

import std;

#include "alias.hpp"

namespace makeDotCpp {
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

export template <std::ranges::range C>
inline C replace(const C& c, auto&& item, auto&& newItem) {
  C cCopy(c);
  std::ranges::replace(cCopy, item, newItem);
  return cCopy;
}

export json::value parseJson(const fs::path& path) {
  std::ifstream is(path);
  std::string input(std::istreambuf_iterator<char>(is), {});
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
}  // namespace makeDotCpp

namespace boost {
namespace json {
export void tag_invoke(const json::value_from_tag&, json::value& jv,
                       const fs::path& path) {
  jv = path.generic_string();
}

export fs::path tag_invoke(const json::value_to_tag<fs::path>&,
                           const json::value& jv) {
  return fs::path(std::string(jv.as_string()));
}
}  // namespace json
}  // namespace boost
