#pragma once
/** @file
 * A part: one of a component's own lines, as a function of what the
 * component knows about it, called with only the parameters it names.
 */
#include <sigilcompose/core/Element.h>

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sigil::compose::kit {

namespace detail {

template <class Fn, class Offer, std::size_t... I>
constexpr bool takesFirst(std::index_sequence<I...>) {
  return std::is_invocable_r_v<Element, const Fn&,
                               std::tuple_element_t<I, Offer>...>;
}

/** The longest prefix of @p Offer's element types @p Fn accepts, or -1
 *  when it accepts none, the empty prefix included. */
template <class Fn, class Offer, std::size_t N>
constexpr std::ptrdiff_t prefixLength() {
  if constexpr (takesFirst<Fn, Offer>(std::make_index_sequence<N>{}))
    return static_cast<std::ptrdiff_t>(N);
  else if constexpr (N == 0)
    return -1;
  else
    return prefixLength<Fn, Offer, N - 1>();
}

template <class Fn, class Offer, std::size_t... I>
Element callWith(const Fn& fn, const Offer& offered,
                 std::index_sequence<I...>) {
  return fn(std::get<I>(offered)...);
}

}  // namespace detail

/** A PART OF A COMPONENT — one of the lines a component writes itself,
 *  as a function of what the component offers about it: for a caption's
 *  label, the text and then the caption. A part is handed ANY callable
 *  whose parameters are a prefix of that offer and is called with the
 *  ones it names, so `[](const Utf8& text) {…}`, `[](const Utf8& text,
 *  const Caption& voice) {…}` and `[] {…}` are all parts of a caption,
 *  as a range's children take a function of the item or of the item and
 *  its index.
 *
 *  Every component's parts default to a leaf in the register's class,
 *  so the sheet in force sets the line; a sketch hands in its own to set
 *  that one line otherwise — the register's leaf with a font over it, or
 *  a leaf of its own — and nothing else under the component changes,
 *  because no sheet moved. An empty part is the default, so a component
 *  falls back to its own leaf where a caller left one unset. */
template <class... Offered>
class Part {
  using Offer = std::tuple<const Offered&...>;
  template <class Fn>
  static constexpr std::ptrdiff_t kPrefix =
      detail::prefixLength<Fn, Offer, sizeof...(Offered)>();

 public:
  Part() = default;
  template <class Fn>
    requires(!std::is_same_v<std::decay_t<Fn>, Part> && kPrefix<Fn> >= 0)
  Part(Fn fn)
      : m_call([fn = std::move(fn)](const Offered&... offered) -> Element {
          return detail::callWith(
              fn, Offer(offered...),
              std::make_index_sequence<static_cast<std::size_t>(
                  kPrefix<Fn>)>{});
        }) {}

  /** The line, built from what the component offers. */
  Element operator()(const Offered&... offered) const {
    return m_call(offered...);
  }
  /** False for the empty part, which a component answers with its own
   *  default. */
  explicit operator bool() const { return static_cast<bool>(m_call); }

 private:
  std::function<Element(const Offered&...)> m_call;
};

}  // namespace sigil::compose::kit
