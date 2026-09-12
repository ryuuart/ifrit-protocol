/** One Scale prop maps a value, draws its axis and sizes its marks. */
// TAGS: Data/Scales

#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildata/scale/Scale.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <array>

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

Element mapping(const Mapping& properties) {
  const bool categories =
      properties.scale.transform == data::Transform::Ordinal ||
      properties.scale.transform == data::Transform::Band ||
      properties.scale.transform == data::Transform::Point;
  const SkRect area = SkRect::MakeLTRB(24, 16, kWidth - 24, kHeight - 28);
  auto body = box().width(kWidth).height(kHeight).fill(
      Fill::color({0.07f, 0.09f, 0.12f, 1}));
  std::vector<kit::Trace> traces;
  if (!categories)
    traces.push_back({[scale = properties.scale](float unit) {
                        const data::Scale inputs{.range = scale.domain};
                        return (float)scale.position(inputs(unit));
                      },
                      kInk, 2.5f});
  body.child(box()
                 .left(area.left())
                 .top(area.top())
                 .width(area.width())
                 .height(area.height())
                 .child(kit::curvePlot(properties.title, std::move(traces),
                                       {.samples = 256,
                                        .rulesY = {0, 0.25f, 0.5f, 0.75f, 1},
                                        .rule = {0.20f, 0.25f, 0.29f, 1}})));
  if (categories)
    body.child(custom(std::string(properties.title) + "-marks",
                      [scale = properties.scale, area](SkCanvas& canvas,
                                                       const PaintContext&) {
                        SkPaint pen;
                        pen.setAntiAlias(true);
                        pen.setColor4f(kInk);
                        data::Scale positions = scale;
                        positions.range = {area.left(), area.right()};
                        for (int i = 0; i < positions.steps; ++i) {
                          const float x = (float)positions(i);
                          const float width = (float)positions.bandwidth();
                          if (width > 0)
                            canvas.drawRect(
                                SkRect::MakeXYWH(x, area.top() + 30, width, 52),
                                pen);
                          else
                            canvas.drawCircle(x, area.centerY(), 7, pen);
                        }
                      })
                   .inset(0));

  const auto label = sigil::weave::textStyle(
      {.size = 11, .color = SkColor4f{0.64f, 0.70f, 0.76f, 1}});
  data::Scale axis = properties.scale;
  if (!categories && axis.transform != data::Transform::Time)
    axis.transform = data::Transform::Linear;
  axis.range = {area.left(), area.right()};
  for (double tick : axis.ticks(4)) {
    const float x = categories
                        ? (float)(axis(tick) + axis.bandwidth() * 0.5)
                        : area.left() + (float)((tick - axis.domain.low) /
                                                axis.domain.extent()) *
                                            area.width();
    body.child(text(toUtf8(kit::formatted("%.3g", tick)), label)
                   .left(x - 20)
                   .top(kHeight - 20)
                   .width(40)
                   .textAlign(sigil::weave::TextAlignment::kCenter));
  }
  return sketch::kit::caption(kWidth, toUtf8(properties.title),
                              toUtf8(properties.note), std::move(body));
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
    ctx.composer.render(sketch::kit::page(
        {.title = u8"DATA INTO MOTION AND MARKS",
         .subtitle =
             u8"One mapping value · eleven transforms · reusable properties",
         .footer = u8"Curves: input along the bottom, normalized output "
                   u8"upward. Category labels are indices."},
        kit::panelGrid({.cells = std::move(cells), .columns = 3, .gap = 16})));
  }
};
}  // namespace

SIGIL_SKETCH(DataScales, "Data",
             "Every scale transform through one reusable mapping component.")
