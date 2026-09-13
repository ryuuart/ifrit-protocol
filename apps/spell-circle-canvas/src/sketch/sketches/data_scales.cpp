/** One Scale prop maps a value, draws its axis and sizes its marks. */
// TAGS: Data/Scales

#include <sigilcompose/kit/Specimen.h>
#include <sigildata/scale/Scale.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <utility>
#include <vector>

namespace data = sigil::data;
namespace sketch = sigil::sketch;
using namespace sigil::compose;

namespace {
constexpr float kWidth = 336;
constexpr float kHeight = 158;
constexpr SkColor4f kInk{0.30f, 0.83f, 0.78f, 1};

struct Mapping {
  const char* title;
  const char* note;
  data::Scale scale;
};

/** THE SHEET EVERY CELL IS DRESSED BY: the ladder behind the curve, the
 *  tick numbers as figures rather than remarks — untracked, in the font
 *  context's own family — and the mapping itself in one accent, whether it
 *  is drawn as a curve, as a band or as a point. */
sigil::weave::StyleSheet scaleSheet() {
  sigil::weave::StyleSheet dressed = sketch::kit::houseTheme().styleSheet();
  dressed.set("plotRule", {.color = SkColor4f{0.20f, 0.25f, 0.29f, 1}});
  dressed.set("plotTick", {.face = sigil::weave::defaultFace(),
                           .size = 11,
                           .color = SkColor4f{0.64f, 0.70f, 0.76f, 1},
                           .track = 0});
  dressed.set("plotTrace", {.color = kInk});
  dressed.set("plotBar", {.color = kInk});
  dressed.set("plotMark", {.color = kInk});
  return dressed;
}

/** The dot a category with no width of its own is drawn as. */
Element dot(const sketch::kit::Datum&, std::size_t) {
  return box()
      .width(14)
      .height(14)
      .corners(Corners{7})
      .fill(Fill::currentInk());
}

Element mapping(const Mapping& properties) {
  using enum data::Transform;
  const data::Transform kind = properties.scale.transform;
  const bool categories = kind == Ordinal || kind == Band || kind == Point;
  const SkRect area = SkRect::MakeLTRB(24, 16, kWidth - 24, kHeight - 28);
  // THE FRAME. A continuous mapping puts its own INPUT across the box —
  // read on a linear abscissa, or on the clock's own ladder for a time
  // scale — and its normalised output up it. A discrete one has no input
  // axis: the categories themselves are the abscissa, which is the scale
  // under study standing in for its own frame.
  const sketch::kit::Plot frame{
      .x = categories ? properties.scale
                      : data::Scale{.domain = properties.scale.domain,
                                    .transform = kind == Time ? Time : Linear},
      .y = {.domain = {0, 1}}};
  std::vector<sketch::kit::Layer> layers{
      sketch::kit::rules({.y = {0, 0.25, 0.5, 0.75, 1}}),
      sketch::kit::axis(
          {.of = sketch::kit::Axis::X, .count = 4, .line = false, .reach = 0})};
  if (kind == Band) {
    // A band owns space, so it is drawn as the space it owns.
    std::vector<sketch::kit::Datum> entries;
    for (int i = 0; i < properties.scale.steps; ++i)
      entries.push_back({(double)i, 0.74});
    layers.push_back(sketch::kit::bands(
        entries, {.y = &sketch::kit::Datum::y, .base = 0.28}));
  } else if (categories) {
    // A point and an ordinal entry own a position and no width.
    std::vector<sketch::kit::Datum> entries;
    for (int i = 0; i < properties.scale.steps; ++i)
      entries.push_back({(double)i, 0.5});
    layers.push_back(sketch::kit::marks(
        entries, dot,
        {.x = &sketch::kit::Datum::x, .y = &sketch::kit::Datum::y}));
  } else {
    layers.push_back(sketch::kit::trace(
        [scale = properties.scale](double value) {
          return scale.position(value);
        },
        {.width = 2.5f, .samples = 256}));
  }
  Element body = box()
                     .width(kWidth)
                     .height(kHeight)
                     .fill(Fill::color({0.07f, 0.09f, 0.12f, 1}))
                     .children({sketch::kit::plot(properties.title, frame,
                                                  std::move(layers))
                                    .left(area.left())
                                    .top(area.top())
                                    .width(area.width())
                                    .height(area.height())});
  return sketch::kit::caption(kWidth, properties.title, properties.note,
                              std::move(body));
}

struct DataScales final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    using enum data::Transform;
    const std::array<Mapping, 11> examples{{
        {"Linear",
         "Equal changes travel equal distances.",
         {.domain = {0, 100}}},
        {"Log",
         "Multiplication becomes equal spacing.",
         {.domain = {1, 1000}, .transform = Log}},
        {"Power",
         "An exponent reshapes the response.",
         {.domain = {0, 100}, .transform = Pow, .exponent = 2}},
        {"Square root",
         "Use radius to represent an area.",
         {.domain = {0, 100}, .transform = Sqrt}},
        {"Symmetric log",
         "A logarithmic response through zero.",
         {.domain = {-100, 100}, .transform = Symlog, .threshold = 5}},
        {"Time",
         "Seconds map onto readable clock ticks.",
         {.domain = {0, 120}, .transform = Time}},
        {"Quantize",
         "Equal input bands select five levels.",
         {.domain = {0, 100}, .transform = Quantize, .steps = 5}},
        {"Threshold",
         "Explicit boundaries select four levels.",
         {.domain = {0, 100},
          .transform = Threshold,
          .thresholds = {15, 40, 80}}},
        {"Ordinal",
         "First and last marks pin the endpoints.",
         {.transform = Ordinal, .steps = 5}},
        {"Band",
         "Each category owns space and a gap.",
         {.transform = Band, .steps = 5, .padding = 0.18, .outerPadding = 0.2}},
        {"Point",
         "Categories own positions, without width.",
         {.transform = Point, .steps = 5, .outerPadding = 0.5}},
    }};
    std::vector<Element> cells;
    for (const Mapping& example : examples) cells.push_back(mapping(example));
    sketch::kit::stage(ctx, {.size = {1100, 980}, .captureAt = 0.05});
    ctx.composer.render(
        sketch::kit::page(
            {.title = u8"DATA INTO MOTION AND MARKS",
             .subtitle = u8"One mapping value · eleven transforms · reusable "
                         u8"properties",
             .footer = u8"Curves: input along the bottom, normalized output "
                       u8"upward. Category labels are indices."},
            kit::panelGrid(
                {.cells = std::move(cells), .columns = 3, .gap = 16}))
            .styleSheet(scaleSheet()));
  }
};
}  // namespace

SIGIL_SKETCH(DataScales, "Data",
             "Every scale transform through one reusable mapping component.")
