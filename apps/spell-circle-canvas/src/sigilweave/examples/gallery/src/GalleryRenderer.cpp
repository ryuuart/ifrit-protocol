/** @file
 * The render-thread side of the gallery item: what a synchronize() takes
 * off the item, how a scene is drawn onto a Skia canvas, and the two
 * backends the frame is presented through.
 */

#include "GalleryRenderer.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkFontArguments.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <rhi/qrhi.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/qt/SigilWeaveQt.h>

#include <QFont>
#include <QQuickWindow>
#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

using namespace gallery;

#include "GalleryFallback.h"
#include "GalleryView.h"

void GalleryViewRenderer::initialize(QRhiCommandBuffer* /*commandBuffer*/) {
#ifdef TEXTFLOW_GALLERY_GPU
  if (!m_graphiteInitializationAttempted) {
    m_graphiteInitializationAttempted = true;
    // Graphite is built on Qt's own device and queue, never a private pair:
    // sharing the queue is what makes this item's submissions order ahead of
    // Qt's scene-graph pass without any explicit synchronisation.
    m_graphiteContext = sigil::skia::createGraphiteContext(rhi());
  }
#endif
}

void GalleryViewRenderer::synchronize(QQuickRhiItem* item) {
  auto* view = static_cast<GalleryView*>(item);
  if (m_scenes.empty()) {
    m_scenes.reserve(sceneRegistry().size());
    for (const SceneDescriptor& descriptor : sceneRegistry())
      m_scenes.push_back(descriptor.make());
  }

  m_sceneIndex = view->m_sceneIndex;
  if (m_animating != view->m_animating) {
    if (m_animating && m_clock.isValid())
      m_pausedSeconds += m_clock.elapsed() / 1000.0;
    m_clock.restart();
    m_animating = view->m_animating;
  }
  m_useGpuBackend = view->m_gpu;
  m_sceneText = view->m_sceneText;
  if (m_fontFamily != view->m_fontFamily) {
    m_fontFamily = view->m_fontFamily;
    m_typefaceDirty = true;
  }
  if (m_fontAxesRevision != view->m_fontAxesRevision) {
    m_fontAxesRevision = view->m_fontAxesRevision;
    m_fontCoordinates.clear();
    m_fontCoordinates.reserve(view->m_fontAxes.size());
    for (const GalleryView::FontAxis& axis : view->m_fontAxes)
      m_fontCoordinates.push_back({axis.tag, axis.value});
    m_typefaceDirty = true;
  }
  m_sceneParameters.text = m_sceneText;
  m_sceneParameters.fontSize = static_cast<float>(view->m_fontSize);
  m_sceneParameters.alignment = static_cast<sigil::weave::TextAlignment>(
      std::clamp(view->m_alignmentIndex, 0, 3));
  m_sceneParameters.lineBreakStrategy =
      static_cast<sigil::weave::LineBreakStrategy>(
          std::clamp(view->m_lineBreakStrategyIndex, 0, 1));
  // Scene-declared parameter values: copied only when they actually changed
  // (revision bumped by setSceneParameter / scene switch).
  if (m_sceneParameterRevision != view->m_sceneParameterRevision ||
      m_sceneIndex != m_lastSyncedSceneIndex) {
    m_sceneParameterRevision = view->m_sceneParameterRevision;
    m_lastSyncedSceneIndex = m_sceneIndex;
    m_sceneParameters.values = view->parameterValuesForScene(m_sceneIndex);
  }
  m_pendingClicks.insert(m_pendingClicks.end(), view->m_pendingClicks.begin(),
                         view->m_pendingClicks.end());
  view->m_pendingClicks.clear();
  m_logicalSize =
      QSize(static_cast<int>(view->width()), static_cast<int>(view->height()));

  // Ship last frame's stats back (GUI thread is blocked right now).
  if (m_statsDirty) {
    m_statsDirty = false;
    const char* mode =
#ifdef TEXTFLOW_GALLERY_GPU
        (m_graphiteContext && m_useGpuBackend) ? "Graphite GPU" : "CPU raster";
#else
        "CPU raster";
#endif
    view->m_stats =
        QStringLiteral(
            "%1 · %2 fps · layout %3 µs · record %4 ms · submit %5 ms\n"
            "%6 runs · %7 reshaped/frame%8")
            .arg(QLatin1String(mode))
            .arg(m_framesPerSecondAverage, 0, 'f', 0)
            .arg(m_layoutMicrosecondsAverage, 0, 'f', 0)
            .arg(m_recordMicrosecondsAverage / 1000.0, 0, 'f', 2)
            .arg(m_submitMicrosecondsAverage / 1000.0, 0, 'f', 2)
            .arg(m_runCount)
            .arg(m_reshapedWordCount)
            .arg(m_glyphCount > 0
                     ? QStringLiteral(" · %1 letters").arg(m_glyphCount)
                     : QString());
    QMetaObject::invokeMethod(view, &GalleryView::statsChanged,
                              Qt::QueuedConnection);
  }
}

void GalleryViewRenderer::renderScene(SkCanvas* canvas, float devicePixelRatio,
                                      QSize logicalSize) {
  using Clock = std::chrono::steady_clock;
  if (!m_fontContext) {
    sk_sp<SkFontMgr> fontManager = sigil::weave::ports::systemFontManager();
    m_fontContext = std::make_unique<sigil::weave::FontContext>(
        fontManager, nullptr, makeGalleryFallbackResolver(*fontManager));
  }

  if (m_sceneIndex != m_lastSceneIndex) {
    m_lastSceneIndex = m_sceneIndex;
    m_clock.restart();
    m_pausedSeconds = 0;
    m_frameNumber = 0;
  }
  if (!m_clock.isValid()) m_clock.start();

  Scene* scene =
      m_scenes[static_cast<size_t>(std::clamp<int>(
                   m_sceneIndex, 0, static_cast<int>(m_scenes.size()) - 1))]
          .get();

  // Font family resolves on this thread (owns the FontContext). Every axis
  // discovered from the selected family is cloned onto the typeface; shaping
  // follows because recordForTypeface mirrors the clone's complete design
  // position into HarfBuzz.
  if (m_fontFamily.isEmpty()) {
    if (m_typefaceDirty) {
      m_resolvedTypeface.reset();
      m_typefaceDirty = false;
    }
    m_sceneParameters.typeface = nullptr;
  } else {
    if (m_typefaceDirty) {
      sk_sp<SkTypeface> typeface =
          resolveGalleryTypeface(m_fontContext->fontManager(), m_fontFamily);
      if (typeface && !m_fontCoordinates.empty()) {
        // Axis application goes through the FontContext's memoized varied
        // clone (ShapingStyle::variations machinery) instead of a raw
        // makeClone: repeated identical axis positions resolve to the same
        // SkTypeface object, so the shape cache stays warm across the
        // dirty/rebuild cycle.
        std::vector<sigil::weave::FontVariation> variations;
        variations.reserve(m_fontCoordinates.size());
        for (const FontCoordinate& coordinate : m_fontCoordinates) {
          sigil::weave::FontVariation variation;
          variation.tag[0] = static_cast<char>(coordinate.tag >> 24u);
          variation.tag[1] = static_cast<char>(coordinate.tag >> 16u);
          variation.tag[2] = static_cast<char>(coordinate.tag >> 8u);
          variation.tag[3] = static_cast<char>(coordinate.tag);
          variation.value = coordinate.value;
          variations.push_back(variation);
        }
        typeface = m_fontContext->variedTypeface(typeface, variations);
      }
      m_resolvedTypeface = std::move(typeface);
      m_typefaceDirty = false;
    }
    m_sceneParameters.typeface = m_resolvedTypeface;
  }

  for (const SkPoint& click : m_pendingClicks) scene->pointerPress(click);
  m_pendingClicks.clear();

  canvas->save();
  canvas->scale(devicePixelRatio, devicePixelRatio);
  const double elapsedSeconds =
      m_pausedSeconds + (m_animating ? m_clock.elapsed() / 1000.0 : 0.0);

  const uint64_t shapeCallsBefore = m_fontContext->stats().shapeCalls;
  const auto recordingStartTime = Clock::now();
  const FrameStats frameStatistics = scene->render(
      canvas, {logicalSize.width(), logicalSize.height()}, elapsedSeconds,
      m_frameNumber, m_sceneParameters, *m_fontContext);
  const double recordMicroseconds = std::chrono::duration<double, std::micro>(
                                        Clock::now() - recordingStartTime)
                                        .count();
  canvas->restore();

  if (m_animating) m_frameNumber++;

  m_reshapedWordCount = m_fontContext->stats().shapeCalls - shapeCallsBefore;
  m_runCount = frameStatistics.runCount;
  m_glyphCount = frameStatistics.glyphCount;
  m_layoutMicrosecondsAverage =
      m_layoutMicrosecondsAverage == 0
          ? frameStatistics.layoutMicroseconds
          : m_layoutMicrosecondsAverage * 0.95 +
                frameStatistics.layoutMicroseconds * 0.05;
  m_recordMicrosecondsAverage =
      m_recordMicrosecondsAverage == 0
          ? recordMicroseconds
          : m_recordMicrosecondsAverage * 0.95 + recordMicroseconds * 0.05;
  if (m_animating && m_interFrameTimer.isValid()) {
    const double elapsedMilliseconds =
        static_cast<double>(m_interFrameTimer.nsecsElapsed()) / 1e6;
    if (elapsedMilliseconds > 0.1) {
      const double framesPerSecond = 1000.0 / elapsedMilliseconds;
      m_framesPerSecondAverage =
          m_framesPerSecondAverage == 0
              ? framesPerSecond
              : m_framesPerSecondAverage * 0.95 + framesPerSecond * 0.05;
    }
  }
  m_interFrameTimer.restart();
  if (++m_statisticsFrameCount % 15 == 0) m_statsDirty = true;
}

void GalleryViewRenderer::render(QRhiCommandBuffer* commandBuffer) {
  using Clock = std::chrono::steady_clock;
  QRhiTexture* texture = colorTexture();
  if (!texture || m_scenes.empty() || m_logicalSize.width() < 8 ||
      m_logicalSize.height() < 8)
    return;
  const QSize pixelSize = texture->pixelSize();
  const float devicePixelRatio = static_cast<float>(pixelSize.width()) /
                                 static_cast<float>(m_logicalSize.width());

#ifdef TEXTFLOW_GALLERY_GPU
  if (m_graphiteContext && m_useGpuBackend) {
    // GPU: record straight into the item's texture, submit asynchronously —
    // Qt's scene-graph pass on the same Metal queue orders after it.
    sigil::skia::OffscreenSurface surface =
        sigil::skia::wrapTexture(*m_graphiteContext, texture, pixelSize);
    if (SkCanvas* canvas = surface.canvas()) {
      renderScene(canvas, devicePixelRatio, m_logicalSize);
      const auto submissionStartTime = Clock::now();
      surface.submit();
      const double submitMicroseconds =
          std::chrono::duration<double, std::micro>(Clock::now() -
                                                    submissionStartTime)
              .count();
      m_submitMicrosecondsAverage =
          m_submitMicrosecondsAverage == 0
              ? submitMicroseconds
              : m_submitMicrosecondsAverage * 0.95 + submitMicroseconds * 0.05;
      // Nothing recorded on `commandBuffer` itself; the texture is ready for
      // the scene graph once Graphite's work executes ahead of it in queue
      // order. The command buffer is still needed for the fallback path below.
      return;
    }
  }
#endif

  // CPU fallback: raster into a pixel buffer, then upload into the texture.
  m_rasterPixels.resize(static_cast<size_t>(pixelSize.width()) *
                        static_cast<size_t>(pixelSize.height()));
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(),
      static_cast<size_t>(pixelSize.width()) * sizeof(uint32_t));
  if (!surface) return;
  renderScene(surface->getCanvas(), devicePixelRatio, m_logicalSize);

  const auto submissionStartTime = Clock::now();
  QRhiResourceUpdateBatch* batch = rhi()->nextResourceUpdateBatch();
  QRhiTextureSubresourceUploadDescription sub(
      m_rasterPixels.data(), m_rasterPixels.size() * sizeof(uint32_t));
  batch->uploadTexture(texture, QRhiTextureUploadDescription({0, 0, sub}));
  commandBuffer->resourceUpdate(batch);
  const double submitMicroseconds = std::chrono::duration<double, std::micro>(
                                        Clock::now() - submissionStartTime)
                                        .count();
  m_submitMicrosecondsAverage =
      m_submitMicrosecondsAverage == 0
          ? submitMicroseconds
          : m_submitMicrosecondsAverage * 0.95 + submitMicroseconds * 0.05;
}
