/** @file
 * THE PER-WALK GLYPH STRUCTURE every track shares: one enumeration of the
 * laid-out glyphs, with each one's place in its word, line, sentence and
 * style span filled in as the walk goes. Built once per frame and read by
 * the selection resolvers, the cascade and the painter alike, because all
 * three ask the same question about the same walk and a second enumeration
 * would number the glyphs its own way.
 *
 * Nothing here touches an Instance or a canvas.
 */

#include <sigilweave/choreograph/Choreograph.h>  // forEachPlacedGlyph
#include <sigilweave/query/Query.h>              // the unit granularities

#include <algorithm>
#include <vector>

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
bool startsUnit(sigil::weave::Unit granularity,
                const sigil::weave::PlacedGlyph& glyph,
                const sigil::weave::PlacedGlyph& previous, bool first) {
  if (first) return true;
  switch (granularity) {
    case sigil::weave::Unit::Glyph:
      return true;
    case sigil::weave::Unit::Cluster:
      // The text offset, not the shaped-run-local cluster: a base and a
      // combining mark that fell back to a second font are two runs and one
      // cluster, and a stagger that separated them would leave the accent
      // behind in mid-air.
      return glyph.wordIndex != previous.wordIndex ||
             glyph.textIndex != previous.textIndex;
    case sigil::weave::Unit::Word:
      return glyph.wordIndex != previous.wordIndex;
    case sigil::weave::Unit::Line:
      return glyph.lineIndex != previous.lineIndex;
    case sigil::weave::Unit::Sentence:
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
  for (auto& lane : storyUnitOf) lane.clear();
  unitCounts = {};
  storyUnitCounts = {};

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
          if (startsUnit((sigil::weave::Unit)lane, placed, previous, first))
            ++unitCounts[lane];
          unitOf[lane].push_back(unitCounts[lane] - 1);
        }
        if (textScope.inChain) {
          // The three lanes the placed glyph carries a STORY ordinal for.
          // A cluster and a glyph are walk positions and the walk is this
          // frame's, so they keep the frame's numbering.
          storyUnitOf[(size_t)sigil::weave::Unit::Word].push_back(
              placed.wordIndex);
          storyUnitOf[(size_t)sigil::weave::Unit::Sentence].push_back(
              placed.sentenceIndex);
          storyUnitOf[(size_t)sigil::weave::Unit::Line].push_back(
              info.lineIndex);
          storyUnitOf[(size_t)sigil::weave::Unit::Cluster].push_back(
              unitOf[(size_t)sigil::weave::Unit::Cluster].back());
          storyUnitOf[(size_t)sigil::weave::Unit::Glyph].push_back(
              unitOf[(size_t)sigil::weave::Unit::Glyph].back());
        }
        previous = placed;
        first = false;
      });

  if (textScope.inChain) {
    storyUnitCounts = unitCounts;
    storyUnitCounts[(size_t)sigil::weave::Unit::Word] =
        (uint32_t)paragraph.words().size();
    storyUnitCounts[(size_t)sigil::weave::Unit::Sentence] =
        (uint32_t)paragraph.sentenceStarts().size();
    storyUnitCounts[(size_t)sigil::weave::Unit::Line] =
        textScope.storyLines ? textScope.storyLines
                             : unitCounts[(size_t)sigil::weave::Unit::Line];
  }

  // The two per-word facts an effect reads (which letter of its word, of
  // how many) need the word's size, which is only known once the word has
  // been walked — so they are a second pass over the finished runs.
  const uint32_t total = (uint32_t)glyphs.size();
  const std::vector<uint32_t>& wordUnits =
      unitOf[(size_t)sigil::weave::Unit::Word];
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

}  // namespace detail

}  // namespace sigil::compose
