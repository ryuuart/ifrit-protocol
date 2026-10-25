/** @file
 * A window's texture drawn through Graphite on the window's own device,
 * or through a raster buffer uploaded into it.
 */

#include "sigilsketch/window/WindowPainter.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <rhi/qrhi.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/core/Painting.h>
#include <sigilsketch/live/Host.h>
#include <sigilskia/graphite/PaintOrder.h>
#include <sigilskia/graphite/Readback.h>
#include <sigilskia/qt/QtInterop.h>

#include <QtCore/QByteArray>
#include <QtCore/QSize>

#ifdef __APPLE__
#include <rhi/qrhi_platform.h>
#endif

namespace sigil::sketch {

WindowPainter::WindowPainter() = default;

WindowPainter::~WindowPainter() { release(); }

bool WindowPainter::initialize(QRhi* rhi) {
  release();
  // Metal only: the Vulkan adapter does not hand the image's final layout
  // back to QRhi's state tracker, so Qt's later sampling of it would be
  // undefined.
  if (!rhi || rhi->backend() != QRhi::Metal) return false;
#ifdef __APPLE__
  // A QRhi RENDERING ON THE PROCESS'S OWN PAINTER — the window was handed
  // its device and queue as it opened — draws its frames through that
  // painter's context, so a picture drawn with no frame open stands where
  // the frames are drawn and is drawn as it stands.
  if (const auto* handles =
          static_cast<const QRhiMetalNativeHandles*>(rhi->nativeHandles())) {
    const Painter own = standingOwnPainter();
    if (own.graphite && !device() &&
        own.device->native().mtlDevice == handles->dev &&
        own.device->native().mtlCommandQueue == handles->cmdQueue) {
      m_borrowedContext = own.graphite;
      return true;
    }
  }
#endif
  m_graphiteContext = skia::createGraphiteContext(rhi);
#ifdef __APPLE__
  // ONE METAL CONTEXT FOR THE WINDOW AND ITS PAGES. The shared device
  // paints its pages on Metal; handed the window's own queue and context,
  // a page is painted where the window draws, and an overlay page is
  // drawn as it stands rather than read back. The device refuses a queue
  // on another Metal device and keeps its own then.
  geometry::device::Device* shared = device();
  if (shared && m_graphiteContext) {
    if (const auto* handles =
            static_cast<const QRhiMetalNativeHandles*>(rhi->nativeHandles())) {
      core::hardware::NativeDevice native;
      native.backend = core::hardware::Backend::Metal;
      native.mtlDevice = handles->dev;
      native.mtlCommandQueue = handles->cmdQueue;
      m_windowDevice = core::hardware::GpuDevice::adopt(native).value_or(nullptr);
      if (!m_windowDevice ||
          !shared->paintWith(m_windowDevice.get(), m_graphiteContext.get()))
        m_windowDevice.reset();
    }
  }
#endif
  return m_graphiteContext != nullptr;
}

void WindowPainter::release() {
  if (m_windowDevice) {
    // The device may already be gone at the end of the run, and then
    // there is nothing to take back.
    if (geometry::device::Device* shared = device())
      (void)shared->paintWith(nullptr, nullptr);
    m_windowDevice.reset();
  }
  m_captureCanvas.reset();
  m_graphiteContext.reset();
  m_borrowedContext = nullptr;
}

skia::GraphiteContext* WindowPainter::graphite() const {
  return m_graphiteContext ? m_graphiteContext.get() : m_borrowedContext;
}

void WindowPainter::installCaptureBackend(Host& host) {
  if (!graphite()) return;
  host.setCaptureBackend(
      {[this](const SkImageInfo& info) -> sk_sp<SkSurface> {
         skia::GraphiteContext* context = graphite();
         if (!context) return nullptr;
         return SkSurfaces::RenderTarget(context->recorder(), info);
       },
       [this](SkSurface& surface, const SkPixmap& out) {
         skia::GraphiteContext* context = graphite();
         return context && skia::readbackPixels(*context, surface, out);
       },
       [this](SkSurface& surface) -> SkCanvas* {
         skia::GraphiteContext* context = graphite();
         if (!context) return nullptr;
         m_captureCanvas = std::make_unique<skia::PaintOrderCanvas>(
             *context, surface.getCanvas());
         return m_captureCanvas.get();
       }});
}

bool WindowPainter::paint(QRhi* rhi, QRhiTexture* texture,
                          QRhiCommandBuffer* commandBuffer,
                          const std::function<void(SkCanvas&)>& paint) {
  skia::GraphiteContext* context = graphite();
  if (!context) return paintRaster(rhi, texture, commandBuffer, paint);
  skia::OffscreenSurface surface =
      skia::wrapTexture(*context, texture, texture->pixelSize());
  SkCanvas* canvas = surface.canvas();
  if (!canvas) return false;
  paint(*canvas);
  surface.submit();
  return true;
}

bool WindowPainter::paintRaster(QRhi* rhi, QRhiTexture* texture,
                                QRhiCommandBuffer* commandBuffer,
                                const std::function<void(SkCanvas&)>& paint) {
  const QSize pixelSize = texture->pixelSize();
  // One reusable raster buffer and one explicit upload, so the frame is
  // copied only by the backend's staging upload.
  m_rasterPixels.resize((size_t)pixelSize.width() * (size_t)pixelSize.height());
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      SkImageInfo::Make(pixelSize.width(), pixelSize.height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      m_rasterPixels.data(), (size_t)pixelSize.width() * sizeof(std::uint32_t));
  if (!surface) return false;
  paint(*surface->getCanvas());
  QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
  // fromRawData keeps this upload view non-owning; the buffer stays
  // stable through QRhi's endFrame.
  const QByteArray uploadBytes = QByteArray::fromRawData(
      reinterpret_cast<const char*>(m_rasterPixels.data()),
      (qsizetype)m_rasterPixels.size() * (qsizetype)sizeof(std::uint32_t));
  QRhiTextureSubresourceUploadDescription subresource(uploadBytes);
  batch->uploadTexture(texture,
                       QRhiTextureUploadDescription({0, 0, subresource}));
  commandBuffer->resourceUpdate(batch);
  return true;
}

}  // namespace sigil::sketch
