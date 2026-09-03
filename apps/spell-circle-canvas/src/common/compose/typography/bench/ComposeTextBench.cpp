// SigilComposeTypography benchmarks: dense static text replayed, baked and
// projected, and kinetic text driving batched glyph draws every frame. The
// Graphite arms are where the glyph atlas is exercised at all; the raster
// arms are the portable pair the GPU numbers are read against.

#include <include/core/SkCanvas.h>
#include <include/core/SkM44.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Typography.h>

#include <cmath>
#include <memory>
#include <string>

#include "BenchSupport.h"

using namespace sigil::compose;
using sigil::compose::bench::Host;

namespace {

/** Six paragraphs of sixty pangrams at 15 px in a 780 px column: enough
 *  glyphs that replay cost is the number, not the frame overhead. */
Element denseBlock(Cache mode) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 15.0f;
  std::u8string para;
  for (int i = 0; i < 60; ++i)
    para += u8"the quick brown fox jumps over the lazy dog ";
  auto block = box().padding(12).width(780).cache(mode).fill(
      Fill::color({0.1f, 0.1f, 0.12f, 1}));
  for (int i = 0; i < 6; ++i) block.child(text(para, style));
  return block;
}

void denseArm(benchmark::State& state, Cache mode) {
  Host host(800, 2400);
  host.composer.render(denseBlock(mode));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
  state.counters["texturesLive"] = (double)host.composer.stats().texturesLive;
}

}  // namespace

/** Dense text block, picture replay per draw: the raster re-raster cost
 *  that Cache::Texture exists to eliminate. */
static void BM_Draw_DenseText_PictureReplay(benchmark::State& state) {
  denseArm(state, Cache::Picture);
}
BENCHMARK(BM_Draw_DenseText_PictureReplay);

static void BM_Draw_DenseText_TextureBlit(benchmark::State& state) {
  denseArm(state, Cache::Texture);
}
BENCHMARK(BM_Draw_DenseText_TextureBlit);

/** Reserved slot for an sktext::gpu::Slug replay arm, registered but not
 *  implemented. Slug plans glyph → strike → atlas once at conversion rather
 *  than on every replay, so it is the natural third arm beside the picture
 *  and texture dense-text arms. It is not built because the type lives in
 *  Skia's include/private/chromium/, which carries no version guarantee and
 *  can disappear on a Skia update, so taking it on means writing a seam to
 *  hide it behind.
 *
 *  Whether that seam is worth writing depends entirely on how much replay
 *  cost is left to win on the GPU path — read
 *  BM_Draw_DenseText_PictureReplay_Graphite against
 *  BM_Draw_DenseText_TextureBlit_Graphite first. The gap between them is the
 *  ceiling on anything Slug could recover. Registering the name keeps this
 *  arm ordered next to its siblings if that gap ever grows. */
static void BM_Draw_DenseText_SlugReplay(benchmark::State& state) {
  state.SkipWithMessage(
      "not implemented: with ordered recordings, dense-text picture replay "
      "already runs close to the texture-blit floor, so the win available "
      "here is bounded by that gap — compare "
      "BM_Draw_DenseText_PictureReplay_Graphite against "
      "BM_Draw_DenseText_TextureBlit_Graphite. This arm stays registered "
      "as the re-open hook if replay cost ever grows");
}
BENCHMARK(BM_Draw_DenseText_SlugReplay);

/** Kinetic typography on raster: a looping fx() reveal drives batched
 *  RSXform glyph draws every frame across `lines` lines of text. */
static void BM_Draw_KineticText(benchmark::State& state) {
  const int lines = (int)state.range(0);
  Host host(800, 1200);
  choreograph::Output<float> progress{0.0f};
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 22.0f;
  auto block = box().column().gap(8).padding(16);
  for (int i = 0; i < lines; ++i)
    block.child(text(u8"KINETIC ATLAS RESIDENCY PROBE 0123456789", style)
                    .fx({.effect = fx::rise(24), .progress = &progress}));
  host.composer.render(block);
  host.draw();
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 1.0f / 60.0f;
    progress = std::fmod(t, 1.0f);  // the reveal loops forever
    host.draw();
  }
  state.counters["lines"] = (double)lines;
}
BENCHMARK(BM_Draw_KineticText)->Arg(14)->Arg(56)->Unit(benchmark::kMicrosecond);

/** The same looping reveal set DOWN COLUMNS, beating over `unit::Line` —
 *  which in a vertical passage is a column. Read against
 *  BM_Draw_KineticText: the deviation is applied in the frame the layout
 *  placed each glyph in, and a column places every glyph as its own
 *  positioned run rather than a word's worth at a time. */
static void BM_Draw_KineticColumns(benchmark::State& state) {
  const int passages = (int)state.range(0);
  Host host(800, 1200);
  choreograph::Output<float> progress{0.0f};
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 22.0f;
  style.shaping.languageTag = "ja";
  auto block = box().row().gap(8).padding(16);
  for (int i = 0; i < passages; ++i)
    block.child(
        text(u8"縦組みの文章は上から下へ流れ右から左へと列が進む", style)
            .width(160)
            .height(1100)
            .writingMode(sigil::weave::WritingMode::kVerticalRL)
            .fx({.effect = fx::rise(24),
                 .stagger = {.eachMs = 120},
                 .over = unit::Line,
                 .progress = &progress}));
  host.composer.render(block);
  host.draw();
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 1.0f / 60.0f;
    progress = std::fmod(t, 1.0f);
    host.draw();
  }
  state.counters["passages"] = (double)passages;
}
BENCHMARK(BM_Draw_KineticColumns)
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMicrosecond);

#ifdef COMPOSE_BENCH_GRAPHITE

// ---- Dense static text on Graphite ---------------------------------------
// Automatic texture promotion is off on the GPU path, so a dense static text
// block genuinely replays its draw calls every frame and every glyph goes
// through strike and atlas planning once per Recording. Two Skia knobs act
// on exactly that shape: RecorderOptions::fRequireOrderedRecordings (with
// unordered recordings, Recorder::snap() invalidates the atlases, evicting
// the text atlas on every snap), and sktext::gpu::Slug, which would cache
// the planning. Neither can be evaluated on CPU raster, which has no glyph
// atlas at all.
//
// Same corpus and geometry as the raster pair above (denseBlock, 800x2400)
// so the arms are directly comparable; the only differences are the target
// surface and the per-iteration snap + insert + submit.

using sigil::compose::bench::GraphiteTarget;

namespace {

void denseGraphiteArm(benchmark::State& state, Cache mode) {
  GraphiteTarget target(state, 800, 2400);
  if (!target.ok()) return;
  Host host(800, 2400);
  host.composer.render(denseBlock(mode));
  host.composer.draw(target.canvas());
  target.submit();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.draw(target.canvas());
    target.submit();
  }
  state.counters["texturesLive"] = (double)host.composer.stats().texturesLive;
}

}  // namespace

static void BM_Draw_DenseText_PictureReplay_Graphite(benchmark::State& state) {
  denseGraphiteArm(state, Cache::Picture);
}
BENCHMARK(BM_Draw_DenseText_PictureReplay_Graphite);

/** The bake, on the same target: the pixels the atlas work is being
 *  compared against. Without this arm the picture-replay number has no
 *  floor to be read against on the GPU path. */
static void BM_Draw_DenseText_TextureBlit_Graphite(benchmark::State& state) {
  denseGraphiteArm(state, Cache::Texture);
}
BENCHMARK(BM_Draw_DenseText_TextureBlit_Graphite);

/** Kinetic typography on Graphite: a looping fx() reveal drives batched
 *  RSXform glyph draws every frame, which is the shape that stresses the
 *  glyph atlas hardest. The distinct glyph variants come from the kinetic
 *  path's own quantization (alpha to 32 steps, rotations snapped), so the
 *  atlas cardinality this produces is the library's, not the benchmark's.
 *
 *  To price atlas eviction, run this arm once at the default atlas budget
 *  and again with SIGILSKIA_GLYPH_ATLAS_BYTES set to something smaller
 *  (the environment knob makeContextOptions reads); the difference between
 *  the two runs is the eviction cost. */
static void BM_Draw_KineticText_Graphite(benchmark::State& state) {
  const int lines = (int)state.range(0);
  GraphiteTarget target(state, 800, 1200);
  if (!target.ok()) return;
  Host host(800, 1200);
  choreograph::Output<float> progress{0.0f};
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 22.0f;
  auto block = box().column().gap(8).padding(16);
  for (int i = 0; i < lines; ++i)
    block.child(text(u8"KINETIC ATLAS RESIDENCY PROBE 0123456789", style)
                    .fx({.effect = fx::rise(24), .progress = &progress}));
  host.composer.render(block);
  host.composer.draw(target.canvas());
  target.submit();
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 1.0f / 60.0f;
    progress = std::fmod(t, 1.0f);  // the reveal loops forever
    host.composer.draw(target.canvas());
    target.submit();
  }
  state.counters["lines"] = (double)lines;
}
BENCHMARK(BM_Draw_KineticText_Graphite)->Arg(14)->Arg(56);

// ---------------------------------------------------------------------------
// Dense GLYPHS under a perspective CTM on Graphite. A projective matrix can
// push glyphs off the atlas path and onto path filling, which costs very
// differently; these arms separate that from the ordinary cost of drawing
// the same text under any non-identity transform.
//
// Three arms differ ONLY in the matrix applied inside save()/concat()/
// restore(): identity, the card tilt with its perspective row removed (an
// affine control — a pure cos(25°) y-compression, so the same pixels are
// resampled without any w-divide), and the full projective tilt. Reading the
// affine arm against identity gives the cost of the transform; reading
// perspective against affine gives the cost of the projection alone.
//
// Cache::None is required here, not incidental: device-space bakes refuse to
// form under a perspective CTM, so a Cache::Texture arm would silently fall
// back to a quantized local-scale bake and measure that ladder instead of
// the projection.

namespace {

enum class PerspArm { Identity, AffineTilt, Perspective };

/** A game-UI card tilt: rotateX(25°) about the panel centre (400, 1200),
 *  viewed through a CSS-style perspective(2400) — viewer distance equal to
 *  one panel height, expressed as the w-divide term -1/2400 at row 3,
 *  column 2. Over the 800x2400 panel that puts w in roughly [0.79, 1.21], so
 *  the projection is strong enough to matter and mild enough to stay a
 *  plausible UI transform. The affine control is the same product with the
 *  perspective factor left out. */
SkM44 perspArmMatrix(PerspArm arm) {
  if (arm == PerspArm::Identity)
    return SkM44();  // identity — the arms stay structurally identical
  SkM44 persp;       // identity
  persp.setRC(3, 2, -1.0f / 2400.0f);
  const SkM44 rotX = SkM44::Rotate({1, 0, 0}, 25.0f * SK_ScalarPI / 180.0f);
  return SkM44::Translate(400, 1200) *
         (arm == PerspArm::Perspective ? persp * rotX : rotX) *
         SkM44::Translate(-400, -1200);
}

void graphitePerspectiveArm(benchmark::State& state, PerspArm arm) {
  GraphiteTarget target(state, 800, 2400);
  if (!target.ok()) return;
  Host host(800, 2400);
  host.composer.render(denseBlock(Cache::None));
  const SkM44 m = perspArmMatrix(arm);
  SkCanvas& canvas = target.canvas();
  // Warm the pipeline compile (and the glyph atlas) under the arm's own
  // matrix, synced like the measured frames.
  canvas.save();
  canvas.concat(m);
  host.composer.draw(canvas);
  canvas.restore();
  target.submitSynced();
  for ([[maybe_unused]] auto iteration : state) {
    canvas.save();
    canvas.concat(m);
    host.composer.draw(canvas);
    canvas.restore();
    target.submitSynced();
  }
}

}  // namespace

static void BM_Draw_DenseText_Persp_Identity_Graphite(benchmark::State& state) {
  graphitePerspectiveArm(state, PerspArm::Identity);
}
BENCHMARK(BM_Draw_DenseText_Persp_Identity_Graphite)
    ->Unit(benchmark::kMillisecond);

static void BM_Draw_DenseText_Persp_AffineTilt_Graphite(
    benchmark::State& state) {
  graphitePerspectiveArm(state, PerspArm::AffineTilt);
}
BENCHMARK(BM_Draw_DenseText_Persp_AffineTilt_Graphite)
    ->Unit(benchmark::kMillisecond);

static void BM_Draw_DenseText_Persp_Perspective_Graphite(
    benchmark::State& state) {
  graphitePerspectiveArm(state, PerspArm::Perspective);
}
BENCHMARK(BM_Draw_DenseText_Persp_Perspective_Graphite)
    ->Unit(benchmark::kMillisecond);

#endif  // COMPOSE_BENCH_GRAPHITE

BENCHMARK_MAIN();
