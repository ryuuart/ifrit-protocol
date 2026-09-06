/** @file
 * Readings set beside the type: which units of the base each one names,
 * and the reading handed to SigilWeave to be placed against them. The
 * band, the placement and the share a broken base carries are all
 * SigilWeave's; this file maps an element's declaration onto them and keeps
 * the results where the kernel can draw them.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Beside.h>
#include <sigilweave/unicode/Unicode.h>

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
  if (!inst.description || !inst.description->textData || !inst.paragraph)
    return;
  const std::vector<Annotation>& annotations =
      inst.description->textData->annotations;
  if (annotations.empty()) return;
  const sigil::weave::WritingMode mode = inst.paragraph->writingMode();

  for (const Annotation& annotation : annotations) {
    if (annotation.readings.empty()) continue;
    // WHICH UNITS: the published per-unit answer, which reports a base
    // that broke across a line or a column on BOTH of them, with the source
    // unit beside each — so the split below is a fact the placement already
    // knows and not a case handled here.
    static thread_local std::vector<uint32_t> sources;
    const std::vector<TextUnit> units =
        unitsOfText(impl, inst, annotation.where, annotation.unit, &sources);
    if (units.empty() || sources.size() != units.size()) continue;
    const bool column = mode == sigil::weave::WritingMode::kVerticalRL;

    auto place = [&](const TextUnit& unit, const std::u16string& text) {
      if (text.empty()) return;
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
      if (placed.layout.runs.empty()) return;
      placed.paragraph = std::move(reading);
      inst.textAnnotations.push_back(std::move(placed));
    };

    // ONE READING PER BASE, NOT PER UNIT. A base that broke is reported on
    // every line it reached, and those pieces are ONE base with ONE
    // reading: the cursor into the readings advances once for the whole
    // run of them, so everything after a break stays paired with the base
    // it names.
    size_t reading = 0;
    for (size_t index = 0; index < units.size(); ++reading) {
      // The pieces of one base are the entries the placement reported for
      // ONE source unit; nothing is decided here. They cannot be told apart
      // by their text ranges, because the space a line breaks at is placed
      // on neither side of the break.
      size_t last = index;
      while (last + 1 < units.size() && sources[last + 1] == sources[last])
        ++last;
      // A LIST OF ONE reads every unit alike, which is how a row of
      // identical emphasis marks is written — a broken base included, since
      // the one reading is what each of its pieces is asked to carry. A
      // list of many pairs off with the bases.
      const bool alike = annotation.readings.size() == 1;
      const std::u8string* source = alike
                                        ? &annotation.readings.front()
                                        : (reading < annotation.readings.size()
                                               ? &annotation.readings[reading]
                                               : nullptr);
      if (!source || source->empty()) {
        index = last + 1;
        continue;
      }
      std::u16string text = weave::unicode::toUtf16(*source);
      if (alike || last == index) {
        for (size_t piece = index; piece <= last; ++piece)
          place(units[piece], text);
        index = last + 1;
        continue;
      }
      // THE SHARE IS THE ADVANCE'S, cut piece by piece: what a piece
      // carries stands to what the pieces after it carry as its advance
      // does to theirs, which is the only proportion a reading has to go
      // by — its own characters need not correspond to the base's one for
      // one.
      const auto advance = [&](size_t piece) {
        return column ? units[piece].rect.height() : units[piece].rect.width();
      };
      for (size_t piece = index; piece < last; ++piece) {
        float rest = 0;
        for (size_t after = piece + 1; after <= last; ++after)
          rest += advance(after);
        std::u16string head =
            sigil::weave::shareOfReading(text, advance(piece), rest);
        text.erase(0, head.size());
        place(units[piece], head);
      }
      place(units[last], text);
      index = last + 1;
    }
  }
}

}  // namespace sigil::compose
