#include "SkiaSceneBackend.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <sigilmeasure/time/FrameTimer.h>
#include <sigilskia/qt/QtInterop.h>
#include <sigilweave/qt/SigilWeaveQt.h>
#include <spdlog/spdlog.h>

#include <memory>

#include "SceneRenderer.h"
#include "SpellCircleRenderer.h"

using sigil::weave::qt::toSkColor;

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
  explicit SkiaSceneBackendImpl(
      std::unique_ptr<sigil::skia::GraphiteContext> context)
      : m_context(std::move(context)) {}

  QCanvasImage drawScene(SpellCircleRenderer& renderer, QCanvasPainter* painter,
                         QCanvasOffscreenCanvas& canvas,
                         QSize pixelSize) override {
    // All coordinates are in 0..canvasWidth / 0..canvasHeight scene space —
    // pixelSize always matches that scene size (see
    // SpellCircleRenderer::prePaint), so the resolved geometry in
    // renderer.m_resolved can be drawn directly with no extra scaling here.
    m_frames.begin();
    sigil::skia::OffscreenSurface surface =
        sigil::skia::wrapTexture(*m_context, canvas.texture(), pixelSize);
    SkCanvas* skCanvas = surface.canvas();
    if (!skCanvas) return {};

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
    style.fontSize = static_cast<float>(renderer.m_font.pointSizeF() * scale);
    // The settings dialog's "Font Style" picker (e.g. "Light", "Semibold
    // Italic") sets weight/slant via QFontDatabase::font() (see
    // FontDatabase.cpp); sigil::weave::qt::toSkTypeface carries both through,
    // resolved against the same font manager the labels are shaped with.
    style.typeface = sigil::weave::qt::toSkTypeface(
        m_sceneRenderer.fontContext().fontManager(), renderer.m_font);

    m_sceneRenderer.draw(skCanvas, renderer.m_resolved, style);

    // Record and submit are the frame's two lanes: the work lane closes
    // when every command is recorded, the frame lane when the GPU has been
    // handed the frame.
    m_frames.composed();
    surface.submit();
    m_frames.finished();

    if (m_sceneFrame++ % 600 == 0) {
      const double recordMs = m_frames.work().last();
      const double submitMs = m_frames.frame().last() - recordMs;
      spdlog::info(
          "drawScene: record {:.0f} us (mean {:.0f}), submit {:.0f} "
          "us (mean {:.0f})",
          recordMs * 1000.0, m_frames.work().mean() * 1000.0, submitMs * 1000.0,
          (m_frames.frame().mean() - m_frames.work().mean()) * 1000.0);
    }

    // The offscreen canvas's blended output is premultiplied-alpha; without
    // this flag drawImage() re-applies alpha on top of already alpha-baked-in
    // color, darkening every translucent fill.
    return painter->addImage(canvas,
                             QCanvasPainter::ImageFlag::GenerateMipmaps |
                                 QCanvasPainter::ImageFlag::Premultiplied);
  }

 private:
  std::unique_ptr<sigil::skia::GraphiteContext> m_context;

  // Shared Qt-free scene drawing (FontContext + label caches inside).
  // Lives on the render thread with this backend — see SceneRenderer's
  // threading rule.
  spellcircle::SceneRenderer m_sceneRenderer;
  uint64_t m_sceneFrame = 0;
  sigil::measure::FrameTimer m_frames;  // record = work lane, submit = the rest
};

std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi* rhi) {
  std::unique_ptr<sigil::skia::GraphiteContext> context =
      sigil::skia::createGraphiteContext(rhi);
  if (!context) return nullptr;
  return std::make_unique<SkiaSceneBackendImpl>(std::move(context));
}
