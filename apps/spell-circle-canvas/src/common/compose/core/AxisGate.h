#pragma once

/** @file
 * Internal to the kernel — the variable-axis substitution gate: one verdict
 * per (face, axis), probed once, read silently by the span fold and with a
 * single warning by the draw-time verbs.
 */

#include "Instance.h"

namespace sigil::compose::detail {

// ---------------------------------------------------------------------------
// The variable-axis substitution gate — one verdict per (face, axis),
// wherever a driven axis is judged.

/** What driving one axis on one face is allowed to do, and across what
 *  range. `min`/`max` are the axis's own design range, which is what the
 *  driven value is stepped across — a bounded step count over the range
 *  keeps the varied-clone population bounded whatever numbers an effect
 *  feeds it.
 */
struct AxisGate {
  bool allowed = false;
  float min = 0, max = 0;
  /// Whether a refusal has been written for this (face, axis) yet. A gate
  /// may be probed silently before any draw-time verb reaches it, so the
  /// one warning is tied to the first draw-time refusal and not to the
  /// first probe.
  bool warned = false;
};

/** The verdict for (face, axis), probed once and remembered, WITHOUT a
 *  warning: probing samples every glyph advance at both extremes of the
 *  axis, so it is a per-face cost and never a per-frame one. For a caller
 *  with another way to honour the axis — a span restyle can re-shape — a
 *  refusal is a routing decision and nothing to warn about. */
inline AxisGate& axisGateProbe(sigil::weave::FontContext& fonts,
                               const sk_sp<SkTypeface>& face,
                               const char (&tag)[5]) {
  static thread_local std::map<std::pair<uint32_t, uint32_t>, AxisGate> table;
  const uint32_t axisTag = SkSetFourByteTag(tag[0], tag[1], tag[2], tag[3]);
  auto [entry, fresh] =
      table.try_emplace({face ? face->uniqueID() : 0u, axisTag}, AxisGate{});
  AxisGate& gate = entry->second;
  if (!fresh) return gate;
  gate.allowed = face && fonts.axisIsAdvanceInvariant(face, tag);
  if (!gate.allowed) return gate;
  const int count = face->getVariationDesignParameters({});
  if (count > 0) {
    std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
    face->getVariationDesignParameters({axes.data(), axes.size()});
    for (const auto& axis : axes)
      if (axis.tag == axisTag) {
        gate.min = axis.min;
        gate.max = axis.max;
      }
  }
  return gate;
}

/** The gate for (face, axis) as a DRAW-TIME verb reads it: the probe above,
 *  and one warning on the first refusal. */
inline const AxisGate& axisGate(sigil::weave::FontContext& fonts,
                                const sk_sp<SkTypeface>& face,
                                const char (&tag)[5]) {
  AxisGate& gate = axisGateProbe(fonts, face, tag);
  if (!gate.allowed && !gate.warned) {
    gate.warned = true;
    // ONE REFUSAL FOR EVERY VERB THAT REACHES A DRAW-TIME AXIS —
    // variationDrive and a `TextEffect::variableAxis` track — because it is
    // one gate and they all fail it for the same reason. Naming a verb here
    // would send an author reading about the one they did not write.
    SkDebugf(
        "sigilcompose: axis \"%s\" is absent or moves advances on this font "
        "— refused (the glyphs keep the pen positions shaping gave them, so "
        "the text draws at its shaped coordinates; GRAD is the "
        "advance-invariant weight, or re-shape through a style)\n",
        tag);
  }
  return gate;
}

}  // namespace sigil::compose::detail
