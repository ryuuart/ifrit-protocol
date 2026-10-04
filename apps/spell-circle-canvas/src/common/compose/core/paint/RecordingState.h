#pragma once

/** @file
 * What the paint walk knows about the recording it is painting into: the
 * matrix that recording's ops reach the device through, how many
 * recordings stand open and how many of them may replay under another
 * matrix, what the innermost one has come to hold, and the canvas behind
 * it. A scope that opens a recording, a bake layer or a coverage trace
 * copies this value on entry, sets only the fields its own canvas changes,
 * and assigns the copy back on exit.
 */

#include <include/core/SkMatrix.h>

#include <algorithm>
#include <cstdint>
#include <limits>

class SkCanvas;

namespace sigil::compose::detail {

struct RecordingState {
  // The matrix the innermost open recording's ops reach the device through
  // when it is replayed — identity outside any recording. A node inside a
  // recording composes its canvas matrix through this to find the device
  // grid it is drawn on; the inverse is what a device blit concatenates so
  // the replay lands it at the device rect it was baked for.
  SkMatrix replay = SkMatrix::I();
  SkMatrix replayInverse = SkMatrix::I();
  // >0 while painting INTO an SkPicture, or into the coverage trace's
  // offscreen raster. A node painted here is painted only when the
  // recording is taken, so nothing it observes frame over frame is a
  // history.
  int depth = 0;
  // …and of those, how many may be REPLAYED UNDER A DIFFERENT MATRIX than
  // they were made under: a recording made at, or beneath, a node whose
  // transform is declared live replays under the motion, and the coverage
  // trace rasterizes at a scale of its own. A device-space bake is blitted
  // with the matrix reset at an absolute device rect, so it may be taken
  // only while this is zero — at the root, or inside recordings that are
  // all PINNED to the matrix they were made under (Instance::pictureMatrix)
  // and remade when it changes.
  int unpinnedDepth = 0;
  // Accumulated over the recording being taken: how many device-space
  // blits it holds — its own nodes' and those of every held picture
  // replayed inside it — and whether a node inside was refused a device
  // bake for matrix motion alone, so the picture is retaken once the
  // matrix holds still. Stamped onto the instance when the recording ends.
  uint32_t deviceBakes = 0;
  bool deviceDeferred = false;
  // An eligible group needs another observation before its pixels can be
  // held. Recordings carry that visit request to the owning draw.
  bool groupPending = false;
  // Whether the outermost open recording's device matrix is the one it
  // was drawn under last frame. Inside a recording this stands in for the
  // per-frame device-rect history a device bake needs and cannot keep.
  bool matrixStable = true;
  // …and THE WINDOW OF HOST SCALES OVER WHICH WHAT IT HOLDS IS THE SAME
  // PICTURE. Most of a recording is scale-free — the ops carry paths and
  // paints, and the matrix they meet is the replay's. A raster inside one
  // is not: a traced silhouette is a staircase of whole device pixels and
  // a local texture bake is a texel grid the blit stretches over the
  // node's own units, and both were measured off the scale the recording
  // was taken at. Each such raster narrows the window to the scales that
  // would have produced the same grid — a point for a trace, the ladder
  // rung's own span for a local bake — and a recording whose window the
  // host has left is remade, exactly as one holding a device blit is
  // remade under another matrix. A recording holding no raster keeps the
  // whole line and is never remade for the scale alone.
  float scaleLowest = 0.0f;
  float scaleHighest = std::numeric_limits<float>::infinity();
  // The canvas that receives the open recording's or bake layer's
  // commands: the borrowed destination behind an outermost recording, a
  // bake layer's own surface, or a coverage trace's raster. A held image is
  // bound for this canvas, never for the recorder a picture is drawn into.
  SkCanvas* canvas = nullptr;

  /** Narrow the open recording's window to the scales that would answer
   *  the same grid as the raster just taken. */
  void narrowScaleWindow(float lowest, float highest) {
    scaleLowest = std::max(scaleLowest, lowest);
    scaleHighest = std::min(scaleHighest, highest);
  }
  /** Empty what a new holder holds: no device blits, no deferral, no
   *  pending group visit. */
  void clearHeld() {
    deviceBakes = 0;
    deviceDeferred = false;
    groupPending = false;
  }
  /** Reopen the scale window to the whole line. */
  void clearScaleWindow() {
    scaleLowest = 0.0f;
    scaleHighest = std::numeric_limits<float>::infinity();
  }
};

}  // namespace sigil::compose::detail
