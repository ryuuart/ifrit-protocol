#pragma once

/** @file
 * One file, photographed or measured: the still `--frame` writes at a
 * stated moment, and the frame-time gate `--bench` reads off the same
 * sketch on a raster surface of its own canvas.
 */

#include <filesystem>
#include <string>

namespace sigil::sketch {
class Host;
}

/** What a run was asked to photograph or to measure. */
struct CaptureOptions {
  std::string outputPath;
  /** Seconds of fixed-step stepping before the capture, as the CALLER
   *  stated it. Negative means unstated, which is not the same as 1.5:
   *  a still then lands at the moment the sketch itself declared, and
   *  only a sketch that declares none falls back to the number. */
  double at = -1.0;
  float scale = 1.0f;  // multiplier over the sketch's canvas size
  int frames = 1;      // >1 captures a numbered sequence
  double fps = 60.0;   // fixed-step rate
  bool bench = false;  // measure, do not write
  int benchFrames = 120;
  double jitterDt = 0.0;
};

/** The frame-time gate, measured on the sketch's REAL canvas.
 *
 *  Every frame runs the true per-frame path — clear, advance, draw, and
 *  a readback that forces the raster to complete rather than leaving
 *  work queued behind the timer. Warm to `--at` first: the first frames
 *  of any sketch pay for program compiles, texture bakes, snapshots and
 *  glyph atlases, and folding those into the sample measures the wrong
 *  thing.
 *
 *  A SLOW SKETCH IS NOT A FAILURE: the verdict is the output, not the
 *  exit status, so benching several in a row does not stop at the first
 *  one over budget. It exits 0 whenever it measured; a sketch that never
 *  built, or a surface that could not be allocated, exits 1. */
int runBench(sigil::sketch::Host& host, const CaptureOptions& options,
             const std::filesystem::path& path);

/** THE STILL, AT THE MOMENT THE SKETCH DECLARED unless the caller named
 *  another, and the numbered sequence when more than one frame was
 *  asked for. Non-zero when the sketch never built or a frame could not
 *  be written. */
int runFrames(sigil::sketch::Host& host, const CaptureOptions& options);
