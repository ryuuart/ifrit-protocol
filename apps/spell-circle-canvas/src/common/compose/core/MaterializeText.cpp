/** @file
 * Text materialisation: the paragraph a text description becomes, and the
 * layout options the engine is asked for it under.
 */

#include <sigilmaterial/color/Color.h>

#include <algorithm>
#include <span>
#include <variant>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

sigil::weave::TextStyle Composer::Impl::leafStyle(const Instance& inst) const {
  const TextData& text = *inst.description->textData;
  if (text.inherits)
    return sigil::weave::toTextStyle(inst.cascadeResolved ? inst.font
                                                          : rootFont);
  if (!text.rich.empty()) return text.rich.base();
  return text.style;
}

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
  inst.inheritedInkRanges.clear();
  // AN INHERITING LEAF IS SET IN THE FONT IN FORCE where it stands — the
  // instance's resolved font once the cascade pass has run, the root's
  // before it, and the pass materialises again if the two differ. The
  // font it was set in is kept beside the paragraph so the pass can tell
  // a change that re-shapes from one that repaints alone.
  const bool inherits = text.inherits;
  const sigil::weave::Type& fontNow =
      inst.cascadeResolved ? inst.font : rootFont;
  const sigil::weave::TextStyle inherited =
      inherits ? sigil::weave::toTextStyle(fontNow) : sigil::weave::TextStyle{};
  if (inherits) inst.textFont = fontNow;
  if (text.paragraphOverride) {
    *inst.paragraph = *text.paragraphOverride;
  } else if (!text.rich.empty()) {
    // An inheriting rich text's unstyled runs are set in the font in
    // force, its partial runs over it, and only a run written with a whole
    // style keeps the style it was written with.
    const sigil::weave::TextStyle& base =
        inherits ? inherited : text.rich.base();
    // The runs concatenate with nothing between them: a rich text's spacing
    // is the author's own, exactly as it is in the strings they wrote.
    // The names the runs were written with resolve against the rules of
    // the sheets in force here, each run a virtual child of this leaf
    // whose class is its name, so a value built anywhere is set in the
    // classes of the tree it is placed in; a value carrying its own sheet
    // keeps that.
    sigil::weave::RichText resolved;
    const bool bySheet = !text.rich.hasStyles() && !inst.runStyles.empty();
    if (bySheet) {
      resolved = text.rich;
      resolved.styles(inst.runStyles);
    }
    const sigil::weave::RichText& rich = bySheet ? resolved : text.rich;
    for (const sigil::weave::RichText::Run& run : rich.runs()) {
      sigil::weave::TextStyle style = run.style;
      bool inksInherited = false;
      if (inherits && !run.total) {
        style = run.over ? sigil::weave::overlay(base, *run.over) : base;
        inksInherited = !(run.over && run.over->color);
      }
      if (!run.slotName.empty()) {
        // A slot run reserves a box instead of setting glyphs. The names go
        // into one list in declaration order, which is the order weave
        // matches its placeholder records to the U+FFFCs in the text.
        inst.textSlotKeys.push_back(run.slotName);
        inst.paragraph->appendPlaceholder(
            {run.slotSize.width(), run.slotSize.height(), run.slotBaselineDrop},
            style);
        continue;
      }
      // The extent a named run occupies, read off the text as it grows: a
      // name is a handle on THIS run's characters, not on the style span it
      // produced, so the restyles below may cut the spans to pieces and
      // selectors::style still answers with the run.
      const auto begin = (uint32_t)inst.paragraph->text().size();
      inst.paragraph->appendText(run.utf8, style);
      const auto end = (uint32_t)inst.paragraph->text().size();
      if (!run.styleName.empty())
        inst.textNamedRuns.push_back({run.styleName, {begin, end}});
      if (inksInherited)
        inst.inheritedInkRanges.push_back({{begin, end}, run.over});
    }
  } else if (inherits) {
    inst.paragraph->appendText(text.utf8, inherited);
    inst.inheritedInkRanges.push_back(
        {{0, (uint32_t)inst.paragraph->text().size()}, std::nullopt});
  } else {
    inst.paragraph->appendText(text.utf8, text.style);
  }
  // The writing mode and the line-break locale belong to the Paragraph,
  // not to the layout options, so the block in force lands them here. A
  // field the block leaves unset leaves a passed-in paragraph's own
  // standing. A path run has no columns to advance — its baseline IS the
  // geometry — so the path wins and says so.
  if (inst.block.writingMode)
    inst.paragraph->setWritingMode(*inst.block.writingMode);
  if (inst.block.lineBreakLocale)
    inst.paragraph->setLineBreakLocale(*inst.block.lineBreakLocale);
  inst.textBlock = inst.block;
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
  // LATER WINS PER DIMENSION, and the paint dimension belongs to the spans
  // that state paint alone: a reshaping span over text an earlier paint
  // span coloured applies its other dimensions and leaves that colour
  // standing. Otherwise the two would have to be declared in one
  // particular order to both take effect, with nothing said when they
  // were not — a reshaping span is laid over the whole style the range is
  // set in, so it carries a paint whether or not its author stated one,
  // and it would silently repaint the selection. Written as a REPLAY: the
  // style is applied whole, then every earlier paint span that reaches
  // the same characters is re-applied over it, in declaration order, so
  // the last one to cover a character is still the one standing.
  //
  // Every selection is resolved up front, against the text as written:
  // a restyle never edits the text, so the ranges hold, and the fold below
  // needs to know what the LATER restyles cover before it decides about an
  // earlier one.
  const size_t restyleCount = text.spanRestyles.size();
  std::vector<std::vector<sigil::weave::CharRange>> resolvedRanges(
      restyleCount);
  // A span is laid over the style the range is set in; one that names no
  // shaping field is applied as a repaint. Both are settled here, once,
  // and kept for the ink-only replay.
  const sigil::weave::TextStyle restyleBase = leafStyle(inst);
  std::vector<sigil::weave::TextStyle> resolvedStyles(restyleCount);
  std::vector<bool> resolvedPaintOnly(restyleCount);
  for (size_t i = 0; i < restyleCount; ++i) {
    const SpanRestyle& restyle = text.spanRestyles[i];
    resolvedStyles[i] = styleOfSpan(restyleBase, restyle, inst);
    resolvedPaintOnly[i] = !sigil::weave::reshapes(restyle.partial);
  }
  // The ranges are the painter's answer: text that carries none — a
  // description built without a text verb — is restyled by nothing.
  const TextPainterOperations* painter = textPainterOf(inst);
  for (size_t i = 0; i < restyleCount; ++i)
    if (painter)
      resolvedRanges[i] =
          painter->ranges(text.spanRestyles[i].where, *inst.paragraph, fonts,
                          lines, columns, inst.textNamedRuns, scopeOf(inst));
  if (inst.textState) inst.textState->spanAxisTracks.clear();
  // What an ink-only repaint replays: the ranges as resolved here, and
  // which restyles the fold below took instead of applying.
  if (restyleCount > 0 || inst.textState) {
    TextState& state = textStateOf(inst);
    state.restyleRanges = resolvedRanges;
    state.restyleFolded.assign(restyleCount, false);
    state.restyleStyles = resolvedStyles;
    state.restylePaintOnly = resolvedPaintOnly;
  }
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
  // Nothing is carried until a paint span has actually painted something,
  // and a passage that declares none takes neither the search nor the
  // replay below.
  bool paintDeclared = false;
  for (size_t i = 0; i < restyleCount; ++i) {
    const SpanRestyle& restyle = text.spanRestyles[i];
    const sigil::weave::TextStyle& style = resolvedStyles[i];
    const std::vector<sigil::weave::CharRange>& ranges = resolvedRanges[i];
    if (ranges.empty()) continue;
    if (resolvedPaintOnly[i]) {
      // The batch form: N ranges cost one span-list rebuild, and shaping
      // keys are untouched, so nothing re-shapes and nothing relayouts.
      inst.paragraph->setPaint(ranges, style.paint);
      paintDeclared = true;
      continue;
    }
    // The text this restyle covers whose paint an earlier paint span
    // owns — the merge, resolved as ranges: each piece with the paint that
    // owns it, in declaration order, and the pieces alone for the fold.
    std::vector<std::pair<std::vector<sigil::weave::CharRange>,
                          const sigil::weave::PaintStyle*>>
        carried;
    std::vector<sigil::weave::CharRange> carriedRanges;
    if (paintDeclared)
      for (size_t j = 0; j < i; ++j) {
        if (!resolvedPaintOnly[j]) continue;
        std::vector<sigil::weave::CharRange> shared =
            overlap(ranges, resolvedRanges[j]);
        if (shared.empty()) continue;
        carriedRanges.insert(carriedRanges.end(), shared.begin(), shared.end());
        carried.emplace_back(std::move(shared), &resolvedStyles[j].paint);
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
    if (painter && painter->foldable(inst, style, ranges, *inst.paragraph,
                                     carriedRanges, folded)) {
      bool coveredLater = false;
      for (size_t j = i + 1; j < restyleCount && !coveredLater; ++j) {
        if (resolvedPaintOnly[j]) continue;
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
        textStateOf(inst).restyleFolded[i] = true;
        continue;
      }
    }
    for (const sigil::weave::CharRange& range : ranges)
      inst.paragraph->setStyle(range.start, range.end, style);
    // …and the earlier paints back over it, so the style's own paint
    // stands only where no paint span reached.
    for (const auto& [where, paint] : carried)
      inst.paragraph->setPaint(where, *paint);
  }
}

sigil::weave::TextStyle Composer::Impl::styleOfSpan(
    const sigil::weave::TextStyle& base, const SpanRestyle& span,
    const Instance& inst) const {
  sigil::weave::Type partial = span.partial;
  if (span.inkVar) {
    const VarValue* value = inst.vars ? inst.vars->find(*span.inkVar) : nullptr;
    const material::Color* colour =
        value ? std::get_if<material::Color>(value) : nullptr;
    if (colour)
      partial.color = *colour;
    else
      warnNoSuchVar(*span.inkVar, true);
  }
  sigil::weave::TextStyle style = sigil::weave::overlay(base, partial);
  // A paint stated as the ink IS the ink: its own alpha rules, so the
  // colour it replaces must not fade it.
  if (span.inkShader) {
    style.paint.foreground.setShader(span.inkShader->shaderValue);
    style.paint.foreground.setAlphaf(1.0f);
  }
  return style;
}

sigil::weave::ParagraphLayoutOptions Composer::Impl::textLayoutOptions(
    const Instance& inst) const {
  sigil::weave::ParagraphLayoutOptions options;
  if (!inst.description || !inst.description->textData) return options;
  const TextData& text = *inst.description->textData;
  // The passed value is the ground the setters are written over, so a
  // full-control caller keeps every field no setter named.
  options = text.layoutOptions;
  // The options in force: the leaf's own over what its matched rules say.
  const TextOptions& inForce = inst.textOptions;
  inForce.applyTo(options);
  // The layout-wide fields the block in force states — alignment, the
  // breaking strategy, hyphenation, justification, tab stops, the line
  // tables — over what the leaf's own options and a passed-in layout hold.
  sigil::weave::apply(options, inst.block);
  // THE BLOCK IN FORCE: what every block the leaf's own list does not
  // reach is set in, and what a named block's partial is laid over. A
  // whole style the leaf wrote inherits nothing, as a whole text style
  // does. The initial letter lands on the first block whichever way that
  // block was styled.
  const sigil::weave::ParagraphStyle lane =
      sigil::weave::toParagraphStyle(inst.block);
  options.blockDefault = lane;
  if (inForce.set & TextOptions::kBlockClasses) {
    // The names resolve against the rules of the sheets in force here; a
    // name no rule speaks about changes nothing about its block, and says
    // so.
    options.blocks.clear();
    for (const std::string& name : inForce.blockClassNames) {
      const auto matched = std::find_if(
          inst.blockStyles.begin(), inst.blockStyles.end(),
          [&](const auto& entry) { return entry.first == name; });
      if (matched == inst.blockStyles.end()) {
        warnNoSuchParagraphStyle(name, inst.sheetsInForce);
        options.blocks.push_back(lane);
      } else {
        options.blocks.push_back(sigil::weave::overlay(lane, matched->second));
      }
    }
  }
  if ((inForce.set & TextOptions::kInitialLetter) && inForce.initial) {
    if (options.blocks.empty()) options.blocks.push_back(lane);
    options.blocks.front().initial = *inForce.initial;
  }
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
