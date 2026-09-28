#pragma once

/** @file
 * @ingroup geometry-path
 *
 * SigilGeometry blend — a study of Illustrator's Object > Blend, built
 * on the polyline resampling currency. The tool that made 90s
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
 * neither gray nor purple mud. A colour is a `glm::vec4` of straight
 * (unpremultiplied) red, green, blue and alpha in [0, 1]: the library
 * names no colour type of its own, and a consumer that paints with one
 * converts at its door.
 */

#include <glm/vec4.hpp>

#include <optional>
#include <span>
#include <vector>

#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Polyline.h"

// A step is painted onto the canvas a host owns; the canvas is the one
// renderer type this header names, and only by declaration.
class SkCanvas;

/** THE STEPS BETWEEN TWO OUTLINES, paint and all. A blend runs between
 *  consecutive keys — two or more shapes with their paint attributes —
 *  and answers the intermediate shapes at a spacing the caller names.
 *  It stands on the polyline resampling currency, so shapes that do not
 *  already correspond are made to correspond before they interpolate.
 *  This is the airbrush ribbon, the smooth-colour halo and the star
 *  becoming a circle in eight steps. */
namespace sigil::geometry::path::blend {

/** One end (or waypoint) of a blend: an outline plus the paint
 *  attributes that interpolate alongside it. */
struct Key {
  Outline path;
  glm::vec4 fill = {1, 1, 1, 1};
  std::optional<glm::vec4> stroke;
  float strokeWidth = 0;
  float opacity = 1;

  /** Value equality: the same outline wearing the same paint. */
  bool operator==(const Key&) const = default;
};

/** What decides how many intermediates fall between one key and the
 *  next. */
enum class Spacing : uint8_t {
  Steps,        ///< exactly `steps` intermediates between key pairs
  Distance,     ///< one step every `distance` px of spine
  SmoothColor,  ///< steps chosen so adjacent colors are indistinguishable
};

/** Whether a step turns as it rides the spine, or only travels along
 *  it. */
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
  Outline spine;
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

  /** Value equality, dial for dial — the same spacing over the same
   *  spine. */
  bool operator==(const Options&) const = default;
};

/** One drawable step of the blend, keys included when asked. `t` runs 0
 *  to 1 over the whole multi-key sequence. */
struct Step {
  Outline path;
  glm::vec4 fill = {1, 1, 1, 1};
  std::optional<glm::vec4> stroke;
  float strokeWidth = 0;
  float opacity = 1;
  float t = 0;

  /** Value equality: the same outline wearing the same paint at the
   *  same place in the sequence. */
  bool operator==(const Step&) const = default;
};

/** Expand the blend: every step's outline and paint, back-to-front in
 *  key order — drawing them in order reproduces Illustrator's stacking
 *  (later keys sit on top). */
std::vector<Step> make(std::span<const Key> keys, const Options& options = {});

/** Two-key convenience. */
std::vector<Step> make(const Key& from, const Key& to,
                       const Options& options = {});

/** Draw the expanded steps onto a host's canvas: fill (and stroke when
 *  present) per step, antialiased, each colour's alpha multiplied by the
 *  step's opacity. */
void draw(SkCanvas& canvas, std::span<const Step> steps);

namespace detail {
/** OKLab round trip used for color interpolation — exposed for tests. */
glm::vec4 lerpOklab(const glm::vec4& a, const glm::vec4& b, float t);
}  // namespace detail

}  // namespace sigil::geometry::path::blend
