/** @file
 * The host-less pen: a one-pixel canvas on the installed host-less
 * device, or on raster, and the crossings from that device to a pen and
 * to the CPU.
 */

#include <include/core/SkCanvas.h>
#include <sigildraw/Hostless.h>

#include <new>

namespace sigil::draw {
namespace {

HostlessDevice*& installed() {
  static HostlessDevice* device = nullptr;
  return device;
}

}  // namespace

void useHostlessDevice(HostlessDevice* device) { installed() = device; }

HostlessDevice* hostlessDevice() { return installed(); }

HostlessPen::HostlessPen(float density, float width, float height,
                         weave::FontContext* fonts) {
  // ONE PIXEL ON THE DEVICE: nothing is drawn on it, it only answers the
  // surface a buffer begun under this pen is formed on, which is a
  // surface beside it and so on the same device.
  const SkImageInfo one = SkImageInfo::MakeN32Premul(1, 1);
  if (HostlessDevice* device = hostlessDevice())
    m_surface = device->makeSurface(one);
  if (!m_surface) m_surface = SkSurfaces::Raster(one);
  if (!m_surface) throw std::bad_alloc();
  SkCanvas& canvas = *m_surface->getCanvas();
  m_saveCount = canvas.save();
  canvas.scale(density, density);
  Frame frame;
  frame.width = width;
  frame.height = height;
  frame.fonts = fonts;
  m_pen.begin(canvas, frame);
}

HostlessPen::~HostlessPen() {
  m_pen.end();
  m_surface->getCanvas()->restoreToCount(m_saveCount);
  if (HostlessDevice* device = hostlessDevice()) device->submit();
}

bool HostlessPen::drawnAsItStands(Pen& target, const SkImage& image) {
  if (!image.isTextureBacked()) return true;
  SkCanvas* canvas = target.canvas();
  HostlessDevice* device = hostlessDevice();
  return canvas && device && device->drawsOn(*canvas);
}

sk_sp<SkImage> HostlessPen::drawableOn(Pen& target,
                                       const sk_sp<SkImage>& image) {
  if (!image || drawnAsItStands(target, *image)) return image;
  return rasterImage(image);
}

sk_sp<SkImage> HostlessPen::rasterImage(const sk_sp<SkImage>& image) {
  if (!image || !image->isTextureBacked()) return image;
  HostlessDevice* device = hostlessDevice();
  return device ? device->rasterImage(image) : nullptr;
}

}  // namespace sigil::draw
