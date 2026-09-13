#pragma once

/** @file
 * A FRAME WITH SCALES, AND LAYERS AS FUNCTIONS OF IT. One `plot` holds a
 * frame — two `data::Scale`s, a pad, and Cartesian or polar coordinates —
 * and a run of layers, each a value the plot calls with that frame to make
 * its element. Nothing here computes a pixel from a datum: the frame's
 * RANGES are the box's, filled in where the box is known, so a sketch
 * states what its numbers MEAN and never where they land.
 *
 * THE CLASSES. Every part a chart draws names a class and reads its look
 * from the `weave::StyleSheet` in force where it lands — `axis` for the
 * axis line, `tick` for the tick marks and their numbers, `rule` for the
 * hairlines across the field, `trace` for a curve, `area` for the band
 * under one, `mark` for a datum's own element, `bar` for the band a datum
 * is drawn as, and `label` for a word placed in the field.
 * `Theme::styleSheet()` registers a default for each, so a plot under a
 * page is dressed without saying anything; a sketch that wants otherwise
 * states a sheet of its own on the plot or on its root.
 *
 * A recording reads its colour through the INK in force, which is what the
 * class resolves to, exactly as text's colour is. Only a stroke width, a
 * radius and a sampling count are props: no colour, size or face is.
 *
 * A PLOT OF SEVERAL SERIES names a class per layer. `styleClass` on a
 * layer's props is the class it reads INSTEAD of the one its part is named
 * for — the same kind of part, a different entry on the sheet, which is
 * what CSS's class attribute is for — and the sketch registers that name
 * on the sheet it states. Empty is the part's own class.
 *
 *     kit::plot("fit", {.x = {.domain = {0, 440000}},
 *                       .y = {.domain = {0, 180}}, .pad = 8},
 *               {kit::axis({.of = kit::Axis::X}),
 *                kit::axis({.of = kit::Axis::Y}),
 *                kit::rules({.y = {90}}),
 *                kit::trace([](double men) { return 3.828e-4 * men; }),
 *                kit::marks(corps, dot, {.x = &Corps::men, .y = &Corps::px}),
 *                kit::label("r² = 0.993", 40000, 170)})
 *         .width(560).height(236)
 */

#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/kit/Part.h>
#include <sigilcore/callable/Callable.h>
#include <sigildata/scale/Scale.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

/** WHICH OF A FRAME'S TWO SCALES a layer reads: the one the domain runs
 *  along, or the one the values run along. On a polar frame `X` is the
 *  angle and `Y` the radius. */
enum class Axis : std::uint8_t { X, Y };

/** ONE DATUM, in the frame's OWN DOMAIN UNITS — never in pixels. A layer
 *  that places elements carries a run of these and the frame maps them
 *  where the box is known. */
struct Datum {
  double x = 0.0;
  double y = 0.0;
  bool operator==(const Datum&) const = default;
};

/** POLAR COORDINATES for a frame: x becomes the angle and y the radius.
 *
 *  A band angle scale with a square-root radius scale is the coxcomb, and
 *  that is the whole reason this is a property of the frame rather than a
 *  second component: the wedge, the ring, the spoke and the rim label are
 *  the same layers reading the same two scales. */
struct Polar {
  /** The angles the x domain is carried onto, DEGREES, in Skia's canvas
   *  convention — 0° is +x and the sweep runs clockwise — so the default
   *  starts at twelve o'clock and reads round to the right. */
  data::Interval sweep{-90.0, 270.0};
  /** Where the y domain's LOW end stands, as a fraction of the outer
   *  radius: the hole a ring leaves in the middle. 0 reaches the centre,
   *  which is the pie. */
  float inner = 0.0f;
  bool operator==(const Polar&) const = default;
};

/** THE FRAME EVERY LAYER IS A FUNCTION OF: what the two axes MEAN, the
 *  room left inside the box, and whether the box is read as a plane or as
 *  a wheel.
 *
 *  THE RANGES ARE THE BOX'S AND NOT THE SKETCH'S. Whatever `x.range` and
 *  `y.range` are written as is replaced: x runs across the box and y runs
 *  UP it, inside `pad` on every side, and on a polar frame x runs over
 *  `Polar::sweep` and y from the inner radius out to the outer. So a
 *  frame states domains, transforms, steps and band paddings, and the
 *  mapping is complete only where a box is in hand — which is layout and
 *  paint, never describe.
 *
 *  A comparable value, cheap to copy: it can be a field on a description,
 *  bound into a context or built fresh per frame. */
struct Plot {
  /** The values that run ACROSS, or round. */
  data::Scale x;
  /** The values that run UP, or outward. */
  data::Scale y;
  /** px of room inside the box on every side, so a mark at the extremes
   *  of its domain is not half a stroke outside the drawing. */
  float pad = 0.0f;
  /** Unset is Cartesian. */
  std::optional<Polar> polar;
  bool operator==(const Plot&) const = default;

  /** SCALE @p which RANGED ONTO A BOX of @p size — px across for a
   *  Cartesian x, px down-to-up for a Cartesian y (the range runs from
   *  the box's bottom to its top), degrees for a polar x and px of radius
   *  for a polar y.
   *
   *  Its `apply` answers where a band STARTS and its `bandwidth` how wide
   *  the band came out, which is what the layer that draws bands reads;
   *  `at` below answers where a datum's MARK stands, which is the middle
   *  of that band. */
  [[nodiscard]] data::Scale scale(Axis which, SkSize size) const;

  /** WHERE THE MARK FOR (@p xValue, @p yValue) STANDS in a box of
   *  @p size — the middle of the datum's band where the x transform has
   *  bands, the value's own position where it does not. Every trace,
   *  rule, tick, mark and label is drawn through this one mapping, so a
   *  caption and the datum it names cannot land on two arithmetics.
   *
   *  A degenerate domain maps to the middle of its axis rather than to
   *  infinity, so a plot built before its data arrived draws a line
   *  through the middle instead of nothing. */
  [[nodiscard]] SkPoint at(double xValue, double yValue, SkSize size) const;

  /** THE MIDDLE OF A POLAR FRAME'S BOX — the hub every ring, spoke and
   *  wedge is struck from. The middle of the box for a Cartesian one,
   *  which is what it is. */
  [[nodiscard]] SkPoint centre(SkSize size) const;

  /** THE OUTER RADIUS of a polar frame in a box of @p size: half the
   *  box's shorter side, less `pad`. */
  [[nodiscard]] float radius(SkSize size) const;

  /** WHERE @p xValue STANDS AS AN ANGLE, degrees, band-centred as `at`
   *  is. Meaningless on a Cartesian frame, and answers 0 there. */
  [[nodiscard]] double angle(double xValue) const;

  /** WHERE @p yValue STANDS AS A FRACTION OF THE OUTER RADIUS —
   *  `Polar::inner` at the low end of the y domain and 1 at the high end.
   *  It takes no box, because a wedge's shape is the same at every size:
   *  that is what lets a wedge be DESCRIBED before it is laid out, and
   *  only its box wait for layout. */
  [[nodiscard]] double radiusFraction(double yValue) const;
};

/** ONE LAYER OF A PLOT: a function of the frame, of the plot's own key and
 *  of the layer's index in the run, answering the element that draws it.
 *
 *  It is called with the parameters it NAMES, so `[](const Plot& f) {…}`
 *  and `[] {…}` are layers too — a drawing of the caller's own goes into a
 *  plot beside the library's, reading the same frame. The key and the
 *  index are offered because a layer that records a path must name its
 *  recording, and a name has to be unique among the plots on a sheet. */
using Layer = sigil::core::Callable<compose::Element(
    const Plot&, std::string_view, std::size_t)>;

/** THE PLOT: @p frame, and @p layers over it in the order they were
 *  written, each filling the plot's own box.
 *
 *  IT DOES NOT PLACE OR SIZE ITSELF. Every layer is absolute against the
 *  plot's box, so the box is the caller's to give — a width and a height,
 *  a rect, or a grow in the row it stands in — and a plot with no size is
 *  a plot with no pixels. @p key names the plot and every recording under
 *  it; two plots on one sheet need two keys.
 *
 *      kit::plot("s", frame, {kit::axis({.of = kit::Axis::Y}),
 *                             kit::trace(exact)}).width(324).height(64)
 */
[[nodiscard]] compose::Element plot(std::string_view key, const Plot& frame,
                                    std::vector<Layer> layers);

// ---------------------------------------------------------------------------
// The layers that draw a path: keyed recordings, mapped at paint

/** The leaf a tick's number defaults to: @p words in the class `plotTick`, so
 *  the sheet in force sets it — and so a tick and the number under it are
 *  one mark taking one colour. */
[[nodiscard]] inline compose::Element tickLabel(const compose::Utf8& words) {
  return compose::text(words).styleClass("plotTick");
}

/** HOW AN AXIS IS DRAWN: which of the frame's two scales it measures,
 *  where it stands on the other one, and what it ticks.
 *
 *  ON A POLAR FRAME an x axis is the RIM and the spokes that divide it,
 *  and a y axis is the rings and one spoke to number them along — which
 *  is why `at` defaults to the far end of the other scale for a polar x
 *  and to its near end everywhere else: a wheel is read at its rim. */
struct Ruler {
  Axis of = Axis::X;
  /** Where the line stands, as a value on the OTHER scale. Unset is that
   *  scale's low end — the bottom for a Cartesian x axis, the left for a
   *  Cartesian y one, the first spoke for a polar y — and its high end
   *  for a polar x, which is the rim. */
  std::optional<double> at;
  /** The domain values ticked. Empty asks the scale for a readable ladder
   *  of about `count`, which for a band, point or ordinal scale is every
   *  entry. */
  std::vector<double> ticks;
  int count = 5;
  /** Whether the axis draws its own line — the axis without one is a
   *  ladder of ticks, which is what an inner axis usually is. */
  bool line = true;
  float width = 1.0f;
  /** How far a tick reaches past the line, px; unset is the theme's tick
   *  reach. */
  std::optional<float> reach;
  /** Between a tick's far end and its number, px. */
  float gap = 3.0f;
  /** Whether the ticks are numbered. */
  bool numbers = true;
  /** THE NUMBER AT ONE TICK, as a function of the VALUE — because how a
   *  number reads is the data's business and not the kit's — and then of
   *  this ruler; a part takes the parameters it names. Empty is the value
   *  to three significant figures in the class `plotTick`. */
  compose::kit::Part<double, Ruler> tickLine;
};

/** THE AXIS — its line in the class `plotAxis`, its ticks and their numbers in
 *  the class `plotTick`. */
[[nodiscard]] Layer axis(const Ruler& how);

/** HAIRLINES ACROSS THE FIELD at the domain values they name: the values
 *  a curve is READ AGAINST, in the domain's own units rather than in
 *  fractions, because a rule at one time constant, at 1.0, at the clock's
 *  own step rate is what makes a drawn number checkable.
 *
 *  On a polar frame an x rule is a spoke and a y rule a ring. */
struct Rules {
  /** Domain values on the x scale: a line up the box at each. */
  std::vector<double> x;
  /** Domain values on the y scale: a line across the box at each. */
  std::vector<double> y;
  float width = 1.0f;
  /** The class read instead of `rule`, for a second ladder of its own. */
  std::string styleClass;
};

/** THE RULES, in the class `plotRule`. */
[[nodiscard]] Layer rules(const Rules& how);

/** A FUNCTION OF ONE VARIABLE, over the x domain.
 *
 *  The curve is a callable rather than a table because what is being
 *  looked at is usually an expression — an ease, a decay, a spring walked
 *  from its own state, r = f(θ) round a wheel — and pinning it to samples
 *  would be pinning the answer instead of asking the question. */
struct Trace {
  float width = 1.5f;
  /** How finely the curve is walked. A staircase reads as a staircase
   *  only when the sampling is finer than its steps. */
  int samples = 240;
  /** Domain values DOTTED on the curve — the published samples a
   *  reference quotes, which is what turns a drawn curve into a check of
   *  one. */
  std::vector<double> marks;
  float markRadius = 2.5f;
  /** The class read instead of `trace`, for a second series. */
  std::string styleClass;
};

/** THE CURVE, in the class `plotTrace`: @p f walked across the x domain and
 *  stroked, with a dot at every marked sample.
 *
 *  It is ONE recording rather than a node per sample, and it prunes on the
 *  plot's key: a callable compares to nothing, so the key is the caller's
 *  statement that this is the same drawing. */
[[nodiscard]] Layer trace(sigil::core::Callable<double(double)> f,
                          const Trace& how = {});

/** THE BAND BETWEEN A CURVE AND A BASE. */
struct Area {
  /** The y the band is closed back along. */
  double base = 0.0;
  int samples = 240;
  /** The class read instead of `area`, for a second band. */
  std::string styleClass;
};

/** THE AREA, filled in the class `plotArea`. */
[[nodiscard]] Layer area(sigil::core::Callable<double(double)> f,
                         const Area& how = {});

// ---------------------------------------------------------------------------
// The layers that place elements: containers whose scheme puts each child
// at its datum's position, so a mark is a real element — keyed, animatable
// and hit-testable

/** WHERE A PLACED ELEMENT SITS relative to the point the frame maps its
 *  datum to.
 *
 *  `Align::Center` centres it on the point, `Start` puts its near edge
 *  there and `End` its far edge, per axis, the way a grid cell's alignment
 *  reads. `offset` moves it after that, px — ACROSS and then OUTWARD on a
 *  polar frame, where across is the tangent at that angle and outward runs
 *  from the hub, which is what puts a word on a rim. */
struct Anchor {
  compose::Align across = compose::Align::Center;
  compose::Align down = compose::Align::Center;
  SkVector offset{0, 0};
  bool operator==(const Anchor&) const = default;
};

/** HOW A ROW'S TWO NUMBERS ARE READ, and where its mark stands.
 *
 *  Each reader is a pointer to a member (`&Row::rate`) or any callable
 *  answering a number. An unset reader answers the row's INDEX, which is
 *  what a band, point or ordinal scale takes, so a plot of a column
 *  against its categories names one field and not two. */
template <class Row>
struct Marks {
  std::function<double(const Row&)> x;
  std::function<double(const Row&)> y;
  Anchor anchor;
  /** The class read instead of `mark`, for a second series. */
  std::string styleClass;
};

/** THE MARKS — one child per row of @p rows, @p mark built from the row
 *  and its index, each placed where the frame maps that row's datum.
 *
 *  The container is in the class `plotMark`, and the ink inherits, so a child
 *  written as `box().fill(Fill::currentInk())` or `stroke(1.0f)` is
 *  painted in the class's colour without naming one.
 *
 *      kit::marks(lab, dot, {.x = &Sample::a, .y = &Sample::b})
 */
template <std::ranges::input_range R, class Row = std::ranges::range_value_t<R>>
[[nodiscard]] Layer marks(
    R&& rows, std::type_identity_t<compose::kit::Part<Row, std::size_t>> mark,
    const std::type_identity_t<Marks<Row>>& how = {});

/** HOW THE BAND A DATUM OWNS IS DRAWN OUT to that datum's value: a bar on
 *  a Cartesian frame, a wedge on a polar one.
 *
 *  It is the band the x scale hands each entry, filled from `base` to the
 *  value the y scale places — so a band x scale with a square-root y scale
 *  is the polar-area diagram, and the same two props in a Cartesian frame
 *  are a column chart. Readers are as `Marks`'s: unset x is the row's
 *  index. */
template <class Row>
struct Bands {
  std::function<double(const Row&)> x;
  std::function<double(const Row&)> y;
  /** The y the band grows FROM. */
  double base = 0.0;
  float corners = 0.0f;
  /** THE ELEMENT ONE DATUM IS DRAWN AS, as a function of its index and
   *  then of its value; a part takes the parameters it names. Empty is a
   *  box filled in the ink in force. On a polar frame the layer puts the
   *  wedge's own shape on whatever this answers, so a part that states a
   *  shape of its own is overruled there. */
  compose::kit::Part<std::size_t, double> part;
  /** The class read instead of `bar`, for a second series. */
  std::string styleClass;
};

/** THE BANDS — one child per row, in the class `plotBar`.
 *
 *      kit::bands(months, {.y = &Month::disease})
 */
template <std::ranges::input_range R, class Row = std::ranges::range_value_t<R>>
[[nodiscard]] Layer bands(R&& rows,
                          const std::type_identity_t<Bands<Row>>& how = {});

/** HOW A WORD PLACED IN THE FIELD STANDS. */
struct Label {
  Anchor anchor;
  /** The class read instead of `label` — a word in the colour of the
   *  series it names says so with that series' own class. */
  std::string styleClass;
};

/** A WORD AT A POINT OF THE FIELD, in the class `plotLabel` — what names a
 *  curve, a region or one reading, placed through the same mapping the
 *  drawing is, so it lands ON the thing it names.
 *
 *      kit::label("s_exact", 1.6, 0.2)
 */
[[nodiscard]] Layer label(compose::Utf8 words, double x, double y,
                          const Label& how = {});

// ---------------------------------------------------------------------------
// What the two placing templates funnel into: the data as domain values and
// the children as elements, with the frame still to come.

namespace detail {

/** The layer that places @p children, one per datum of @p data, each
 *  anchored on the point the frame maps that datum to; @p word names the
 *  part in the container's key, @p styleClass dresses it. */
[[nodiscard]] Layer anchored(std::vector<Datum> data,
                             std::vector<compose::Element> children,
                             const Anchor& anchor, std::string_view word,
                             std::string_view styleClass);
/** @p stated, or @p own where a layer named no class of its own. */
[[nodiscard]] inline std::string_view classOf(const std::string& stated,
                                              std::string_view own) {
  return stated.empty() ? own : std::string_view(stated);
}

/** The layer that draws @p data's bands out from @p base; @p word names
 *  the part in each band's key, @p styleClass dresses it. */
[[nodiscard]] Layer banded(std::vector<Datum> data, double base, float corners,
                           compose::kit::Part<std::size_t, double> part,
                           std::string_view word, std::string_view styleClass);

/** @p read of @p row, or @p index where no reader was named. */
template <class Row>
double number(const std::function<double(const Row&)>& read, const Row& row,
              std::size_t index) {
  return read ? read(row) : (double)index;
}

}  // namespace detail

template <std::ranges::input_range R, class Row>
Layer marks(R&& rows,
            std::type_identity_t<compose::kit::Part<Row, std::size_t>> mark,
            const std::type_identity_t<Marks<Row>>& how) {
  std::vector<Datum> data;
  std::vector<compose::Element> children;
  std::size_t index = 0;
  for (const Row& row : rows) {
    data.push_back(
        {detail::number(how.x, row, index), detail::number(how.y, row, index)});
    children.push_back(mark ? mark(row, index) : compose::box());
    ++index;
  }
  return detail::anchored(std::move(data), std::move(children), how.anchor,
                          "mark", detail::classOf(how.styleClass, "plotMark"));
}

template <std::ranges::input_range R, class Row>
Layer bands(R&& rows, const std::type_identity_t<Bands<Row>>& how) {
  std::vector<Datum> data;
  std::size_t index = 0;
  for (const Row& row : rows) {
    data.push_back(
        {detail::number(how.x, row, index), detail::number(how.y, row, index)});
    ++index;
  }
  return detail::banded(std::move(data), how.base, how.corners, how.part, "bar",
                        detail::classOf(how.styleClass, "plotBar"));
}

}  // namespace sigil::sketch::kit
