/** @file
 * The text-fx seam's VALUES and their resolution against a laid-out
 * paragraph: the comparable `TextEffect`, `Selector` and `Stagger`, the
 * per-walk glyph structure every track shares, and the cascade arithmetic
 * that turns one master progress into a local time per glyph.
 *
 * Nothing here touches an Instance or a canvas. Paint.cpp drives it: build
 * the structure once per frame, resolve each track's selection (cached on
 * the element), then walk the glyphs asking this file for each one's local
 * time and composing the deviations.
 */

#include "sigilcompose/TextFx.h"

#include <sigilweave/Choreograph.h>
#include <sigilweave/Query.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <unordered_set>
#include <utility>

#include "ComposeRuntime.h"

namespace sigil::compose {

// ---------------------------------------------------------------------------
// TextEffect

TextEffect::TextEffect(std::string name, std::vector<float> params,
                       GlyphModFn fn, float reach,
                       std::vector<choreograph::EaseFn> curves) {
  auto state = std::make_shared<State>();
  state->name = std::move(name);
  state->params = std::move(params);
  state->curves = std::move(curves);
  state->fn = std::move(fn);
  state->reach = reach;
  m_state = std::move(state);
}

TextEffect TextEffect::composite(std::string name, std::vector<float> params,
                                 std::vector<TextEffect> operands,
                                 GlyphModFn fn, float reach) {
  auto state = std::make_shared<State>();
  state->name = std::move(name);
  state->params = std::move(params);
  state->operands = std::move(operands);
  state->fn = std::move(fn);
  state->reach = reach;
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

const std::string& TextEffect::name() const {
  static const std::string kEmpty;
  return m_state ? m_state->name : kEmpty;
}

std::span<const float> TextEffect::params() const {
  return m_state ? std::span<const float>(m_state->params)
                 : std::span<const float>();
}

std::span<const TextEffect> TextEffect::operands() const {
  return m_state ? std::span<const TextEffect>(m_state->operands)
                 : std::span<const TextEffect>();
}

bool TextEffect::operator==(const TextEffect& other) const {
  if (m_state == other.m_state) return true;  // copies of one value
  if (!m_state || !other.m_state) return false;
  if (m_state->name != other.m_state->name ||
      m_state->params != other.m_state->params ||
      m_state->operands != other.m_state->operands)
    return false;
  if (m_state->curves.size() != other.m_state->curves.size()) return false;
  for (size_t i = 0; i < m_state->curves.size(); ++i)
    if (!detail::easeEqual(m_state->curves[i], other.m_state->curves[i]))
      return false;
  return true;
}

Phase TextEffect::until(float t) const { return Phase(*this, t); }

// ---------------------------------------------------------------------------
// Selector

Selector Selector::of(State s) {
  Selector out;
  out.m_state = std::make_shared<const State>(std::move(s));
  return out;
}

bool Selector::operator==(const Selector& other) const {
  if (m_state == other.m_state) return true;
  if (!m_state || !other.m_state) return false;  // one is "everything"
  return *m_state == *other.m_state;
}

Selector Selector::take(int n) const {
  State s = m_state ? *m_state : State{};
  s.take = n;
  return of(std::move(s));
}

Selector Selector::drop(int n) const {
  State s = m_state ? *m_state : State{};
  s.drop = n;
  return of(std::move(s));
}

namespace {
Selector combine(Selector::Kind kind, const Selector& a, const Selector& b) {
  Selector::State s;
  s.kind = kind;
  s.operands = {a, b};
  return Selector::of(std::move(s));
}
}  // namespace

Selector Selector::operator|(const Selector& other) const {
  return combine(Kind::Union, *this, other);
}
Selector Selector::operator&(const Selector& other) const {
  return combine(Kind::Intersect, *this, other);
}
Selector Selector::operator!() const {
  State s;
  s.kind = Kind::Complement;
  s.operands = {*this};
  return of(std::move(s));
}

namespace sel {
namespace {
Selector indexed(Selector::Kind kind, uint32_t lo, uint32_t hi) {
  Selector::State s;
  s.kind = kind;
  s.lo = lo;
  s.hi = hi;
  return Selector::of(std::move(s));
}
Selector needle(Selector::Kind kind, std::u8string_view pattern) {
  Selector::State s;
  s.kind = kind;
  s.pattern = std::u8string(pattern);
  return Selector::of(std::move(s));
}
}  // namespace

Selector word(uint32_t index) {
  return indexed(Selector::Kind::Word, index, index + 1);
}
Selector words(uint32_t lo, uint32_t hi) {
  return indexed(Selector::Kind::Words, lo, hi);
}
Selector line(uint32_t index) {
  return indexed(Selector::Kind::Line, index, index + 1);
}
Selector sentence(uint32_t index) {
  return indexed(Selector::Kind::Sentence, index, index + 1);
}
Selector range(sigil::weave::CharRange chars) {
  return indexed(Selector::Kind::Range, chars.start, chars.end);
}
Selector regex(std::u8string_view utf8Pattern) {
  return needle(Selector::Kind::Regex, utf8Pattern);
}
Selector text(std::u8string_view utf8Substring) {
  return needle(Selector::Kind::Text, utf8Substring);
}
Selector style(std::string_view name) {
  // A style name is ASCII-or-whatever the author typed, and the needle slot
  // holds UTF-8 bytes; both resolvers compare it against the same bytes a
  // run's name was written with, so no transcoding is involved either way.
  return needle(Selector::Kind::Style,
                std::u8string_view((const char8_t*)name.data(), name.size()));
}
Selector each(Unit granularity) {
  Selector::State s;
  s.kind = Selector::Kind::Each;
  s.each = granularity;
  return Selector::of(std::move(s));
}
}  // namespace sel

// ---------------------------------------------------------------------------
// Stagger

Stagger& Stagger::then(Unit granularity, Stagger nested) {
  nested.over = granularity;
  inner = std::make_shared<const Stagger>(std::move(nested));
  return *this;
}

Stagger stagger(Unit granularity, Stagger spec) {
  spec.over = granularity;
  return spec;
}

Stagger cues(std::vector<float> startMs, Stagger spec) {
  spec.cueMs = std::move(startMs);
  return spec;
}

// ---------------------------------------------------------------------------
// The per-walk structure every track shares

namespace detail {

namespace {
/** A new unit of `granularity` begins here. Draw order is the enumeration
 *  order forEachPlacedGlyph guarantees, so "changed since the previous
 *  glyph" is the whole test — no map, no sort, and a cluster's glyphs stay
 *  together because a cluster's glyphs are adjacent by construction. */
bool startsUnit(Unit granularity, const sigil::weave::PlacedGlyph& glyph,
                const sigil::weave::PlacedGlyph& previous, bool first) {
  if (first) return true;
  switch (granularity) {
    case Unit::Glyph:
      return true;
    case Unit::Cluster:
      // The text offset, not the shaped-run-local cluster: a base and a
      // combining mark that fell back to a second font are two runs and one
      // cluster, and a stagger that separated them would leave the accent
      // behind in mid-air.
      return glyph.wordIndex != previous.wordIndex ||
             glyph.textIndex != previous.textIndex;
    case Unit::Word:
      return glyph.wordIndex != previous.wordIndex;
    case Unit::Line:
      return glyph.lineIndex != previous.lineIndex;
    case Unit::Sentence:
      return glyph.sentenceIndex != previous.sentenceIndex;
  }
  return true;
}
}  // namespace

void GlyphStructure::build(const sigil::weave::ParagraphLayout& layout,
                           const sigil::weave::Paragraph& paragraph) {
  glyphs.clear();
  for (auto& lane : unitOf) lane.clear();
  unitCounts = {};

  sigil::weave::PlacedGlyph previous;
  bool first = true;
  sigil::weave::forEachPlacedGlyph(
      layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        GlyphInfo info;
        info.index = glyphs.size();
        info.rest = placed.rest;
        info.advance = placed.advance;
        info.fontSize = placed.shaped ? placed.shaped->fontSize : 0.0f;
        info.cluster = placed.cluster;
        info.textIndex = placed.textIndex;
        info.wordIndex = placed.wordIndex;
        info.lineIndex = (uint32_t)std::max(placed.lineIndex, 0);
        info.styleIndex = placed.styleIndex;
        info.sentenceIndex = placed.sentenceIndex;
        glyphs.push_back(info);

        for (size_t lane = 0; lane < kUnits; ++lane) {
          if (startsUnit((Unit)lane, placed, previous, first))
            ++unitCounts[lane];
          unitOf[lane].push_back(unitCounts[lane] - 1);
        }
        previous = placed;
        first = false;
      });

  // The two per-word facts an effect reads (which letter of its word, of
  // how many) need the word's size, which is only known once the word has
  // been walked — so they are a second pass over the finished runs.
  const uint32_t total = (uint32_t)glyphs.size();
  const std::vector<uint32_t>& wordUnits = unitOf[(size_t)Unit::Word];
  for (uint32_t begin = 0; begin < total;) {
    uint32_t end = begin + 1;
    while (end < total && wordUnits[end] == wordUnits[begin]) ++end;
    for (uint32_t i = begin; i < end; ++i) {
      glyphs[i].glyphInWord = i - begin;
      glyphs[i].wordGlyphCount = end - begin;
      glyphs[i].count = total;
    }
    begin = end;
  }
}

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

void resolveInto(const Selector& selector, const GlyphStructure& structure,
                 const sigil::weave::Paragraph& paragraph,
                 std::span<const NamedRun> named, std::vector<uint8_t>& out) {
  const size_t count = structure.glyphs.size();
  out.assign(count, 0);
  const Selector::State* s = selector.state();
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
    case Selector::Kind::All:
      std::fill(out.begin(), out.end(), (uint8_t)1);
      break;
    case Selector::Kind::Word:
    case Selector::Kind::Words:
      byIndex([](const GlyphInfo& g) { return g.wordIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Line:
      byIndex([](const GlyphInfo& g) { return g.lineIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Sentence:
      byIndex([](const GlyphInfo& g) { return g.sentenceIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Range:
      byIndex([](const GlyphInfo& g) { return g.textIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Text:
      markRanges(sigil::weave::findAllOccurrences(paragraph, s->pattern),
                 structure, out);
      break;
    case Selector::Kind::Regex: {
      std::optional<std::vector<sigil::weave::CharRange>> matches =
          sigil::weave::findRegexMatches(paragraph, s->pattern);
      if (!matches) {
        warnBadSelectorPattern(s->pattern);
        break;  // an unresolvable pattern selects nothing
      }
      markRanges(*matches, structure, out);
      break;
    }
    case Selector::Kind::Style: {
      const std::vector<sigil::weave::CharRange> runs =
          namedRunRanges(named, s->pattern);
      if (runs.empty()) {
        warnNoSuchStyleName(s->pattern);
        break;  // content that carries no such name selects nothing
      }
      markRanges(runs, structure, out);
      break;
    }
    case Selector::Kind::Each: {
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
    case Selector::Kind::Union:
    case Selector::Kind::Intersect: {
      std::vector<uint8_t> lhs, rhs;
      resolveInto(s->operands[0], structure, paragraph, named, lhs);
      resolveInto(s->operands[1], structure, paragraph, named, rhs);
      for (size_t i = 0; i < count; ++i)
        out[i] = s->kind == Selector::Kind::Union ? (lhs[i] | rhs[i])
                                                  : (lhs[i] & rhs[i]);
      break;
    }
    case Selector::Kind::Complement: {
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
  static thread_local std::unordered_set<std::string> seen;
  std::string key((const char*)pattern.data(), pattern.size());
  if (!seen.insert(key).second) return;
  std::fprintf(stderr,
               "SigilCompose: sel::regex(\"%s\") does not compile — this "
               "track selects no glyphs\n",
               key.c_str());
}

void warnNoSuchStyleName(const std::u8string& name) {
  // Once per distinct name, for the reason the pattern warning is: this
  // resolves on every reflow, and a name that is wrong is wrong every time.
  static thread_local std::unordered_set<std::string> seen;
  std::string key((const char*)name.data(), name.size());
  if (!seen.insert(key).second) return;
  std::fprintf(stderr,
               "SigilCompose: sel::style(\"%s\") — no run of this text was "
               "written under that name, so it addresses nothing (only a "
               "rich() run added with add(text, styleName) carries one)\n",
               key.c_str());
}

std::vector<uint8_t> resolveSelection(const Selector& selector,
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

std::u16string toUtf16(std::u8string_view utf8) {
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
    } else if ((byte & 0xE0) == 0xC0) {
      length = 2;
      code = byte & 0x1Fu;
    } else if ((byte & 0xF0) == 0xE0) {
      length = 3;
      code = byte & 0x0Fu;
    } else if ((byte & 0xF8) == 0xF0) {
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
          if ((continuation & 0xC0) != 0x80) {
            code = 0xFFFD;
            length = k;
            break;
          }
          code = (code << 6) | (continuation & 0x3Fu);
        }
      }
    }
    if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) code = 0xFFFD;
    if (code >= 0x10000) {
      const char32_t rest = code - 0x10000;
      out.push_back((char16_t)(0xD800 + (rest >> 10)));
      out.push_back((char16_t)(0xDC00 + (rest & 0x3FF)));
    } else {
      out.push_back((char16_t)code);
    }
    i += length;
  }
  return out;
}

namespace {

using Ranges = std::vector<sigil::weave::CharRange>;

/** Sorted, merged, empties dropped — the one normal form every answer is
 *  in, so union, intersection and complement are honest interval
 *  arithmetic and not a pile of special cases. */
Ranges normalize(Ranges ranges) {
  std::erase_if(ranges,
                [](const sigil::weave::CharRange& r) { return r.empty(); });
  std::sort(
      ranges.begin(), ranges.end(),
      [](const sigil::weave::CharRange& a, const sigil::weave::CharRange& b) {
        return a.start != b.start ? a.start < b.start : a.end < b.end;
      });
  Ranges merged;
  for (const sigil::weave::CharRange& r : ranges) {
    if (!merged.empty() && r.start <= merged.back().end)
      merged.back().end = std::max(merged.back().end, r.end);
    else
      merged.push_back(r);
  }
  return merged;
}

Ranges intersectRanges(const Ranges& a, const Ranges& b) {
  Ranges out;
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    const uint32_t start = std::max(a[i].start, b[j].start);
    const uint32_t end = std::min(a[i].end, b[j].end);
    if (start < end) out.push_back({start, end});
    if (a[i].end < b[j].end)
      ++i;
    else
      ++j;
  }
  return out;
}

Ranges complementRanges(const Ranges& ranges, uint32_t length) {
  Ranges out;
  uint32_t cursor = 0;
  for (const sigil::weave::CharRange& r : ranges) {
    if (r.start > cursor) out.push_back({cursor, r.start});
    cursor = std::max(cursor, r.end);
  }
  if (cursor < length) out.push_back({cursor, length});
  return out;
}

}  // namespace

void warnWritingModeOnPath() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  std::fprintf(stderr,
               "SigilCompose: onPath() and writingMode() on one text leaf — "
               "a path run's baseline IS its geometry and has no columns to "
               "advance, so the path stands and the writing mode is dropped\n");
}

void warnFlowAroundVertical() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  std::fprintf(stderr,
               "SigilCompose: flowAround() on vertical text — exclusions are "
               "cut out of horizontal line bands, so the columns run without "
               "them\n");
}

namespace {

/** Once per process: an `sel::each` slice asked of a text range. */
void warnSliceIgnored() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  std::fprintf(stderr,
               "SigilCompose: Selector::take/drop slice GLYPHS inside a "
               "unit, which a text range cannot express — this span restyle "
               "covers whole units\n");
}

Ranges resolveTextRangesInto(
    const Selector& selector, sigil::weave::Paragraph& paragraph,
    sigil::weave::FontContext& fonts,
    std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns,
    std::span<const NamedRun> named) {
  const auto length = (uint32_t)paragraph.text().size();
  const Selector::State* s = selector.state();
  if (!s) return {{0, length}};  // default-constructed: everything
  switch (s->kind) {
    case Selector::Kind::All:
      return {{0, length}};
    case Selector::Kind::Word:
    case Selector::Kind::Words: {
      Ranges words = sigil::weave::wordRanges(paragraph, fonts);
      Ranges out;
      for (uint32_t i = s->lo; i < s->hi && i < words.size(); ++i)
        out.push_back(words[i]);
      return normalize(std::move(out));
    }
    case Selector::Kind::Line: {
      // A vertical passage numbers COLUMNS where a horizontal one numbers
      // lines, and only one of the two lists is ever populated.
      Ranges out;
      for (const sigil::weave::LineMetrics& line : lines)
        if ((uint32_t)line.lineIndex >= s->lo &&
            (uint32_t)line.lineIndex < s->hi)
          out.push_back({line.textBegin, line.textEnd});
      for (const sigil::weave::ColumnMetrics& column : columns)
        if ((uint32_t)column.lineIndex >= s->lo &&
            (uint32_t)column.lineIndex < s->hi)
          out.push_back({column.textBegin, column.textEnd});
      return normalize(std::move(out));
    }
    case Selector::Kind::Sentence: {
      const std::span<const uint32_t> starts = paragraph.sentenceStarts();
      Ranges out;
      for (uint32_t i = s->lo; i < s->hi && i < starts.size(); ++i)
        out.push_back(
            {starts[i], i + 1 < starts.size() ? starts[i + 1] : length});
      return normalize(std::move(out));
    }
    case Selector::Kind::Range:
      return normalize({{std::min(s->lo, length), std::min(s->hi, length)}});
    case Selector::Kind::Text:
      return normalize(sigil::weave::findAllOccurrences(paragraph, s->pattern));
    case Selector::Kind::Regex: {
      std::optional<Ranges> matches =
          sigil::weave::findRegexMatches(paragraph, s->pattern);
      if (!matches) {
        warnBadSelectorPattern(s->pattern);
        return {};
      }
      return normalize(*std::move(matches));
    }
    case Selector::Kind::Style: {
      Ranges runs = namedRunRanges(named, s->pattern);
      if (runs.empty()) warnNoSuchStyleName(s->pattern);
      return normalize(std::move(runs));
    }
    case Selector::Kind::Each: {
      // A unit's whole extent. The glyph slice has no text-range meaning;
      // saying so once beats a restyle that silently covers more than the
      // author asked for.
      if (s->take >= 0 || s->drop > 0) warnSliceIgnored();
      switch (s->each) {
        case Unit::Word:
          return normalize(sigil::weave::wordRanges(paragraph, fonts));
        case Unit::Line: {
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
    case Selector::Kind::Union: {
      Ranges out = resolveTextRangesInto(s->operands[0], paragraph, fonts,
                                         lines, columns, named);
      Ranges rhs = resolveTextRangesInto(s->operands[1], paragraph, fonts,
                                         lines, columns, named);
      out.insert(out.end(), rhs.begin(), rhs.end());
      return normalize(std::move(out));
    }
    case Selector::Kind::Intersect:
      return intersectRanges(
          resolveTextRangesInto(s->operands[0], paragraph, fonts, lines,
                                columns, named),
          resolveTextRangesInto(s->operands[1], paragraph, fonts, lines,
                                columns, named));
    case Selector::Kind::Complement:
      return complementRanges(
          resolveTextRangesInto(s->operands[0], paragraph, fonts, lines,
                                columns, named),
          length);
  }
  return {};
}

}  // namespace

std::vector<sigil::weave::CharRange> resolveTextRanges(
    const Selector& selector, sigil::weave::Paragraph& paragraph,
    sigil::weave::FontContext& fonts,
    std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns,
    std::span<const NamedRun> named) {
  return resolveTextRangesInto(selector, paragraph, fonts, lines, columns,
                               named);
}

bool selectorNeedsLayout(const Selector& selector) {
  const Selector::State* s = selector.state();
  if (!s) return false;
  if (s->kind == Selector::Kind::Line) return true;
  if (s->kind == Selector::Kind::Each && s->each == Unit::Line) return true;
  for (const Selector& operand : s->operands)
    if (selectorNeedsLayout(operand)) return true;
  return false;
}

void TextOptions::applyTo(sigil::weave::ParagraphLayoutOptions& options) const {
  if (set & kAlignment) options.alignment = alignment;
  if (set & kLineBreak) options.lineBreakStrategy = lineBreak;
  if (set & kHyphenation) options.hyphenation = hyphenation;
  if (set & kEllipsis) options.overflow.ellipsis = ellipsis;
  if (set & kMaxLines) options.overflow.maxLines = maxLines;
  if (set & kLastLine) {
    options.justification.lastLineAlignment = lastLineAlignment;
    options.justification.justifyLastLine = justifyLastLine;
  }
}

// ---------------------------------------------------------------------------
// The cascade

namespace {
/** SplitMix64's finalizer over one key — the same mixer Rng steps, used
 *  here to order units rather than to shape a glyph. */
uint64_t mix64Value(uint64_t z) {
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}
}  // namespace

void cascadeOrder(Stagger::From from, uint32_t count,
                  std::vector<float>& order) {
  order.assign(count, 0.0f);
  // A cascade of ONE is a cascade with no spread, whichever end it claims
  // to start from: every shape below must put that single member at 0.
  const float last = count > 1 ? (float)(count - 1) : 0.0f;
  switch (from) {
    case Stagger::From::Start:
      for (uint32_t i = 0; i < count; ++i) order[i] = (float)i;
      break;
    case Stagger::From::End:
      for (uint32_t i = 0; i < count; ++i) order[i] = (float)(count - 1 - i);
      break;
    case Stagger::From::Center:
      for (uint32_t i = 0; i < count; ++i)
        order[i] = std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case Stagger::From::Edges:
      for (uint32_t i = 0; i < count; ++i)
        order[i] = last - std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case Stagger::From::Random: {
      // Rank each unit by a hash of its index: deterministic, so the same
      // text scatters the same way on every frame and after a relayout.
      std::vector<uint32_t> indices(count);
      std::iota(indices.begin(), indices.end(), 0u);
      std::stable_sort(indices.begin(), indices.end(),
                       [count](uint32_t a, uint32_t b) {
                         return mix64Value(a * 2654435761ull + count) <
                                mix64Value(b * 2654435761ull + count);
                       });
      for (uint32_t rank = 0; rank < count; ++rank)
        order[indices[rank]] = (float)rank;
      break;
    }
  }
}

namespace {
/** The per-unit spacing this cascade asks for, in ms. Amount-mode divides
 *  a fixed total across however many units there are; otherwise the
 *  spacing is fixed and the total grows. */
float spacingMs(const Stagger& spec, uint32_t count) {
  if (spec.amountMs > 0 && count > 1) return spec.amountMs / (float)(count - 1);
  return std::max(spec.eachMs, 0.0f);
}

/** The table entry unit `index` reads. Past the end it is the LAST entry:
 *  a short table piles its tail on one beat, which is visible, rather than
 *  extrapolating times its author never wrote. */
float cueAt(const std::vector<float>& table, uint32_t index) {
  return table[std::min<size_t>(index, table.size() - 1)];
}

/** The latest start any of `count` units reads out of `table` — what the
 *  master progress has to span for the last beat to open. A table is not
 *  required to ascend, so this is a max and not the final entry. */
float lastCueMs(const std::vector<float>& table, uint32_t count) {
  float latest = 0.0f;
  const size_t read = std::min<size_t>(count, table.size());
  for (size_t i = 0; i < read; ++i) latest = std::max(latest, table[i]);
  return latest;
}
}  // namespace

void warnCueTableMismatch(size_t cueCount, size_t unitCount) {
  // Once per distinct shape: a cascade is rebuilt every frame, and one
  // mistyped table would otherwise scroll the same line past its author
  // forever. Distinct shapes still each get their say, because two tracks
  // can be wrong in two different ways.
  static thread_local std::unordered_set<uint64_t> seen;
  const uint64_t key = ((uint64_t)cueCount << 32) | (uint32_t)unitCount;
  if (!seen.insert(key).second) return;
  std::fprintf(stderr,
               "SigilCompose: a cue table of %zu times against %zu units — "
               "%s\n",
               cueCount, unitCount,
               cueCount < unitCount
                   ? "every unit past the table's end starts at its last time"
                   : "the times past the last unit are never read");
}

void Cascade::build(const Stagger& spec, uint32_t outerCount,
                    uint32_t innerCount) {
  duration = std::max(spec.durationMs, 1.0f);
  const uint32_t outer = std::max(outerCount, 1u);
  cascadeOrder(spec.from, outer, outerOrder);
  outerEach = spacingMs(spec, outer);
  outerCue = spec.cueMs;
  if (!outerCue.empty() && outerCue.size() != outer)
    warnCueTableMismatch(outerCue.size(), outer);

  if (spec.inner) {
    const uint32_t inner = std::max(innerCount, 1u);
    cascadeOrder(spec.inner->from, inner, innerOrder);
    innerEach = spacingMs(*spec.inner, inner);
    innerCue = spec.inner->cueMs;
    if (!innerCue.empty() && innerCue.size() != inner)
      warnCueTableMismatch(innerCue.size(), inner);
    // A NESTED cascade owns the beat: its own duration is what one unit's
    // motion lasts, and a beat is exactly as long as the inner ladder
    // needs. The outer durationMs would otherwise be a second, conflicting
    // statement about the same span.
    duration = std::max(spec.inner->durationMs, 1.0f);
    beatMs = duration + (innerCue.empty() ? innerEach * (float)(inner - 1)
                                          : lastCueMs(innerCue, inner));
    innerDistribution = spec.inner->distribution;
  } else {
    innerOrder.clear();
    innerCue.clear();
    innerEach = 0.0f;
    beatMs = duration;
    innerDistribution = nullptr;
  }
  outerDistribution = spec.distribution;
  totalMs = beatMs + (outerCue.empty() ? outerEach * (float)(outer - 1)
                                       : lastCueMs(outerCue, outer));
}

float Cascade::startMs(uint32_t outerUnit, uint32_t innerUnit) const {
  // Without a distribution curve the delay is the plain product the flat
  // cascade has always been — NOT the same product routed through a
  // normalise-and-rescale, which would differ in the last bit and move
  // every pixel of a settled reveal.
  const auto delayOf = [](const std::vector<float>& order, uint32_t index,
                          float each, const choreograph::EaseFn& shape) {
    if (order.empty()) return 0.0f;
    const uint32_t clamped = std::min<uint32_t>(index, order.size() - 1);
    if (!shape) return order[clamped] * each;
    const float last = order.size() > 1 ? (float)(order.size() - 1) : 1.0f;
    return shape(order[clamped] / last) * (each * last);
  };
  // A table states the delay; the ladder computes one. Nothing else about
  // the cascade changes between the two.
  return (outerCue.empty()
              ? delayOf(outerOrder, outerUnit, outerEach, outerDistribution)
              : cueAt(outerCue, outerUnit)) +
         (innerCue.empty()
              ? delayOf(innerOrder, innerUnit, innerEach, innerDistribution)
              : cueAt(innerCue, innerUnit));
}

float Cascade::localTime(float master, uint32_t outerUnit,
                         uint32_t innerUnit) const {
  return std::clamp(
      (master * totalMs - startMs(outerUnit, innerUnit)) / duration, 0.0f,
      1.0f);
}

void TrackCascade::build(const Stagger& spec, const GlyphStructure& structure,
                         const std::vector<uint8_t>& selected) {
  const auto count = (uint32_t)structure.glyphs.size();
  const std::vector<uint32_t>& outerLane = structure.unitOf[(size_t)spec.over];
  outerUnit.assign(count, 0);
  uint32_t outerCount = 0;
  if (spec.beatsOver == Beats::Text) {
    // THE PARAGRAPH'S OWN NUMBERING, which is the whole point of the
    // setting: a unit's beat does not depend on which of its glyphs this
    // track happens to address, so two tracks that split one paragraph run
    // one clock however differently their selections resolve.
    for (uint32_t g = 0; g < count; ++g) outerUnit[g] = outerLane[g];
    outerCount = structure.unitCounts[(size_t)spec.over];
  } else {
    // Renumber the units the SELECTION covers, from 0, in draw order — then
    // a stagger's From, its amount-mode division and its distribution all
    // read the count the author sees rather than the paragraph's.
    uint32_t previous = ~0u;
    for (uint32_t g = 0; g < count; ++g) {
      if (!selected[g]) continue;
      if (outerLane[g] != previous) {
        previous = outerLane[g];
        ++outerCount;
      }
      outerUnit[g] = outerCount - 1;
    }
  }

  uint32_t innerCount = 0;
  if (spec.inner) {
    // The nested level is numbered against the same list as the outer one:
    // one setting governs the cascade, so a nested beat cannot be counted
    // one way at the top and another underneath.
    const bool overText = spec.beatsOver == Beats::Text;
    const std::vector<uint32_t>& innerLane =
        structure.unitOf[(size_t)spec.inner->over];
    innerUnit.assign(count, 0);
    uint32_t within = 0, previousOuter = ~0u, previousInner = ~0u;
    for (uint32_t g = 0; g < count; ++g) {
      if (!overText && !selected[g]) continue;
      if (outerUnit[g] != previousOuter) {
        previousOuter = outerUnit[g];
        previousInner = ~0u;
        within = 0;
      }
      if (innerLane[g] != previousInner) {
        previousInner = innerLane[g];
        ++within;
      }
      innerUnit[g] = within - 1;
      innerCount = std::max(innerCount, within);
    }
  } else {
    innerUnit.clear();
  }
  cascade.build(spec, outerCount, innerCount);
}

// ---------------------------------------------------------------------------
// Composition

/** FIELD PIN. `compose()` and `lerpMod()` below are hand-written exhaustive
 *  lists over GlyphMod's members, and so is the routing decision in
 *  Paint.cpp that sends a glyph down the matrix path. All three fail the
 *  same way when a field is added and one of them is not told: silently, by
 *  drawing the deviation of some other track or some other moment. */
void glyphModFieldPin(GlyphMod& v) {
  auto& [dx, dy, scale, rotateDeg, alpha, colorMul, scaleX, scaleY, skewXDeg,
         skewYDeg, axis, codepoint] = v;
  static_assert(
      std::tuple_size_v<decltype(std::tie(dx, dy, scale, rotateDeg, alpha,
                                          colorMul, scaleX, scaleY, skewXDeg,
                                          skewYDeg, axis, codepoint))> == 12,
      "GlyphMod gained or lost a field — rule on it in compose() and "
      "lerpMod() below, and in the matrix ROUTING in Paint.cpp (a field an "
      "RSXform cannot carry has to send its glyph down the matrix path), "
      "then bump this count.");
}

void compose(GlyphMod& into, const GlyphMod& next) {
  into.dx += next.dx;
  into.dy += next.dy;
  into.rotateDeg += next.rotateDeg;
  into.skewXDeg += next.skewXDeg;
  into.skewYDeg += next.skewYDeg;
  into.scale *= next.scale;
  into.scaleX *= next.scaleX;
  into.scaleY *= next.scaleY;
  into.alpha *= next.alpha;
  into.colorMul = {
      into.colorMul.fR * next.colorMul.fR, into.colorMul.fG * next.colorMul.fG,
      into.colorMul.fB * next.colorMul.fB, into.colorMul.fA * next.colorMul.fA};
  // The two SUBSTITUTIONS are last-one-wins rather than combined: two
  // tracks naming two outlines for one glyph have no arithmetic between
  // them, and averaging their numbers would draw a third thing neither
  // asked for.
  if (next.axis) into.axis = next.axis;
  if (next.codepoint) into.codepoint = next.codepoint;
}

GlyphMod lerpMod(const GlyphMod& a, const GlyphMod& b, float w) {
  GlyphMod out;
  out.dx = a.dx + (b.dx - a.dx) * w;
  out.dy = a.dy + (b.dy - a.dy) * w;
  out.rotateDeg = a.rotateDeg + (b.rotateDeg - a.rotateDeg) * w;
  out.skewXDeg = a.skewXDeg + (b.skewXDeg - a.skewXDeg) * w;
  out.skewYDeg = a.skewYDeg + (b.skewYDeg - a.skewYDeg) * w;
  out.scale = a.scale + (b.scale - a.scale) * w;
  out.scaleX = a.scaleX + (b.scaleX - a.scaleX) * w;
  out.scaleY = a.scaleY + (b.scaleY - a.scaleY) * w;
  out.alpha = a.alpha + (b.alpha - a.alpha) * w;
  out.colorMul = {a.colorMul.fR + (b.colorMul.fR - a.colorMul.fR) * w,
                  a.colorMul.fG + (b.colorMul.fG - a.colorMul.fG) * w,
                  a.colorMul.fB + (b.colorMul.fB - a.colorMul.fB) * w,
                  a.colorMul.fA + (b.colorMul.fA - a.colorMul.fA) * w};
  // An axis coordinate is the one substitution with a continuum: two
  // phases driving the SAME axis blend their values and the face
  // interpolates between them. Everything else CUTS at the middle of the
  // window — there is no half-way glyph between two outlines, and lerping
  // a code point would draw whatever letter happened to sit between them
  // in the font's encoding.
  out.axis = a.axis;
  if (a.axis && b.axis && std::memcmp(a.axis->tag, b.axis->tag, 4) == 0)
    out.axis->value = a.axis->value + (b.axis->value - a.axis->value) * w;
  else if (w >= 0.5f)
    out.axis = b.axis;
  out.codepoint = w >= 0.5f ? b.codepoint : a.codepoint;
  return out;
}

uint64_t glyphSeed(const GlyphInfo& g, uint32_t lane) {
  // The glyph's identity, and only that: its position in the walk and the
  // text offset it came from. Both hold across relayouts while the text is
  // unchanged, which is what makes a seeded scatter cacheable. `lane`
  // separates the operands of a composite, so mixing two scatters gives two
  // scatters rather than one drawn twice — and it is the OPERAND's index,
  // never a counter, so a phase draws the same numbers whether it is being
  // crossfaded into or playing alone.
  return mix64Value(((uint64_t)g.textIndex << 32) ^ (uint64_t)g.index) +
         mix64Value(lane);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// The combinators

namespace fx {

TextEffect seq(std::vector<Phase> phases) {
  if (phases.empty()) return TextEffect();
  // The last phase always runs to the end of local time, whatever it was
  // declared with — otherwise the tail of every sequence is undefined.
  phases.back() = Phase(phases.back().effect(), 1.0f).xfade(0.0f);

  std::vector<TextEffect> operands;
  std::vector<float> params;
  operands.reserve(phases.size());
  params.reserve(phases.size() * 2);
  float reach = 0;
  for (const Phase& p : phases) {
    operands.push_back(p.effect());
    params.push_back(p.endsAt());
    params.push_back(p.overlap());
    reach = std::max(reach, p.effect().reach());
  }
  return TextEffect::composite(
      "seq", params, operands,
      [phases](const GlyphInfo& g, float t, Rng&) {
        // Which window `t` falls in, and where inside it.
        const auto windowAt = [&](size_t i) {
          const float begin = i == 0 ? 0.0f : phases[i - 1].endsAt();
          const float end = std::max(phases[i].endsAt(), begin);
          return std::pair<float, float>(begin, end);
        };
        size_t index = phases.size() - 1;
        for (size_t i = 0; i < phases.size(); ++i)
          if (t < phases[i].endsAt()) {
            index = i;
            break;
          }
        const auto [begin, end] = windowAt(index);
        const float width = end - begin;
        const float local =
            width > 0 ? std::clamp((t - begin) / width, 0.0f, 1.0f) : 1.0f;
        Rng own(compose::detail::glyphSeed(g, (uint32_t)index));
        GlyphMod mod = phases[index].effect()(g, local, own);
        // The crossfade window sits at the END of this phase, so at the
        // joint the blend has already reached the next phase's own start.
        const float overlap = phases[index].overlap();
        if (overlap > 0 && index + 1 < phases.size() && t > end - overlap) {
          const float w =
              std::clamp((t - (end - overlap)) / overlap, 0.0f, 1.0f);
          Rng nextRng(compose::detail::glyphSeed(g, (uint32_t)index + 1));
          const GlyphMod next = phases[index + 1].effect()(g, 0.0f, nextRng);
          mod = compose::detail::lerpMod(mod, next, w);
        }
        return mod;
      },
      reach);
}

namespace {

/** One entry's numbers, laid end to end. Every field of a GlyphMod is here,
 *  at a fixed stride, so two tables compare exactly when they say the same
 *  thing — structural equality over the whole table rather than a digest of
 *  it. The two substitutions ride along as numbers: a code point IS one, and
 *  an axis is its four tag bytes, its value, and whether it was set at all. */
void appendKeyParams(std::vector<float>& out, const Key& key) {
  const GlyphMod& m = key.mod;
  out.insert(out.end(), {key.at, m.dx, m.dy, m.scale, m.rotateDeg, m.alpha,
                         m.colorMul.fR, m.colorMul.fG, m.colorMul.fB,
                         m.colorMul.fA, m.scaleX, m.scaleY, m.skewXDeg,
                         m.skewYDeg, (float)m.codepoint, m.axis ? 1.0f : 0.0f});
  const sigil::weave::FontVariation axis =
      m.axis.value_or(sigil::weave::FontVariation());
  for (const char byte : axis.tag) out.push_back((float)(unsigned char)byte);
  out.push_back(axis.value);
}

/** How far past its box a table may throw a glyph. Every published entry is
 *  read: the offsets outright, and a growth or a lean as the fraction of the
 *  glyph it displaces, against the nominal display size no effect knows at
 *  construction. Over-reporting is safe, so the two are added rather than
 *  reasoned about. */
float keysReach(const std::vector<Key>& table) {
  float reach = 0;
  for (const Key& key : table) {
    const GlyphMod& m = key.mod;
    const float grown = std::max({std::abs(m.scale * m.scaleX),
                                  std::abs(m.scale * m.scaleY), 1.0f}) -
                        1.0f;
    const bool leans = m.rotateDeg != 0 || m.skewXDeg != 0 || m.skewYDeg != 0;
    reach = std::max(reach, std::abs(m.dx) + std::abs(m.dy) +
                                (grown + (leans ? 0.5f : 0.0f)) *
                                    fx::detail::kNominalSizePx);
  }
  return reach;
}

}  // namespace

TextEffect keys(std::vector<Key> table, choreograph::EaseFn ease) {
  if (table.empty()) return TextEffect();
  std::vector<float> params;
  params.reserve(table.size() * 21);
  // The table-wide curve first, then one slot per entry whether or not that
  // entry overrode it: equal tables then always compare curve lists of equal
  // length, and a curve moved from one entry to another is a difference.
  std::vector<choreograph::EaseFn> curves;
  curves.reserve(table.size() + 1);
  curves.push_back(ease);
  for (const Key& key : table) {
    appendKeyParams(params, key);
    curves.push_back(key.ease);
  }
  const float reach = keysReach(table);
  return TextEffect(
      "keys", std::move(params),
      [table = std::move(table), ease = std::move(ease)](const GlyphInfo&,
                                                         float t, Rng&) {
        t = std::clamp(t, 0.0f, 1.0f);
        if (t <= table.front().at) return table.front().mod;
        for (size_t i = 1; i < table.size(); ++i) {
          if (t > table[i].at) continue;
          const Key& from = table[i - 1];
          const Key& to = table[i];
          const float span = to.at - from.at;
          // A zero-width segment is a STEP, and the later entry is what a
          // step lands on.
          const float u = span > 0 ? (t - from.at) / span : 1.0f;
          // The curve is the one named on the segment's OPENING entry, which
          // is where a keyframe list states it.
          const choreograph::EaseFn& curve = from.ease ? from.ease : ease;
          return compose::detail::lerpMod(from.mod, to.mod,
                                          curve ? curve(u) : u);
        }
        return table.back().mod;
      },
      reach, std::move(curves));
}

TextEffect hold(TextEffect effect) {
  const float reach = effect.reach();
  std::vector<TextEffect> operands{effect};
  return TextEffect::composite(
      "hold", {}, std::move(operands),
      [effect = std::move(effect)](const GlyphInfo& g, float t, Rng& rng) {
        // Local time is CLAMPED at both ends, so a unit whose beat has not
        // opened is handed 0 and one whose beat is over is handed 1 — which
        // makes t at its floor the whole signal there is that a beat is
        // still to come.
        if (t <= 0.0f) {
          GlyphMod mod;
          mod.alpha = 0.0f;
          return mod;
        }
        // The glyph's OWN stream, not a lane of its own: a held effect draws
        // exactly the numbers it would have drawn unheld.
        return effect(g, t, rng);
      },
      reach);
}

TextEffect scramble(std::u32string charset, int steps) {
  // The charset rides the effect's PARAMETERS, one code point per float:
  // every code point is exactly representable, so two scrambles over the
  // same characters compare equal and two over different ones do not —
  // structural equality, not a hash that could collide.
  std::vector<float> params;
  params.reserve(charset.size() + 1);
  params.push_back((float)steps);
  for (char32_t point : charset) params.push_back((float)(uint32_t)point);
  const uint32_t ticks = (uint32_t)std::max(steps, 1);
  return TextEffect(
      "scramble", std::move(params),
      [charset = std::move(charset), ticks](const GlyphInfo&, float t,
                                            Rng& rng) {
        GlyphMod mod;
        if (charset.empty()) return mod;
        // ONE draw from the glyph's own stream, and everything below is
        // derived from it — so a glyph churns through the same characters
        // at the same moments on every frame and after every relayout,
        // which is what lets a settled scramble cache instead of boiling
        // forever.
        const uint32_t seed = rng.bits();
        // Each glyph resolves at its own moment, and every one of them has
        // resolved by t = 1: the point of the effect is that the true text
        // is what the reader is left with.
        const float settle = 0.35f + (float)(seed >> 24) * (0.6f / 255.0f);
        if (t >= settle) return mod;
        const uint32_t tick =
            (uint32_t)(std::clamp(t, 0.0f, 1.0f) * (float)ticks);
        mod.codepoint =
            charset[compose::detail::mix64Value(seed + tick * 0x9e3779b9u) %
                    charset.size()];
        return mod;
      },
      0.0f);
}

TextEffect mix(std::vector<TextEffect> effects) {
  if (effects.empty()) return TextEffect();
  float reach = 0;
  for (const TextEffect& e : effects) reach += e.reach();
  std::vector<TextEffect> operands = effects;
  return TextEffect::composite(
      "mix", {}, std::move(operands),
      [effects = std::move(effects)](const GlyphInfo& g, float t, Rng&) {
        GlyphMod out;
        for (size_t i = 0; i < effects.size(); ++i) {
          Rng own(compose::detail::glyphSeed(g, (uint32_t)i));
          compose::detail::compose(out, effects[i](g, t, own));
        }
        return out;
      },
      reach);
}

}  // namespace fx

}  // namespace sigil::compose
