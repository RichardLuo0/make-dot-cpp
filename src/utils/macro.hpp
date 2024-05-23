#pragma once

#define COMMA ,
#define UNPACK(...) __VA_ARGS__

#define UNUSED(x) (void)x;

#define OVERLOAD_2(_1, _2, NAME, ...) NAME
#define OVERLOAD_3(_1, _2, _3, NAME, ...) NAME
#define OVERLOAD_4(_1, _2, _3, _4, NAME, ...) NAME

#define chainMethod(SETTER_NAME, TYPE, VALUE)   \
 public:                                        \
  inline auto &SETTER_NAME(const TYPE &VALUE) { \
    this->_##SETTER_NAME(VALUE);                \
    return *this;                               \
  }                                             \
                                                \
 private:                                       \
  inline void _##SETTER_NAME(const TYPE &VALUE)

#define chainVar_0(TYPE, NAME, DEFAULT_VALUE, SETTER_NAME) \
  TYPE NAME = DEFAULT_VALUE;                               \
                                                           \
 public:                                                   \
  inline auto &SETTER_NAME(const TYPE &NAME) noexcept {    \
    this->NAME = NAME;                                     \
    return *this;                                          \
  }                                                        \
                                                           \
 private:
#define chainVar_1(TYPE, NAME, SETTER_NAME)             \
  TYPE NAME;                                            \
                                                        \
 public:                                                \
  inline auto &SETTER_NAME(const TYPE &NAME) noexcept { \
    this->NAME = NAME;                                  \
    return *this;                                       \
  }                                                     \
                                                        \
 private:
#define chainVar(...) \
  OVERLOAD_4(__VA_ARGS__, chainVar_0, chainVar_1)(__VA_ARGS__)

#define chainVarList(TYPE, NAME, SETTER_NAME, VALUE) \
  std::deque<TYPE> NAME;                             \
                                                     \
  chainMethod(SETTER_NAME, TYPE, VALUE)

#define chainVarSet(TYPE, NAME, SETTER_NAME, VALUE) \
  std::unordered_set<TYPE> NAME;                    \
                                                    \
  chainMethod(SETTER_NAME, TYPE, VALUE)

#define defException(NAME, ARGS, MSG)                                   \
  class NAME : public std::exception {                                  \
   private:                                                             \
    std::string msg;                                                    \
                                                                        \
   public:                                                              \
    NAME ARGS : msg(MSG) {}                                             \
                                                                        \
   public:                                                              \
    const char *what() const noexcept override { return msg.c_str(); }; \
  };
