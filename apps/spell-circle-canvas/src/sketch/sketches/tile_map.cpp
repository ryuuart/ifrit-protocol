/** @file
 * tile map — what a memo is worth, made visible: one tile is edited on a
 * timer, and only the chunk holding it is described again.
 *
 * The map is four `memo` chunks of 8×5 tiles, each keyed by props that
 * say which chunk it is, how many times it has changed, and which of its
 * cells carries an edit. Every 0.7 s ONE cell in ONE chunk is given a
 * different atlas region. Its chunk's props are then unequal to the ones
 * the reconciler is holding, so that chunk's describe runs and its
 * subtree re-records; the other three compare equal, their describes are
 * skipped, and their recordings replay.
 *
 * THE FLASH IS THE PROOF. The chunk that re-recorded is washed in a
 * colour that decays over the next half second — an ordinary bound
 * opacity on a sibling of the memo, so the wash is outside what the memo
 * holds and fades without touching it. What flashes is what the
 * reconciler did work on; three quarters of the map never moves.
 *
 * THE READOUT is the composer's own count for the frame that did the
 * work — nodes described, memo hits, instances patched, recordings held,
 * nodes painted live — read at the NEXT edit, once the frame that did the work has
 * been drawn, because a still cannot show the numbers of the frame it is
 * itself part of. The counts are a function of the description and are
 * the same on every machine; the milliseconds are a function of the run
 * and are shown only in the window, because a number a sketch measured
 * about its own execution differs from itself between two captures.
 *
 * EDIT THESE FIRST
 *   kPeriod — seconds between edits
 *   kFade   — how long the flash takes to die
 *   kChunkCols / kChunkRows / kChunks — the map
 */

#include <include/core/SkBitmap.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
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

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace motion = sigil::motion;
namespace ch = choreograph;

using namespace sigil::compose;
using sigil::compose::toU8;

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
constexpr float kHeadH = 46.0f;
constexpr float kFootH = 38.0f;
constexpr float kGap = 16.0f;
constexpr float kCanvasW = kChunks * kChunkCols * kTile + 2 * kPad;
constexpr float kCanvasH =
    kChunkRows * kTile + kHeadH + kFootH + 2 * kGap + 2 * kPad;

constexpr SkColor4f kGround{0.03f, 0.03f, 0.07f, 1};
constexpr SkColor4f kInk{0.84f, 0.87f, 0.94f, 1};
constexpr SkColor4f kAsh{0.55f, 0.60f, 0.70f, 1};
constexpr SkColor4f kRule{0.16f, 0.17f, 0.24f, 1};
/** What a re-recorded chunk is washed in. */
constexpr SkColor4f kFlash{1.0f, 0.58f, 0.20f, 0.55f};

/** This page's look: the map's own near-black, and a header set close
 *  enough to the grid that the chunks keep the width they ask for. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look;
  look.palette = {
      .ground = kGround, .ink = kInk, .ash = kAsh, .rule = kRule};
  look.type.title = {.size = 20, .track = 2.6f};
  look.type.subtitle = {.size = 11.5f, .track = 0.4f};
  look.type.footer = {.size = 11, .track = 0.6f};
  look.spacing.marginX = kPad;
  look.spacing.marginTop = kPad * 0.6f;
  look.spacing.marginBottom = kPad * 0.5f;
  look.spacing.contentGap = kGap;
  return look;
}

/** THE TILESET: a procedural four-cell atlas — floor, brick wall, moss
 *  floor, ember — built once and read with `image(atlas).region(cell)`,
 *  so forty tiles are forty regions of one image. */
std::shared_ptr<sigil::image::ImageAsset> atlas() {
  static std::shared_ptr<sigil::image::ImageAsset> asset = [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
    SkCanvas& c = *s->getCanvas();
    SkPaint p;
    // 0: floor
    p.setColor(SkColorSetRGB(0x18, 0x16, 0x24));
    c.drawRect(SkRect::MakeXYWH(0, 0, 16, 16), p);
    p.setColor(SkColorSetRGB(0x24, 0x20, 0x34));
    c.drawRect(SkRect::MakeXYWH(2, 2, 2, 2), p);
    c.drawRect(SkRect::MakeXYWH(10, 9, 2, 2), p);
    // 1: brick wall
    p.setColor(SkColorSetRGB(0x5a, 0x33, 0x2c));
    c.drawRect(SkRect::MakeXYWH(16, 0, 16, 16), p);
    p.setColor(SkColorSetRGB(0x3a, 0x1f, 0x1c));
    for (int row = 0; row < 4; ++row)
      c.drawRect(SkRect::MakeXYWH(16, (float)row * 4 + 3, 16, 1), p);
    c.drawRect(SkRect::MakeXYWH(16 + 7, 0, 1, 16), p);
    // 2: moss floor
    p.setColor(SkColorSetRGB(0x1c, 0x2a, 0x1e));
    c.drawRect(SkRect::MakeXYWH(32, 0, 16, 16), p);
    p.setColor(SkColorSetRGB(0x2f, 0x49, 0x2c));
    c.drawRect(SkRect::MakeXYWH(35, 4, 3, 2), p);
    c.drawRect(SkRect::MakeXYWH(42, 10, 4, 3), p);
    // 3: ember
    p.setColor(SkColorSetRGB(0x2a, 0x12, 0x18));
    c.drawRect(SkRect::MakeXYWH(48, 0, 16, 16), p);
    p.setColor(SkColorSetRGB(0xff, 0x7a, 0x33));
    c.drawRect(SkRect::MakeXYWH(54, 6, 4, 4), p);
    p.setColor(SkColorSetRGB(0xff, 0xc4, 0x6b));
    c.drawRect(SkRect::MakeXYWH(55, 7, 2, 2), p);
    return std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(s->makeImageSnapshot()));
  }();
  return asset;
}

/** ONE EDIT: which cell of a chunk carries a region other than the one
 *  the map's own rule gives it, and which region that is. Part of the
 *  memo's props, which is what makes an edit a describe. */
struct Edit {
  int cell = -1;
  int id = 0;
  bool operator==(const Edit&) const = default;
};

/** THE MEMO'S PROPS: everything the chunk's description depends on. Two
 *  props that compare equal are a describe the reconciler does not
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

Element chunkElement(const Chunk& chunk) {
  Element tiles =
      box().width(Dim(kChunkCols * kTile)).height(Dim(kChunkRows * kTile));
  for (int y = 0; y < kChunkRows; ++y)
    for (int x = 0; x < kChunkCols; ++x) {
      const int cell = y * kChunkCols + x;
      const int id =
          cell == chunk.edit.cell ? chunk.edit.id : tileAt(chunk.index, x, y);
      tiles.child(image(atlas())
                      .region(SkRect::MakeXYWH((float)id * 16, 0, 16, 16))
                      .inset((float)x * kTile, (float)y * kTile, 0, 0)
                      .width(Dim(kTile))
                      .height(Dim(kTile)));
    }
  return tiles;
}

}  // namespace

namespace {

struct TileMap final : sketch::Sketch {
  std::array<int, kChunks> revisions{};
  std::array<Edit, kChunks> edits{};
  /** The wash on each chunk, one live value per chunk: set to 1 when the
   *  chunk re-recorded and decayed by the ticker from there. */
  std::array<ch::Output<float>, kChunks> flash{};
  std::array<double, kChunks> editedAt{};
  double clock = 0.0;
  double nextMutation = 0.0;

  /** The composer's own count for the frame that did the work. */
  Composer::Stats worked;

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(
        ctx, {.size = {kCanvasW, kCanvasH}, .captureAt = 6.0});
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
      return true;
    });
    ctx.composer.render(describe(ctx));
  }

  Element describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    Element grid = box().row().width(Dim(kChunks * kChunkCols * kTile));
    for (int i = 0; i < kChunks; ++i) {
      Element chunk =
          stack()
              .width(Dim(kChunkCols * kTile))
              .height(Dim(kChunkRows * kTile))
              // Recorded, so a describe that runs is a recording
              // written and the footer's count is the work itself.
              .child(memo(Chunk{i, revisions[(size_t)i], edits[(size_t)i]},
                          chunkElement)
                         .key("chunk" + std::to_string(i))
                         .cache(Cache::Picture))
              // The wash: a sibling of the memo, so fading it costs the
              // memo nothing and the memo's own recording stands.
              .child(box()
                         .key("flash" + std::to_string(i))
                         .absolute()
                         .inset(0)
                         .fill(Fill::color(kFlash))
                         .opacity(&flash[(size_t)i]));
      grid.child(std::move(chunk));
    }

    char counts[160];
    std::snprintf(counts, sizeof(counts),
                  "described %zu  \xc2\xb7  memo hits %zu  \xc2\xb7  patched "
                  "%zu  \xc2\xb7  recordings held %zu  \xc2\xb7  painted "
                  "live %zu",
                  worked.describedNodes, worked.memoHits, worked.patchedNodes,
                  worked.picturesLive, worked.nodesPainted);
    // The counts are a function of the description and belong on a
    // plate; the milliseconds are a function of the run and do not, so
    // a capture is told where they are read instead.
    char timing[96];
    if (ctx.deterministic)
      std::snprintf(timing, sizeof(timing), "reconcile and paint: in the "
                                            "window, not on the plate");
    else
      std::snprintf(timing, sizeof(timing),
                    "reconcile %.3f ms  \xc2\xb7  paint %.3f ms",
                    worked.reconcileMs, worked.paintMs);

    return sketch::kit::page(
        {.title = u8"MEMO CHUNKING",
         .subtitle = u8"one tile edited every 0.7 s \xe2\x80\x94 the chunk "
                     u8"that holds it is described again and washed; the "
                     u8"other three replay",
         .footer = toU8(std::string(counts) + "   |   " + timing)},
        std::move(grid));
  }

  /** THE DATA PATH, and only when the data changes: one cell of one
   *  chunk is given a different region, that chunk's props stop being
   *  equal, and the tree is described again. Between edits nothing is
   *  described at all — the wash fades on its lane. */
  void update(double elapsed, sketch::SketchContext& ctx) override {
    if (elapsed < nextMutation) return;
    nextMutation = elapsed + kPeriod;
    // What the PREVIOUS edit cost, now that the frame which did the work
    // has been drawn: a describe is counted when it runs and a recording
    // when the draw after it writes one.
    worked = ctx.composer.stats();
    const long long step = motion::stepIndex(elapsed, 1.0 / kPeriod);
    const int chunk = (int)(step % kChunks);
    const uint32_t h = (uint32_t)step * 2654435761u;
    const int cell = (int)(h % (uint32_t)kCellsPerChunk);
    // A region the map's own rule would not have put there, so an edit
    // is always visible.
    const int rule = tileAt(chunk, cell % kChunkCols, cell / kChunkCols);
    edits[(size_t)chunk] = Edit{cell, (rule + 1 + (int)(h % 3u)) % 4};
    ++revisions[(size_t)chunk];
    editedAt[(size_t)chunk] = clock;
    ctx.composer.render(describe(ctx));
  }
};

}  // namespace

SIGIL_SKETCH_AS(TileMap, "tile map", "Kit \xc2\xb7 API",
                "memo chunking \xe2\x80\x94 one tile edited on a timer, the "
                "chunk that re-recorded washed and decaying, and the "
                "reconcile counts under it")
