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
  /** Point stars in layers from far to near over a coloured haze, the near
   *  layers coarser, brighter and sliding past the far ones. */
  Stars,
  /** A sky deepening toward the top under a drifting layer of cloud,
   *  shaded from above. */
  Clouds,
  /** A round tunnel streaming toward the viewer, dark gaps between its
   *  bright rings and seams twisting along its wall. */
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
  /** The box the field is authored over, as a share of the box the paint
   *  is laid over: the unit square (the default) is all of it — a
   *  passage's text box for an ink, the node's own box for a fill, each
   *  glyph's, word's or line's box for an ink that restarts on each — and
   *  `{{0, 0}, {0.5f, 1}}` is its left half. */
  geometry::path::Rect box{{0, 0}, {1, 1}};
};

/** The text paint @p paint describes, as a material: its field's program
 *  over the box, the moment and the drift. Two calls with equal constant
 *  properties answer equal materials, so a node inked with one prunes. */
[[nodiscard]] material::Material textPaint(const TextPaint& paint = {});

}  // namespace sigil::compose::kit
