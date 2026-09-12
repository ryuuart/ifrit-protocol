/** @file
 * Text materialisation: the paragraph a text description becomes, and the
 * layout options the engine is asked for it under.
 */

#include <algorithm>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

void Composer::Impl::materializeText(
    Instance& inst, std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns) {
  const TextData& text = *inst.description->textData;
  inst.paragraph.emplace();
  // Cleared for every content form, so the names a node answers for are
  // exactly the ones its CURRENT content declares.
  inst.textSlotKeys.clear();
  inst.textSlotRects.clear();
  inst.textNamedRuns.clear();
  if (text.paragraphOverride) {
    *inst.paragraph = *text.paragraphOverride;
  } else if (!text.rich.empty()) {
    // The runs concatenate with nothing between them: a rich text's spacing
    // is the author's own, exactly as it is in the strings they wrote.
    for (const sigil::weave::RichText::Run& run : text.rich.runs()) {
      if (!run.slotName.empty()) {
        // A slot run reserves a box instead of setting glyphs. The names go
        // into one list in declaration order, which is the order weave
        // matches its placeholder records to the U+FFFCs in the text.
        inst.textSlotKeys.push_back(run.slotName);
        inst.paragraph->appendPlaceholder(
            {run.slotSize.width(), run.slotSize.height(), run.slotBaselineDrop},
            run.style);
        continue;
      }
      // The extent a named run occupies, read off the text as it grows: a
      // name is a handle on THIS run's characters, not on the style span it
      // produced, so the restyles below may cut the spans to pieces and
      // selectors::style still answers with the run.
      const auto begin = (uint32_t)inst.paragraph->text().size();
      inst.paragraph->appendText(run.utf8, run.style);
      if (!run.styleName.empty())
        inst.textNamedRuns.push_back(
            {run.styleName, {begin, (uint32_t)inst.paragraph->text().size()}});
    }
  } else {
    inst.paragraph->appendText(text.utf8, text.style);
  }
  // The writing mode is the Paragraph's, not the layout options', so the
  // field-masked override lands here: a mode nobody set leaves a passed-in
  // paragraph's own mode standing. A path run has no columns to advance —
  // its baseline IS the geometry — so the path wins and says so.
  if (text.options.set & TextOptions::kWritingMode)
    inst.paragraph->setWritingMode(text.options.writingMode);
  // The line-break tailoring is the Paragraph's too, and lands under the
  // same mask rule: a locale nobody named leaves a passed-in paragraph's
  // own standing.
  if (text.options.set & TextOptions::kLineBreakLocale)
    inst.paragraph->setLineBreakLocale(text.options.lineBreakLocale);
  if (text.onPath &&
      inst.paragraph->writingMode() != sigil::weave::WritingMode::kHorizontal) {
    warnWritingModeOnPath();
    inst.paragraph->setWritingMode(sigil::weave::WritingMode::kHorizontal);
  }

  // The restyles run in DECLARATION ORDER over the finished paragraph, so a
  // later one simply overwrites the spans an earlier one wrote wherever the
  // two overlap — which is the "later wins" rule, spelled as span surgery
  // rather than as a merge nobody could predict.
  //
  // LATER WINS PER DIMENSION, and the paint dimension is `spanPaint`'s: a
  // `spanStyle` over text an earlier `spanPaint` coloured applies its
  // other dimensions and leaves that colour standing. Otherwise the two
  // verbs would have to be declared in one particular order to both take
  // effect, with nothing said when they were not — the style, being a
  // whole style, carries a paint whether or not its author was thinking
  // about paint, and it would silently repaint the selection its own
  // default. Written as a REPLAY: the style is applied whole, then every
  // earlier `spanPaint` that reaches the same characters is re-applied
  // over it, in declaration order, so the last one to cover a character
  // is still the one standing.
  //
  // Every selection is resolved up front, against the text as written:
  // a restyle never edits the text, so the ranges hold, and the fold below
  // needs to know what the LATER restyles cover before it decides about an
  // earlier one.
  const size_t restyleCount = text.spanRestyles.size();
  std::vector<std::vector<sigil::weave::CharRange>> resolvedRanges(
      restyleCount);
  // The ranges are the painter's answer: text that carries none — a
  // description built without a text verb — is restyled by nothing.
  const TextPainterOperations* painter = textPainterOf(inst);
  for (size_t i = 0; i < restyleCount; ++i)
    if (painter)
      resolvedRanges[i] =
          painter->ranges(text.spanRestyles[i].where, *inst.paragraph, fonts,
                          lines, columns, inst.textNamedRuns, scopeOf(inst));
  if (inst.textState) inst.textState->spanAxisTracks.clear();
  // The intersection of two selections, as the ranges they share.
  const auto overlap = [](std::span<const sigil::weave::CharRange> a,
                          std::span<const sigil::weave::CharRange> b) {
    std::vector<sigil::weave::CharRange> shared;
    for (const sigil::weave::CharRange& x : a)
      for (const sigil::weave::CharRange& y : b)
        if (x.start < y.end && y.start < x.end)
          shared.push_back(
              {std::max(x.start, y.start), std::min(x.end, y.end)});
    return shared;
  };
  // Nothing is carried until a `spanPaint` has actually painted something,
  // and a passage that declares none takes neither the search nor the
  // replay below.
  bool paintDeclared = false;
  for (size_t i = 0; i < restyleCount; ++i) {
    const SpanRestyle& restyle = text.spanRestyles[i];
    const std::vector<sigil::weave::CharRange>& ranges = resolvedRanges[i];
    if (ranges.empty()) continue;
    if (restyle.paintOnly) {
      // The batch form: N ranges cost one span-list rebuild, and shaping
      // keys are untouched, so nothing re-shapes and nothing relayouts.
      inst.paragraph->setPaint(ranges, restyle.style.paint);
      paintDeclared = true;
      continue;
    }
    // The text this restyle covers whose paint an earlier `spanPaint`
    // owns — the merge, resolved as ranges: each piece with the paint that
    // owns it, in declaration order, and the pieces alone for the fold.
    std::vector<std::pair<std::vector<sigil::weave::CharRange>,
                          const sigil::weave::PaintStyle*>>
        carried;
    std::vector<sigil::weave::CharRange> carriedRanges;
    if (paintDeclared)
      for (size_t j = 0; j < i; ++j) {
        if (!text.spanRestyles[j].paintOnly) continue;
        std::vector<sigil::weave::CharRange> shared =
            overlap(ranges, resolvedRanges[j]);
        if (shared.empty()) continue;
        carriedRanges.insert(carriedRanges.end(), shared.begin(), shared.end());
        carried.emplace_back(std::move(shared),
                             &text.spanRestyles[j].style.paint);
      }
    // THE FOLD. A style that differs from the text it covers only in
    // advance-invariant variable-font axes — a grade over the numerals, an
    // optical size over a heading — does not need the words re-shaped to be
    // honoured: the glyphs keep the pen positions shaping gave them and the
    // coordinate reaches them at draw time, as a track. Anything else the
    // style changes is a reshape, and so is an axis the face moves advances
    // on, so the restyle falls through to the span surgery below. The
    // fold keeps the "later wins" rule by declining wherever a LATER
    // reshaping restyle covers the same text: a track deviates whatever
    // the paragraph shaped, and a later style must be the one that stands.
    std::vector<std::pair<std::string, float>> folded;
    if (painter && painter->foldable(inst, restyle.style, ranges,
                                     *inst.paragraph, carriedRanges, folded)) {
      bool coveredLater = false;
      for (size_t j = i + 1; j < restyleCount && !coveredLater; ++j) {
        if (text.spanRestyles[j].paintOnly) continue;
        for (const sigil::weave::CharRange& a : ranges)
          for (const sigil::weave::CharRange& b : resolvedRanges[j])
            if (a.start < b.end && b.start < a.end) coveredLater = true;
      }
      if (!coveredLater) {
        for (const auto& [tag, value] : folded) {
          const char axis[5] = {tag[0], tag[1], tag[2], tag[3], '\0'};
          Track track;
          track.where = restyle.where;
          track.effect = TextEffect::variableAxis(axis, value);
          textStateOf(inst).spanAxisTracks.push_back(std::move(track));
        }
        continue;
      }
    }
    for (const sigil::weave::CharRange& range : ranges)
      inst.paragraph->setStyle(range.start, range.end, restyle.style);
    // …and the earlier paints back over it, so the style's own paint
    // stands only where no `spanPaint` reached.
    for (const auto& [where, paint] : carried)
      inst.paragraph->setPaint(where, *paint);
  }
}

sigil::weave::ParagraphLayoutOptions Composer::Impl::textLayoutOptions(
    const Instance& inst) const {
  sigil::weave::ParagraphLayoutOptions options;
  if (!inst.description || !inst.description->textData) return options;
  const TextData& text = *inst.description->textData;
  // The passed value is the ground the setters are written over, so a
  // full-control caller keeps every field no setter named.
  options = text.layoutOptions;
  text.options.applyTo(options);
  // OVERFLOW IS THE NORMAL CASE ON EVERY FRAME BUT THE LAST. A frame that
  // threads into another has a remainder by design, and a marker there
  // would say the text was cut when it was only continued; the last frame
  // of a chain is the one that threads nowhere, and it keeps whatever
  // ellipsis the leaf asked for.
  if (!text.threadTo.empty()) options.overflow.ellipsis.clear();
  // THE NEXT FRAME'S MEASURE, which only the chain knows and the widow
  // rule needs: the lines it counts are the remainder, and the remainder
  // is set in the frame after this one. 0 until the chain has been walked
  // once, which is weave's "not known".
  options.nextMeasure = inst.threadNextMeasure;
  // THE BAND A RESERVING READING NEEDS, asked before anything is broken and
  // answered from the reading's own metrics — which is the whole of why a
  // reservation is a layout input and not a cycle. Only the engine can
  // measure a face, so the painter answers; a text that dresses nothing has
  // no annotations either.
  if (!text.annotations.empty()) {
    const TextPainterOperations* painter = textPainterOf(inst);
    if (!painter) painter = detail::registeredTextEngine();
    if (painter) {
      const sigil::weave::ReservedBand band =
          painter->reservedBand(const_cast<Instance&>(inst), text.annotations);
      options.reserved.before += band.before;
      options.reserved.after += band.after;
    }
  }
  return options;
}

}  // namespace sigil::compose
