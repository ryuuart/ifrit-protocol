#pragma once

/** @file
 * THE RENDER-THREAD SIDE of the gallery item: the scenes, the font
 * context they are set through, the Graphite context when the build has
 * one, and the raster framebuffer beside it. Everything here runs on the
 * render thread and reads the item only inside synchronize().
 */

#include <QElapsedTimer>
#include <QQuickRhiItem>
#include <QSize>
#include <QString>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "SceneRegistry.h"

#ifdef TEXTFLOW_GALLERY_GPU
#include <sigilskia/qt/QtInterop.h>
#endif

namespace sigil::weave {
class FontContext;
}

class SkCanvas;

/// The item this renders for.
class GalleryView;

using gallery::Scene;
using gallery::SceneParameters;

// ── Render-thread side ─────────────────────────────────────────────────────

class GalleryViewRenderer : public QQuickRhiItemRenderer {
 public:
  void initialize(QRhiCommandBuffer* commandBuffer) override;
  void synchronize(QQuickRhiItem* item) override;
  void render(QRhiCommandBuffer* commandBuffer) override;

 private:
  struct FontCoordinate {
    uint32_t tag = 0;
    float value = 0;
    bool operator==(const FontCoordinate&) const = default;
  };

  /// Draws the selected scene in logical coordinates onto an Skia canvas.
  void renderScene(SkCanvas* canvas, float devicePixelRatio, QSize logicalSize);

  std::vector<std::unique_ptr<Scene>> m_scenes;
  std::unique_ptr<sigil::weave::FontContext> m_fontContext;
#ifdef TEXTFLOW_GALLERY_GPU
  std::unique_ptr<sigil::skia::GraphiteContext> m_graphiteContext;
  bool m_graphiteInitializationAttempted = false;
#endif
  std::vector<uint32_t> m_rasterPixels;  // CPU fallback framebuffer.

  SceneParameters m_sceneParameters;
  uint64_t m_sceneParameterRevision = std::numeric_limits<uint64_t>::max();
  int m_lastSyncedSceneIndex = -1;
  QString m_sceneText;
  QString m_fontFamily;
  std::vector<FontCoordinate> m_fontCoordinates;
  uint64_t m_fontAxesRevision = std::numeric_limits<uint64_t>::max();
  bool m_typefaceDirty = true;
  int m_sceneIndex = 0;
  bool m_animating = true;
  bool m_useGpuBackend = true;

  // Rebuilt only when synchronize() sees a different family name or a bumped
  // axis revision; a frame that changes neither reuses this typeface, and
  // with it the shape cache keyed on it.
  sk_sp<SkTypeface> m_resolvedTypeface;
  std::vector<SkPoint> m_pendingClicks;
  QSize m_logicalSize;

  QElapsedTimer m_clock;
  double m_pausedSeconds = 0;
  int m_frameNumber = 0;
  int m_lastSceneIndex = -1;

  // Stats accumulated in render(), copied back in the next synchronize().
  double m_layoutMicrosecondsAverage = 0;
  double m_recordMicrosecondsAverage = 0;
  double m_submitMicrosecondsAverage = 0;
  double m_framesPerSecondAverage = 0;
  uint64_t m_reshapedWordCount = 0;
  int m_runCount = 0;
  int m_glyphCount = 0;
  QElapsedTimer m_interFrameTimer;
  int m_statisticsFrameCount = 0;
  bool m_statsDirty = false;
};
