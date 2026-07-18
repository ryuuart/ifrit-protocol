#include "SkiaSceneBackend.h"
#include "SceneRenderer.h"
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#include "SpellCircleRenderer.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>

#include <textflowqt/TextFlowQt.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>

using textflowqt::toSkColor;

// SkiaSceneBackendImpl deliberately lives at global scope (not in an
// anonymous namespace) so that SpellCircleRenderer's
// `friend class SkiaSceneBackendImpl;` declaration names this exact type —
// a class in an anonymous namespace gets internal linkage under a
// compiler-generated unique name, and the friend declaration would instead
// silently forward-declare an unrelated, inaccessible ::SkiaSceneBackendImpl.
//
// The scene drawing itself lives in the shared Qt-free
// spellcircle::SceneRenderer (src/scene/SceneRenderer.cpp) — this class is
// the Qt frame around it: wrapping the QCanvasOffscreenCanvas texture as an
// SkSurface, translating the renderer's Qt-typed style fields, and
// registering the finished image with QCanvasPainter.
class SkiaSceneBackendImpl final : public CanvasSceneBackend {
public:
  explicit SkiaSceneBackendImpl(std::unique_ptr<SkiaGraphiteContext> context)
      : m_context(std::move(context)) {}

  QCanvasImage drawScene(SpellCircleRenderer &renderer, QCanvasPainter *painter,
                         QCanvasOffscreenCanvas &canvas,
                         QSize pixelSize) override {
    // All coordinates are in 0..canvasWidth / 0..canvasHeight scene space —
    // pixelSize always matches that scene size (see
    // SpellCircleRenderer::prePaint), so the resolved geometry in
    // renderer.m_resolved can be drawn directly with no extra scaling here.
    const auto frameStart = std::chrono::steady_clock::now();
    SkiaOffscreenSurface surface(*m_context, canvas.texture(), pixelSize);
    SkCanvas *skCanvas = surface.canvas();
    if (!skCanvas)
      return {};

    skCanvas->clear(SK_ColorTRANSPARENT);

    const double scale = renderer.m_scale;
    spellcircle::SceneStyle style;
    style.accentColor = toSkColor(renderer.m_accentColor);
    style.strokeWidth = static_cast<float>(renderer.m_strokeWidth * scale);
    style.labelOffset = static_cast<float>(renderer.m_labelOffset * scale);
    style.pointDistance = static_cast<float>(renderer.m_pointDistance * scale);
    style.boxWidth = static_cast<float>(renderer.m_boxWidth * scale);
    style.boxHeight = static_cast<float>(renderer.m_boxHeight * scale);
    style.boxPadding = static_cast<float>(renderer.m_boxPadding * scale);
    style.boxDistance = static_cast<float>(renderer.m_boxDistance * scale);
    style.fontSize =
        static_cast<float>(renderer.m_font.pointSizeF() * scale);
    // The settings dialog's "Font Style" picker (e.g. "Light", "Semibold
    // Italic") sets weight/slant via QFontDatabase::font() (see
    // FontDatabase.cpp); textflowqt::toSkTypeface carries both through,
    // resolved against the same font manager the labels are shaped with.
    style.typeface = textflowqt::toSkTypeface(
        m_sceneRenderer.fontContext().fontManager(), renderer.m_font);

    m_sceneRenderer.draw(skCanvas, renderer.m_resolved, style);

    const auto recordEnd = std::chrono::steady_clock::now();
    surface.submit();
    const auto submitEnd = std::chrono::steady_clock::now();

    const double recordMicroseconds =
        std::chrono::duration<double, std::micro>(recordEnd - frameStart)
            .count();
    const double submitMicroseconds =
        std::chrono::duration<double, std::micro>(submitEnd - recordEnd)
            .count();
    m_recordMicrosecondsAverage =
        m_recordMicrosecondsAverage == 0
            ? recordMicroseconds
            : m_recordMicrosecondsAverage * 0.98 + recordMicroseconds * 0.02;
    m_submitMicrosecondsAverage =
        m_submitMicrosecondsAverage == 0
            ? submitMicroseconds
            : m_submitMicrosecondsAverage * 0.98 + submitMicroseconds * 0.02;
    if (m_sceneFrame++ % 600 == 0)
      spdlog::info("drawScene: record {:.0f} us (ema {:.0f}), submit {:.0f} "
                   "us (ema {:.0f})",
                   recordMicroseconds, m_recordMicrosecondsAverage,
                   submitMicroseconds, m_submitMicrosecondsAverage);

    // The offscreen canvas's blended output is premultiplied-alpha; without
    // this flag drawImage() re-applies alpha on top of already alpha-baked-in
    // color, darkening every translucent fill.
    return painter->addImage(canvas,
                             QCanvasPainter::ImageFlag::GenerateMipmaps |
                                 QCanvasPainter::ImageFlag::Premultiplied);
  }

private:
  std::unique_ptr<SkiaGraphiteContext> m_context;

  // Shared Qt-free scene drawing (FontContext + label caches inside).
  // Lives on the render thread with this backend — see SceneRenderer's
  // threading rule.
  spellcircle::SceneRenderer m_sceneRenderer;
  uint64_t m_sceneFrame = 0;
  double m_recordMicrosecondsAverage = 0;
  double m_submitMicrosecondsAverage = 0;
};

std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi *rhi) {
  std::unique_ptr<SkiaGraphiteContext> context =
      SkiaGraphiteContext::create(rhi);
  if (!context)
    return nullptr;
  return std::make_unique<SkiaSceneBackendImpl>(std::move(context));
}
