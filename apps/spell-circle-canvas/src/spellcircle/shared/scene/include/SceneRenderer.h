#pragma once

// Qt-free Skia/TextFlow drawing of a ResolvedScene. This is the single scene
// drawing implementation shared by the Qt app's Skia Graphite backend and
// the native macOS app — both hand it an SkCanvas (wrapping whatever GPU or
// raster surface they own) plus pre-scaled style values.

#include "SceneGeometry.h"
#include "SceneLabels.h"

#include <textflow/FontContext.h>
#include <textflow/SingleLineParagraphCache.h>

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <memory>

class SkCanvas;

namespace spellcircle {

/** Style inputs for SceneRenderer::draw(). All lengths are in native canvas
 *  pixels — callers apply their own scale factor before filling this in
 *  (matching the `value * scale` convention of the Qt GraphicsConfig). */
struct SceneStyle {
  SkColor accentColor = SkColorSetRGB(0xff, 0x00, 0x00);
  float strokeWidth = 4.0f;
  float labelOffset = 0.0f;
  float pointDistance = 40.0f;
  float boxWidth = 360.0f;
  float boxHeight = 140.0f;
  float boxPadding = 16.0f;
  float boxDistance = 40.0f;
  float fontSize = 36.0f;
  // Null falls back to the FontContext's default (platform) typeface.
  sk_sp<SkTypeface> typeface;
};

/**
 * Draws ResolvedScenes onto SkCanvases, owning the TextFlow caches that make
 * repeated labels cheap (FontContext word-shape cache, single-line paragraph
 * cache, measured ring-label geometry).
 *
 * Not thread-safe: the FontContext is created lazily on the first draw()
 * and every later draw() must come from that same thread (the Qt render
 * thread in the Qt app, the main thread in the native app).
 */
class SceneRenderer {
public:
  SceneRenderer();
  ~SceneRenderer();

  SceneRenderer(const SceneRenderer &) = delete;
  SceneRenderer &operator=(const SceneRenderer &) = delete;

  /** Draws @p scene onto @p canvas. The canvas is not cleared first — the
   *  caller decides the backdrop (transparent for publishing, opaque for a
   *  window). */
  void draw(SkCanvas *canvas, const ResolvedScene &scene,
            const SceneStyle &style);

  /** The renderer's font context (created on first use, on the calling
   *  thread — same threading rule as draw()). Exposed so callers can resolve
   *  configured font families against the same SkFontMgr the labels are
   *  shaped with. */
  textflow::FontContext &fontContext();

private:
  std::unique_ptr<textflow::FontContext> m_textContext;
  textflow::SingleLineParagraphCache m_labelParagraphs;
  RingLabelGeometryCache m_ringLabelGeometry;
};

} // namespace spellcircle
