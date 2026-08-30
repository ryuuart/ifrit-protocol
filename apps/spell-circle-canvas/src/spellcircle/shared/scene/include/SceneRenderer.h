#pragma once

// Draws a resolved scene — circles, edges, boxes, labels — with Skia and the
// SigilWeave text library. No Qt, and no surface, window, or GPU context of its
// own: the caller hands over an SkCanvas wrapping whatever it owns, plus style
// values already converted to native canvas pixels.
//
// Both front ends (the Qt app and the native macOS app) draw through this one
// implementation, which is why nothing toolkit-specific may enter it: a change
// here must make sense on either side.

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/cache/SingleLineParagraphCache.h>
#include <sigilweave/fonts/FontContext.h>

#include <memory>

#include "SceneGeometry.h"
#include "SceneLabels.h"

class SkCanvas;

namespace spellcircle {

/** Style inputs for SceneRenderer::draw(). Every length here is already in
 *  native canvas pixels: the caller multiplies its configured values by its
 *  global scale factor before filling this in, and the renderer scales nothing
 *  further. Note that these lengths do NOT pass through the author-space
 *  conversion the scene geometry does — a box is the size asked for on the
 *  canvas, whatever coordinate space the scene was authored in. */
struct SceneStyle {
  SkColor accentColor = SkColorSetRGB(0xff, 0x00, 0x00);
  float strokeWidth = 4.0f;
  float labelOffset = 0.0f;
  // Distance from the anchor point to the CENTRE of the point's value label,
  // along the outward ray resolved with the scene.
  float pointDistance = 40.0f;
  // A minimum, not a fixed width: a box widens to fit its text plus padding on
  // both sides, so long labels are never clipped.
  float boxWidth = 360.0f;
  float boxHeight = 140.0f;
  float boxPadding = 16.0f;
  // The gap between the anchor point and the box's nearest face, along the
  // outward ray resolved with the scene. The box's centre is pushed out by
  // this distance plus its half-extent along the ray, so a box that grows to
  // fit a longer label widens away from its point, never over it.
  float boxDistance = 40.0f;
  float fontSize = 36.0f;
  // Null is allowed and falls back to the font context's default platform
  // typeface, so labels never silently draw nothing.
  sk_sp<SkTypeface> typeface;
};

/**
 * Draws resolved scenes onto canvases. The instance exists to own the caches
 * that make redrawing the same labels every frame cheap: shaped words inside
 * the font context, laid-out single-line paragraphs, and measured ring geometry
 * for the curved circle labels. A throwaway renderer per frame would re-shape
 * and re-measure everything.
 *
 * NOT THREAD-SAFE, and the rule is stronger than one-caller-at-a-time: the font
 * context is created lazily on the first draw() and inherits that caller's
 * thread, so every later draw() must run on the same one. Pin a renderer to a
 * single drawing thread for its whole life — the render thread in the Qt app,
 * the main thread in the native macOS app. Use two renderers rather than
 * sharing one across threads; nothing here detects the violation.
 */
class SceneRenderer {
 public:
  SceneRenderer();
  ~SceneRenderer();

  SceneRenderer(const SceneRenderer&) = delete;
  SceneRenderer& operator=(const SceneRenderer&) = delete;

  /** Draws @p scene onto @p canvas. The canvas is NOT cleared first: the caller
   *  owns the backdrop, which is what lets the same drawing serve a transparent
   *  texture for publishing and an opaque window. A null canvas is ignored. */
  void draw(SkCanvas* canvas, const ResolvedScene& scene,
            const SceneStyle& style);

  /** The renderer's font context, created on first use and bound to the calling
   *  thread — the same threading rule as draw(), and calling this first is what
   *  decides which thread that is. Exposed so a caller can resolve a configured
   *  font family into a typeface through the very font manager the labels will
   *  be shaped with, instead of a second one that might resolve it differently.
   */
  sigil::weave::FontContext& fontContext();

 private:
  std::unique_ptr<sigil::weave::FontContext> m_textContext;
  sigil::weave::SingleLineParagraphCache m_labelParagraphs;
  RingLabelGeometryCache m_ringLabelGeometry;
};

}  // namespace spellcircle
