/** @file
 * The text leaf's own verbs — the glyph stroke, the layout options that
 * override a paragraph's field by field, the frame chain and the
 * exclusion — with the options' fold onto SigilWeave's layout options and
 * the UTF-16 boundary the leaf speaks to it across. The verbs that dress
 * type are declared beside these and defined by the typography tier.
 */

#include <sigilcore/reconcile/Env.h>

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::textStroke(float width, Fill fill) {
  auto& t = m_node->textData.ensure();
  t.hasTextStroke = width > 0.0f;
  t.textStrokeWidth = width;
  t.textStrokeFill = std::move(fill);
  return *this;
}

Element& Element::textAlign(sigil::weave::TextAlignment a) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.alignment = a;
  options.set |= detail::TextOptions::kAlignment;
  return *this;
}

Element& Element::writingMode(sigil::weave::WritingMode mode) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.writingMode = mode;
  options.set |= detail::TextOptions::kWritingMode;
  return *this;
}

Element& Element::lineBreak(sigil::weave::LineBreakStrategy strategy) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.lineBreak = strategy;
  options.set |= detail::TextOptions::kLineBreak;
  return *this;
}

Element& Element::hyphenation(sigil::weave::HyphenationOptions spec) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.hyphenation = spec;
  options.set |= detail::TextOptions::kHyphenation;
  return *this;
}

Element& Element::ellipsis(std::u8string_view marker) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.ellipsis = detail::toUtf16(marker);
  options.set |= detail::TextOptions::kEllipsis;
  return *this;
}

Element& Element::paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.blocks = std::move(blocks);
  options.set |= detail::TextOptions::kBlocks;
  return *this;
}

Element& Element::paragraphs(std::span<const std::string_view> names) {
  // The set is read HERE, inside the author's describe scope, exactly as a
  // named character run reads its own: the finished description then holds
  // real styles and depends on no scope that has since ended.
  const sigil::weave::ParagraphStyleSet* set =
      core::env::inherited<sigil::weave::ParagraphStyleSet>();
  std::vector<sigil::weave::ParagraphStyle> resolved;
  resolved.reserve(names.size());
  for (const std::string_view name : names) {
    const sigil::weave::ParagraphStyle* found = set ? set->find(name) : nullptr;
    // A name nobody registered would otherwise resolve to the set's base
    // and set the block in a default the author never asked for, which
    // looks exactly like a style that did not take.
    if (!found) detail::warnNoSuchParagraphStyle(name, set != nullptr);
    resolved.push_back(found ? *found
                       : set ? set->base()
                             : sigil::weave::ParagraphStyle{});
  }
  return paragraphs(std::move(resolved));
}

Element& Element::paragraph(sigil::weave::ParagraphStyle style) {
  return paragraphs(
      std::vector<sigil::weave::ParagraphStyle>{std::move(style)});
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
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.justification = std::move(spec);
  options.set |= detail::TextOptions::kJustification;
  return *this;
}

Element& Element::tabStops(sigil::weave::TabStopOptions stops) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.tabStops = std::move(stops);
  options.set |= detail::TextOptions::kTabStops;
  return *this;
}

Element& Element::live(bool on, float budgetMicroseconds) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.live = on;
  options.budgetMicroseconds = budgetMicroseconds;
  options.set |= detail::TextOptions::kLive;
  return *this;
}

Element& Element::kinsoku(sigil::weave::KinsokuTable table) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.kinsoku = std::move(table);
  options.set |= detail::TextOptions::kLineTables;
  return *this;
}

Element& Element::hanging(sigil::weave::HangingTable table) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.hanging = std::move(table);
  options.set |= detail::TextOptions::kLineTables;
  return *this;
}

Element& Element::mojikumi(sigil::weave::MojikumiTable table, float tsume) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.mojikumi = std::move(table);
  options.tsume = tsume;
  options.set |= detail::TextOptions::kLineTables;
  return *this;
}

Element& Element::reserve(sigil::weave::ReservedBand band) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.reserved = band;
  options.set |= detail::TextOptions::kReserved;
  return *this;
}

Element& Element::lineBreakLocale(std::string_view locale) {
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.lineBreakLocale = std::string(locale);
  options.set |= detail::TextOptions::kLineBreakLocale;
  return *this;
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
  detail::TextOptions& options = m_node->textData.ensure().options;
  options.lastLineAlignment = alignment;
  options.justifyLastLine = justify;
  options.set |= detail::TextOptions::kLastLine;
  return *this;
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
  if (set & kAlignment) options.alignment = alignment;
  if (set & kLineBreak) options.lineBreakStrategy = lineBreak;
  if (set & kHyphenation) options.hyphenation = hyphenation;
  if (set & kEllipsis) options.overflow.ellipsis = ellipsis;
  if (set & kMaxLines) options.overflow.maxLines = maxLines;
  if (set & kJustification) options.justification = justification;
  if (set & kLastLine) {
    options.justification.lastLineAlignment = lastLineAlignment;
    options.justification.justifyLastLine = justifyLastLine;
  }
  if (set & kTabStops) options.tabStops = tabStops;
  if (set & kBlocks) options.blocks = blocks;
  if (set & kFrame) options.frame = frame;
  if (set & kLive) {
    options.live = live;
    options.knuthPlass.budgetMicroseconds = budgetMicroseconds;
  }
  if (set & kLineTables) {
    options.kinsoku = kinsoku;
    options.hanging = hanging;
    options.mojikumi = mojikumi;
    options.tsume = tsume;
  }
  if (set & kReserved) options.reserved = reserved;
}

std::u16string detail::toUtf16(std::u8string_view utf8) {
  // Hand-rolled rather than borrowed from the weave layer: compose speaks
  // UTF-8 at its surface and UTF-16 at exactly this boundary, and a full
  // Unicode library for that is a dependency the kernel does not otherwise
  // need. Ill-formed input yields U+FFFD, never a silent truncation.
  std::u16string out;
  out.reserve(utf8.size());
  for (size_t i = 0; i < utf8.size();) {
    const auto byte = (unsigned char)utf8[i];
    char32_t code = 0xFFFD;
    size_t length = 1;
    if (byte < 0x80) {
      code = byte;
    } else if ((byte & 0xE0u) == 0xC0) {
      length = 2;
      code = byte & 0x1Fu;
    } else if ((byte & 0xF0u) == 0xE0) {
      length = 3;
      code = byte & 0x0Fu;
    } else if ((byte & 0xF8u) == 0xF0) {
      length = 4;
      code = byte & 0x07u;
    }
    if (length > 1) {
      if (i + length > utf8.size()) {
        code = 0xFFFD;
        length = utf8.size() - i;
      } else {
        for (size_t k = 1; k < length; ++k) {
          const auto continuation = (unsigned char)utf8[i + k];
          if ((continuation & 0xC0u) != 0x80) {
            code = 0xFFFD;
            length = k;
            break;
          }
          code = (code << 6u) | (continuation & 0x3Fu);
        }
      }
    }
    if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) code = 0xFFFD;
    if (code >= 0x10000) {
      const char32_t rest = code - 0x10000;
      out.push_back((char16_t)(0xD800 + (rest >> 10u)));
      out.push_back((char16_t)(0xDC00 + (rest & 0x3FFu)));
    } else {
      out.push_back((char16_t)code);
    }
    i += length;
  }
  return out;
}

}  // namespace sigil::compose
