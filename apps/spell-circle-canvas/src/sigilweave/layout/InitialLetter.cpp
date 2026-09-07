/** @file
 * The initial letter: the sizing rule that reads a face's own metrics, the
 * geometry wrapper that cuts the notch out of the bands the initial covers,
 * and the placement of the initial's glyphs and of the remainder of the
 * word it split.
 *
 * The notch is cut by WRAPPING THE GEOMETRY, exactly as the line clamp is
 * imposed by wrapping it: an initial is stated in pen travel taken off the
 * head of a band, which is the one thing every geometry answers in. So a
 * block minus exclusions, a column, and a line riding a contour all wrap an
 * initial with nothing written for any of them, and both breakers see the
 * shortened bands as ordinary geometry.
 */

#include "sigilweave/layout/InitialLetter.h"

#include <hb.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPathBuilder.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <glm/vec2.hpp>

#include "ParagraphLayoutInternal.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/unicode/Unicode.h"

namespace sigil::weave {

namespace {

/// The face's own ratio of the chosen reference metric to its em, measured
/// at the size the style already carries. A face states cap height, x
/// height and ascent in its own design units and no two faces agree, which
/// is the whole reason the size is derived rather than chosen.
float referenceRatio(FontContext& fontContext, const TextStyle& style,
                     InitialLetter::Align align) {
  const float size =
      style.shaping.fontSize > 0 ? style.shaping.fontSize : 16.0f;
  if (align == InitialLetter::Align::kIdeographic) return 1.0f;  // the em box
  const sk_sp<SkTypeface> typeface = fontContext.variedTypeface(
      style.shaping.typeface, style.shaping.variations);
  SkFontMetrics metrics;
  makeFont(typeface, size).getMetrics(&metrics);
  const float reference = align == InitialLetter::Align::kHanging
                              ? -metrics.fAscent
                              : metrics.fCapHeight;
  if (reference <= 0) return 0.7f;  // a face that states none: a plain cap
  return reference / size;
}

}  // namespace

float initialLetterSize(FontContext& fontContext, const TextStyle& cap,
                        float linePitch, float lines, float firstLineReference,
                        InitialLetter::Align align) {
  if (lines <= 0) return 0;
  // The two alignments together: the initial's reference metric reaches
  // from the first line's reference point down to the baseline `lines`
  // lines on, which is `lines - 1` whole pitches plus the first line's own
  // reference height.
  const float span = (lines - 1.0f) * linePitch + firstLineReference;
  const float ratio = referenceRatio(fontContext, cap, align);
  if (ratio <= 0 || span <= 0) return 0;
  return span / ratio;
}

namespace detail {

float InitialLetterPlan::notchAt(int band) const {
  if (band < 0 || band >= bands) return 0;
  if (perBand.empty()) return notch;
  return perBand[static_cast<size_t>(band)];
}

InitialLetterPlan planInitialLetter(FontContext& fontContext,
                                    const Paragraph& paragraph,
                                    const Block& block,
                                    const Paragraph::Strut& strut) {
  InitialLetterPlan plan;
  const InitialLetter& asked = block.style.initial;
  if (asked.lines <= 0 || asked.graphemes == 0) return plan;
  const std::vector<Word>& words = paragraph.words();
  if (block.firstWord >= words.size()) return plan;
  const Word& opening = words[block.firstWord];
  if (opening.placeholderIndex >= 0) return plan;
  const std::u16string& text = paragraph.text();
  if (opening.textBegin >= opening.textEnd || opening.textEnd > text.size())
    return plan;

  // WHERE THE INITIAL ENDS is a question about the text, so it is answered
  // in the reader's units: grapheme clusters of the block's opening word.
  // An initial asked for more letters than the opening word holds takes the
  // word, never the space after it — a following word set large is a second
  // block's business.
  static thread_local std::vector<uint32_t> clusters;
  unicode::graphemeBoundaries(
      std::u16string_view(text).substr(opening.textBegin,
                                       opening.textEnd - opening.textBegin),
      clusters);
  const size_t take = std::min<size_t>(asked.graphemes, clusters.size() - 1);
  if (take == 0) return plan;
  const uint32_t capEnd = opening.textBegin + clusters[take];

  const std::span<const WordSegment> segments = opening.segments();
  const uint32_t styleIndex =
      segments.empty() ? 0u : segments.front().styleIndex;
  const std::vector<StyleSpan>& spans = paragraph.spans();
  TextStyle capStyle =
      asked.style
          ? *asked.style
          : (styleIndex < spans.size() ? spans[styleIndex].style : TextStyle{});
  const float reference =
      asked.align == InitialLetter::Align::kIdeographic
          ? (spans.empty()
                 ? block.ascent
                 : spans[std::min<size_t>(styleIndex, spans.size() - 1)]
                       .style.shaping.fontSize)
          : (asked.align == InitialLetter::Align::kHanging ? strut.ascent
                                                           : strut.capHeight);
  const float fontSize = initialLetterSize(fontContext, capStyle, block.pitch,
                                           asked.lines, reference, asked.align);
  if (fontSize <= 0) return plan;
  capStyle.shaping.fontSize = fontSize;

  const std::u16string_view capText = std::u16string_view(text).substr(
      opening.textBegin, capEnd - opening.textBegin);
  UChar32 firstCodepoint = 0;
  {
    size_t unit = 0;
    U16_NEXT(capText.data(), unit, capText.size(), firstCodepoint);
  }
  const char* languageTag = capStyle.shaping.languageTag.empty()
                                ? nullptr
                                : capStyle.shaping.languageTag.c_str();
  sk_sp<SkTypeface> typeface = fontContext.resolveTypeface(
      capStyle.shaping.typeface, firstCodepoint, languageTag);
  if (!typeface) typeface = fontContext.defaultTypeface();
  // A COLUMN'S INITIAL IS SET DOWN THE COLUMN. The notch a column loses is
  // pen travel off its head, which for an upright cap is its vertical
  // advance, so the cap is shaped the way the text around it is set.
  const bool vertical = paragraph.writingMode() == WritingMode::kVerticalRL;
  plan.vertical = vertical;
  plan.glyphs =
      shapeWord(fontContext, capStyle.shaping, typeface, capText,
                static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, vertical);
  if (!plan.glyphs || plan.glyphs->glyphs.empty()) return plan;

  // WHAT IS LEFT OF THE WORD THE INITIAL SPLIT is set at the body's size
  // and stands at the head of the first band, because it is the same word:
  // the reader sees "W" and "hale" and not two words. It is one shaped run
  // and never breaks, which is what the remainder of a word is.
  if (capEnd < opening.textEnd) {
    TextStyle bodyStyle =
        styleIndex < spans.size() ? spans[styleIndex].style : TextStyle{};
    const std::u16string_view rest =
        std::u16string_view(text).substr(capEnd, opening.textEnd - capEnd);
    UChar32 restCodepoint = 0;
    size_t unit = 0;
    U16_NEXT(rest.data(), unit, rest.size(), restCodepoint);
    const char* restLanguage = bodyStyle.shaping.languageTag.empty()
                                   ? nullptr
                                   : bodyStyle.shaping.languageTag.c_str();
    sk_sp<SkTypeface> restFace = fontContext.resolveTypeface(
        bodyStyle.shaping.typeface, restCodepoint, restLanguage);
    if (!restFace) restFace = fontContext.defaultTypeface();
    plan.remainder =
        shapeWord(fontContext, bodyStyle.shaping, restFace, rest,
                  static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, vertical);
    if (plan.remainder)
      plan.tail = plan.remainder->advance + opening.spaceWidth;
  }

  // A sink of its own is how many lines BELOW the first baseline the
  // initial sits, so a negative one would lift the cap off the top of the
  // frame; the first baseline is as high as it goes.
  const int sink = std::max(
      0, asked.sink.value_or(static_cast<int>(std::floor(asked.lines)) - 1));
  plan.blockIndex = block.index;
  plan.pitch = block.pitch;
  plan.ascent = block.ascent;
  plan.bands = std::max(1, sink + 1);
  plan.notch = plan.glyphs->advance + asked.margin;
  plan.fontSize = fontSize;
  plan.sinkOffset = static_cast<float>(sink) * block.pitch;
  plan.styleIndex = styleIndex;
  plan.wordIndex = block.firstWord;
  plan.textEnd = capEnd;
  plan.capSpan = (asked.lines - 1.0f) * block.pitch + reference;
  plan.wrap = asked.wrap;

  // WRAPPING THE GLYPH rather than its box: the notch is how far the
  // initial's own contours reach into each band, so a line tucks under the
  // diagonal of an A and stands clear of the bowl of an O. The outline is
  // measured pen-relative and the bands are read off it against the
  // initial's baseline.
  if (asked.wrap == InitialLetter::Wrap::kGlyph) {
    const SkFont font = makeFont(plan.glyphs->typeface, plan.glyphs->fontSize,
                                 plan.glyphs->scaleX, plan.glyphs->aliased);
    SkPathBuilder builder;
    for (size_t glyph = 0; glyph < plan.glyphs->glyphs.size(); ++glyph) {
      const std::optional<SkPath> contour =
          font.getPath(plan.glyphs->glyphs[glyph]);
      if (!contour) continue;
      builder.addPath(contour->makeOffset(plan.glyphs->positions[glyph].x(),
                                          plan.glyphs->positions[glyph].y()));
    }
    SkPath outline = builder.detach();
    if (!outline.isEmpty()) {
      plan.perBand.assign(static_cast<size_t>(plan.bands), 0.0f);
      // The reach is read off the FLATTENED outline: a curve's control
      // points lie outside the curve, so a bowl measured to its control
      // polygon would push the lines further off than the ink does.
      constexpr float kFlattenTolerance = 0.5f;
      static thread_local std::vector<std::vector<glm::vec2>> contours;
      contours.clear();
      for (geometry::path::Polyline& polyline :
           geometry::path::flatten(outline, kFlattenTolerance))
        if (polyline.points.size() >= 2)
          contours.push_back(std::move(polyline.points));

      // Band b of the block spans [b·pitch, (b+1)·pitch) below the first
      // band's near edge; the initial's baseline sits `sinkOffset` below
      // the first baseline, which is `ascent` below that near edge.
      for (int band = 0; band < plan.bands; ++band) {
        const float top = static_cast<float>(band) * block.pitch -
                          block.ascent - plan.sinkOffset;
        const float bottom = top + block.pitch;
        // How far the outline gets into this band: the rightmost point
        // inside it, and the ends of every segment crossing it, so a long
        // straight stem is not missed between its two ends.
        float reach = 0;
        for (const std::vector<glm::vec2>& contour : contours) {
          for (size_t index = 0; index < contour.size(); ++index) {
            const glm::vec2& point = contour[index];
            const glm::vec2& previous = contour[index == 0 ? 0 : index - 1];
            if (point.y >= top && point.y <= bottom)
              reach = std::max(reach, point.x);
            if ((previous.y < top) != (point.y < top) ||
                (previous.y < bottom) != (point.y < bottom))
              reach = std::max(reach, std::max(previous.x, point.x));
          }
        }
        plan.perBand[static_cast<size_t>(band)] =
            reach > 0 ? reach + asked.margin : 0.0f;
      }
      plan.outline = std::move(outline);
    }
  }
  return plan;
}

void InitialLetterGeometry::cutFromHead(std::vector<LineInterval>& intervals,
                                        float travel) {
  while (travel > 0 && !intervals.empty()) {
    LineInterval& first = intervals.front();
    const float take = std::min(travel, first.length);
    if (first.contour.valid())
      first.contourStart += take * first.advanceScale;
    else
      first.origin +=
          SkVector{first.direction.x() * take, first.direction.y() * take};
    first.length -= take;
    travel -= take;
    if (first.length <= 0) intervals.erase(intervals.begin());
  }
}

bool InitialLetterGeometry::lineIntervals(
    const LineRequest& request, std::vector<LineInterval>& intervals) {
  if (!m_inner.lineIntervals(request, intervals)) return false;
  if (!m_plan.active() || m_inert) return true;
  if (request.blockIndex == m_plan.blockIndex && request.lineInBlock == 0 &&
      !m_seated && !intervals.empty()) {
    // The head of the block's first band, BEFORE anything is taken out of
    // it — where the initial itself stands, and the one fact the placement
    // cannot recover once the notch has been cut.
    m_seat = intervals.front();
    m_seated = true;
    m_seatBandStart = request.bandStart;
    const SkVector direction = m_seat.direction;
    m_inert = !m_seat.contour.valid() &&
              !(direction.x() == 1 && direction.y() == 0) &&
              !(direction.x() == 0 && direction.y() == 1);
    // Nothing upright can stand in a notch cut out of a slanted band, so a
    // seat that runs in no axis direction keeps its whole band rather than
    // opening a hole no cap fills.
    if (m_inert) return true;
  }

  // WHICH BAND OF THE INITIAL THIS IS. Inside the initial's own block the
  // band is counted; past its end it is measured, because a block shorter
  // than the sink hands the rest of the initial's depth to the block after
  // it and the cap has to stay clear of that one too.
  int band = -1;
  if (request.blockIndex == m_plan.blockIndex)
    band = request.lineInBlock;
  else if (m_seated && m_plan.pitch > 0 &&
           request.bandStart >= m_seatBandStart) {
    constexpr float kBandEpsilon = 0.001f;
    band = (int)std::floor(
        (request.bandStart - m_seatBandStart) / m_plan.pitch + kBandEpsilon);
  }
  if (band < 0 || band >= m_plan.bands || intervals.empty()) return true;

  cutFromHead(intervals, m_plan.notchAt(band));
  if (band == 0 && m_plan.tail > 0) {
    // Where the notch actually ended is where the remainder of the split
    // word goes: an exclusion beside the initial can carry the cut into
    // the next interval of the band, and the remainder belongs there.
    if (!intervals.empty()) {
      m_tailSeat = intervals.front();
      m_tailSeated = true;
    }
    cutFromHead(intervals, m_plan.tail);
  }
  return true;
}

void placeInitialLetter(const InitialLetterPlan& plan,
                        const InitialLetterGeometry& geometry,
                        ParagraphLayout& layout) {
  if (!plan.active() || !plan.glyphs || !geometry.seated()) return;
  const LineInterval& seat = geometry.seat();
  SkPoint origin = seat.origin;
  const SkVector direction = seat.direction;
  const bool downTheColumn = direction.x() == 0 && direction.y() == 1;
  if (seat.contour.valid()) {
    // A CONTOUR HAS NO BASELINE TO SINK TO: a loop's bands are one line
    // wound round, so the initial stands upright where the pen enters the
    // contour and the notch it cut is the room it stands in.
    SkVector tangent = {1, 0};
    seat.placeAt(0, 0, 0, &origin, &tangent);
  } else if (downTheColumn) {
    // A COLUMN'S INITIAL HANGS FROM THE COLUMN HEAD, which is where the
    // seat already is, and the glyphs stack down from there. It sinks
    // ACROSS the columns instead, and its ink is centred on the axis it
    // stands on, so half the sink carries it over the columns it cut.
    origin += SkVector{-plan.sinkOffset * 0.5f, 0};
  } else if (direction.x() == 1 && direction.y() == 0) {
    // Lines stack down the page and the initial sinks with them.
    origin += SkVector{0, plan.sinkOffset};
  } else {
    return;
  }

  // WHICH LINE THE INITIAL IS ON. Its block's first line — which is the
  // line of the first run of the word after the one the initial split, and
  // the line after everything already placed when the initial took the
  // block's whole opening.
  int lineIndex = -1;
  size_t insertAt = layout.runs.size();
  int lastLine = -1;
  for (size_t index = 0; index < layout.runs.size(); ++index) {
    const PositionedRun& run = layout.runs[index];
    if (run.wordIndex >= plan.wordIndex) {
      lineIndex = run.lineIndex;
      insertAt = index;
      break;
    }
    lastLine = std::max(lastLine, run.lineIndex);
  }
  if (lineIndex < 0) lineIndex = lastLine + 1;

  PositionedRun cap;
  cap.shaped = plan.glyphs.get();
  cap.blob = wordBlob(*plan.glyphs);
  cap.origin = origin;
  cap.styleIndex = plan.styleIndex;
  cap.wordIndex = plan.wordIndex;
  cap.lineIndex = lineIndex;
  cap.advance = plan.glyphs->advance;
  if (!cap.blob) return;
  layout.shapedByTheLayout.push_back(plan.glyphs);
  // In logical order: the initial opens its own block and never stands
  // before the words of the blocks above it.
  layout.runs.insert(layout.runs.begin() + (long)insertAt, std::move(cap));

  // The remainder of the split word stands where the notch ended, in the
  // room the cut left for it on the initial's own band.
  if (plan.remainder && !plan.remainder->glyphs.empty()) {
    const LineInterval& tail =
        geometry.tailSeated() ? geometry.tailSeat() : seat;
    PositionedRun rest;
    rest.shaped = plan.remainder.get();
    rest.blob = wordBlob(*plan.remainder);
    rest.origin =
        geometry.tailSeated()
            ? tail.origin
            : seat.origin + SkVector{seat.direction.x() * plan.notchAt(0),
                                     seat.direction.y() * plan.notchAt(0)};
    rest.styleIndex = plan.styleIndex;
    rest.wordIndex = plan.wordIndex;
    rest.lineIndex = lineIndex;
    rest.advance = plan.remainder->advance;
    if (rest.blob && !seat.contour.valid()) {
      layout.shapedByTheLayout.push_back(plan.remainder);
      layout.runs.insert(layout.runs.begin() + (long)insertAt + 1,
                         std::move(rest));
    }
  }

  layout.initial.placed = true;
  layout.initial.baseline = origin;
  layout.initial.fontSize = plan.fontSize;
  layout.initial.bands = plan.bands;
  layout.initial.notch = plan.notch;
  layout.initial.textEnd = plan.textEnd;
  if (plan.glyphs->vertical) {
    // A vertical cap hangs from its origin: the em box across the column,
    // its own pen travel down it.
    layout.initial.box =
        SkRect::MakeXYWH(origin.x() - plan.fontSize * 0.5f, origin.y(),
                         plan.fontSize, plan.glyphs->advance);
  } else {
    const SkFont font = makeFont(plan.glyphs->typeface, plan.glyphs->fontSize,
                                 plan.glyphs->scaleX, plan.glyphs->aliased);
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    layout.initial.box = SkRect::MakeXYWH(
        origin.x(), origin.y() + metrics.fAscent, plan.glyphs->advance,
        -metrics.fAscent + metrics.fDescent);
  }
}

}  // namespace detail

}  // namespace sigil::weave
