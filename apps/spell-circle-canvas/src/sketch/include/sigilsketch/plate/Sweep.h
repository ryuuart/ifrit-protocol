#pragma once

/** @file
 * The headless sweep: every sketch stepped to its declared moment and
 * photographed, with the timing table beside it.
 */

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
 *  the pixel count. It bounds what a HOST chose; a sketch that declares
 *  an oversample of its own is rendered at exactly that, because the
 *  reason to declare one is a grid a fractional scale would destroy.
 *
 *  Out here because it is the width a plate on disk comes out at, which
 *  is a fact about the file rather than about the sweep's arithmetic:
 *  anything that reads a plate and expects the ceiling reads it from
 *  here rather than typing the number again. */
inline constexpr float kPlateWidthCeiling = 2400.0f;

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
  std::string outDir = "sketch_plates";
  /** Draw on the device: a Graphite surface for the sketches that paint
   *  onto a canvas, the device runtime for the ones that light a set. */
  bool gpu = false;
  /** One registry entry, or -1 for the whole registry. Asking for ONE
   *  means you want that sketch's real number, so the per-sketch time
   *  budget that keeps a whole sweep to a few minutes does not apply. */
  int only = -1;
  /** Only the sketches drawn through this runtime (`canvas`, `set`);
   *  empty is all of them. */
  std::string kind;
  /** Force cost-based re-baking off. On a device the backend-aware
   *  default already holds it off; naming it makes the comparison
   *  reproducible on either backend rather than a behaviour it adds. */
  bool noPromotion = false;
  /** Take every still at this scene time, overriding both the derived
   *  frame and any sketch's declared moment. Sweeping at two different
   *  times and diffing tells you which sketches are still in motion at
   *  the moment they are photographed. */
  double captureAt = -1.0;
  /** Skip the benchmark phases and go straight to the capture. */
  bool ledger = false;
  /** Also write one JSON line per sketch with the steady-state sample
   *  numbers — the machine-readable lane a frame-time gate parses.
   *  Stdout is untouched. Refused under `ledger`: a ledger run performs
   *  no benchmark, and a timing file of zeros would be a lie. */
  std::string timingJson;
};

/** Runs the sweep. Returns 0 when every selected sketch rendered. */
int sweep(const SweepOptions& options, weave::FontContext& fonts,
          Assets& assets);

}  // namespace sigil::sketch
