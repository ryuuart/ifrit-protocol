/** @file
 * Readings set beside the type: the band they reserve in the base's strut,
 * asked before anything is broken, and their placement on the units the
 * base's layout put down.
 */

#include <include/core/SkCanvas.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "ComposeRuntime.h"
#include "TextEngine.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** The line height a reading's own type asks for. A reading decides its
 *  own size — the library carries no fraction of a base's — so the band it
 *  needs is simply that type's strut. */
float readingHeight(sigil::weave::FontContext& fonts,
                    const sigil::weave::TextStyle& style) {
  sigil::weave::Paragraph probe;
  probe.appendText(u8"​", style);  // one zero-width unit: metrics only
  return probe.strut(fonts).height;
}

/** The half of `reading` that belongs to a piece of a base carrying
 *  `fraction` of that base's advance. A base unit that broke across a line
 *  reports its pieces in draw order, so the reading is cut in the same
 *  order and by the same proportion — the cut lands on a UTF-16 boundary,
 *  never inside a surrogate pair. */
std::u16string sliceOfReading(const std::u16string& reading, float from,
                              float to) {
  const auto length = static_cast<float>(reading.size());
  auto begin = static_cast<size_t>(std::lround(from * length));
  auto end = static_cast<size_t>(std::lround(to * length));
  begin = std::min(begin, reading.size());
  end = std::clamp(end, begin, reading.size());
  // A cut between the halves of a surrogate pair would make two ill-formed
  // strings out of one well-formed one; step off it.
  const auto onLowSurrogate = [&](size_t at) {
    return at < reading.size() && (reading[at] & 0xFC00u) == 0xDC00u;
  };
  if (onLowSurrogate(begin)) --begin;
  if (onLowSurrogate(end)) --end;
  return reading.substr(begin, end - begin);
}

}  // namespace

sigil::weave::ReservedBand detail::reservedBandOf(
    Composer::Impl& impl, std::span<const Annotation> annotations) {
  sigil::weave::ReservedBand band;
  for (const Annotation& annotation : annotations) {
    if (!annotation.reserve || annotation.readings.empty()) continue;
    const float depth =
        readingHeight(impl.fonts, annotation.style) + annotation.gap;
    if (annotation.side == Annotation::Side::Before)
      band.before = std::max(band.before, depth);
    else
      band.after = std::max(band.after, depth);
  }
  return band;
}

void detail::resolveTextAnnotations(Composer::Impl& impl, Instance& inst) {
  inst.textAnnotations.clear();
  if (!inst.desc || !inst.desc->textData || !inst.paragraph) return;
  const std::vector<Annotation>& annotations =
      inst.desc->textData->annotations;
  if (annotations.empty()) return;
  const bool vertical =
      inst.paragraph->writingMode() == sigil::weave::WritingMode::kVerticalRL;

  for (const Annotation& annotation : annotations) {
    if (annotation.readings.empty()) continue;
    const std::vector<TextUnit> units =
        unitsOfText(impl, inst, annotation.where, annotation.unit);
    if (units.empty()) continue;

    // A base unit that broke across a line or a column reports its pieces
    // in draw order under one source unit. The pieces arrive already split
    // by `unitsOfText`, so what is left here is to hand each piece its own
    // share of the reading — which is the base's advance either side of the
    // break, and nothing else.
    const float readingDepth = readingHeight(impl.fonts, annotation.style);
    for (size_t index = 0; index < units.size(); ++index) {
      const TextUnit& unit = units[index];
      const std::u8string& source =
          annotation.readings.size() == 1
              ? annotation.readings.front()
              : (index < annotation.readings.size() ? annotation.readings[index]
                                                    : std::u8string());
      if (source.empty()) continue;
      // Pieces of one source unit share a reading: the run of entries with
      // the same source is found by their line index changing, and each
      // takes the fraction of the run's advance it carries.
      std::u16string text = detail::toUtf16(source);
      if (annotation.readings.size() == 1 && units.size() > 1) {
        // One reading for every unit: each unit gets the whole of it.
      } else if (index + 1 < units.size() &&
                 units[index + 1].range.start == unit.range.end &&
                 units[index + 1].lineIndex != unit.lineIndex) {
        // The next entry continues this base across a break: this piece
        // takes its share and the next takes the rest.
        const float here = vertical ? unit.rect.height() : unit.rect.width();
        const float next = vertical ? units[index + 1].rect.height()
                                    : units[index + 1].rect.width();
        const float total = here + next;
        if (total > 0) text = sliceOfReading(text, 0.0f, here / total);
      }

      auto paragraph = std::make_shared<sigil::weave::Paragraph>();
      paragraph->appendText(text, annotation.style);
      if (vertical)
        paragraph->setWritingMode(sigil::weave::WritingMode::kVerticalRL);
      const float extent = paragraph->naturalWidth(impl.fonts);
      if (extent <= 0) continue;

      // WHERE THE READING STANDS: centred on the base's own extent along
      // the reading direction, and one standoff clear of the base's band
      // across it. `Before` is above a line and to the RIGHT of a column —
      // the sides each writing mode reads its furniture on.
      sigil::weave::LineInterval interval;
      if (vertical) {
        const float centre = (unit.rect.top() + unit.rect.bottom()) * 0.5f;
        const float across =
            annotation.side == Annotation::Side::Before
                ? unit.rect.right() + annotation.gap + readingDepth * 0.5f
                : unit.rect.left() - annotation.gap - readingDepth * 0.5f;
        interval.origin = {across, centre - extent * 0.5f};
        interval.direction = {0, 1};
      } else {
        const float centre = (unit.rect.left() + unit.rect.right()) * 0.5f;
        const float across =
            annotation.side == Annotation::Side::Before
                ? unit.rect.top() - annotation.gap
                : unit.rect.bottom() + annotation.gap + readingDepth;
        interval.origin = {centre - extent * 0.5f, across};
        interval.direction = {1, 0};
      }
      interval.length = extent + 1.0f;

      sigil::weave::LineSetFlow flow({{interval}});
      Instance::PlacedAnnotation placed;
      placed.layout =
          sigil::weave::layoutParagraph(impl.fonts, *paragraph, flow);
      placed.paragraph = std::move(paragraph);
      inst.textAnnotations.push_back(std::move(placed));
    }
  }
}

}  // namespace sigil::compose
