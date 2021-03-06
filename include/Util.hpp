#ifndef UTIL_HPP_
# define UTIL_HPP_

# include <utility>
/*
 * TODO: clean up this file.
 */

template<class R, class Op>
class Supplier
{
private:
  Op op;

public:
  constexpr Supplier(Op op)
    : op(op)
  {}

  constexpr R get() {
    return op();
  }
};

template<class Y, class X, class Op>
class Function
{
private:
  Op op;

public:
  constexpr Function(Op op)
    : op(op)
  {}

  constexpr Y apply(X const &x) {
    return op(x);
  }
};

template<class PREDICATE>
struct Not
{
  PREDICATE predicate;

  template<class... PARAMS>
  constexpr auto operator()(PARAMS &&... params)
  {
    return !predicate(std::forward<PARAMS>(params)...);
  }
};

template<class T>
auto makeNot(T t)
{
  return (Not<T>{t});
}

template<class T>
auto const &clamp(T const &a, T const &min, T const &max)
{
  return a < min ? min : a > max ? max : a;
}

struct NOOP
{
  template<class... PARAMS>
  constexpr void operator()(PARAMS &&... ) const
  {};
};

#endif // !UTIL_HPP_
