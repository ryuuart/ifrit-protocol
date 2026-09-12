/** @file
 * WHICH GLYPHS A SELECTOR ADDRESSES, answered twice over one vocabulary.
 *
 * The glyph resolver answers "which of the placed glyphs", which is the
 * only question a per-glyph deviation can ask. A span restyle runs on the
 * Paragraph instead, before shaping, so it needs the same selectors
 * answered in UTF-16 ranges. One vocabulary, two resolvers, because the
 * two consumers genuinely address different things — and the diagnostics
 * for a selector that addresses nothing are here with them.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the library's one channel
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/query/Query.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "TextEngine.h"

namespace sigil::compose {

namespace detail {

// ---------------------------------------------------------------------------
// Selection

namespace {

/** Marks every glyph whose cluster falls inside one of `ranges`. */
void markRanges(const std::vector<sigil::weave::CharRange>& ranges,
                const GlyphStructure& structure, std::vector<uint8_t>& out) {
  for (const sigil::weave::CharRange& r : ranges)
    for (size_t i = 0; i < structure.glyphs.size(); ++i)
      if (structure.glyphs[i].textIndex >= r.start &&
          structure.glyphs[i].textIndex < r.end)
        out[i] = 1;
}

/** The extents a named-run table gives one name, in declaration order —
 *  each caller puts them in the form it needs. Empty when no run answers to
 *  the name, which is the one case the callers warn about. */
std::vector<sigil::weave::CharRange> namedRunRanges(
    std::span<const NamedRun> named, const std::u8string& name) {
  std::vector<sigil::weave::CharRange> out;
  const std::string_view wanted((const char*)name.data(), name.size());
  for (const NamedRun& run : named)
    if (run.name == wanted) out.push_back(run.chars);
  return out;
}

void resolveInto(const sigil::weave::Selector& selector,
                 const GlyphStructure& structure,
                 const sigil::weave::Paragraph& paragraph,
                 std::span<const NamedRun> named, std::vector<uint8_t>& out) {
  const size_t count = structure.glyphs.size();
  out.assign(count, 0);
  const sigil::weave::Selector::State* s = selector.state();
  if (!s) {  // default-constructed: everything
    std::fill(out.begin(), out.end(), (uint8_t)1);
    return;
  }
  const auto byIndex = [&](auto&& field, uint32_t lo, uint32_t hi) {
    for (size_t i = 0; i < count; ++i) {
      const uint32_t v = field(structure.glyphs[i]);
      if (v >= lo && v < hi) out[i] = 1;
    }
  };
  switch (s->kind) {
    case sigil::weave::Selector::Kind::All:
      std::fill(out.begin(), out.end(), (uint8_t)1);
      break;
    case sigil::weave::Selector::Kind::Word:
      byIndex([](const GlyphInfo& g) { return g.wordIndex; }, s->lo, s->hi);
      break;
    case sigil::weave::Selector::Kind::Line:
      byIndex([](const GlyphInfo& g) { return g.lineIndex; }, s->lo, s->hi);
      break;
    case sigil::weave::Selector::Kind::Sentence:
      byIndex([](const GlyphInfo& g) { return g.sentenceIndex; }, s->lo, s->hi);
      break;
    case sigil::weave::Selector::Kind::Range:
      byIndex([](const GlyphInfo& g) { return g.textIndex; }, s->lo, s->hi);
      break;
    case sigil::weave::Selector::Kind::Text:
      markRanges(sigil::weave::findAllOccurrences(paragraph, s->pattern),
                 structure, out);
      break;
    case sigil::weave::Selector::Kind::Regex: {
      std::optional<std::vector<sigil::weave::CharRange>> matches =
          sigil::weave::findRegexMatches(paragraph, s->pattern);
      if (!matches) {
        warnBadSelectorPattern(s->pattern);
        break;  // an unresolvable pattern selects nothing
      }
      markRanges(*matches, structure, out);
      break;
    }
    case sigil::weave::Selector::Kind::Named: {
      const std::vector<sigil::weave::CharRange> runs =
          namedRunRanges(named, s->pattern);
      if (runs.empty()) {
        warnNoSuchStyleName(s->pattern);
        break;  // content that carries no such name selects nothing
      }
      markRanges(runs, structure, out);
      break;
    }
    case sigil::weave::Selector::Kind::Scope: {
      // The frame-local address, resolved on the leaf being addressed: it
      // is everything on the frame it names and nothing anywhere else, so
      // intersecting it with a story-wide form cuts that form to one frame.
      const std::u8string_view key(
          (const char8_t*)structure.scope.frameKey.data(),
          structure.scope.frameKey.size());
      if (key.empty()) warnNoSuchFrameKey(s->pattern);
      if (!key.empty() && key == s->pattern)
        std::fill(out.begin(), out.end(), (uint8_t)1);
      break;
    }
    case sigil::weave::Selector::Kind::Each: {
      // Every unit sliced the same way, at GLYPH granularity inside it.
      // `drop(n)` and `take(n)` partition a unit exactly: the two answer
      // opposite sides of the same cut, so no glyph is in both and none is
      // in neither.
      const std::vector<uint32_t>& units = structure.unitOf[(size_t)s->each];
      const int drop = std::max(s->drop, 0);
      int within = 0;
      for (size_t i = 0; i < count; ++i) {
        if (i > 0 && units[i] != units[i - 1]) within = 0;
        const bool afterDrop = within >= drop;
        const bool beforeTake =
            s->take < 0 || within < drop + std::max(s->take, 0);
        if (afterDrop && beforeTake) out[i] = 1;
        ++within;
      }
      break;
    }
    case sigil::weave::Selector::Kind::Union:
    case sigil::weave::Selector::Kind::Intersect: {
      std::vector<uint8_t> lhs, rhs;
      resolveInto(s->operands[0], structure, paragraph, named, lhs);
      resolveInto(s->operands[1], structure, paragraph, named, rhs);
      for (size_t i = 0; i < count; ++i)
        out[i] = s->kind == sigil::weave::Selector::Kind::Union
                     ? (lhs[i] | rhs[i])
                     : (lhs[i] & rhs[i]);
      break;
    }
    case sigil::weave::Selector::Kind::Complement: {
      std::vector<uint8_t> inner;
      resolveInto(s->operands[0], structure, paragraph, named, inner);
      for (size_t i = 0; i < count; ++i) out[i] = inner[i] ? 0 : 1;
      break;
    }
  }
}

}  // namespace

void warnBadSelectorPattern(const std::u8string& pattern) {
  // Once per distinct pattern: a selector resolved every reflow would
  // otherwise scroll the same line past the author forever.
  static thread_local boost::unordered_flat_set<std::string> seen;
  std::string key((const char*)pattern.data(), pattern.size());
  if (!seen.insert(key).second) return;
  SkDebugf(
      "compose: weave::selectors::regex(\"%s\") does not compile — this track "
      "selects no glyphs\n",
      key.c_str());
}

void warnNoSuchStyleName(const std::u8string& name) {
  // Once per distinct name, for the reason the pattern warning is: this
  // resolves on every reflow, and a name that is wrong is wrong every time.
  static thread_local boost::unordered_flat_set<std::string> seen;
  std::string key((const char*)name.data(), name.size());
  if (!seen.insert(key).second) return;
  SkDebugf(
      "compose: selectors::style(\"%s\") — no run of this text was written "
      "under "
      "that name, so it addresses nothing (only a weave::rich() run added "
      "with add(text, styleName) carries one)\n",
      key.c_str());
}

void warnNoSuchFrameKey(const std::u8string& key) {
  static thread_local boost::unordered_flat_set<std::string> seen;
  std::string name((const char*)key.data(), key.size());
  if (!seen.insert(name).second) return;
  SkDebugf(
      "compose: selectors::inFrame(\"%s\") on a text leaf with no key() of its "
      "own — a frame-local address is matched against the leaf's own key, so "
      "this one can never match and addresses nothing\n",
      name.c_str());
}

std::vector<uint8_t> resolveSelection(const sigil::weave::Selector& selector,
                                      const GlyphStructure& structure,
                                      const sigil::weave::Paragraph& paragraph,
                                      std::span<const NamedRun> named) {
  std::vector<uint8_t> out;
  resolveInto(selector, structure, paragraph, named, out);
  return out;
}

// ---------------------------------------------------------------------------
// Selection as TEXT RANGES — what span restyling addresses
//
// The glyph resolver above answers "which of the placed glyphs", which is
// the only question a per-glyph deviation can ask. A restyle runs on the
// Paragraph instead, before shaping, so it needs the same selectors
// answered in UTF-16 ranges. One vocabulary, two resolvers, because the two
// consumers genuinely address different things.

namespace {

using Ranges = std::vector<sigil::weave::CharRange>;

/** Sorted, merged, empties dropped — the one normal form every answer is
 *  in. A code-unit index is exact, so the interval algebra runs with no
 *  epsilon and no clamp beyond the paragraph's own length. */
Ranges normalize(Ranges ranges, uint32_t length = UINT32_MAX) {
  return core::normalizeIntervals<sigil::weave::CharRange>(std::move(ranges),
                                                           0u, length);
}

Ranges intersectRanges(const Ranges& a, const Ranges& b) {
  return core::intersectIntervals<sigil::weave::CharRange>(a, b);
}

Ranges complementRanges(const Ranges& ranges, uint32_t length) {
  return core::complementIntervals<sigil::weave::CharRange>(ranges, 0u, length);
}

}  // namespace

namespace {

/** Once per process: an `weave::selectors::each` slice asked of a text range.
 */
void warnSliceIgnored() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "compose: weave::Selector::take/drop slice GLYPHS inside a unit, which "
      "a text range cannot express — this span restyle covers whole units\n");
}

Ranges resolveTextRangesInto(
    const sigil::weave::Selector& selector, sigil::weave::Paragraph& paragraph,
    sigil::weave::FontContext& fonts,
    std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns,
    std::span<const NamedRun> named, const TextScope& scope) {
  const auto length = (uint32_t)paragraph.text().size();
  const sigil::weave::Selector::State* s = selector.state();
  if (!s) return {{0, length}};  // default-constructed: everything
  switch (s->kind) {
    case sigil::weave::Selector::Kind::All:
      return {{0, length}};
    case sigil::weave::Selector::Kind::Word: {
      Ranges words = sigil::weave::wordRanges(paragraph, fonts);
      Ranges out;
      for (uint32_t i = s->lo; i < s->hi && i < words.size(); ++i)
        out.push_back(words[i]);
      return normalize(std::move(out));
    }
    case sigil::weave::Selector::Kind::Line: {
      // A vertical passage numbers COLUMNS where a horizontal one numbers
      // lines, and only one of the two lists is ever populated. The index
      // asked for is the STORY's, so it is brought back to this frame's
      // numbering before it is compared — the geometry knows only the lines
      // this frame placed.
      Ranges out;
      const uint32_t offset = scope.lineOffset;
      const auto within = [&](int index) {
        const auto story = (uint32_t)index + offset;
        return story >= s->lo && story < s->hi;
      };
      for (const sigil::weave::LineMetrics& line : lines)
        if (within(line.lineIndex))
          out.push_back({line.textBegin, line.textEnd});
      for (const sigil::weave::ColumnMetrics& column : columns)
        if (within(column.lineIndex))
          out.push_back({column.textBegin, column.textEnd});
      return normalize(std::move(out));
    }
    case sigil::weave::Selector::Kind::Scope: {
      const std::u8string_view key((const char8_t*)scope.frameKey.data(),
                                   scope.frameKey.size());
      if (key.empty()) warnNoSuchFrameKey(s->pattern);
      if (!key.empty() && key == s->pattern) return {{0, length}};
      return {};
    }
    case sigil::weave::Selector::Kind::Sentence: {
      const std::span<const uint32_t> starts = paragraph.sentenceStarts();
      Ranges out;
      for (uint32_t i = s->lo; i < s->hi && i < starts.size(); ++i)
        out.push_back(
            {starts[i], i + 1 < starts.size() ? starts[i + 1] : length});
      return normalize(std::move(out));
    }
    case sigil::weave::Selector::Kind::Range:
      return normalize({{std::min(s->lo, length), std::min(s->hi, length)}});
    case sigil::weave::Selector::Kind::Text:
      return normalize(sigil::weave::findAllOccurrences(paragraph, s->pattern));
    case sigil::weave::Selector::Kind::Regex: {
      std::optional<Ranges> matches =
          sigil::weave::findRegexMatches(paragraph, s->pattern);
      if (!matches) {
        warnBadSelectorPattern(s->pattern);
        return {};
      }
      return normalize(*std::move(matches));
    }
    case sigil::weave::Selector::Kind::Named: {
      Ranges runs = namedRunRanges(named, s->pattern);
      if (runs.empty()) warnNoSuchStyleName(s->pattern);
      return normalize(std::move(runs));
    }
    case sigil::weave::Selector::Kind::Each: {
      // A unit's whole extent. The glyph slice has no text-range meaning;
      // saying so once beats a restyle that silently covers more than the
      // author asked for.
      if (s->take >= 0 || s->drop > 0) warnSliceIgnored();
      switch (s->each) {
        case sigil::weave::Unit::Word:
          return normalize(sigil::weave::wordRanges(paragraph, fonts));
        case sigil::weave::Unit::Line: {
          Ranges out;
          for (const sigil::weave::LineMetrics& line : lines)
            out.push_back({line.textBegin, line.textEnd});
          for (const sigil::weave::ColumnMetrics& column : columns)
            out.push_back({column.textBegin, column.textEnd});
          return normalize(std::move(out));
        }
        default:
          return {{0, length}};
      }
    }
    case sigil::weave::Selector::Kind::Union: {
      Ranges out = resolveTextRangesInto(s->operands[0], paragraph, fonts,
                                         lines, columns, named, scope);
      Ranges rhs = resolveTextRangesInto(s->operands[1], paragraph, fonts,
                                         lines, columns, named, scope);
      out.insert(out.end(), rhs.begin(), rhs.end());
      return normalize(std::move(out));
    }
    case sigil::weave::Selector::Kind::Intersect:
      return intersectRanges(
          resolveTextRangesInto(s->operands[0], paragraph, fonts, lines,
                                columns, named, scope),
          resolveTextRangesInto(s->operands[1], paragraph, fonts, lines,
                                columns, named, scope));
    case sigil::weave::Selector::Kind::Complement:
      return complementRanges(
          resolveTextRangesInto(s->operands[0], paragraph, fonts, lines,
                                columns, named, scope),
          length);
  }
  return {};
}

}  // namespace

std::vector<sigil::weave::CharRange> resolveTextRanges(
    const sigil::weave::Selector& selector, sigil::weave::Paragraph& paragraph,
    sigil::weave::FontContext& fonts,
    std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns,
    std::span<const NamedRun> named, TextScope scope) {
  return resolveTextRangesInto(selector, paragraph, fonts, lines, columns,
                               named, scope);
}

}  // namespace detail

}  // namespace sigil::compose
