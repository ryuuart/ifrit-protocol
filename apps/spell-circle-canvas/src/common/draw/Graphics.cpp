/** @file
 * The offscreen buffer: a surface formed at the host's density, a pen
 * over it, and the two ways a frame puts it back down.
 */

#include <include/core/SkCanvas.h>
#include <sigildraw/Graphics.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw {

Graphics::Graphics(float width, float height)
    : m_width(std::max(1.0f, width)), m_height(std::max(1.0f, height)) {}

void Graphics::form(Pen& host) {
  const float density = host.contentScale();
  const SkISize extent{
      std::max(1, (int)std::lround(m_width * density)),
      std::max(1, (int)std::lround(m_height * density))};
  if (m_surface && m_extent == extent) return;
  const SkImageInfo info = SkImageInfo::MakeN32Premul(extent);
  // Made through the host's canvas so it lives where the host draws;
  // raster is what a host with no device offers, and what a device
  // canvas answers when it cannot make one.
  sk_sp<SkSurface> surface =
      host.canvas() ? host.canvas()->makeSurface(info) : nullptr;
  if (!surface) surface = SkSurfaces::Raster(info);
  m_surface = std::move(surface);
  m_extent = extent;
  m_scale = (float)extent.width() / m_width;
  m_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
}

Pen& Graphics::begin(Pen& host) {
  form(host);
  SkCanvas* canvas = m_surface->getCanvas();
  canvas->save();
  // The buffer's pen draws in canvas units whatever density the surface
  // was formed at, so a sketch writes one set of numbers.
  canvas->scale(m_scale, m_scale);
  Frame frame;
  frame.width = m_width;
  frame.height = m_height;
  frame.seconds = host.millis() / 1000.0;
  frame.deltaSeconds = host.deltaTime / 1000.0;
  frame.frameCount = host.frameCount;
  frame.fonts = host.fonts();
  pen.begin(*canvas, frame);
  m_open = true;
  return pen;
}

void Graphics::end() {
  if (!m_open) return;
  pen.end();
  m_surface->getCanvas()->restore();
  m_open = false;
}

sk_sp<SkImage> Graphics::image() const {
  return m_surface ? m_surface->makeImageSnapshot() : nullptr;
}

void Pen::image(const Graphics& buffer, float x, float y) {
  // Placed by the buffer's CANVAS size, never by its pixel count: the
  // two differ by the host's density, and a buffer drawn at its pixel
  // count would land at the wrong size on a doubled screen.
  image(buffer.image(), x, y, buffer.width(), buffer.height());
}

void Pen::image(const Graphics& buffer, float x, float y, float w, float h) {
  image(buffer.image(), x, y, w, h);
}

}  // namespace sigil::draw
