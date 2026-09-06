/** @file
 * What one FRAME of a moving text costs: a one-word edit, a paint
 * restyle, a size restyle, a span restyle, exclusions moving in lines and
 * in columns, and a whole-text replacement — each against the same warm
 * relayout, so what the update adds is the difference between two arms.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkPathBuilder.h>
#include <sigilweave/layout/Beside.h>

#include <cmath>
#include <numbers>
#include <random>
#include <string>
#include <vector>

#include "BenchOptions.h"
#include "support/Corpus.h"
#include "support/Layouts.h"

using namespace sigil::weave;
using namespace sigil::weave::bench;

namespace {

// ── Per-frame update scenarios ────────────────────────────────────────────

// One word of a 500-word mixed paragraph changes per frame: everything
// else must come out of the shape cache.
void BM_Update_EditOneWord_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  // Same-length alternatives so the text stays put across iterations.
  const char8_t* alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, 7, alternatives[(alternativeIndex++) % 4]);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_EditOneWord_500w)->Unit(benchmark::kMicrosecond);

// The same edit under Knuth-Plass.
void BM_Update_EditOneWord_KnuthPlass_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/false), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 40000));
  const ParagraphLayoutOptions options = knuthPlass();
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  const char8_t* alternatives[] = {u8"changed", u8"updated", u8"swapped",
                                   u8"resized"};
  paragraph.replaceText(0, 3, alternatives[0]);
  int alternativeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, 7, alternatives[(alternativeIndex++) % 4]);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_EditOneWord_KnuthPlass_500w)->Unit(benchmark::kMicrosecond);

// Paint-only restyle (a colour flash on a word) — must not reshape.
void BM_Update_PaintRestyle_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  SkColor colors[] = {SK_ColorRED, SK_ColorBLUE, SK_ColorGREEN};
  int colorIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.setPaint(40, 60, PaintStyle{colors[colorIndex++ % 3]});
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_PaintRestyle_500w)->Unit(benchmark::kMicrosecond);

// The same colour flash on a paragraph whose text is thirty times what the
// geometry holds, restyled inside the placed window. A paint edit runs no
// analysis over the text that never fits, so what this arm is read against
// is the fully-placed one above rather than its own word count.
void BM_Update_PaintRestyle_Overflowed_30000w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(30000, /*mixed=*/false), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));
  ParagraphLayout warm = layoutParagraph(sigil::test::fonts(), paragraph, flow);
  const uint32_t placedEnd =
      paragraph.words()[warm.firstUnplacedWord].textBegin;
  uint32_t frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    PaintStyle hue(0xFF000000u | (frame++ * 1234567u));
    paragraph.setPaint(0, placedEnd, hue);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 30000);
}
BENCHMARK(BM_Update_PaintRestyle_Overflowed_30000w)
    ->Unit(benchmark::kMicrosecond);

// Shaping-relevant restyle (a size bump on one word).
void BM_Update_SizeRestyle_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  float sizes[] = {18.0f, 20.0f, 22.0f, 24.0f};
  int sizeIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    TextStyle style = basicStyle();
    style.shaping.fontSize = sizes[(sizeIndex++) % 4];
    paragraph.setStyle(40, 60, style);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_SizeRestyle_500w)->Unit(benchmark::kMicrosecond);

// One continuous paint span swept across a 500-word paragraph (crosses
// many lines): re-analysis and placement, zero reshaping.
void BM_Update_SpanRestyleAcrossLines_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  const uint32_t textLength = (uint32_t)paragraph.text().size();
  uint32_t rangeStart = 0;
  for ([[maybe_unused]] auto iteration : state) {
    rangeStart = (rangeStart + 97) % (textLength / 2);
    paragraph.setPaint(rangeStart, rangeStart + textLength / 3,
                       PaintStyle{SK_ColorRED});
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_SpanRestyleAcrossLines_500w)->Unit(benchmark::kMicrosecond);

// Shapes sweeping through a mixed-language paragraph, relayout every
// frame. The text never changes, so this is placement arithmetic.
void BM_Update_MovingExclusions_300w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), basicStyle());
  ExclusionFlow flow(SkRect::MakeWH(700, 3000));
  flow.exclusions().push_back(
      {silhouette::circle(SkRect::MakeXYWH(100, 100, 160, 160)), 8});
  flow.exclusions().push_back(
      {silhouette::rectangle(SkRect::MakeXYWH(400, 600, 180, 120)), 8});
  layoutParagraph(sigil::test::fonts(), paragraph, flow);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  float animationTime = 0;
  for ([[maybe_unused]] auto iteration : state) {
    animationTime += 0.03f;
    flow.exclusions()[0].offset = {
        200 * std::sin(animationTime),
        900 * (0.5f + 0.5f * std::sin(animationTime * 0.7f))};
    flow.exclusions()[1].offset = {-150 * std::cos(animationTime),
                                   300 * std::sin(animationTime * 1.3f)};
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Update_MovingExclusions_300w)->Unit(benchmark::kMicrosecond);

// The same sweep with arbitrary SkPath obstacles (a star and a donut with
// a live hole). Moving via pathOffset reuses the cached flattening, so
// the per-frame cost is scanline interval math, not path processing.
void BM_Update_MovingPathExclusions_300w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(300, /*mixed=*/true), basicStyle());

  SkPathBuilder star;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    const float angle =
        -std::numbers::pi_v<float> / 2.0f +
        (float)pointIndex * 4.0f * std::numbers::pi_v<float> / 5.0f;
    const SkPoint point = {150 + 110 * std::cos(angle),
                           150 + 110 * std::sin(angle)};
    if (pointIndex == 0)
      star.moveTo(point);
    else
      star.lineTo(point);
  }
  star.close();
  SkPathBuilder donut;
  donut.addCircle(450, 700, 110);
  donut.addCircle(450, 700, 55);
  donut.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeWH(700, 3000));
  flow.exclusions().push_back({silhouette::path(star.detach()), 8});
  flow.exclusions().push_back({silhouette::path(donut.detach()), 8});
  layoutParagraph(sigil::test::fonts(), paragraph, flow);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  float animationTime = 0;
  for ([[maybe_unused]] auto iteration : state) {
    animationTime += 0.03f;
    flow.exclusions()[0].offset = {
        200 * std::sin(animationTime),
        900 * (0.5f + 0.5f * std::sin(animationTime * 0.7f))};
    flow.exclusions()[1].offset = {-150 * std::cos(animationTime),
                                   300 * std::sin(animationTime * 1.3f)};
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Update_MovingPathExclusions_300w)->Unit(benchmark::kMicrosecond);

// The same sweep read DOWN COLUMNS: the identical star and donut, met by
// a column flow instead of a line flow. It is the attribution arm for the
// per-column band scan — the same scan a quarter turn later, over a
// corpus whose break opportunities sit between characters.
void BM_Update_MovingColumnExclusions_300w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeColumnText(300), basicStyle());
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  SkPathBuilder star;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    const float angle =
        -std::numbers::pi_v<float> / 2.0f +
        (float)pointIndex * 4.0f * std::numbers::pi_v<float> / 5.0f;
    const SkPoint point = {150 + 110 * std::cos(angle),
                           150 + 110 * std::sin(angle)};
    if (pointIndex == 0)
      star.moveTo(point);
    else
      star.lineTo(point);
  }
  star.close();
  SkPathBuilder donut;
  donut.addCircle(700, 450, 110);
  donut.addCircle(700, 450, 55);
  donut.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeWH(3000, 700), FlowAxis::kColumns);
  flow.exclusions().push_back({silhouette::path(star.detach()), 8});
  flow.exclusions().push_back({silhouette::path(donut.detach()), 8});
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 26;  // column pitch
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);

  float animationTime = 0;
  for ([[maybe_unused]] auto iteration : state) {
    animationTime += 0.03f;
    flow.exclusions()[0].offset = {
        900 * (0.5f + 0.5f * std::sin(animationTime * 0.7f)),
        200 * std::sin(animationTime)};
    flow.exclusions()[1].offset = {300 * std::sin(animationTime * 1.3f),
                                   -150 * std::cos(animationTime)};
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 300);
}
BENCHMARK(BM_Update_MovingColumnExclusions_300w)->Unit(benchmark::kMicrosecond);

// A clamped column with an overflow marker at its foot: the trim walks
// back up the column shaping nothing new, so what this measures against
// the unclamped column arm is the marker's own cost.
void BM_Layout_Vertical_ClampedEllipsis_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeColumnText(500), basicStyle());
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 26;  // column pitch
  options.overflow.maxLines = 8;
  options.overflow.ellipsis = u"\u2026";
  VerticalBlockFlow flow(SkRect::MakeWH(20000, 600));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Layout_Vertical_ClampedEllipsis_500w)
    ->Unit(benchmark::kMicrosecond);

// The entire paragraph text is replaced every frame, cycling four
// variants: after one cycle every word is cache-hot, so this is the
// steady-state cost of "swap the whole text each frame".
void BM_Update_ReplaceWholeParagraph_500w(benchmark::State& state) {
  std::u8string variants[4];
  for (uint32_t variantIndex = 0; variantIndex < 4; ++variantIndex)
    variants[variantIndex] =
        makeText(500, /*mixed=*/true, /*seed=*/variantIndex + 1);
  Paragraph paragraph;
  paragraph.appendText(variants[0], basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  for (const std::u8string& variant : variants) {
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(), variant);
    layoutParagraph(sigil::test::fonts(), paragraph, flow);
  }
  int variantIndex = 0;
  for ([[maybe_unused]] auto iteration : state) {
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(),
                          variants[(++variantIndex) % 4]);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_ReplaceWholeParagraph_500w)->Unit(benchmark::kMicrosecond);

// The same, but the incoming text has never been seen: every word goes
// through HarfBuzz. The worst case for a full-paragraph update.
void BM_Update_ReplaceWholeParagraph_Cold_500w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(500, /*mixed=*/true), basicStyle());
  BlockFlow flow(SkRect::MakeWH(600, 20000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow);
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          1000);
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    sigil::test::fonts().purgeShapeCache();
    // Unique gibberish words so no shape can be reused, unlike makeText's
    // small vocabulary.
    std::u8string next;
    for (int wordIndex = 0; wordIndex < 500; ++wordIndex) {
      const int wordLength = 3 + (int)(randomEngine() % 8);
      for (int characterIndex = 0; characterIndex < wordLength;
           ++characterIndex)
        next += (char8_t)('a' + randomEngine() % 26);
      next += ' ';
    }
    state.ResumeTiming();
    paragraph.replaceText(0, (uint32_t)paragraph.text().size(), next);
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 500);
}
BENCHMARK(BM_Update_ReplaceWholeParagraph_Cold_500w)
    ->Unit(benchmark::kMicrosecond);

// 2000 multi-script tokens scattered over 2000 rotated intervals —
// letter-confetti at paragraph scale. Warm: placement plus per-glyph
// RSXform baking; every token still shape-cache resolved.

}  // namespace
