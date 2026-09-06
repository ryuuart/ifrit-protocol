/** @file
 * The realtime half of the engine: a story of the size a page holds,
 * composed under an input that moves — the measure animating, the measure
 * seen before, the content churning, and a story refilled frame by frame.
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

// ── The live composer ─────────────────────────────────────────────────────
//
// These are the arms the realtime half of the engine is judged on: a story
// of the size a page holds, composed under an input that MOVES. The
// measure animating and the content churning are the two ways text moves,
// and both are answered by the same composer with the same precompute
// behind it.

// A 600-word story at a measure that moves a pixel per frame: every frame
// is a measure the break store has not seen, so every frame decides its
// breaks. This is the composer's worst honest case.
void BM_Live_Composer_AnimatingMeasure_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  float measure = 380.0f;
  BlockFlow warm(SkRect::MakeWH(measure, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, warm, options);
  int frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    BlockFlow flow(SkRect::MakeWH(measure + (float)(frame++ % 120), 40000));
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Composer_AnimatingMeasure_600w)
    ->Unit(benchmark::kMicrosecond);

// The same story at a measure that returns to widths already seen: the
// break store answers and the frame costs its fill alone.
void BM_Live_Composer_SeenMeasure_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  BlockFlow flow(SkRect::MakeWH(380, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  for ([[maybe_unused]] auto iteration : state) {
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Composer_SeenMeasure_600w)->Unit(benchmark::kMicrosecond);

// A 600-word story with one word in twenty replaced every frame — a
// scramble, a decode, a counter, a feed. The shape cache answers for the
// words that did not change and the composer decides the whole story's
// breaks again, because a word that moved moves every break after it.
void BM_Live_Composer_ContentChurn_600w(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  BlockFlow flow(SkRect::MakeWH(380, 40000));
  layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
  // Same-length replacements, so the churn moves glyphs and not offsets.
  const char8_t* alternatives[] = {u8"aaaa", u8"bbbb", u8"cccc", u8"dddd"};
  std::mt19937
      randomEngine(  // NOLINT(bugprone-random-generator-seed): a fixed corpus
          19);
  std::vector<uint32_t> starts;
  for (const Word& word : paragraph.words())
    if (word.textEnd - word.textBegin == 4) starts.push_back(word.textBegin);
  int frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    const size_t churn = starts.size() / 20;  // one word in twenty
    for (size_t index = 0; index < churn && !starts.empty(); ++index)
      paragraph.replaceText(starts[(size_t)(randomEngine() % starts.size())], 4,
                            alternatives[(frame + (int)index) % 4]);
    ++frame;
    ParagraphLayout layout =
        layoutParagraph(sigil::test::fonts(), paragraph, flow, options);
    benchmark::DoNotOptimize(layout.runs.data());
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Composer_ContentChurn_600w)->Unit(benchmark::kMicrosecond);

// A 600-word story refilled through a chain of six frames every frame —
// the threaded case, where each fill resumes at the word the one before it
// reported and the whole story is shaped once.
void BM_Live_Story_Refill_600w_SixFrames(benchmark::State& state) {
  Paragraph paragraph;
  paragraph.appendText(makeText(600, /*mixed=*/false), basicStyle());
  const ParagraphLayoutOptions options = liveComposer();
  for ([[maybe_unused]] auto iteration : state) {
    uint32_t cursor = 0;
    for (int frameIndex = 0; frameIndex < 6; ++frameIndex) {
      BlockFlow flow(SkRect::MakeWH(380, 700));
      ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph,
                                               flow, options, cursor);
      benchmark::DoNotOptimize(layout.runs.data());
      if (!layout.overflowed()) break;
      cursor = layout.firstUnplacedWord;
    }
  }
  countWords(state, 600);
}
BENCHMARK(BM_Live_Story_Refill_600w_SixFrames)->Unit(benchmark::kMicrosecond);

}  // namespace
