/** @file
 * The text leaf at layout: the measure and baseline callbacks Yoga asks a
 * paragraph through, the on-demand layout behind them, and whether a
 * selector's answer depends on where the lines broke.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <ranges>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

YGSize detail::measureTextNode(YGNodeConstRef node, float width,
                               YGMeasureMode widthMode, float height,
                               YGMeasureMode heightMode) {
  auto* inst =
      static_cast<Instance*>(YGNodeGetContext(const_cast<YGNodeRef>(node)));
  const float constraint = widthMode == YGMeasureModeUndefined ? 1.0e6f : width;
  // The HEIGHT is a measure too once the text runs down the page: it is what
  // a column may fill before the next one starts, the job the width does for
  // a horizontal line. A horizontal leaf ignores it, exactly as it always
  // has.
  const float down = heightMode == YGMeasureModeUndefined ? 1.0e6f : height;
  inst->owner->layoutText(*inst, constraint, down);
  return inst->measuredSize;
}

namespace {

/** WHERE THE FIRST CHARACTER'S BASELINE SITS, down from the top of what the
 *  layout placed — the one number `Align::Baseline` reads off a text leaf.
 *
 *  A horizontal leaf answers with its first line's ascent. A VERTICAL leaf
 *  HAS NO BASELINE: its reading axis is y, and a column's glyphs centre
 *  themselves ACROSS the axis instead of standing on one. It answers with
 *  its first character's own baseline all the same, because that is the
 *  same question asked of the same glyph — which lines a column's opening
 *  character up with a horizontal neighbour's first line, and is the only
 *  answer either writing mode can give the other. */
float textBaseline(const Instance& inst, const SkRect& bounds) {
  if (!inst.lines.empty()) {
    const sigil::weave::LineMetrics& first = inst.lines.front();
    return first.baseline - first.rect().top();
  }
  if (inst.columns.empty()) return 0.0f;
  // Runs arrive in logical order, so the first one carrying glyphs holds the
  // first character. A rotated run is skipped: its placement is baked per
  // glyph and it has no upright baseline to report.
  for (const sigil::weave::PositionedRun& run : inst.textLayout.runs) {
    if (!run.shaped || run.transformed || run.shaped->positions.empty())
      continue;
    return run.origin.y() + run.shaped->positions.front().y() - bounds.top();
  }
  return 0.0f;
}

}  // namespace

float detail::baselineOfTextNode(YGNodeConstRef node, float, float) {
  auto* inst =
      static_cast<Instance*>(YGNodeGetContext(const_cast<YGNodeRef>(node)));
  return inst->measuredBaseline;
}

namespace {

/** Whether @p inst is a FRAME rather than an ordinary text leaf — a link
 *  of a chain over one story, which is bounded by its own depth where a
 *  leaf grows down the page.
 *
 *  A link is one either because it threads onward, which its own
 *  description says, or because something threads into it, which only the
 *  chain walk knows. The LAST link is the second case alone, and it has to
 *  be bounded too: unbounded it holds the whole remainder and draws it
 *  past its box, and the marker it asked for never lands because it never
 *  runs out. */
bool isFrameOfAChain(const Instance& inst) {
  return inst.threadedInto || (inst.description && inst.description->textData &&
                               !inst.description->textData->threadTo.empty());
}

}  // namespace

void Composer::Impl::layoutText(Instance& inst, float constraint,
                                float downConstraint) {
  // onPath: the PATH is the measure, not the box. Laying the run out to
  // the node's width would wrap it, and every line after the first would
  // then be placed along the path from the start again — the glyphs pile
  // up on each other. The box still sizes the path; it does not bound the
  // run.
  if (inst.description && inst.description->textData &&
      inst.description->textData->onPath)
    constraint = 1.0e6f;
  if (constraint == inst.measuredForWidth &&
      downConstraint == inst.measuredForHeight &&
      inst.measuredRev == inst.contentRev)
    return;  // layout is already valid for this content and measure
  sigil::weave::ParagraphLayoutOptions options = textLayoutOptions(inst);
  // HOW DEEP THE FRAME IS is a fact only this side knows: weave is handed a
  // geometry, not a box, and its vertical distribution and first-baseline
  // rule need the depth the node resolved to. A leaf sized by its own
  // content has no room left over, which is what an unconstrained measure
  // reports here.
  if (options.frame.distribute !=
      sigil::weave::FrameOptions::Distribute::kStart)
    options.frame.extent = downConstraint < 1.0e6f ? downConstraint : 0.0f;
  // Vertical-RL: the geometry is columns, not bands, and they hang off the
  // RIGHT edge of the measure — so the constraint is not just a wrap width
  // here, it is where the first column stands. That is why a vertical leaf
  // must be laid out again at its RESOLVED width before it paints, the way
  // aligned horizontal text must (StackingPainter.cpp).
  const bool vertical =
      inst.paragraph &&
      inst.paragraph->writingMode() == sigil::weave::WritingMode::kVerticalRL;
  // One weave silhouette per resolved target, in the form the derive pass
  // resolved it to: an outline for a target whose boundary answered one, an
  // analytic circle for a round one, its box for a target that answered
  // none. The margin is the same DISC standoff in all three, and the
  // silhouettes are the same in both writing modes — an exclusion cuts a
  // column exactly as it cuts a line, so only the flow's axis differs.
  const auto addExclusions = [&](sigil::weave::ExclusionFlow& flow) {
    const float flowMargin =
        inst.description->deriveData
            ? inst.description->deriveData->flowAroundMargin
            : 0.0f;
    for (const detail::Exclusion& exclusion : inst.exclusionsLocal) {
      if (exclusion.circle)
        flow.exclusions().push_back(
            {sigil::weave::silhouette::circle(exclusion.bounds), flowMargin});
      else if (!exclusion.path.isEmpty())
        flow.exclusions().push_back(
            {sigil::weave::silhouette::path(exclusion.path), flowMargin});
      else
        flow.exclusions().push_back(
            {sigil::weave::silhouette::rectangle(exclusion.bounds),
             flowMargin});
    }
  };
  const auto layOut = [&] {
    if (vertical && !inst.exclusionsLocal.empty()) {
      sigil::weave::ExclusionFlow flow(
          SkRect::MakeWH(constraint, downConstraint),
          sigil::weave::FlowAxis::kColumns);
      addExclusions(flow);
      inst.textLayout = sigil::weave::layoutParagraph(
          fonts, *inst.paragraph, flow, options, inst.threadCursor);
    } else if (vertical) {
      sigil::weave::VerticalBlockFlow flow(
          SkRect::MakeWH(constraint, downConstraint));
      inst.textLayout = sigil::weave::layoutParagraph(
          fonts, *inst.paragraph, flow, options, inst.threadCursor);
    } else if (!inst.exclusionsLocal.empty()) {
      const float depth = isFrameOfAChain(inst) ? downConstraint : 1.0e6f;
      sigil::weave::ExclusionFlow flow(SkRect::MakeWH(constraint, depth));
      addExclusions(flow);
      inst.textLayout = sigil::weave::layoutParagraph(
          fonts, *inst.paragraph, flow, options, inst.threadCursor);
    } else {
      // A HORIZONTAL LEAF GROWS: its height is an answer, not a measure,
      // so its flow is unbounded down the page and the box clips whatever
      // does not fit. A FRAME IS THE EXCEPTION, and it is what makes a
      // frame a frame: a leaf that threads into another is bounded by its
      // own depth, so it runs out of room and the remainder is what the
      // next frame begins at.
      const float depth = isFrameOfAChain(inst) ? downConstraint : 1.0e6f;
      sigil::weave::BlockFlow flow(SkRect::MakeWH(constraint, depth));
      inst.textLayout = sigil::weave::layoutParagraph(
          fonts, *inst.paragraph, flow, options, inst.threadCursor);
    }
  };
  // ONE of the two is populated, and which one is the writing mode: a
  // column has no baseline to report and lineMetrics() answers with
  // nothing there, while columnMetrics() answers with nothing in a
  // horizontal passage.
  const auto readGeometry = [&] {
    inst.lines.clear();
    inst.columns.clear();
    if (vertical)
      inst.columns = inst.textLayout.columnMetrics(*inst.paragraph);
    else
      inst.lines = inst.textLayout.lineMetrics(*inst.paragraph);
  };
  layOut();
  readGeometry();
  // A weave::sel::line span restyle needs line geometry to name a line at all,
  // and the materialization that ran at describe time had none. Re-materialize
  // against the lines just produced — plain values, so the paragraph they
  // came from is free to go — and lay out once more. The WHOLE restyle list
  // runs again in declaration order, so the "later wins" rule holds across
  // the line-scoped ones and the rest alike.
  //
  // It resolves against THE TEXT BEFORE THE RESTYLE and stops there: a
  // spanStyle that moves the line breaks does not chase its own result,
  // which is what keeps this two passes rather than a fixed-point search
  // that may not have a fixed point.
  if (inst.description->textData &&
      std::ranges::any_of(inst.description->textData->spanRestyles,
                          [](const detail::SpanRestyle& restyle) {
                            return detail::selectorNeedsLayout(restyle.where);
                          })) {
    materializeText(inst, inst.lines, inst.columns);
    layOut();
    readGeometry();
  }
  // weave::rich().slot(): where the finished layout put each reserved box.
  // Resolved once per layout rather than per read, and by NAME rather than by
  // index, because a slot the geometry could not place is simply absent from
  // the report and every later slot would otherwise shift up onto its rect.
  if (inst.paragraph && !inst.textSlotKeys.empty()) {
    inst.textSlotRects.clear();
    for (const sigil::weave::ParagraphLayout::PlacedPlaceholder& placed :
         inst.textLayout.placeholderRects(*inst.paragraph)) {
      const size_t index = (size_t)placed.index;
      if (index < inst.textSlotKeys.size())
        inst.textSlotRects.emplace_back(inst.textSlotKeys[index], placed.rect);
    }
  }
  // mark(): where each anchored child's selector landed, resolved here for
  // the same reason the slot rects are — the layout has just finished and
  // is the only thing that knows, and resolving once per layout rather than
  // once per read keeps a mark's box as cheap as a slot's. A PATH-laid
  // run's marks are the one exception: their curve resolves against the
  // node's final box, which this measure does not know, so they resolve in
  // ensureLayout's post-layout pass instead.
  if (!inst.description->textData || !inst.description->textData->onPath)
    resolveTextMarks(inst);
  // The readings, laid out on the placement the base just reached. Their
  // band was already in the base's strut, so nothing here moves the base.
  resolveTextAnnotations(inst);
  // WHAT THIS LAYOUT COST, reported for the proof that the node is holding
  // still. A live passage answered entirely from break decisions it already
  // had is set exactly as the frame before it and did no work; one that
  // still decided a break, or degraded because the budget ran out, may be
  // set differently the next frame with no number on this node changing.
  //
  // A DEGRADE IS PROVISIONAL. The block was filled greedily for this frame
  // alone and the setting the author asked for is still what the passage
  // wants, so the layout is NOT held as valid for this measure: the next
  // frame asks again, and everything is back the frame the budget is met.
  inst.textReusedBlocks = inst.textLayout.reusedBlocks;
  inst.textDegradedBlocks = inst.textLayout.degradedBlocks;
  inst.textComposing = options.live && (inst.textLayout.reusedBlocks == 0 ||
                                        inst.textLayout.degradedBlocks > 0);
  inst.measuredForWidth =
      inst.textLayout.degradedBlocks > 0 ? -1.0f : constraint;
  inst.measuredForHeight = downConstraint;
  SkRect bounds = SkRect::MakeEmpty();
  for (const sigil::weave::LineMetrics& line : inst.lines)
    bounds.join(line.rect());
  for (const sigil::weave::ColumnMetrics& column : inst.columns)
    bounds.join(column.rect());
  // THE AXES SWAP. A horizontal passage grows along x and stacks on y; a
  // vertical one grows along y and stacks on x, so the same union answers
  // both — one column of type measures tall and one pitch wide.
  inst.measuredSize = {std::ceil(bounds.width()), std::ceil(bounds.height())};
  inst.measuredBaseline = textBaseline(inst, bounds);
  inst.measuredRev = inst.contentRev;
  // WHERE THE GLYPHS REACH, which is not where the lines do. The union
  // above is the band of every line — its tallest ascent over its deepest
  // descent — and a face is free to draw outside it: a comma's tail sits
  // below the descent, an accent on a capital above the ascent. The band
  // is what the box is measured to, so the ink that hangs past it is
  // painted outside the node's box and anything that sizes a SURFACE from
  // that box cuts it. Taken from the placed blobs, whose bounds a shaped
  // word already carries, so the walk costs no rasterisation. The
  // readings count too: they are drawn in this node's space, from
  // placements this pass has just resolved.
  const auto inkOf = [](const sigil::weave::ParagraphLayout& layout) {
    SkRect ink = SkRect::MakeEmpty();
    for (const sigil::weave::PositionedRun& run : layout.runs) {
      if (!run.blob) continue;  // a placeholder run draws no glyph
      SkRect box = run.blob->bounds();
      // A transformed run's placement is baked into its blob and it draws
      // at the origin; an ordinary one is a shared word blob translated.
      if (!run.transformed) box.offset(run.origin.fX, run.origin.fY);
      ink.join(box);
    }
    return ink;
  };
  SkRect ink = inkOf(inst.textLayout);
  for (const Instance::PlacedAnnotation& reading : inst.textAnnotations)
    ink.join(inkOf(reading.layout));
  if (ink != inst.textInk) {
    // A LEAF OF A STATED SIZE IS LAID OUT AT PAINT, because a box that
    // never reaches the measure callback learns its depth nowhere else —
    // and the bounds a surface was sized from were read before that. So
    // the first ink a leaf reports is news to every recording and bake
    // above it, and they are staled the way any other change to what a
    // node paints stales them. A leaf laid out during the layout phase
    // reports the same rect it reported last time and stales nothing.
    inst.textInk = ink;
    inst.markPaintDirtyUp();
  }
}

bool detail::selectorNeedsLayout(const sigil::weave::Selector& selector) {
  const sigil::weave::Selector::State* s = selector.state();
  if (!s) return false;
  if (s->kind == sigil::weave::Selector::Kind::Line) return true;
  if (s->kind == sigil::weave::Selector::Kind::Each &&
      s->each == sigil::weave::Unit::Line)
    return true;
  for (const sigil::weave::Selector& operand : s->operands)
    if (selectorNeedsLayout(operand)) return true;
  return false;
}

}  // namespace sigil::compose
