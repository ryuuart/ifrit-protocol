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

// TAGS: Materials/Compositing, Media/Images

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Atlas.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <optional>
#include <string>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace material = sigil::material;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 880};
constexpr float kCell = 328;
constexpr float kPicture = 208;

constexpr int kCols = 4, kRows = 2;  // the grid the sheet is cut on
constexpr int kCellSide = 64;        // one cell's pixels
constexpr size_t kPlayhead = 6;      // the frame index the wrap cell reads

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  return look;
}

/** The sheet: eight cells, each a numbered wedge sweeping a little
 *  further round, so a sequence read in the wrong order is obvious.
 *
 *  Built where it is asked for; the sketch holds the one it draws with. A
 *  producer bakes on first use PER TEXTURE, so one made inside a draw
 *  callback rasterises a fresh sheet on every paint, and one held in a
 *  static outlives this dylib, which a reload unloads. */
material::Texture buildSheet() {
  return material::Texture::produce("material_atlas.sheet", [] {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(kCols * kCellSide, kRows * kCellSide));
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SkColor4f{0.10f, 0.12f, 0.16f, 1}.toSkColor());
    for (int i = 0; i < kCols * kRows; ++i) {
      const SkRect cell = arrange::cellRect(arrange::cellAt((size_t)i, kCols),
                                            {kCellSide, kCellSide});
      const float x = cell.fLeft, y = cell.fTop;
      SkPaint back;
      back.setColor4f({0.14f + 0.02f * (float)i, 0.16f, 0.22f, 1});
      canvas->drawRect(
          SkRect::MakeXYWH(x + 2, y + 2, kCellSide - 4, kCellSide - 4), back);
      SkPaint wedge;
      wedge.setAntiAlias(true);
      wedge.setColor4f(
          {0.98f, 0.72f - 0.05f * (float)i, 0.30f + 0.07f * (float)i, 1});
      canvas->drawArc(
          SkRect::MakeXYWH(x + 10, y + 10, kCellSide - 20, kCellSide - 20), -90,
          45.0f * (float)(i + 1), true, wedge);
    }
    return surface->makeImageSnapshot();
  });
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

std::string sequenceNames(const material::Atlas& atlas) {
  std::string names;
  for (const auto& [name, frames] : atlas.sequences()) {
    if (!names.empty()) names += ", ";
    names += name + " (" + std::to_string(frames.size()) + ")";
  }
  return names.empty() ? std::string("none") : names;
}

sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const std::string& note,
                                 std::function<void(SkCanvas&)> draw) {
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture},
              custom(call,
                     [draw = std::move(draw)](SkCanvas& canvas) {
                       if (draw) draw(canvas);
                     })),
          .note = note};
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
  canvas.drawRect(SkRect::MakeWH((float)source.width(), (float)source.height()),
                  paint);
  canvas.restore();
}

}  // namespace

struct MaterialAtlas {
  /** The one sheet everything on the page is cut from, held for the
   *  sketch's life so the three atlases and the picture of the whole thing
   *  are readings of the SAME bake. */
  material::Texture sheet = buildSheet();

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const material::Atlas grid = material::Atlas::grid(sheet, kCols, kRows);
    const std::optional<material::Atlas> packed =
        material::Atlas::fromTexturePacker(sheet, kTexturePackerJson);
    const std::optional<material::Atlas> tagged =
        material::Atlas::fromAseprite(sheet, kAsepriteJson);

    // What a cell whose JSON did not parse draws.
    const std::function<void(SkCanvas&)> nothing;

    const auto strip = [](const material::Atlas& atlas, const char* sequence,
                          size_t count,
                          size_t from) -> std::function<void(SkCanvas&)> {
      return [atlas, sequence, count, from](SkCanvas& canvas) {
        const float side = (kCell - 20) / (float)count;
        for (size_t i = 0; i < count; ++i)
          put(canvas, atlas.frame(sequence, from + i),
              SkRect::MakeXYWH(10 + (float)i * side, (kPicture - side) * 0.5f,
                               side - 4, side - 4),
              {kCellSide, kCellSide});
      };
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "A sheet becomes a sequence",
         .subtitle = "Regions select pixels; metadata supplies a reading order",
         .footer = "Each frame is an ordinary texture region. Sequence lookup "
                   "wraps the index for the caller."},
        box().column().gap(28).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  CUT BY POSITION",
                  .note = "One sheet feeds a continuous, row-major sequence."}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("THE SOURCE SHEET", "the sheet, whole",
                            "Eight wedges progress by 45° in a four-by-two "
                            "sheet.",
                            [sheet = sheet](SkCanvas& canvas) {
                              put(canvas, sheet,
                                  SkRect::MakeXYWH(10, (kPicture - 154) * 0.5f,
                                                   308, 154),
                                  {kCols * kCellSide, kRows * kCellSide});
                            }),
                       cell("FIRST FOUR FRAMES", "Atlas::grid(sheet, 4, 2)",
                            "Row-major indices 0–3.", strip(grid, "all", 4, 0)),
                       cell("NEXT FOUR FRAMES", "… the second row of it",
                            "Continue the same sequence at index 4.",
                            strip(grid, "all", 4, 4))},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::sectionHeader(
                 {.label = "02  READ THE AUTHORING METADATA",
                  .note = "Names and tags group the same pixels; playback "
                          "wraps."}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("NAME STEM → WALK",
                            "Atlas::fromTexturePacker(sheet, json)",
                            "TexturePacker derives walk from the numbered name "
                            "stem.",
                            packed ? strip(*packed, "walk", 4, 0) : nothing),
                       cell("FRAME TAG → SHUT",
                            "Atlas::fromAseprite(sheet, json)",
                            "Aseprite derives shut from its frame tag.",
                            tagged ? strip(*tagged, "shut", 4, 0) : nothing),
                       cell("WRAPPING PLAYHEAD",
                            "frame(\"walk\", 6) · wrapping",
                            "Playhead 6 wraps a four-frame run to 2, 3, 0, 1.",
                            packed ? strip(*packed, "walk", 4, kPlayhead)
                                   : nothing)},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(MaterialAtlas, "Kit · API",
             "one sheet cut three ways — by grid, by "
             "TexturePacker's name stems and by Aseprite's frame tags "
             "— and a playhead that wraps past the end")
