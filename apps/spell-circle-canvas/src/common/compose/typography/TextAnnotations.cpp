/** @file
 * Readings set beside the type: which units of the base each one names,
 * and the reading handed to SigilWeave to be placed against them. The
 * band, the placement and the share a broken base carries are all the
 * engine's (layout/Beside.h); this file maps an element's declaration onto
 * them and keeps the results where the kernel can draw them.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Beside.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "ComposeRuntime.h"
#include "TextEngine.h"

namespace sigil::compose {

using namespace detail;

sigil::weave::ReservedBand detail::reservedBandOf(
    Composer::Impl& impl, std::span<const Annotation> annotations) {
  sigil::weave::ReservedBand band;
  for (const Annotation& annotation : annotations) {
    if (!annotation.reserve || annotation.readings.empty()) continue;
    // The band is the READING'S OWN, asked of the engine before anything
    // is broken — which is what makes a reservation a layout input and not
    // a cycle.
    const float depth =
        sigil::weave::bandBeside(impl.fonts, annotation.style, annotation.gap);
    if (annotation.side == Annotation::Side::Before)
      band.before = std::max(band.before, depth);
    else
      band.after = std::max(band.after, depth);
  }
  return band;
}

void detail::resolveTextAnnotations(Composer::Impl& impl, Instance& inst) {
  inst.textAnnotations.clear();
  if (!inst.description || !inst.description->textData || !inst.paragraph) return;
  const std::vector<Annotation>& annotations = inst.description->textData->annotations;
  if (annotations.empty()) return;
  const sigil::weave::WritingMode mode = inst.paragraph->writingMode();

  for (const Annotation& annotation : annotations) {
    if (annotation.readings.empty()) continue;
    // WHICH UNITS: the published per-unit answer, which reports a base
    // that broke across a line or a column on BOTH of them — so the split
    // below is a fact the placement already knows and not a case handled
    // here.
    const std::vector<TextUnit> units =
        unitsOfText(impl, inst, annotation.where, annotation.unit);
    if (units.empty()) continue;

    for (size_t index = 0; index < units.size(); ++index) {
      const TextUnit& unit = units[index];
      const std::u8string& source =
          annotation.readings.size() == 1
              ? annotation.readings.front()
              : (index < annotation.readings.size() ? annotation.readings[index]
                                                    : std::u8string());
      if (source.empty()) continue;
      std::u16string text = detail::toUtf16(source);
      // A LIST OF ONE reads every unit alike, which is how a row of
      // identical emphasis marks is written; a list of many pairs off with
      // the units, and a base whose pieces continue one another shares one
      // reading between them in the proportion of their advances.
      const bool continues = annotation.readings.size() != 1 &&
                             index + 1 < units.size() &&
                             units[index + 1].range.start == unit.range.end &&
                             units[index + 1].lineIndex != unit.lineIndex;
      if (continues) {
        const bool column = mode == sigil::weave::WritingMode::kVerticalRL;
        text = sigil::weave::shareOfReading(
            text, column ? unit.rect.height() : unit.rect.width(),
            column ? units[index + 1].rect.height()
                   : units[index + 1].rect.width());
      }

      auto reading = std::make_shared<sigil::weave::Paragraph>();
      reading->appendText(text, annotation.style);
      Instance::PlacedAnnotation placed;
      placed.layout = sigil::weave::layoutBeside(
          impl.fonts, *reading,
          {.base = unit.rect,
           .writingMode = mode,
           .side = annotation.side == Annotation::Side::Before
                       ? sigil::weave::Beside::Side::Before
                       : sigil::weave::Beside::Side::After,
           .gap = annotation.gap});
      if (placed.layout.runs.empty()) continue;
      placed.paragraph = std::move(reading);
      inst.textAnnotations.push_back(std::move(placed));
    }
  }
}

}  // namespace sigil::compose
