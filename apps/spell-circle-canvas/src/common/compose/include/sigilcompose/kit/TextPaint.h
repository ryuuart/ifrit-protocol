#pragma once

/** @file
 * @ingroup compose-kit
 *
 * SigilCompose KIT — the text paints: procedural fields a run of type is
 * inked with, each one program over the same box, moment and drift.
 */

#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/values/Animatable.h>

#include <cstdint>

namespace sigil::compose::kit {

/** The field a text paint fills its box with. */
enum class TextField : uint8_t {
  /** Blue water deepening down the box, under fine bright caustics. */
  Water,
  /** Four colour regions meeting across the box, a violet bloom inside. */
  MeshGradient,
  /** Points of light on a transparent ground, one at most in each square
   *  cell, a cell 22/70 of the box's height; it is layered over a base. */
  Sparkle,
  /** Dense light marched through a volume of folded space. */
  StarNest,
  /** A sky ramp under drifting clouds of layered noise. */
  Clouds,
  /** A twisting tunnel whose dark rings stand between bright walls. */
  Tunnel,
};

/** HOW A TEXT PAINT IS MADE: which field, at what moment, over what box.
 *
 *      text(u8"SIGIL").ink(kit::textPaint({.field = kit::TextField::Water}))
 *
 *  Every field is authored over its box: what it varies across, it varies
 *  across the box, and what it spaces — a wave, a cell of sparkle — it
 *  spaces in shares of the box. */
struct TextPaint {
  TextField field = TextField::Water;
  /** The moment the field stands at, in seconds. A time that is not a
   *  constant is bound: the material's `time` follows it at every
   *  resolve, and the field's drift with it. */
  motion::Animatable<float> time = 0.0f;
  /** The box the field is authored over, in the coordinates it is painted
   *  in. The unit square (the default) is what an ink's `PaintBox`
   *  stretches over its passage, its glyphs, words or lines; a fill reads
   *  its node's own pixels, so a fill names the node's box here. */
  geometry::path::Rect box{{0, 0}, {1, 1}};
};

/** The text paint @p paint describes, as a material: its field's program
 *  over the box, the moment and the drift. Two calls with equal constant
 *  properties answer equal materials, so a node inked with one prunes. */
[[nodiscard]] material::Material textPaint(const TextPaint& paint = {});

}  // namespace sigil::compose::kit
