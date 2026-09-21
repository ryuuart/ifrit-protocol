/** @file
 * The text properties — the blocks a passage is styled in, the versal
 * it opens with, where its first baseline sits, what becomes of the
 * room left over, the room beside every line, what happens when it
 * overflows, and the two glyph paints — with the fold of all of them
 * onto SigilWeave's layout options.
 */

#include <sigilweave/unicode/Unicode.h>

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& TextStyleVerbs<Derived>::paragraphs(
    std::vector<sigil::weave::ParagraphStyle> blocks) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.blocks = std::move(blocks);
  options.set |= detail::TextOptions::kBlocks;
  // The two spellings are alternatives, and the last one written stands.
  options.blockClassNames.clear();
  options.set &= ~(uint32_t)detail::TextOptions::kBlockClasses;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::paragraphs(
    std::span<const std::string_view> names) {
  // The names are kept; they resolve against the block sheet in force
  // where the leaf lands, when it lays out, and lie over the block in
  // force there.
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.blockClassNames.assign(names.begin(), names.end());
  options.set |= detail::TextOptions::kBlockClasses;
  options.blocks.clear();
  options.set &= ~(uint32_t)detail::TextOptions::kBlocks;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::initialLetter(
    sigil::weave::InitialLetter initial) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  // The initial belongs to the passage's first block, whichever way that
  // block is styled — a whole style, a name, or the block in force — so
  // it is kept apart and applied to the first block when the leaf lays
  // out.
  options.initial = std::move(initial);
  options.set |= detail::TextOptions::kInitialLetter;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::firstBaseline(
    sigil::weave::FrameOptions::FirstBaseline rule, float offset) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.frame.firstBaseline = rule;
  options.frame.firstBaselineOffset = offset;
  options.set |= detail::TextOptions::kFrame;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::distribute(
    sigil::weave::FrameOptions::Distribute rule,
    float maximumInterlineSpacing) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.frame.distribute = rule;
  options.frame.maximumInterlineSpacing = maximumInterlineSpacing;
  options.set |= detail::TextOptions::kFrame;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::reserve(sigil::weave::ReservedBand band) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.reserved = band;
  options.set |= detail::TextOptions::kReserved;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::live(bool on, int candidates) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.live = on;
  options.candidates = candidates;
  options.set |= detail::TextOptions::kLive;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::ellipsis(Utf8 marker) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.ellipsis = weave::unicode::toUtf16(marker.bytes());
  options.set |= detail::TextOptions::kEllipsis;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::maxLines(int lines) {
  detail::TextOptions& options = declarations()->textData.ensure().options;
  options.maxLines = lines;
  options.set |= detail::TextOptions::kMaxLines;
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::textFill(SurfacePaint paint) {
  detail::TextData& text = declarations()->textData.ensure();
  // An empty paint CLEARS the override, because that is what asking for
  // no glyph paint means. A fill the slot cannot store — the ink in
  // force, a custom property, a bound fill — leaves whatever paint the
  // glyphs already carry: blanking it would repaint them in a colour
  // nobody named, and the reference the caller wrote is the colour they
  // are painted in without an override anyway.
  if (paint.none())
    text.metricFill.reset();
  else if (std::optional<material::skia::Paint> stored = paint.collapsedPaint())
    text.metricFill = std::move(stored);
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::textStroke(float width,
                                             SurfacePaint paint) {
  detail::TextData& text = declarations()->textData.ensure();
  text.hasTextStroke = width > 0.0f;
  text.textStrokeWidth = width;
  // The outline is one comparable Fill on the node, so a plain fill and
  // a static paint collapse onto it. A live or geometry-dependent paint
  // has no single colour to give a slot that is measured without a
  // frame, and the glyphs are outlined in the ink in force rather than
  // in the black an empty fill would leave them.
  text.textStrokeFill = paint.collapsedFill().value_or(Fill::currentInk());
  return self();
}

template <class Derived>
Derived& TextStyleVerbs<Derived>::contentFlowAround(std::string_view key,
                                                    float margin) {
  detail::DeriveData& derive = declarations()->deriveData.ensure();
  derive.flowAroundKeys.emplace_back(key);
  derive.flowAroundMargin = margin;
  // An exclusion subtracts the target's SILHOUETTE where it declares one,
  // which is a read of its outline and not merely of its box.
  derive.reads.push_back({std::string(key), sigil::core::Facet::Outline});
  return self();
}

template class TextStyleVerbs<Element>;

void detail::TextOptions::applyTo(
    sigil::weave::ParagraphLayoutOptions& options) const {
  if (set & kEllipsis) options.overflow.ellipsis = ellipsis;
  if (set & kMaxLines) options.overflow.maxLines = maxLines;
  if (set & kBlocks) options.blocks = blocks;
  if (set & kFrame) options.frame = frame;
  if (set & kLive) {
    options.live = live;
    options.knuthPlass.candidates = candidates;
  }
  if (set & kReserved) options.reserved = reserved;
}

}  // namespace sigil::compose
