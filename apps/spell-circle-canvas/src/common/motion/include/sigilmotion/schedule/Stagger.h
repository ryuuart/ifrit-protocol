#pragma once

/** @file
 * @ingroup motion-schedule
 *
 * `stagger()`: a VALUE that differs per child — a delay that steps from
 * one sibling to the next, a rotation spread from a first child's number
 * to a last child's, a table of cues — which any tween field takes, and
 * which a host resolves against the child's `Place` among its siblings.
 */

#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/time/Duration.h>

#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::motion {

/** WHERE A CHILD STANDS AMONG ITS SIBLINGS: its index and how many there
 *  are. A host knows it when it lays the children out — the same moment a
 *  selector places a sibling — and resolves every staggered value on the
 *  child against it. One child alone is `{0, 1}`. */
struct Place {
  size_t index = 0;
  size_t count = 1;

  bool operator==(const Place&) const = default;
};

/** WHERE A STAGGER STARTS: which sibling takes the first value. */
enum class StaggerFrom : uint8_t {
  /** The first child, then the next — declaration order. */
  First,
  /** The middle child, then outward both ways. */
  Center,
  /** The last child, then backwards. */
  Last,
  /** Both ends at once, meeting in the middle. */
  Edges,
  /** A scatter: a seeded permutation, the same on every frame. */
  Random,
};

/** A stagger's origin: one of the named places, or the index of the child
 *  the values spread out from — `{.from = StaggerFrom::Center}` or
 *  `{.from = 3}`. */
struct StaggerOrigin {
  StaggerFrom place = StaggerFrom::First;
  std::optional<size_t> index;

  StaggerOrigin() = default;
  StaggerOrigin(StaggerFrom from) : place(from) {}  // NOLINT: implicit by design
  template <std::integral Index>
    requires(!std::same_as<Index, bool>)
  StaggerOrigin(Index at)  // NOLINT: implicit by design
      : index(static_cast<size_t>(at)) {}

  bool operator==(const StaggerOrigin&) const = default;
};

/** Which distance a grid stagger reads: straight-line, or along one axis. */
enum class StaggerAxis : uint8_t { Both, Columns, Rows };

/** HOW A STAGGER DEALS ITS VALUES — anime.js's `stagger()` options, plus
 *  the house seed and a caller-stated order. */
template <typename V>
struct StaggerOptions {
  /** Which sibling takes the first value. */
  StaggerOrigin from{};
  /** `{columns, rows}`: the siblings are a grid read row by row, and a
   *  child's distance from the origin is measured across it. Zero columns
   *  is a list. */
  std::array<uint32_t, 2> grid{};
  StaggerAxis axis = StaggerAxis::Both;
  /** How the values crowd: the evenly spaced distances run through this
   *  curve, so an ease-in bunches the early children and spreads the late
   *  ones. Empty is even. */
  Easing ease;
  /** Added to every child's value. */
  V start{};
  /** The last child first. */
  bool reverse = false;
  /** Which scatter `StaggerFrom::Random` deals; 0 keys it on the count. */
  uint32_t seed = 0;
  /** THE ORDER, stated: one number per child, dealt smallest first, ties
   *  together — a radius, a role, a depth. Non-empty, it replaces `from`,
   *  `grid` and `seed`. */
  std::vector<float> rankBy;

  bool operator==(const StaggerOptions& other) const {
    return from == other.from && grid == other.grid && axis == other.axis &&
           easeEqual(ease, other.ease) && start == other.start &&
           reverse == other.reverse && seed == other.seed &&
           rankBy == other.rankBy;
  }
};

/** WHERE CHILD @p place.index SITS in a stagger dealt by @p origin, in
 *  steps from the first — 0, 1, 2… from the first child, reversed from the
 *  last, symmetric about the middle or the ends, a distance across a grid,
 *  a seeded permutation, or the dense ranks of a stated order — and the
 *  largest step any of the siblings sits at. One body for every host, so
 *  an origin means one order wherever it is written. */
struct StaggerStep {
  float step = 0.0f;
  float last = 0.0f;
};
StaggerStep staggerStep(const StaggerOrigin& origin,
                        const std::array<uint32_t, 2>& grid, StaggerAxis axis,
                        uint32_t seed, const std::vector<float>& rankBy,
                        bool reverse, Place place);

/** Every sibling's step at once, written into @p out in place — the form
 *  a schedule resolving a whole run per frame reads. */
void staggerSteps(const StaggerOrigin& origin,
                  const std::array<uint32_t, 2>& grid, StaggerAxis axis,
                  uint32_t seed, const std::vector<float>& rankBy,
                  bool reverse, uint32_t count, std::vector<float>& out);

/** A value a stagger can step and spread: one that adds, subtracts and
 *  scales by a number. Any other value staggers only as a table. */
template <typename V>
concept Spreadable = requires(const V value, double amount) {
  { value + (value - value) * amount };
  { value + value * amount };
};

/**
 * A VALUE THAT MAY DIFFER PER CHILD: a plain value, or a stagger that
 * resolves one from the child's `Place` among its siblings.
 *
 *     .delay = stagger(40ms)                          // 0, 40, 80… ms
 *     .to = stagger({0.0f, 360.0f})                   // first child 0, last 360
 *     .delay = stagger(60ms, {.from = StaggerFrom::Center})
 *
 * A tween field of this type takes a plain value just as well, so writing
 * `.delay = 120ms` is unchanged. What a stagger cannot know is WHICH child
 * it is on: a host resolves it with `at()` against the place it laid the
 * child out at.
 */
template <typename V>
class Staggered {
 public:
  enum class Form : uint8_t { Value, Each, Range, Table };

  Staggered() = default;
  template <typename U>
    requires std::convertible_to<U, V>
  Staggered(U value) : m_value(static_cast<V>(value)) {}  // NOLINT: implicit by design
  /** Any stagger whose values convert, so a table or a range written in
   *  milliseconds is a stagger of `Duration`. */
  template <typename U>
    requires(std::convertible_to<U, V> && !std::same_as<U, V>)
  Staggered(const Staggered<U>& other)  // NOLINT: implicit by design
      : m_form((Form)other.form()),
        m_value(static_cast<V>(other.firstValue())),
        m_last(static_cast<V>(other.lastValue())),
        m_options{other.options().from, other.options().grid,
                  other.options().axis, other.options().ease,
                  static_cast<V>(other.options().start),
                  other.options().reverse, other.options().seed,
                  other.options().rankBy} {
    for (const U& entry : other.table()) m_table.push_back(static_cast<V>(entry));
  }

  /** `stagger(each)`: the step between one child and the next. */
  static Staggered each(V step, StaggerOptions<V> options) {
    Staggered out;
    out.m_form = Form::Each;
    out.m_value = std::move(step);
    out.m_options = std::move(options);
    return out;
  }
  /** `stagger({first, last})`: spread across the children. */
  static Staggered range(V first, V last, StaggerOptions<V> options) {
    Staggered out;
    out.m_form = Form::Range;
    out.m_value = std::move(first);
    out.m_last = std::move(last);
    out.m_options = std::move(options);
    return out;
  }
  /** A value per child, read by index; past the end, the last entry. */
  static Staggered table(std::vector<V> entries) {
    Staggered out;
    out.m_form = Form::Table;
    out.m_table = std::move(entries);
    return out;
  }

  /** THE VALUE FOR THE CHILD AT @p place. A plain value is the same for
   *  every child. */
  [[nodiscard]] V at(Place place) const {
    switch (m_form) {
      case Form::Value:
        return m_value;
      case Form::Table:
        if (m_table.empty()) return V{};
        return m_table[std::min(place.index, m_table.size() - 1)];
      case Form::Each:
      case Form::Range: {
        if constexpr (!Spreadable<V>) {
          return m_value;
        } else {
        const StaggerStep step =
            staggerStep(m_options.from, m_options.grid, m_options.axis,
                        m_options.seed, m_options.rankBy, m_options.reverse,
                        place);
        float along = step.step;
        if (m_options.ease && step.last > 0.0f)
          along = m_options.ease(step.step / step.last) * step.last;
        if (m_form == Form::Each)
          return m_options.start + m_value * (double)along;
        const float unit = step.last > 0.0f ? along / step.last : 0.0f;
        return m_options.start + m_value + (m_last - m_value) * (double)unit;
        }
      }
    }
    return m_value;
  }
  /** The value for a child alone — what a stagger reads where nothing has
   *  placed it among siblings. */
  [[nodiscard]] V value() const { return at({0, 1}); }
  [[nodiscard]] bool isStaggered() const { return m_form != Form::Value; }

  // ── the parts, read back: for a schedule resolving a run, and equality ──
  [[nodiscard]] Form form() const { return m_form; }
  /** The plain value, the step, or the first child's value. */
  [[nodiscard]] const V& firstValue() const { return m_value; }
  /** The last child's value, for a range. */
  [[nodiscard]] const V& lastValue() const { return m_last; }
  [[nodiscard]] const std::vector<V>& table() const { return m_table; }
  [[nodiscard]] const StaggerOptions<V>& options() const { return m_options; }

  bool operator==(const Staggered& other) const {
    return m_form == other.m_form && m_value == other.m_value &&
           m_last == other.m_last && m_table == other.m_table &&
           m_options == other.m_options;
  }

 private:
  Form m_form = Form::Value;
  V m_value{};
  V m_last{};
  std::vector<V> m_table;
  StaggerOptions<V> m_options{};
};

/** A DELAY THAT STEPS from one sibling to the next: `.delay = stagger(40ms)`. */
inline Staggered<Duration> stagger(Duration each,
                                   StaggerOptions<Duration> options = {}) {
  return Staggered<Duration>::each(each, std::move(options));
}

/** A VALUE THAT STEPS from one sibling to the next:
 *  `.to = stagger(10.0f)` is 0, 10, 20… */
template <typename V>
  requires(!std::convertible_to<V, Duration>)
Staggered<V> stagger(V each, StaggerOptions<std::type_identity_t<V>> options = {}) {
  return Staggered<V>::each(std::move(each), std::move(options));
}

/** A VALUE SPREAD ACROSS THE SIBLINGS, the first child's to the last's:
 *  `.to = stagger({0.0f, 360.0f})`, `.delay = stagger({0ms, 620ms})`. */
template <typename V>
Staggered<V> stagger(std::initializer_list<V> firstAndLast,
                     StaggerOptions<std::type_identity_t<V>> options = {}) {
  const V* ends = firstAndLast.begin();
  const V first = firstAndLast.size() > 0 ? ends[0] : V{};
  const V last = firstAndLast.size() > 1 ? ends[firstAndLast.size() - 1] : first;
  return Staggered<V>::range(first, last, std::move(options));
}

/** A TABLE OF STARTS cut against a recording — caption, lyric and
 *  lip-sync timing — read by index: `.delay = cues({0ms, 340ms, 720ms})`.
 *  A child past the end of the table starts at its last entry. */
inline Staggered<Duration> cues(std::vector<Duration> starts) {
  return Staggered<Duration>::table(std::move(starts));
}

}  // namespace sigil::motion
