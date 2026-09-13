/** @file
 * The text verbs — the ones that name a block property and are the
 * block lane's spellings, one field each, inherited by every leaf under
 * the node; and the leaf's own: the glyph stroke, the overflow, the
 * frame, the frame chain and the exclusion — with the leaf's options'
 * fold onto SigilWeave's layout options and the UTF-16 boundary the leaf
 * speaks to it across — and the leaf as it stands at rest, which is a
 * copy of its description. The verbs that dress type are declared beside
 * these and defined by the typography tier.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the rest-of-non-text diagnostic
#include <sigilweave/unicode/Unicode.h>

#include "ComposeInternal.h"

namespace sigil::compose {

namespace {
void warnAtRestOfNonText() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] atRest() on an element that is not text() hands back a "
      "plain copy: a rest pose is what an fx() track's per-glyph deviation "
      "is measured against, and every other element already draws where "
      "its layout put it.\n");
}
}  // namespace

Element Element::atRest() const {
  const std::shared_ptr<detail::ElementNode>& source = node();
  if (source->kind != detail::Kind::Text || !source->textData) {
    warnAtRestOfNonText();
    return *this;
  }
  // A COPY OF THE DESCRIPTION, not a re-description: the copy has to be
  // the same paragraph, laid out the same way, at the same width, or the
  // two disagree about where a letter belongs and a comparison against it
  // is worthless. Everything that could shift a glyph is therefore carried
  // over untouched, and only what moves or restyles one at paint time
  // goes.
  auto rest = std::make_shared<detail::ElementNode>(*source);
  detail::TextData& text = rest->textData.ensure();
  text.tracks.clear();
  text.spanRestyles.clear();
  // …AND NO CHILDREN, which is the same rule read twice. A text node's
  // children are its marks and its slot mounts, and both are already on
  // screen once: copying them would draw each of them twice under a
  // duplicate key, which the composer's key index cannot answer for. The
  // slot RUNS stay — they are content, and they reserve the same space in
  // the copy's paragraph, which is what keeps the two copies' letters in
  // the same places.
  rest->children.clear();
  text.marks.clear();
  if (!rest->key.empty()) rest->key += "-rest";
  return Element{std::move(rest)};
}

Element& Element::textStroke(float width, Fill fill) {
  auto& t = m_node->textData.ensure();
  t.hasTextStroke = width > 0.0f;
  t.textStrokeWidth = width;
  t.textStrokeFill = std::move(fill);
  return *this;
}

Element& Element::textAlign(sigil::weave::TextAlignment a) {
  return block({.alignment = a});
}

Element& Element::writingMode(sigil::weave::WritingMode mode) {
  return block({.writingMode = mode});
}

Element& Element::lineBreak(sigil::weave::LineBreakStrategy strategy) {
  return block({.lineBreak = strategy});
}

Element& Element::hyphenation(sigil::weave::HyphenationOptions spec) {
  return block({.hyphenation = spec});
}

Element& Element::ellipsis(std::u8string_view marker) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.ellipsis = weave::unicode::toUtf16(marker);
  options.set |= detail::TextOptions::kEllipsis;
  return *this;
}

Element& Element::paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.blocks = std::move(blocks);
  options.set |= detail::TextOptions::kBlocks;
  // The two spellings are alternatives, and the last one written stands.
  options.blockClassNames.clear();
  options.set &= ~(uint32_t)detail::TextOptions::kBlockClasses;
  return *this;
}

Element& Element::paragraphs(std::span<const std::string_view> names) {
  // The names are kept; they resolve against the block sheet in force
  // where the leaf lands, when it lays out, and lie over the block in
  // force there.
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.blockClassNames.assign(names.begin(), names.end());
  options.set |= detail::TextOptions::kBlockClasses;
  options.blocks.clear();
  options.set &= ~(uint32_t)detail::TextOptions::kBlocks;
  return *this;
}

Element& Element::paragraph(sigil::weave::ParagraphStyle style) {
  return paragraphs(
      std::vector<sigil::weave::ParagraphStyle>{std::move(style)});
}

Element& Element::initialLetter(sigil::weave::InitialLetter initial) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  // The initial belongs to the passage's first block, whichever way that
  // block is styled — a whole style, a name, or the block in force — so
  // it is kept apart and applied to the first block when the leaf lays
  // out.
  options.initial = std::move(initial);
  options.set |= detail::TextOptions::kInitialLetter;
  return *this;
}

Element& Element::firstBaseline(sigil::weave::FrameOptions::FirstBaseline rule,
                                float offset) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.frame.firstBaseline = rule;
  options.frame.firstBaselineOffset = offset;
  options.set |= detail::TextOptions::kFrame;
  return *this;
}

Element& Element::distribute(sigil::weave::FrameOptions::Distribute rule,
                             float maximumInterlineSpacing) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.frame.distribute = rule;
  options.frame.maximumInterlineSpacing = maximumInterlineSpacing;
  options.set |= detail::TextOptions::kFrame;
  return *this;
}

Element& Element::justification(sigil::weave::JustificationOptions spec) {
  return block({.justification = std::move(spec)});
}

Element& Element::tabStops(sigil::weave::TabStopOptions stops) {
  return block({.tabStops = std::move(stops)});
}

Element& Element::live(bool on, float budgetMicroseconds) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.live = on;
  options.budgetMicroseconds = budgetMicroseconds;
  options.set |= detail::TextOptions::kLive;
  return *this;
}

Element& Element::kinsoku(sigil::weave::KinsokuTable table) {
  return block({.kinsoku = std::move(table)});
}

Element& Element::hanging(sigil::weave::HangingTable table) {
  return block({.hanging = std::move(table)});
}

Element& Element::mojikumi(sigil::weave::MojikumiTable table, float tsume) {
  return block({.mojikumi = std::move(table), .tsume = tsume});
}

Element& Element::balanceChain(uint32_t throughLine) {
  detail::TextData& text = m_node->textData.ensure();
  text.balanceChain = true;
  text.balanceThroughLine = throughLine;
  return *this;
}

Element& Element::reserve(sigil::weave::ReservedBand band) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.reserved = band;
  options.set |= detail::TextOptions::kReserved;
  return *this;
}

Element& Element::lineBreakLocale(std::string_view locale) {
  return block({.lineBreakLocale = std::string(locale)});
}

Element& Element::thread(std::string_view key) {
  m_node->textData.ensure().threadTo = std::string(key);
  // A frame is cut where the frame before it stopped, so it reads the
  // FINEST answer that frame produces — its units, not its box. The link
  // itself is LAST-WINS, so the read is replaced rather than added to: a
  // frame threads into exactly one frame, and a chain that named another
  // one first no longer waits for it.
  detail::DeriveData& derive = m_node->deriveData.ensure();
  std::erase_if(derive.reads, [](const sigil::core::Read& read) {
    return read.facet == sigil::core::Facet::Units;
  });
  derive.reads.push_back({std::string(key), sigil::core::Facet::Units});
  return *this;
}

Element& Element::maxLines(int lines) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.maxLines = lines;
  options.set |= detail::TextOptions::kMaxLines;
  return *this;
}

Element& Element::lastLine(sigil::weave::TextAlignment alignment,
                           bool justify) {
  return block({.lastLineAlignment = alignment, .justifyLastLine = justify});
}

Element& Element::flowAround(std::string_view key, float margin) {
  detail::DeriveData& derive = m_node->deriveData.ensure();
  derive.flowAroundKeys.emplace_back(key);
  derive.flowAroundMargin = margin;
  // An exclusion subtracts the target's SILHOUETTE where it declares one,
  // which is a read of its outline and not merely of its box.
  derive.reads.push_back({std::string(key), sigil::core::Facet::Outline});
  return *this;
}

void detail::TextOptions::applyTo(
    sigil::weave::ParagraphLayoutOptions& options) const {
  if (set & kEllipsis) options.overflow.ellipsis = ellipsis;
  if (set & kMaxLines) options.overflow.maxLines = maxLines;
  if (set & kBlocks) options.blocks = blocks;
  if (set & kFrame) options.frame = frame;
  if (set & kLive) {
    options.live = live;
    options.knuthPlass.budgetMicroseconds = budgetMicroseconds;
  }
  if (set & kReserved) options.reserved = reserved;
}

}  // namespace sigil::compose
