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

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/compute/Hash.h>
#include <sigilcore/compute/Noise.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace noise = sigil::core::noise;
namespace core = sigil::core;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 166;
constexpr float kPicture = 176;

constexpr uint32_t kSeed = 20260903;  // the seed every field is drawn from
constexpr float kBlock = 4;           // px one sample is drawn at
constexpr int kCells = 6;             // the lattice cell, in samples

constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

/** The house sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
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
  return custom(key,
                [sample = std::move(sample)](SkCanvas& canvas,
                                             const PaintContext& pc) {
                  SkPaint paint;
                  paint.setAntiAlias(false);
                  const int columns = (int)(pc.size.width() / kBlock);
                  const int rows = (int)(pc.size.height() / kBlock);
                  for (int y = 0; y < rows; ++y)
                    for (int x = 0; x < columns; ++x) {
                      const float v = sample(x, y);
                      paint.setColor4f(
                          {kFigure.fR * v, kFigure.fG * v, kFigure.fB * v, 1});
                      canvas.drawRect({x * kBlock, y * kBlock, (x + 1) * kBlock,
                                       (y + 1) * kBlock},
                                      paint);
                    }
                })
      .absolute()
      .inset(0);
}

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture})
          .child(std::move(body)));
}

}  // namespace

struct NoiseShelf final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the shelf is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const int columns = (int)(kCell / kBlock);

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("THE MIXERS \xc2\xb7 core::noise hash, "
                       "Mix64Stream, pcgHash, xorshift, lattice, and "
                       "core::hash"),
         .subtitle = toU8("dials \xc2\xb7 one seed for the whole shelf "
                          "\xc2\xb7 four px a sample \xc2\xb7 the "
                          "lattice's cell (6 samples) \xc2\xb7 what the "
                          "key is folded over"),
         .footer = toU8("each of these is a bit-exact function of its "
                        "inputs on every platform, which is what lets a "
                        "shader agree with a CPU preview to the bit "
                        "\xe2\x80\x94 and why the constants and the "
                        "shift schedules are not tuning knobs")},
        kit::cells(
            {.cells = {cell("noise::hash(seed, i)",
                            "the 64-bit avalanche squeezed to a unit float "
                            "\xc2\xb7 indexed by a counter, here the sample's "
                            "own number",
                            field("hash",
                                  [columns](int x, int y) {
                                    return noise::hash(
                                        kSeed, (uint32_t)(y * columns + x));
                                  })),
                       cell("Mix64Stream(seed).unit()",
                            "the same avalanche as a STREAM \xc2\xb7 one "
                            "state stepped by the gamma, so successive draws "
                            "are uncorrelated rather than merely different",
                            field("mix64",
                                  [columns](int x, int y) {
                                    noise::Mix64Stream stream(
                                        kSeed + (uint64_t)(y * columns + x));
                                    return stream.unit();
                                  })),
                       cell("noise::pcgUnit(x)",
                            "the PCG word, which the point-operator compute "
                            "kernel reproduces word for word \xc2\xb7 what "
                            "new code takes",
                            field("pcg",
                                  [columns](int x, int y) {
                                    return noise::pcgUnit(
                                        kSeed + (uint32_t)(y * columns + x));
                                  })),
                       cell("xorshiftUnitNext(state)",
                            "the xorshift step, walked from one state down "
                            "the field \xc2\xb7 a different mixer with a "
                            "different output, kept for the renders seeded "
                            "by it",
                            field("xorshift",
                                  [columns](int x, int y) {
                                    uint32_t state =
                                        kSeed +
                                        (uint32_t)(y * columns + x) * 7u;
                                    noise::xorshiftUnitNext(state);
                                    return noise::xorshiftUnitNext(state);
                                  })),
                       cell("lattice(seed, x, y, 0)",
                            "indexed by a grid POSITION rather than a counter "
                            "\xc2\xb7 what value noise asks at each corner of "
                            "a cell, drawn here one draw per cell",
                            field("lattice",
                                  [](int x, int y) {
                                    const uint32_t h = noise::lattice(
                                        kSeed, x / kCells, y / kCells, 0);
                                    return (float)(h >> 8u) *
                                           (1.0f / 16777216.0f);
                                  })),
                       keys()},
             .gap = 10})));
  }

  /** THE KEY, not a field: the fold a cache address is built out of, over
   *  a word and over text, and what a second fold does to the first. */
  Element keys() {
    const uint64_t a = core::hash::fnv1a(core::hash::kFnvOffset, uint64_t{7});
    const uint64_t text =
        core::hash::fnv1a(core::hash::kFnvOffset, std::string_view("stamp"));
    const uint64_t both = core::hash::fnv1a(text, uint64_t{7});
    const size_t mixed = core::hash::combine(0, 7u);
    Element column = box().column().gap(8);
    for (const std::string& row :
         {kit::formatted("fnv1a(offset, 7)"),
          kit::formatted("  %016llx", (unsigned long long)a),
          kit::formatted("fnv1a(offset, \"stamp\")"),
          kit::formatted("  %016llx", (unsigned long long)text),
          kit::formatted("fnv1a(that, 7)"),
          kit::formatted("  %016llx", (unsigned long long)both),
          kit::formatted("combine(0, 7)"),
          kit::formatted("  %016llx", (unsigned long long)mixed)})
      column.child(text_(row));
    return cell("fnv1a \xc2\xb7 combine",
                "one-way folds over a word and over text \xc2\xb7 an address "
                "and not a field, which is why nothing here is drawn",
                std::move(column).absolute().inset(10));
  }

  Element text_(const std::string& row) {
    return text(toU8(row), sketch::kit::theme().mono(9.5f, kFigure));
  }
};

SIGIL_SKETCH(NoiseShelf, "Specimen",
             "every mixer the core ships as one field each, drawn from one "
             "seed, beside the fold a cache key is built out of")
