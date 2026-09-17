/** @file
 * noise_shelf — every mixer the core ships, one field each, and the key
 * a cache is addressed by.
 *
 * These are DIFFERENT MIXERS with different outputs, kept side by side
 * because each seeds work that is compared byte for byte against stored
 * renders. Every one of them is a bit-exact function of its inputs on
 * every platform, so anything seeded by them re-rolls identically: a
 * scattered brush stamp, a roughened outline, a drifted point cloud, a
 * jittered layout.
 *
 * THE CONSTANTS AND THE SHIFT SCHEDULES ARE NOT TUNING KNOBS. Renders
 * stored as bytes are seeded through here and a GPU kernel reproduces
 * `pcgAdvance`, `pcgMix` and `pcgHash` word for word, so changing a
 * constant does not fail a build — it re-rolls every stored render and
 * desynchronises the two ends of every operator chain that runs on both.
 *
 * Pick by what the caller already uses; new code takes `pcgHash`. The
 * one that is not interchangeable with the others is `lattice`: it is
 * indexed by a grid POSITION rather than by a counter, which is what
 * value noise asks at each corner of a cell and what anything indexed by
 * a cell wants for a stable draw.
 *
 * `fnv1a` and `combine` are the other half — not a field but a KEY: the
 * one-way fold a cache address is built out of, over words and over
 * text, and the last cell prints what a few of them come to.
 *
 * EDIT THESE FIRST
 *   kSeed — the seed every field is drawn from.
 *   kBlock — how many px one sample is drawn at.
 *   kCells — the lattice cell, in samples.
 */

// TAGS: Patterns/Noise

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/compute/Hash.h>
#include <sigilcore/compute/Noise.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>
#include <utility>

namespace arrange = sigil::geometry::arrange;
namespace draw = sigil::draw;
namespace sketch = sigil::sketch;
namespace noise = sigil::core::noise;
namespace core = sigil::core;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 880};
constexpr float kCell = 240;
constexpr float kPicture = 208;

constexpr uint32_t kSeed = 20260903;  // the seed every field is drawn from
constexpr float kBlock = 4;           // px one sample is drawn at
constexpr int kCells = 6;             // the lattice cell, in samples

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.captionWhere = kit::Caption::Where::Below;
  look.spacing.captionGap = 8;
  look.spacing.captionNoteGap = 3;
  return look;
}

/** A FIELD: one sample per block, the value read as a grey. The sampler
 *  is handed the sample's own grid position, so a mixer indexed by a
 *  counter and one indexed by a position are drawn the same way and
 *  differ only in what they answer. */
using Field = std::function<float(int, int)>;

Element field(const char* key, Field sample) {
  // A pen program runs after the describe scope has closed, so the
  // field's ink is read here and carried in by value.
  const SkColor4f ink = sketch::kit::theme().palette.figure;
  return pen(key, [sample = std::move(sample), ink](draw::Pen& pen) {
    pen.noStroke();
    pen.noSmooth();
    const int columns = (int)(pen.width / kBlock);
    const int rows = (int)(pen.height / kBlock);
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < columns; ++x) {
        const float v = sample(x, y);
        const SkRect at = arrange::cellRect({x, y}, {kBlock, kBlock});
        pen.fill(SkColor4f{ink.fR * v, ink.fG * v, ink.fB * v, 1});
        pen.rect(at.fLeft, at.fTop, kBlock, kBlock);
      }
  });
}

/** One line of the key column, in the theme's own terminal voice: what a
 *  fold answers is a word of hex and reads as one. */
Element line(const std::string& row) { return text(row).styleClass("readout"); }

/** The plate every specimen on this sheet stands on, and the
 *  measure its caption is set to. */
const sketch::kit::Cell kSpecimen{
    .plate = {.width = kCell, .height = kPicture}};

}  // namespace

struct NoiseShelf {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the shelf is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const int columns = (int)(kCell / kBlock);

    ctx.composer.render(sketch::kit::page(
        {.title = "Random fields and stable addresses",
         .subtitle = "Separate the kind of input from the kind of result",
         .footer = "The constants define reproducible output. They are part of "
                   "the algorithm, rather than visual tuning controls."},
        box().column().gap(30).children(
            {sketch::kit::sectionHeader(
                 {.label = "COUNTER AND STATE",
                  .note =
                      "Same seed · four pixels per sample · equal windows"}),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "HASH / COUNTER",
                        .control = "noise::hash(seed, i)",
                        .figure = sketch::kit::cell(
                            kSpecimen, "", "",
                            field("hash",
                                  [columns](int x, int y) {
                                    return noise::hash(
                                        kSeed, (uint32_t)(y * columns + x));
                                  })),
                        .note = "A 64-bit avalanche reduced to a unit float."},
                       {.title = "MIX64 / STREAM",
                        .control = "Mix64Stream(seed).unit()",
                        .figure = sketch::kit::cell(
                            kSpecimen, "", "",
                            field("mix64",
                                  [columns](int x, int y) {
                                    noise::Mix64Stream stream(
                                        kSeed + (uint64_t)(y * columns + x));
                                    return stream.unit();
                                  })),
                        .note = "A stream state initialized from each sample "
                                "index."},
                       {.title = "PCG / WORD",
                        .control = "noise::pcgUnit(x)",
                        .figure = sketch::kit::cell(
                            kSpecimen, "", "",
                            field("pcg",
                                  [columns](int x, int y) {
                                    return noise::pcgUnit(
                                        kSeed + (uint32_t)(y * columns + x));
                                  })),
                        .note =
                            "A PCG word, shared by CPU and device operators."},
                       {.title = "XORSHIFT / STATE",
                        .control = "xorshiftUnitNext(state)",
                        .figure = sketch::kit::cell(
                            kSpecimen, "", "",
                            field("xorshift",
                                  [columns](int x, int y) {
                                    uint32_t state =
                                        kSeed +
                                        (uint32_t)(y * columns + x) * 7u;
                                    noise::xorshiftUnitNext(state);
                                    return noise::xorshiftUnitNext(state);
                                  })),
                        .note = "Two state advances per sample."}},
                  .measure = 1020,
                  .gap = 20}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "A POSITION INSTEAD", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {{.title = "LATTICE / POSITION",
                                           .control = "lattice(seed, x, y, 0)",
                                           .figure = sketch::kit::cell(
                                               kSpecimen, "", "",
                                               field("lattice",
                                                     [](int x, int y) {
                                                       const uint32_t h =
                                                           noise::lattice(
                                                               kSeed,
                                                               x / kCells,
                                                               y / kCells, 0);
                                                       return (float)(h >> 8u) *
                                                              (1.0f /
                                                               16777216.0f);
                                                     })),
                                           .note = "One draw per lattice cell; "
                                                   "neighbouring samples share "
                                                   "a value."}},
                                .measure = 240})}),
                      box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "A KEY IS A DIFFERENT OUTPUT",
                                .note =
                                    "The fold addresses a cache; it is read as "
                                    "a word, rather than plotted as a field."}),
                           keys()})})})));
  }

  /** THE KEY, not a field: the fold a cache address is built out of, over
   *  a word and over text, and what a second fold does to the first. */
  Element keys() {
    const uint64_t a = core::hash::fnv1a(core::hash::kFnvOffset, uint64_t{7});
    const uint64_t text =
        core::hash::fnv1a(core::hash::kFnvOffset, std::string_view("stamp"));
    const uint64_t both = core::hash::fnv1a(text, uint64_t{7});
    const size_t mixed = core::hash::combine(0, 7u);
    const auto entry = [](const char* label, uint64_t value) {
      return box().row().gap(28).children(
          {sigil::compose::text(label).width(280).styleClass("readout"),
           sigil::compose::text(
               kit::formatted("%016llx", (unsigned long long)value))
               .styleClass("readout")});
    };
    return sketch::kit::well(
        {.width = 760, .height = 208, .padding = 24, .paddingY = 22},
        box().column().gap(16).children(
            {entry("fnv1a(offset, 7)", a),
             entry("fnv1a(offset, \"stamp\")", text),
             entry("fnv1a(previous, 7)", both),
             entry("combine(0, 7)", (uint64_t)mixed)}));
  }
};

SIGIL_SKETCH(NoiseShelf, "Specimen",
             "every mixer the core ships as one field each, drawn from one "
             "seed, beside the fold a cache key is built out of")
