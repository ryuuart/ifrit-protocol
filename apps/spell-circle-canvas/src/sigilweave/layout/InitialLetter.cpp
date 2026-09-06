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

#include "ParagraphLayoutInternal.h"
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
  plan.glyphs =
      shapeWord(fontContext, capStyle.shaping, typeface, capText,
                static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, false);
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
                  static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, false);
    if (plan.remainder)
      plan.tail = plan.remainder->advance + opening.spaceWidth;
  }

  const int sink = asked.sink.value_or(
      std::max(0, static_cast<int>(std::floor(asked.lines)) - 1));
  plan.blockIndex = block.index;
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
      // Band b of the block spans [b·pitch, (b+1)·pitch) below the first
      // band's near edge; the initial's baseline sits `sinkOffset` below
      // the first baseline, which is `ascent` below that near edge.
      for (int band = 0; band < plan.bands; ++band) {
        const float top = static_cast<float>(band) * block.pitch -
                          block.ascent - plan.sinkOffset;
        const float bottom = top + block.pitch;
        float reach = 0;
        // The reach is how far the outline gets into this band: the
        // rightmost of its control points inside the band, and of the ends
        // of every segment crossing it, so a long straight stem is not
        // missed between its two ends.
        SkPath::Iter iter(outline, true);
        SkPoint points[4];
        SkPath::Verb verb;
        SkPoint previous = {0, 0};
        while ((verb = iter.next(points)) != SkPath::kDone_Verb) {
          const int count = verb == SkPath::kMove_Verb    ? 1
                            : verb == SkPath::kLine_Verb  ? 2
                            : verb == SkPath::kQuad_Verb  ? 3
                            : verb == SkPath::kConic_Verb ? 3
                            : verb == SkPath::kCubic_Verb ? 4
                                                          : 0;
          for (int index = 0; index < count; ++index) {
            const SkPoint point = points[index];
            if (point.y() >= top && point.y() <= bottom)
              reach = std::max(reach, point.x());
            if ((previous.y() < top) != (point.y() < top) ||
                (previous.y() < bottom) != (point.y() < bottom))
              reach = std::max(reach, std::max(previous.x(), point.x()));
            previous = point;
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

bool InitialLetterGeometry::lineIntervals(
    const LineRequest& request, std::vector<LineInterval>& intervals) {
  if (!m_inner.lineIntervals(request, intervals)) return false;
  if (!m_plan.active() || request.blockIndex != m_plan.blockIndex) return true;
  const int band = request.lineInBlock;
  if (band < 0 || band >= m_plan.bands || intervals.empty()) return true;
  if (band == 0 && !m_seated) {
    // The head of the block's first band, BEFORE anything is taken out of
    // it — where the initial itself stands, and the one fact the placement
    // cannot recover once the notch has been cut.
    m_seat = intervals.front();
    m_seated = true;
  }
  float remaining = m_plan.notchAt(band) + (band == 0 ? m_plan.tail : 0.0f);
  while (remaining > 0 && !intervals.empty()) {
    LineInterval& first = intervals.front();
    const float take = std::min(remaining, first.length);
    if (first.contour.valid())
      first.contourStart += take * first.advanceScale;
    else
      first.origin +=
          SkVector{first.direction.x() * take, first.direction.y() * take};
    first.length -= take;
    remaining -= take;
    if (first.length <= 0) intervals.erase(intervals.begin());
  }
  return true;
}

void placeInitialLetter(const InitialLetterPlan& plan,
                        const InitialLetterGeometry& geometry,
                        const Paragraph& paragraph, ParagraphLayout& layout) {
  if (!plan.active() || !plan.glyphs || !geometry.seated()) return;
  const LineInterval& seat = geometry.seat();
  SkPoint origin = seat.origin;
  SkVector direction = seat.direction;
  if (seat.contour.valid()) {
    // A CONTOUR HAS NO BASELINE TO SINK TO: a loop's bands are one line
    // wound round, so the initial stands upright where the pen enters the
    // contour and the notch it cut is the room it stands in.
    SkVector tangent = {1, 0};
    seat.placeAt(0, 0, 0, &origin, &tangent);
    direction = tangent;
  } else {
    // Bands stack across the pen's travel: lines down the page, columns
    // right to left. The initial sinks along that stack.
    SkVector stack{0, 1};
    if (direction.x() == 0 && direction.y() == 1)
      stack = {-1, 0};
    else if (direction.x() != 1 || direction.y() != 0)
      return;
    origin +=
        SkVector{stack.x() * plan.sinkOffset, stack.y() * plan.sinkOffset};
  }
  PositionedRun cap;
  cap.shaped = plan.glyphs.get();
  cap.blob = wordBlob(*plan.glyphs);
  cap.origin = origin;
  cap.styleIndex = plan.styleIndex;
  cap.wordIndex = plan.wordIndex;
  cap.lineIndex = 0;
  cap.advance = plan.glyphs->advance;
  if (!cap.blob) return;
  layout.shapedByTheLayout.push_back(plan.glyphs);
  layout.runs.insert(layout.runs.begin(), std::move(cap));

  // The remainder of the split word stands at the head of the first band,
  // in the room the notch left for it.
  if (plan.remainder && !plan.remainder->glyphs.empty()) {
    PositionedRun rest;
    rest.shaped = plan.remainder.get();
    rest.blob = wordBlob(*plan.remainder);
    rest.origin = seat.origin + SkVector{seat.direction.x() * plan.notchAt(0),
                                         seat.direction.y() * plan.notchAt(0)};
    rest.styleIndex = plan.styleIndex;
    rest.wordIndex = plan.wordIndex;
    rest.lineIndex = 0;
    rest.advance = plan.remainder->advance;
    if (rest.blob && !seat.contour.valid()) {
      layout.shapedByTheLayout.push_back(plan.remainder);
      layout.runs.insert(layout.runs.begin() + 1, std::move(rest));
    }
  }

  const SkFont font = makeFont(plan.glyphs->typeface, plan.glyphs->fontSize,
                               plan.glyphs->scaleX, plan.glyphs->aliased);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  layout.initial.placed = true;
  layout.initial.baseline = origin;
  layout.initial.fontSize = plan.fontSize;
  layout.initial.bands = plan.bands;
  layout.initial.notch = plan.notch;
  layout.initial.textEnd = plan.textEnd;
  layout.initial.box = SkRect::MakeXYWH(
      origin.x(), origin.y() + metrics.fAscent, plan.glyphs->advance,
      -metrics.fAscent + metrics.fDescent);
  (void)paragraph;
}

}  // namespace detail

}  // namespace sigil::weave
