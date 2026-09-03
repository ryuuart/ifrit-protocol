/** @file
 * material_atlas — a sheet, its regions and its sequences, from a grid
 * and from the two tools' JSON.
 *
 * An `Atlas` is three things at once. A SHEET, which is an ordinary
 * texture. A list of REGIONS, each with the pixels it occupies on that
 * sheet and — for a sprite the tool trimmed — the untrimmed size and the
 * offset the kept pixels sit at inside it. And named SEQUENCES of region
 * indices, which is what makes a run of frames playable.
 *
 * The two importers derive their sequences differently, and the
 * difference is the tools': TexturePacker has no notion of an animation,
 * so a sequence is derived per NAME STEM — `walk_01`, `walk_02` become
 * "walk", in numeric order. Aseprite has frame tags, so a sequence is
 * derived per TAG, and only when there are none does one sequence "all"
 * stand for the sheet.
 *
 * `region(name)` is the sheet texture cut to that region, so a region is
 * an ordinary texture and needs no second sampling path. `frame(sequence,
 * index)` wraps past the end, which is what makes a frame counter a
 * counter rather than a modulus at every call site.
 *
 * The JSON here is written out in this file rather than loaded, so the
 * sheet is a function of the numbers above it and of nothing on disk.
 *
 * EDIT THESE FIRST
 *   kCols, kRows — the grid the sheet is baked and cut on.
 *   kCellSide    — one cell's pixels.
 *   kPlayhead    — the frame index the wrapping cell reads.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Atlas.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkSurface.h>

#include <cstdio>
#include <functional>
#include <optional>
#include <string>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 660};
constexpr float kCell = 341;
constexpr float kPicture = 200;

constexpr int kCols = 4, kRows = 2;  // the grid the sheet is cut on
constexpr int kCellSide = 64;        // one cell's pixels
constexpr size_t kPlayhead = 6;      // the frame index the wrap cell reads

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.09f, 0.095f, 0.11f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

/** The sheet: eight cells, each a numbered wedge sweeping a little
 *  further round, so a sequence read in the wrong order is obvious. */
const material::Texture& sheet() {
  static const material::Texture texture =
      material::Texture::produce("material_atlas.sheet", [] {
        sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
            kCols * kCellSide, kRows * kCellSide));
        SkCanvas* canvas = surface->getCanvas();
        canvas->clear(SkColor4f{0.10f, 0.12f, 0.16f, 1}.toSkColor());
        for (int i = 0; i < kCols * kRows; ++i) {
          const float x = (float)(i % kCols) * kCellSide;
          const float y = (float)(i / kCols) * kCellSide;
          SkPaint back;
          back.setColor4f({0.14f + 0.02f * (float)i, 0.16f, 0.22f, 1});
          canvas->drawRect(
              SkRect::MakeXYWH(x + 2, y + 2, kCellSide - 4, kCellSide - 4),
              back);
          SkPaint wedge;
          wedge.setAntiAlias(true);
          wedge.setColor4f({0.98f, 0.72f - 0.05f * (float)i,
                            0.30f + 0.07f * (float)i, 1});
          canvas->drawArc(
              SkRect::MakeXYWH(x + 10, y + 10, kCellSide - 20, kCellSide - 20),
              -90, 45.0f * (float)(i + 1), true, wedge);
        }
        return surface->makeImageSnapshot();
      });
  return texture;
}

/** TexturePacker's hash form, with two name stems so two sequences fall
 *  out of the names alone. */
constexpr char kTexturePackerJson[] = R"({
  "frames": {
    "walk_01": {"frame": {"x":0,"y":0,"w":64,"h":64}},
    "walk_02": {"frame": {"x":64,"y":0,"w":64,"h":64}},
    "walk_03": {"frame": {"x":128,"y":0,"w":64,"h":64}},
    "walk_04": {"frame": {"x":192,"y":0,"w":64,"h":64}},
    "idle_01": {"frame": {"x":0,"y":64,"w":64,"h":64}},
    "idle_02": {"frame": {"x":64,"y":64,"w":64,"h":64}}
  },
  "meta": {"size": {"w":256,"h":128}}
})";

/** Aseprite's array form with frame tags, so the sequences come from the
 *  TAGS and the names carry nothing. */
constexpr char kAsepriteJson[] = R"({
  "frames": [
    {"filename":"0","frame":{"x":0,"y":0,"w":64,"h":64}},
    {"filename":"1","frame":{"x":64,"y":0,"w":64,"h":64}},
    {"filename":"2","frame":{"x":128,"y":0,"w":64,"h":64}},
    {"filename":"3","frame":{"x":192,"y":0,"w":64,"h":64}},
    {"filename":"4","frame":{"x":0,"y":64,"w":64,"h":64}},
    {"filename":"5","frame":{"x":64,"y":64,"w":64,"h":64}},
    {"filename":"6","frame":{"x":128,"y":64,"w":64,"h":64}},
    {"filename":"7","frame":{"x":192,"y":64,"w":64,"h":64}}
  ],
  "meta": {
    "size": {"w":256,"h":128},
    "frameTags": [
      {"name":"open","from":0,"to":3},
      {"name":"shut","from":4,"to":7}
    ]
  }
})";

std::string line(const char* format, auto... args) {
  char buffer[256];
  std::snprintf(buffer, sizeof buffer, format, args...);
  return buffer;
}

std::string sequenceNames(const material::Atlas& atlas) {
  std::string names;
  for (const auto& [name, frames] : atlas.sequences()) {
    if (!names.empty()) names += ", ";
    names += name + " (" + std::to_string(frames.size()) + ")";
  }
  return names.empty() ? std::string("none") : names;
}

Element cell(const char* call, const std::string& note,
             std::function<void(SkCanvas&)> draw) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   custom(call,
                          [draw = std::move(draw)](SkCanvas& canvas,
                                                   const PaintContext&) {
                            if (draw) draw(canvas);
                          })
                       .width(kCell)
                       .height(kPicture)
                       .clip()
                       .fill(Fill::color(kCellGround)));
}

/** A texture drawn at a stated rect, which is how a region is used: it
 *  is an ordinary texture and needs no second sampling path. */
void put(SkCanvas& canvas, const material::Texture& texture, SkRect where,
         SkISize source) {
  if (source.isEmpty()) return;
  SkPaint paint;
  paint.setShader(texture.shader());
  canvas.save();
  canvas.translate(where.x(), where.y());
  canvas.scale(where.width() / (float)source.width(),
               where.height() / (float)source.height());
  canvas.drawRect(SkRect::MakeWH((float)source.width(),
                                 (float)source.height()),
                  paint);
  canvas.restore();
}

}  // namespace

struct MaterialAtlas final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    const material::Atlas grid =
        material::Atlas::grid(sheet(), kCols, kRows);
    const std::optional<material::Atlas> packed =
        material::Atlas::fromTexturePacker(sheet(), kTexturePackerJson);
    const std::optional<material::Atlas> tagged =
        material::Atlas::fromAseprite(sheet(), kAsepriteJson);

    const auto strip = [](const material::Atlas& atlas, const char* sequence,
                          size_t count,
                          size_t from) -> std::function<void(SkCanvas&)> {
      return [atlas, sequence, count, from](SkCanvas& canvas) {
        const float side = (kCell - 20) / (float)count;
        for (size_t i = 0; i < count; ++i)
          put(canvas, atlas.frame(sequence, from + i),
              SkRect::MakeXYWH(10 + (float)i * side,
                               (kPicture - side) * 0.5f, side - 4, side - 4),
              {kCellSide, kCellSide});
      };
    };

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("MATERIAL ATLAS \xc2\xb7 Atlas grid, "
                           "fromTexturePacker, fromAseprite, region, frame"),
             .subtitle = toU8(line(
                 "dials \xc2\xb7 the grid (%d by %d of %d px) \xc2\xb7 the "
                 "source JSON \xc2\xb7 the sequence \xc2\xb7 the playhead "
                 "(%zu, past the end of a four-frame run)",
                 kCols, kRows, kCellSide, kPlayhead)),
             .footer = toU8("a region is an ordinary texture cut from the "
                            "sheet, so a sprite needs no second sampling "
                            "path \xe2\x80\x94 and frame() wraps, so a "
                            "playhead is a counter and not a modulus at "
                            "every call site"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {kit::cells(
                          {.cells =
                               {cell("the sheet, whole",
                                     line("%d by %d cells of %d px \xc2\xb7 "
                                          "each wedge sweeps 45\xc2\xb0 "
                                          "further than the last, so a run "
                                          "read out of order shows it",
                                          kCols, kRows, kCellSide),
                                     [](SkCanvas& canvas) {
                                       put(canvas, sheet(),
                                           SkRect::MakeXYWH(
                                               10, (kPicture - 160) * 0.5f,
                                               320, 160),
                                           {kCols * kCellSide,
                                            kRows * kCellSide});
                                     }),
                                cell("Atlas::grid(sheet, 4, 2)",
                                     line("equal cells, row-major, named by "
                                          "index \xc2\xb7 sequences: %s",
                                          sequenceNames(grid).c_str()),
                                     strip(grid, "all", 4, 0)),
                                cell("\xe2\x80\xa6" " the second row of it",
                                     "the same sequence read from index 4 "
                                     "\xc2\xb7 one list of indices, and the "
                                     "caller says where in it to start",
                                     strip(grid, "all", 4, 4))},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {cell("Atlas::fromTexturePacker(sheet, json)",
                                     packed ? line("a sequence per NAME STEM "
                                                   "\xc2\xb7 %s \xc2\xb7 "
                                                   "walk_01\xe2\x80\xa6"
                                                   "walk_04 in numeric order",
                                                   sequenceNames(*packed)
                                                       .c_str())
                                            : "not that JSON",
                                     packed ? strip(*packed, "walk", 4, 0)
                                            : std::function<void(SkCanvas&)>{}),
                                cell("Atlas::fromAseprite(sheet, json)",
                                     tagged ? line("a sequence per frame TAG "
                                                   "\xc2\xb7 %s \xc2\xb7 the "
                                                   "names carry nothing here",
                                                   sequenceNames(*tagged)
                                                       .c_str())
                                            : "not that JSON",
                                     tagged ? strip(*tagged, "shut", 4, 0)
                                            : std::function<void(SkCanvas&)>{}),
                                cell("frame(\"walk\", 6) \xc2\xb7 wrapping",
                                     packed
                                         ? line("index %zu of a four-frame run "
                                                "\xc2\xb7 past the end "
                                                "wraps, so the strip reads "
                                                "2, 3, 0, 1",
                                                kPlayhead)
                                         : "not that JSON",
                                     packed ? strip(*packed, "walk", 4,
                                                    kPlayhead)
                                            : std::function<void(SkCanvas&)>{})},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(MaterialAtlas, "Kit \xc2\xb7 API",
             "one sheet cut three ways \xe2\x80\x94 by grid, by "
             "TexturePacker's name stems and by Aseprite's frame tags "
             "\xe2\x80\x94 and a playhead that wraps past the end")
