#pragma once

/** @file
 * @ingroup sketch-plate
 *
 * The headless sweep: every sketch stepped to its declared moment and
 * photographed, with the timing table printed beside it.
 */

#include <functional>
#include <string>
#include <string_view>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

class Assets;

/** How a plate file is named: this, the sketch's filed name, `.png`. The
 *  name is what a baseline manifest holds, so it is spelled once. */
inline constexpr std::string_view kPlatePrefix = "plate_";

/** HOW WIDE A PLATE MAY BE before the oversample gives way rather than
 *  the pixel count. It bounds the default host density; an explicit
 *  density or a sketch's declared oversample is rendered exactly as
 *  requested, preserving grids a fractional scale would destroy.
 *
 *  Out here because it is the width a plate on disk comes out at, which
 *  is a fact about the file rather than about the sweep's arithmetic:
 *  anything that reads a plate and expects the ceiling reads it from
 *  here rather than typing the number again. */
inline constexpr float kPlateWidthCeiling = 2400.0f;

class Session;

/** THE DENSITY A PLATE OF @p session IS PHOTOGRAPHED AT, in device pixels
 *  per canvas unit: the oversample the sketch declared, whole, where it
 *  declared one; otherwise its runtime's own oversample, held under the
 *  width ceiling and never below one. Every raster the session bakes on
 *  the way to the plate is baked at it from its first frame. */
[[nodiscard]] float plateDensity(const Session& session);

/** THE PIXELS A PLATE IS: a canvas of @p width by @p height canvas units
 *  photographed at @p density, each side times the density with any
 *  fraction of a pixel dropped. The sweep sizes its plate by it and
 *  `Host::photograph` sizes the still a written `--frame` and a protocol
 *  session take by it, so the two are one picture at every density,
 *  including the fractional one the width ceiling leaves a wide canvas.
 *  Zero by zero where a side is negative, not a number, or more pixels
 *  than an int counts. */
struct PlateExtent {
  int width = 0;
  int height = 0;
};
[[nodiscard]] PlateExtent plateExtent(float width, float height, float density);

/** WHAT ONE HEADLESS RUN DOES.
 *
 *  The sweep answers two questions that want opposite conditions, which
 *  is why `ledger` exists rather than one mode doing both. A byte-
 *  identity sweep wants the image and nothing else: it skips every
 *  benchmark phase and steps straight to the deterministic capture
 *  frame, which is most of a sweep's wall clock. A timing sweep wants
 *  the benchmark phases and does not care about the plate. The plate a
 *  ledger run writes is bit-identical to the plate a timing run writes,
 *  because the capture is a function of the declared moment alone. */
struct SweepOptions {
  std::string outputDirectory = "sketch_plates";
  /** Draw on the device: a Graphite surface for the sketches that paint
   *  onto a canvas, the device runtime for the ones that light a 3D scene. */
  bool gpu = false;
  /** Device pixels per canvas unit, overriding the declared oversample
   *  and width ceiling. Zero uses plateDensity(); positive values apply
   *  from the first frame, including the rasters formed before capture. */
  float density = 0.0f;
  /** One registry entry, or -1 for the whole registry. Asking for ONE
   *  means you want that sketch's real number, so the per-sketch time
   *  budget that keeps a whole sweep to a few minutes does not apply. */
  int only = -1;
  /** Only the sketches drawn through this runtime (`scene2d`, `scene3d`);
   *  empty is all of them. */
  std::string kind;
  /** Take every still at this scene time, in seconds, overriding both
   *  the derived frame and any sketch's declared moment; negative leaves
   *  each sketch its own. Zero is a time like any other: the still is the
   *  scene's first frame. Sweeping at two different times and diffing
   *  tells you which sketches are still in motion at the moment they are
   *  photographed. */
  double at = -1.0;
  /** Skip the benchmark phases and go straight to the capture. */
  bool ledger = false;
  /** Run before each sketch and once after the last, on the thread the
   *  sweep draws on and between two sketches' frames: where a host that
   *  answers a protocol while it sweeps dispatches its hub. Empty runs
   *  nothing, and nothing a plate holds depends on it. */
  std::function<void()> betweenSketches;
};

/** Runs the sweep. Returns 0 when every selected sketch rendered. */
int sweep(const SweepOptions& options, weave::FontContext& fonts,
          Assets& assets);

}  // namespace sigil::sketch
