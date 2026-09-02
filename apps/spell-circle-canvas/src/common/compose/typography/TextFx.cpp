/** @file
 * The text-fx seam's resolution against a laid-out paragraph: the per-walk
 * glyph structure every track shares, selection as glyphs and as text
 * ranges, the walk that decides which beat each glyph falls in at each
 * level, the composition algebra, and the stock combinators. The
 * arithmetic over those beat numbers is SigilMotion's cascade; the
 * comparable values themselves — `TextEffect`, `Selector` — are the
 * kernel's and are built in Element.cpp.
 *
 * Nothing here touches an Instance or a canvas. TextFxPainting.cpp drives it:
 * build the structure once per frame, resolve each track's selection (cached on
 * the element), then walk the glyphs asking this file for each one's local
 * time and composing the deviations.
 */

#include <sigilcompose/typography/TextFx.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/query/Query.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <unordered_set>
#include <utility>

#include "TextEngine.h"

namespace sigil::compose {

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
                           const sigil::weave::Paragraph& paragraph,
                           TextScope textScope) {
  scope = textScope;
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
        // THE STORY'S LINE, not this frame's: a frame of a chain is told
        // where its first line stands in the story's numbering, and every
        // line it placed is that many further on.
        info.lineIndex =
            (uint32_t)std::max(placed.lineIndex, 0) + textScope.lineOffset;
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
    case Selector::Kind::InFrame: {
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

void warnNoSuchFrameKey(const std::u8string& key) {
  static thread_local std::unordered_set<std::string> seen;
  std::string name((const char*)key.data(), key.size());
  if (!seen.insert(name).second) return;
  std::fprintf(stderr,
               "SigilCompose: sel::inFrame(\"%s\") on a text leaf with no "
               "key() of its own — a frame-local address is matched against "
               "the leaf's own key, so this one can never match and "
               "addresses nothing\n",
               name.c_str());
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
    std::span<const NamedRun> named, const TextScope& scope) {
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
        if (within(line.lineIndex)) out.push_back({line.textBegin, line.textEnd});
      for (const sigil::weave::ColumnMetrics& column : columns)
        if (within(column.lineIndex))
          out.push_back({column.textBegin, column.textEnd});
      return normalize(std::move(out));
    }
    case Selector::Kind::InFrame: {
      const std::u8string_view key((const char8_t*)scope.frameKey.data(),
                                   scope.frameKey.size());
      if (key.empty()) warnNoSuchFrameKey(s->pattern);
      if (!key.empty() && key == s->pattern) return {{0, length}};
      return {};
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
                                         lines, columns, named, scope);
      Ranges rhs = resolveTextRangesInto(s->operands[1], paragraph, fonts,
                                         lines, columns, named, scope);
      out.insert(out.end(), rhs.begin(), rhs.end());
      return normalize(std::move(out));
    }
    case Selector::Kind::Intersect:
      return intersectRanges(
          resolveTextRangesInto(s->operands[0], paragraph, fonts, lines,
                                columns, named, scope),
          resolveTextRangesInto(s->operands[1], paragraph, fonts, lines,
                                columns, named, scope));
    case Selector::Kind::Complement:
      return complementRanges(
          resolveTextRangesInto(s->operands[0], paragraph, fonts, lines,
                                columns, named, scope),
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
    std::span<const NamedRun> named, TextScope scope) {
  return resolveTextRangesInto(selector, paragraph, fonts, lines, columns,
                               named, scope);
}

void TrackCascade::build(const Track& track, const GlyphStructure& structure,
                         const std::vector<uint8_t>& selected) {
  const motion::Spread& spec = track.stagger;
  const auto count = (uint32_t)structure.glyphs.size();
  const std::vector<uint32_t>& outerLane = structure.unitOf[(size_t)track.over];
  outerUnit.assign(count, 0);
  uint32_t outerCount = 0;
  if (track.beatsOver == Beats::Text) {
    // THE PARAGRAPH'S OWN NUMBERING, which is the whole point of the
    // setting: a unit's beat does not depend on which of its glyphs this
    // track happens to address, so two tracks that split one paragraph run
    // one clock however differently their selections resolve.
    for (uint32_t g = 0; g < count; ++g) outerUnit[g] = outerLane[g];
    outerCount = structure.unitCounts[(size_t)track.over];
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
    const bool overText = track.beatsOver == Beats::Text;
    const std::vector<uint32_t>& innerLane =
        structure.unitOf[(size_t)track.innerOver];
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
 *  TextFxPainting.cpp that sends a glyph down the matrix path. All three fail
 * the same way when a field is added and one of them is not told: silently, by
 *  drawing the deviation of some other track or some other moment. */
void glyphModFieldPin(GlyphMod& v) {
  auto& [dx, dy, scale, rotateDeg, alpha, colorMul, colorAdd, colorScreen,
         scaleX, scaleY, skewXDeg, skewYDeg, axis, codepoint] = v;
  static_assert(
      std::tuple_size_v<decltype(std::tie(
              dx, dy, scale, rotateDeg, alpha, colorMul, colorAdd, colorScreen,
              scaleX, scaleY, skewXDeg, skewYDeg, axis, codepoint))> == 14,
      "GlyphMod gained or lost a field — rule on it in compose() and "
      "lerpMod() below, in appendKeyParams() (a field a keys table cannot "
      "spell makes two different tables compare equal), and in the matrix "
      "ROUTING in TextFxPainting.cpp (a field an RSXform cannot carry has to "
      "send "
      "its glyph down the matrix path), then bump this count.");
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
  // The additive term ADDS unclamped — the clamp happens once, at the draw,
  // so two half flashes make one full one rather than each clamping alone.
  into.colorAdd = {
      into.colorAdd.fR + next.colorAdd.fR, into.colorAdd.fG + next.colorAdd.fG,
      into.colorAdd.fB + next.colorAdd.fB, into.colorAdd.fA + next.colorAdd.fA};
  // The screen term SCREENS: 1 − (1−a)(1−b), commutative and associative,
  // so stacked glows compose in any track order and never leave [0,1].
  const auto screen = [](float a, float b) { return 1 - (1 - a) * (1 - b); };
  into.colorScreen = {screen(into.colorScreen.fR, next.colorScreen.fR),
                      screen(into.colorScreen.fG, next.colorScreen.fG),
                      screen(into.colorScreen.fB, next.colorScreen.fB),
                      screen(into.colorScreen.fA, next.colorScreen.fA)};
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
  // The two colour terms lerp componentwise like every other continuous
  // field — a flash decays through straight interpolation of its own
  // channels, not through the compose() arithmetic, which is for stacking.
  const auto lerpColor = [w](const SkColor4f& x, const SkColor4f& y) {
    return SkColor4f{x.fR + (y.fR - x.fR) * w, x.fG + (y.fG - x.fG) * w,
                     x.fB + (y.fB - x.fB) * w, x.fA + (y.fA - x.fA) * w};
  };
  out.colorAdd = lerpColor(a.colorAdd, b.colorAdd);
  out.colorScreen = lerpColor(a.colorScreen, b.colorScreen);
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
  return mix64Value(((uint64_t)g.textIndex << 32u) ^ (uint64_t)g.index) +
         mix64Value(lane);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// The combinators

namespace fx {

namespace {
/** A combinator's placement fact, derived from what it may evaluate: it
 *  moves glyphs when any operand does. Deriving rather than declaring is
 *  what keeps the answer exact through nesting — a `fx::mix` of three tints
 *  and one `fx::rise` displaces, the same mix without the rise does not. */
template <typename Range>
bool anyDisplaces(const Range& operands) {
  for (const auto& operand : operands)
    if (operand.displaces()) return true;
  return false;
}
}  // namespace

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
  const bool displaces = anyDisplaces(operands);
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
      reach, displaces);
}

namespace {

/** One entry's numbers, laid end to end. Every field of a GlyphMod is here,
 *  at a fixed stride, so two tables compare exactly when they say the same
 *  thing — structural equality over the whole table rather than a digest of
 *  it. The two substitutions ride along as numbers: a code point IS one, and
 *  an axis is its four tag bytes, its value, and whether it was set at all. */
void appendKeyParams(std::vector<float>& out, const Key& key) {
  const GlyphMod& m = key.mod;
  out.insert(out.end(), {key.at,
                         m.dx,
                         m.dy,
                         m.scale,
                         m.rotateDeg,
                         m.alpha,
                         m.colorMul.fR,
                         m.colorMul.fG,
                         m.colorMul.fB,
                         m.colorMul.fA,
                         m.colorAdd.fR,
                         m.colorAdd.fG,
                         m.colorAdd.fB,
                         m.colorAdd.fA,
                         m.colorScreen.fR,
                         m.colorScreen.fG,
                         m.colorScreen.fB,
                         m.colorScreen.fA,
                         m.scaleX,
                         m.scaleY,
                         m.skewXDeg,
                         m.skewYDeg,
                         (float)m.codepoint,
                         m.axis ? 1.0f : 0.0f});
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

/** Whether a table moves its glyphs off their pen positions — read off the
 *  entries, because the mods ARE the data here and no author needs to say
 *  twice what the table already says. Any offset, any lean, any growth: a
 *  glyph under it lands somewhere other than where the layout put it, and
 *  interpolation between two such entries only ever lands between them, so
 *  a table of entries that all leave the pen alone can never move it. The
 *  colour terms, the fade and the two substitutions are not placement. */
bool keysDisplace(const std::vector<Key>& table) {
  for (const Key& key : table) {
    const GlyphMod& m = key.mod;
    if (m.dx != 0 || m.dy != 0 || m.rotateDeg != 0 || m.skewXDeg != 0 ||
        m.skewYDeg != 0 || m.scale != 1 || m.scaleX != 1 || m.scaleY != 1)
      return true;
  }
  return false;
}

}  // namespace

TextEffect keys(std::vector<Key> table, choreograph::EaseFn ease) {
  if (table.empty()) return TextEffect();
  std::vector<float> params;
  params.reserve(table.size() * 29);
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
  const bool displaces = keysDisplace(table);
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
      reach, std::move(curves), displaces);
}

TextEffect hold(TextEffect effect) {
  const float reach = effect.reach();
  // The veto is alpha, which moves nothing: a hold places its glyphs exactly
  // where the effect it wraps places them.
  const bool displaces = effect.displaces();
  std::vector<TextEffect> operands{effect};
  return TextEffect::composite(
      "hold", {}, std::move(operands),
      [effect = std::move(effect)](const GlyphInfo& g, float t, Rng& rng) {
        // Local time is CLAMPED at both ends, so a unit whose beat has not
        // opened is handed 0 and one whose beat is over is handed 1 — which
        // makes t at its floor the whole signal there is that a beat is
        // still to come. A LOOPING cascade has no such state: its fold
        // keeps every unit somewhere in its cycle and touches 0 only at
        // the instant of re-opening, so there this veto blanks that one
        // instant and nothing else — a looping effect gates its own
        // arrival instead of borrowing this one.
        if (t <= 0.0f) {
          GlyphMod mod;
          mod.alpha = 0.0f;
          return mod;
        }
        // The glyph's OWN stream, not a lane of its own: a held effect draws
        // exactly the numbers it would have drawn unheld.
        return effect(g, t, rng);
      },
      reach, displaces);
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
        const float settle = 0.35f + (float)(seed >> 24u) * (0.6f / 255.0f);
        if (t >= settle) return mod;
        const uint32_t tick =
            (uint32_t)(std::clamp(t, 0.0f, 1.0f) * (float)ticks);
        mod.codepoint =
            charset[compose::detail::mix64Value(seed + tick * 0x9e3779b9u) %
                    charset.size()];
        return mod;
      },
      // A substitution draws a different outline AT THE ORIGINAL'S PEN
      // POSITION — that is the whole condition the runtime enforces on it —
      // so a churning glyph never moves.
      0.0f, {}, /*displaces=*/false);
}

TextEffect mix(std::vector<TextEffect> effects) {
  if (effects.empty()) return TextEffect();
  float reach = 0;
  for (const TextEffect& e : effects) reach += e.reach();
  const bool displaces = anyDisplaces(effects);
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
      reach, displaces);
}

TextEffect pass(Material material) {
  return TextEffect::pass(std::move(material));
}

}  // namespace fx

}  // namespace sigil::compose
