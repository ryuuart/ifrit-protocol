#pragma once

/** @file
 * @ingroup core-callable
 *
 * A CALL WITH THE PARAMETERS THE CALLABLE NAMED: the prefix search, the
 * concept over it, and the type-erased holder a verb stores one in.
 */

/** @defgroup core-callable The callable leaf
 *  A verb that offers a caller more than most callers read takes its
 *  callable through here, so a lambda naming none, some or all of the
 *  offered parameters is the same verb's argument and nobody spells a
 *  parameter only to ignore it. The parameters a callable names are the
 *  FIRST of what the verb offers, dropped from the end and never from
 *  the middle. */

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sigil::core {

namespace detail {

/** Whether @p Fn accepts the first `sizeof...(I)` of @p Offer's element
 *  types and answers something @p Result accepts. `void` as the result
 *  accepts any answer, which is what `std::is_invocable_r_v` already means
 *  by it. The callable is asked as an LVALUE, so a `mutable` lambda — a
 *  steppable carrying its own accumulator, say — is as good a callable as a
 *  stateless one. */
template <class Result, class Fn, class Offer, std::size_t... I>
constexpr bool takesFirst(std::index_sequence<I...>) {
  return std::is_invocable_r_v<Result, std::remove_reference_t<Fn>&,
                               std::tuple_element_t<I, Offer>...>;
}

/** The longest prefix of @p Offer's element types @p Fn accepts, or -1 when
 *  it accepts none, the empty prefix included. */
template <class Result, class Fn, class Offer, std::size_t N>
constexpr std::ptrdiff_t prefixLength() {
  if constexpr (takesFirst<Result, Fn, Offer>(std::make_index_sequence<N>{}))
    return static_cast<std::ptrdiff_t>(N);
  else if constexpr (N == 0)
    return -1;
  else
    return prefixLength<Result, Fn, Offer, N - 1>();
}

/** @p fn over the elements @p I names of the reference tuple @p offered,
 *  answering @p Result — which discards what the call answers where
 *  `Result` is `void`. */
template <class Result, class Fn, class Offer, std::size_t... I>
Result callWith(Fn& fn, Offer& offered, std::index_sequence<I...>) {
  if constexpr (std::is_void_v<Result>)
    fn(std::get<I>(offered)...);
  else
    return fn(std::get<I>(offered)...);
}

/** What that call answers. Declared and never defined: it is read in an
 *  unevaluated `decltype` over the very expression `callWith` performs, so
 *  the answer type cannot drift from the call. */
template <class Fn, class Offer, std::size_t... I>
auto answerOf(Fn& fn, Offer& offered, std::index_sequence<I...>)
    -> decltype(fn(std::get<I>(offered)...));

}  // namespace detail

/** HOW MANY OF @p Signature's PARAMETERS @p Fn NAMED, in order from the
 *  first, or -1 when it names a set that is no prefix of them — the search
 *  every loosened verb in the tree shares. The signature's result decides
 *  what the call must answer, `void` accepting anything. */
template <class Fn, class Signature>
constexpr std::ptrdiff_t namedParameters = -1;

/** The search itself, for a signature written as a function type. */
template <class Fn, class Result, class... Offered>
constexpr std::ptrdiff_t namedParameters<Fn, Result(Offered...)> =
    detail::prefixLength<Result, std::remove_reference_t<Fn>,
                         std::tuple<Offered...>, sizeof...(Offered)>();

/** @p Fn IS CALLABLE WITH A PREFIX of what @p Signature offers: given
 *  `void(SkCanvas&, const PaintContext&)`, `[](SkCanvas& c) {…}`,
 *  `[] {…}` and `[] {…}` all are, and
 *  `[](const PaintContext& ctx) {…}` is not — a caller drops the
 *  parameters it does not read from the END of the offer, never from the
 *  middle. */
template <class Fn, class Signature>
concept PrefixCallable = namedParameters<Fn, Signature> >= 0;

/** @p fn CALLED WITH THE FIRST OF @p offered IT NAMED, answering whatever
 *  it answers — for a verb that erases the callable into a store of its
 *  own (a vector of steppables, a struct field already spelled as a
 *  `std::function`) rather than holding a `Callable`. */
template <class Fn, class... Offered>
  requires PrefixCallable<Fn, void(Offered...)>
decltype(auto) callPrefix(Fn&& fn, Offered&&... offered) {
  constexpr auto named = std::make_index_sequence<static_cast<std::size_t>(
      namedParameters<Fn, void(Offered...)>)>{};
  auto args = std::forward_as_tuple(std::forward<Offered>(offered)...);
  using Answer = decltype(detail::answerOf(fn, args, named));
  return detail::callWith<Answer>(fn, args, named);
}

/** A callable that names only the parameters it reads, declared for a
 *  function signature; only the function-type form below is defined. */
template <class Signature>
class Callable;

/** A CALLABLE THAT NAMES ONLY THE PARAMETERS IT READS — a copyable,
 *  type-erased call of @p Result(Offered...), built from ANY callable
 *  whose parameters are a prefix of the signature's and called with the
 *  ones it named. A verb that offers a caller more than most callers
 *  read holds one of these instead of a `std::function`, so that
 *  `custom([](SkCanvas& c) {…})` and `custom([](SkCanvas& c, const
 *  PaintContext& ctx) {…})` are both that verb's argument and neither
 *  spells a parameter to ignore it.
 *
 *  It holds what a `std::function` holds — one call, copied with the value,
 *  false while empty — and nothing more: a call is not a value, so two of
 *  them never compare equal and whatever prunes on a description holding
 *  one must key on something else (a scheme's own equality, or the key a
 *  caller states beside the callable). */
template <class Result, class... Offered>
class Callable<Result(Offered...)> {
 public:
  Callable() = default;
  template <class Fn>
    requires(!std::is_same_v<std::decay_t<Fn>, Callable> &&
             PrefixCallable<Fn, Result(Offered...)>)
  Callable(Fn fn)  // NOLINT: implicit by design (the verb takes the lambda)
      : m_call([fn = std::move(fn)](Offered... offered) mutable -> Result {
          auto args = std::forward_as_tuple(std::forward<Offered>(offered)...);
          return detail::callWith<Result>(
              fn, args,
              std::make_index_sequence<static_cast<std::size_t>(
                  namedParameters<Fn, Result(Offered...)>)>{});
        }) {}

  /** The call, with everything the signature offers; what the callable did
   *  not name is evaluated and dropped. */
  Result operator()(Offered... offered) const {
    return m_call(std::forward<Offered>(offered)...);
  }
  /** False for the empty call, which a holder answers with its own
   *  default. */
  explicit operator bool() const { return static_cast<bool>(m_call); }

 private:
  std::function<Result(Offered...)> m_call;
};

}  // namespace sigil::core
