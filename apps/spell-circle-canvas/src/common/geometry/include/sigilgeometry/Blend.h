#pragma once

/** @file
 * SigilGeometry blend — a study of Illustrator's Object > Blend, rebuilt
 * on the Geometry.h resampling currency. The tool that made 90s
 * airbrush ribbons, smooth-color type halos, and every "morph a star
 * into a circle in eight steps" poster.
 *
 * The Illustrator model, kept faithfully:
 *  - a blend runs between consecutive KEYS (two or more shapes with
 *    their paint attributes);
 *  - spacing is Specified Steps, Specified Distance (steps derived from
 *    spine length), or Smooth Color (steps derived from how far apart
 *    the key colors are — enough that adjacent steps differ by less
 *    than a display quantum);
 *  - the steps ride a SPINE: by default the straight line between key
 *    anchors, replaceable with any path (blend along a spiral);
 *    Orientation chooses "align to page" (steps keep their upright) or
 *    "align to path" (steps rotate with the spine tangent).
 *
 * Shape correspondence is resampling + cyclic alignment rather than
 * anchor matching: both outlines become N arc-length samples, the
 * target is rotated/reversed to the least-squares-nearest start (the
 * stable version of Illustrator's "drag between two anchor points").
 * Colors interpolate in OKLab so a red-to-blue blend passes through
 * neither gray nor purple mud.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPath.h>

#include <optional>
#include <span>
#include <vector>

#include "sigilgeometry/Polyline.h"

class SkCanvas;

namespace sigil::geometry::blend {

/** One end (or waypoint) of a blend: an outline plus the paint
 *  attributes that interpolate alongside it. */
struct Key {
  SkPath path;
  SkColor4f fill = SkColors::kWhite;
  std::optional<SkColor4f> stroke;
  float strokeWidth = 0;
  float opacity = 1;
};

enum class Spacing : uint8_t {
  Steps,        ///< exactly `steps` intermediates between key pairs
  Distance,     ///< one step every `distance` px of spine
  SmoothColor,  ///< steps chosen so adjacent colors are indistinguishable
};

enum class Orientation : uint8_t {
  AlignToPage,  ///< steps translate along the spine but keep upright
  AlignToPath,  ///< steps rotate with the spine tangent
};

/** Every dial of a blend: how many intermediates and how their count
 *  is decided, the spine they ride and how they orient along it, and
 *  how finely the outlines are resampled while interpolating. */
struct Options {
  Spacing spacing = Spacing::Steps;
  int steps = 8;        ///< Spacing::Steps: intermediates per key pair
  float distance = 24;  ///< Spacing::Distance: px between step anchors
  /** Replacement spine. Empty = straight line between key centroids.
   *  With K keys the spine is split by arc length into K-1 equal spans,
   *  one per key pair (Illustrator splits at the spine's anchors; equal
   *  spans are the resampled equivalent). */
  SkPath spine;
  bool reverseSpine = false;
  Orientation orientation = Orientation::AlignToPage;
  /** Arc-length samples per contour during interpolation. More = closer
   *  to the true intermediate outline, at linear cost. */
  int samples = 128;
  /** Fit each step's outline with smooth Catmull-Rom cubics instead of
   *  a dense polygon. */
  bool smoothOutlines = false;
  /** Include the keys themselves in the returned sequence (Illustrator
   *  always draws them; turn off to get only the intermediates). */
  bool includeKeys = true;
};

/** One drawable step of the blend, keys included when asked. `t` runs 0
 *  to 1 over the whole multi-key sequence. */
struct Step {
  SkPath path;
  SkColor4f fill = SkColors::kWhite;
  std::optional<SkColor4f> stroke;
  float strokeWidth = 0;
  float opacity = 1;
  float t = 0;
};

/** Expand the blend: every step's outline and paint, back-to-front in
 *  key order — drawing them in order reproduces Illustrator's stacking
 *  (later keys sit on top). */
std::vector<Step> make(std::span<const Key> keys, const Options& options = {});

/** Two-key convenience. */
std::vector<Step> make(const Key& from, const Key& to,
                       const Options& options = {});

/** Draw the expanded steps: fill (and stroke when present) per step. */
void draw(SkCanvas& canvas, std::span<const Step> steps);

namespace detail {
/** OKLab round trip used for color interpolation — exposed for tests. */
SkColor4f lerpOklab(const SkColor4f& a, const SkColor4f& b, float t);
}  // namespace detail

}  // namespace sigil::geometry::blend
