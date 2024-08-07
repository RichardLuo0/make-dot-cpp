module;
#include <boost/describe.hpp>
#include <boost/algorithm/string.hpp>

export module makeDotCpp.utils;

import std;
import boost.json;

#include "alias.hpp"
#include "macro.hpp"

namespace makeDotCpp {
namespace ranges {
export template <class C, class Item>
concept range = std::ranges::range<C> &&
                std::is_convertible_v<std::ranges::range_value_t<C>, Item>;

auto foreachIndex = []<std::size_t... Ids>(std::index_sequence<Ids...>,
                                           auto&& func) {
  (func.template operator()<Ids>() || ...);
};

export template <std::ranges::view... Cs>
  requires(sizeof...(Cs) > 0)
class concatView : public std::ranges::view_interface<concatView<Cs...>> {
 protected:
  using T = std::common_type_t<std::ranges::range_value_t<Cs>...>;

  struct Iterator {
   protected:
    const std::tuple<Cs...>* nested;
    template <class C>
    struct CurIt {
      std::ranges::iterator_t<C> it;
      std::ranges::iterator_t<C> end;

      CurIt() {}
      CurIt(const C& c) : it(c.begin()), end(c.end()) {}

      bool operator==(const CurIt& other) const { return it == other.it; }
    };
    std::variant<CurIt<Cs>...> cur;
    std::size_t id;  // If id is sizeof...(Cs), its the end iterator

    void forwardToValidRange() {
      while (std::visit([](auto&& cur) { return cur.it == cur.end; }, cur))
        if (id == sizeof...(Cs) - 1) {
          id = sizeof...(Cs);
          break;
        } else {
          foreachIndex(
              std::make_index_sequence<sizeof...(Cs) - 1>(),
              [&]<std::size_t Id>() {
                if (Id == id) {
                  id = id + 1;
                  cur.template emplace<Id + 1>(std::get<Id + 1>(*nested));
                  return true;
                }
                return false;
              });
        }
    }

   public:
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    Iterator() = default;

    template <std::size_t I>
    Iterator(std::in_place_index_t<I>, const auto& nested)
        : nested(&nested), id(I) {
      if constexpr (I == sizeof...(Cs))
        cur.template emplace<0>();
      else {
        cur.template emplace<I>(std::get<I>(nested));
        forwardToValidRange();
      }
    }

    const T& operator*() const {
      return std::visit([](auto&& cur) -> const T& { return *cur.it; }, cur);
    }

    T* operator->() {
      return std::visit([](auto&& cur) -> T* { return &cur.it; }, cur);
    }

    Iterator& operator++() {
      std::visit([](auto&& cur) { ++cur.it; }, cur);
      forwardToValidRange();
      return *this;
    }

    Iterator operator++(int) {
      Iterator old = *this;
      operator++();
      return old;
    }

    bool operator==(const Iterator& it) const {
      return id == it.id && (cur == it.cur || id == sizeof...(Cs));
    }
  };

  const std::tuple<Cs...> nested;

 public:
  concatView(Cs&&... views) : nested{std::move(views)...} {}

  auto begin() const { return Iterator(std::in_place_index<0>, nested); }

  auto end() const {
    return Iterator(std::in_place_index<sizeof...(Cs)>, nested);
  }
};

export inline auto concat(auto&&... cs) noexcept {
  return concatView{std::ranges::views::all(cs)...};
}

export template <class C>
struct to {};

export template <class C>
inline C operator|(std::ranges::range auto&& range, to<C>) {
  C result;
  std::ranges::copy(range.begin(), range.end(),
                    std::inserter(result, result.end()));
  return result;
}

export template <class Item>
inline std::vector<Item> operator|(std::ranges::range auto&& range,
                                   to<std::vector<Item>>) {
  std::vector<Item> result;
  result.reserve(std::ranges::size(range));
  std::ranges::copy(range.begin(), range.end(), std::back_inserter(result));
  return result;
}
}  // namespace ranges

export inline auto concat(auto&&... cs) noexcept {
  std::vector<std::common_type_t<std::ranges::range_value_t<decltype(cs)>...>>
      container;
  std::size_t size = 0;
  ((size += cs.size()), ...);
  container.reserve(size);
  auto emplace = [&](auto&& c) {
    for (const auto& e : c) {
      container.emplace_back(e);
    }
  };
  (emplace(cs), ...);
  return container;
}

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

export inline std::string readAsStr(const Path& path) {
  std::ifstream is(path);
  is.exceptions(std::ifstream::failbit);
  return {std::istreambuf_iterator<char>(is), {}};
}

export inline json::value parseJson(const Path& path) {
  const auto input = readAsStr(path);
  return json::parse(input, {},
                     {.allow_comments = true, .allow_trailing_commas = true});
}

export inline Path commonBase(const ranges::range<Path> auto& pathList) {
  Path result;
  if (pathList.size() == 0) return result;
  if (pathList.size() == 1) return pathList.begin()->parent_path();
  auto itList =
      pathList | std::views::drop(1) | std::views::transform([](auto&& path) {
        return std::make_pair(path.begin(), --path.end());
      }) |
      ranges::to<std::vector<std::pair<Path::iterator, Path::iterator>>>();
  for (auto folder : *pathList.begin()) {
    for (auto& pair : itList) {
      auto& [it, end] = pair;
      if (!(it != end && folder == *it)) return result;
      it++;
    }
    result /= folder;
  }
  return result;
}

export template <class T, class Ctx>
std::shared_ptr<T> tag_invoke(const json::value_to_tag<std::shared_ptr<T>>&,
                              const json::value& jv, const Ctx& ctx) {
  return std::make_shared<T>(json::value_to<T>(jv, ctx));
}

export template <class T, class Ctx>
std::unique_ptr<T> tag_invoke(const json::value_to_tag<std::unique_ptr<T>>&,
                              const json::value& jv, const Ctx& ctx) {
  return std::make_unique<T>(json::value_to<T>(jv, ctx));
}

export template <class T>
struct Merge : public T {};

export template <class T>
struct Required : public T {};

export DEF_EXCEPTION(RequiredJsonMember, (const std::string& name),
                     "required json member: " + name);

template <class T>
struct isRequired : std::false_type {};

template <class T>
struct isRequired<Required<T>> : std::true_type {};

export template <class T, class Ctx>
Merge<T> tag_invoke(const json::value_to_tag<Merge<T>>&, const json::value& jv,
                    Ctx&& ctx) {
  Merge<T> t;
  const auto obj = jv.as_object();
  using namespace boost::describe;
  boost::mp11::mp_for_each<describe_members<T, mod_any_access | mod_inherited>>(
      [&](auto&& m) {
        auto& member = t.*m.pointer;
        using memberType = std::remove_reference_t<decltype(member)>;
        const auto* value = obj.if_contains(m.name);
        if (value) {
          auto cValue =
              json::try_value_to<memberType>(*value, std::forward<Ctx>(ctx));
          if (cValue) member = std::move(*cValue);
        } else if (isRequired<memberType>::value)
          throw RequiredJsonMember(m.name);
      });
  return t;
}

export template <class T>
void tag_invoke(const json::value_from_tag&, json::value& jv,
                const Merge<T>& merge) {
  jv = json::value_from(static_cast<T>(merge));
}

export template <class T, class Ctx>
Required<T> tag_invoke(const json::value_to_tag<Required<T>>&,
                       const json::value& jv, Ctx&& ctx) {
  return Required<T>(json::value_to<T>(jv, ctx));
}

export template <class T>
void tag_invoke(const json::value_from_tag&, json::value& jv,
                const Required<T>& t) {
  jv = json::value_from(static_cast<T>(t));
}
}  // namespace makeDotCpp
