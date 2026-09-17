/** @file
 * Four memoized map chunks share one generated image atlas.
 * Each timed mutation edits one tile; a bound sibling wash marks the edited
 * chunk without invalidating its memo. A separate composer measures the map
 * alone with promotion disabled. The displayed counts belong to the previous
 * completed edit; elapsed timings appear only in a live window.
 */

// TAGS: Geometry/Layout, Runtime/Caching

#include <include/core/SkBitmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace motion = sigil::motion;
namespace ch = choreograph;

using namespace sigil::compose;

namespace {

/** The side every 16 px atlas cell is drawn at. It is a SQUARE only
 *  while the row of chunks gets the width it asks for. */
constexpr float kTile = 27.0f;
constexpr int kChunkCols = 8, kChunkRows = 5, kChunks = 4;
constexpr int kCellsPerChunk = kChunkCols * kChunkRows;
/** Seconds between edits, and the time constant the flash dies on. */
constexpr double kPeriod = 0.7;
constexpr float kFade = 0.45f;

constexpr float kPad = 40.0f;
constexpr float kGap = 20;
constexpr float kCanvasW = 1120;
constexpr float kCanvasH = 710;

constexpr SkColor4f kGround{0.03f, 0.03f, 0.07f, 1};
constexpr SkColor4f kInk{0.84f, 0.87f, 0.94f, 1};
constexpr SkColor4f kAsh{0.55f, 0.60f, 0.70f, 1};
constexpr SkColor4f kRule{0.16f, 0.17f, 0.24f, 1};
/** What a re-recorded chunk is washed in. */
constexpr SkColor4f kFlash{1.0f, 0.58f, 0.20f, 0.55f};

/** This page's look: the map's own near-black, and a header set close
 *  enough to the grid that the chunks keep the width they ask for. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette = {.ground = kGround, .ink = kInk, .ash = kAsh, .rule = kRule};
  look.spacing.marginX = kPad;
  look.spacing.marginTop = kPad * 0.6f;
  look.spacing.marginBottom = kPad * 0.5f;
  look.spacing.contentGap = kGap;
  return look;
}

/** ONE MARK inside a 16 px tile, in the tile's own coordinates. */
struct Mark {
  float x, y, w, h;
  SkColor color;
};
/** ONE TILE: the ground it is laid on and the marks over it. */
struct Tile {
  SkColor ground;
  std::vector<Mark> marks;
};

/** THE TILESET, AS A TABLE — floor, brick wall, moss floor, ember, each a
 *  16 px square. */
const std::array<Tile, 4> kTileset{{
    {0xff181624, {{2, 2, 2, 2, 0xff242034}, {10, 9, 2, 2, 0xff242034}}},
    // four courses of mortar and one head joint
    {0xff5a332c,
     {{0, 3, 16, 1, 0xff3a1f1c},
      {0, 7, 16, 1, 0xff3a1f1c},
      {0, 11, 16, 1, 0xff3a1f1c},
      {0, 15, 16, 1, 0xff3a1f1c},
      {7, 0, 1, 16, 0xff3a1f1c}}},
    {0xff1c2a1e, {{3, 4, 3, 2, 0xff2f492c}, {10, 10, 4, 3, 0xff2f492c}}},
    {0xff2a1218, {{6, 6, 4, 4, 0xffff7a33}, {7, 7, 2, 2, 0xffffc46b}}},
}};

/** THE TILESET BAKED: the table drawn into one 64x16 image, read back
 *  with `image(atlas).region(cell)`, so forty tiles are forty regions of
 *  one image.
 *
 *  ONE ASSET FOR THE WHOLE MAP, held on the sketch. An image node is
 *  compared by asset POINTER, so a fresh bake per tile would make every one
 *  of the forty unequal to itself on the next describe and defeat the memo
 *  this sketch is about. Held there and not in a static, since this file is
 *  a dylib a reload unloads. */
std::shared_ptr<sigil::image::ImageAsset> atlas() {
  sk_sp<SkSurface> sheet =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
  SkCanvas& canvas = *sheet->getCanvas();
  SkPaint paint;
  for (size_t i = 0; i < kTileset.size(); ++i) {
    canvas.save();
    canvas.translate((float)i * 16, 0);
    paint.setColor(kTileset[i].ground);
    canvas.drawRect(SkRect::MakeWH(16, 16), paint);
    for (const Mark& mark : kTileset[i].marks) {
      paint.setColor(mark.color);
      canvas.drawRect(SkRect::MakeXYWH(mark.x, mark.y, mark.w, mark.h), paint);
    }
    canvas.restore();
  }
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(sheet->makeImageSnapshot()));
}

/** ONE EDIT: which cell of a chunk carries a region other than the one
 *  the map's own rule gives it, and which region that is. Part of the
 *  memo's properties, which is what makes an edit a describe. */
struct Edit {
  int cell = -1;
  int id = 0;
  bool operator==(const Edit&) const = default;
};

/** THE MEMO'S PROPS: everything the chunk's description depends on. Two
 *  properties that compare equal are a describe the reconciler does not
 *  run. */
struct Chunk {
  int index = 0;
  int revision = 0;
  Edit edit;
  bool operator==(const Chunk&) const = default;
};

/** The map's own rule: a seeded pattern of walls, moss and embers, so a
 *  chunk is a function of where it stands and of nothing else. */
int tileAt(int chunk, int x, int y) {
  const uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^
                     (uint32_t)chunk * 83492791u;
  if (x % 3 == 1 && (h & 5u) != 0) return 1;  // maze walls, broken columns
  if ((h % 11) == 3) return 2;                // moss
  if ((h % 23) == 7) return 3;                // ember
  return 0;                                   // floor
}

Element chunkElement(const std::shared_ptr<sigil::image::ImageAsset>& tileset,
                     const Chunk& chunk) {
  return box()
      .width(kChunkCols * kTile)
      .height(kChunkRows * kTile)
      .children({each(kCellsPerChunk, [&](int cell) {
        const arrange::Cell at = arrange::cellAt((size_t)cell, kChunkCols);
        const int id = cell == chunk.edit.cell
                           ? chunk.edit.id
                           : tileAt(chunk.index, at.column, at.row);
        return image(tileset)
            .region(SkRect::MakeXYWH((float)id * 16, 0, 16, 16))
            .rect(arrange::cellRect(at, {kTile, kTile}));
      })});
}

}  // namespace

namespace {

struct TileMap {
  std::shared_ptr<sigil::image::ImageAsset> tileset = atlas();
  std::array<int, kChunks> revisions{};
  std::array<Edit, kChunks> edits{};
  /** The wash on each chunk, one live value per chunk: set to 1 when the
   *  chunk re-recorded and decayed by the ticker from there. */
  std::array<ch::Output<float>, kChunks> flash{};
  std::array<double, kChunks> editedAt{};
  double clock = 0.0;
  double nextMutation = 0.0;

  /** The composer's own count for the frame that did the work, and the
   *  composer it is taken from.
   *
   *  A COUNT OF RECORDINGS IS TAKEN AGAINST A CACHING POLICY. Nodes
   *  described, memo hits and instances patched are the reconciler's and
   *  are a function of the description alone; recordings held and nodes
   *  painted live are the paint tier's, and a host that promotes turns a
   *  recording into a bake and a painted node into a blit. So the counts
   *  are read off a composer the sheet owns rather than off the one the
   *  host opened: the same tree, stepped on the same clock, with the
   *  promoter held off — which is the regime this sheet is about. It
   *  draws into nothing; a no-draw canvas runs the whole phase and fills
   *  no pixels. */
  std::unique_ptr<Composer> probe;
  Composer::Stats worked;

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = {kCanvasW, kCanvasH}, .captureAt = 6.0});
    revisions.fill(0);
    edits.fill(Edit{});
    editedAt.fill(-1000.0);
    for (ch::Output<float>& f : flash) f = 0.0f;
    clock = 0.0;
    nextMutation = kPeriod;
    worked = Composer::Stats{};
    // The flash is a lane, not a re-describe: one ticker writes every
    // chunk's wash from the age of its last edit, and the bound opacity
    // beside each memo reads it.
    ctx.ticker.add([this](double dt) {
      clock += dt;
      for (int i = 0; i < kChunks; ++i)
        flash[(size_t)i] =
            motion::decay((float)(clock - editedAt[(size_t)i]), kFade);
    });
    probe = std::make_unique<Composer>(ctx.ticker, *ctx.fonts);
    probe->setSize({kChunks * kChunkCols * kTile, kChunkRows * kTile});
    probe->setAutoTexturePromotion(Composer::PromotionPolicy::Off);
    probe->render(map());
    ctx.composer.render(describe(ctx));
  }

  Element map() {
    // THE MAP: one memo a chunk, and beside each the wash that says the
    // reconciler described it again.
    return box()
        .row()
        .width(kChunks * kChunkCols * kTile)
        .children({each(kChunks, [this](int i) {
          return stack()
              .width(kChunkCols * kTile)
              .height(kChunkRows * kTile)
              // Recorded, so a describe that runs is a recording
              // written and the footer's count is the work itself.
              .children({memo(Chunk{i, revisions[(size_t)i], edits[(size_t)i]},
                              [tileset = tileset](const Chunk& c) {
                                return chunkElement(tileset, c);
                              })
                             .key("chunk" + std::to_string(i))
                             .cache(Cache::Picture),
                         // The wash: a sibling of the memo, so fading it
                         // costs the memo nothing and the memo's own
                         // recording stands.
                         box()
                             .key("flash" + std::to_string(i))
                             .cover()
                             .fill(Fill::color(kFlash))
                             .opacity(&flash[(size_t)i])});
        })});
  }

  Element describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // The counts are a function of the description and belong on a
    // plate; the milliseconds are a function of the run and do not, so
    // a capture is told where they are read instead.
    const std::string timing =
        ctx.deterministic
            ? kit::formatted(
                  "reconcile and paint: in the "
                  "window, not on the plate")
            : kit::formatted("reconcile %.3f ms  ·  paint %.3f ms",
                             worked.reconcileMs, worked.paintMs);

    const char* names[] = {"FLOOR", "BRICK", "MOSS", "EMBER"};
    Element atlasLegend = box().row().children({each(4, [&](int i) {
      return box().column().gap(10).width(96).children(
          {image(tileset)
               .region(SkRect::MakeXYWH((float)i * 16, 0, 16, 16))
               .width(80)
               .height(80)
               .sampling(SkSamplingOptions(SkFilterMode::kNearest)),
           text(names[i]).styleClass("captionLabel")});
    })});
    Element labels = box().row().children({each(kChunks, [this](int i) {
      return box()
          .width(kChunkCols * kTile)
          .column()
          .gap(8)
          .children({text(kit::formatted("CHUNK %02d", i + 1))
                         .styleClass("captionLabel"),
                     text(kit::formatted("revision %d", revisions[(size_t)i]))
                         .styleClass("readout")});
    })});
    return sketch::kit::page(
        {.title = "Edit one tile. Keep three chunks.",
         .subtitle = "One atlas, four memoized regions. The warm wash marks "
                     "the chunk whose description changed.",
         .footer = timing},
        box().column().gap(24).children(
            {box().row().gap(32).children(
                 {std::move(atlasLegend),
                  box().column().gap(14).width(440).children(
                      {text("THE SOURCE ATLAS").styleClass("captionLabel"),
                       text("Four 16 × 16 regions share one image asset. Every "
                            "0.7 seconds, one cell selects a different region.")
                           .width(440),
                       text("The flash fades on a sibling of the memo. Its "
                            "animation does not invalidate the chunk it "
                            "highlights.")
                           .width(440)})}),
             sketch::kit::sectionHeader(
                 {.label = "THE LIVE MAP", .note = "8 × 5 tiles per chunk"}),
             sketch::kit::well(
                 {.width = 1040,
                  .height = 220,
                  .content = sketch::kit::Well::Content{}},
                 box().column().gap(14).children({std::move(labels), map()})),
             sketch::kit::sectionHeader({.label = "WORK AT THE PREVIOUS EDIT",
                                         .note = "Measured on the map alone"}),
             box().row().gap(40).children(
                 {sketch::kit::readout(
                      {{"Described nodes",
                        kit::formatted("%zu", worked.describedNodes)},
                       {"Memo hits", kit::formatted("%zu", worked.memoHits)}},
                      {.measure = 250, .ruled = true}),
                  sketch::kit::readout(
                      {{"Patched instances",
                        kit::formatted("%zu", worked.patchedNodes)},
                       {"Recordings held",
                        kit::formatted("%zu", worked.picturesLive)}},
                      {.measure = 250, .ruled = true}),
                  sketch::kit::readout(
                      {{"Painted live",
                        kit::formatted("%zu", worked.nodesPainted)}},
                      {.measure = 250})})}));
  }

  /** THE DATA PATH, and only when the data changes: one cell of one
   *  chunk is given a different region, that chunk's properties stop being
   *  equal, and the tree is described again. Between edits nothing is
   *  described at all — the wash fades on its lane. */
  void update(double elapsed, sketch::SketchContext& ctx) {
    // The probe is stepped every frame the sheet is, because a recording
    // is counted by the draw that writes it.
    if (probe) {
      SkNoDrawCanvas nowhere((int)ctx.size.width(), (int)ctx.size.height());
      probe->draw(nowhere);
    }
    if (elapsed < nextMutation) return;
    nextMutation = elapsed + kPeriod;
    // What the PREVIOUS edit cost, now that the frame which did the work
    // has been drawn: a describe is counted when it runs and a recording
    // when the draw after it writes one.
    worked = probe->stats();
    const long long step = motion::stepIndex(elapsed, 1.0 / kPeriod);
    const int chunk = (int)(step % kChunks);
    const uint32_t h = (uint32_t)step * 2654435761u;
    const int cell = (int)(h % (uint32_t)kCellsPerChunk);
    // A region the map's own rule would not have put there, so an edit
    // is always visible.
    const arrange::Cell where = arrange::cellAt((size_t)cell, kChunkCols);
    const int rule = tileAt(chunk, where.column, where.row);
    edits[(size_t)chunk] = Edit{cell, (rule + 1 + (int)(h % 3u)) % 4};
    ++revisions[(size_t)chunk];
    editedAt[(size_t)chunk] = clock;
    probe->render(map());
    ctx.composer.render(describe(ctx));
  }
};

}  // namespace

SIGIL_SKETCH_AS(TileMap, "tile map", "Kit · API",
                "memo chunking — one tile edited on a timer, the "
                "chunk that re-recorded washed and decaying, and the "
                "reconcile counts under it")
