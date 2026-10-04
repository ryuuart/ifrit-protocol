/** @file
 * The offscreen buffer: a surface formed at the host's density, a pen
 * over it, and the two ways a frame puts it back down.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSamplingOptions.h>
#include <sigildraw/Graphics.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

namespace sigil::draw {
namespace {

float canvasDimension(float value) {
  if (!std::isfinite(value))
    throw std::invalid_argument("Graphics dimensions must be finite");
  return std::max(1.0f, value);
}

int pixelDimension(float units, float density) {
  const double pixels = std::round(static_cast<double>(units) * density);
  if (!std::isfinite(pixels) || pixels > std::numeric_limits<int>::max())
    throw std::length_error("Graphics pixel extent exceeds its integer range");
  return static_cast<int>(std::max(1.0, pixels));
}

}  // namespace

Graphics::Graphics(float width, float height)
    : m_width(canvasDimension(width)), m_height(canvasDimension(height)) {}

void Graphics::setDensityFloor(float devicePixelsPerUnit) {
  if (!std::isfinite(devicePixelsPerUnit))
    throw std::invalid_argument("Graphics density must be finite");
  m_densityFloor = std::max(0.0f, devicePixelsPerUnit);
}

void Graphics::form(Pen& host) {
  const float density = std::max(host.contentScale(), m_densityFloor);
  if (!std::isfinite(density) || density < 0)
    throw std::invalid_argument(
        "Graphics host density must be finite and nonnegative");
  const SkISize extent{pixelDimension(m_width, density),
                       pixelDimension(m_height, density)};
  const SkSize scale{(float)extent.width() / m_width,
                     (float)extent.height() / m_height};
  if (m_surface && m_extent == extent) {
    // The units the buffer's pen draws in are its canvas size whatever
    // extent the surface was formed at, so a `resize` too small to move
    // the rounded extent still moves the scale.
    m_scale = scale;
    return;
  }
  const SkImageInfo info = SkImageInfo::MakeN32Premul(extent);
  // Made through the host's canvas so it lives where the host draws;
  // raster is what a host with no device offers, and what a device
  // canvas answers when it cannot make one.
  sk_sp<SkSurface> surface =
      host.canvas() ? host.canvas()->makeSurface(info) : nullptr;
  if (!surface) surface = SkSurfaces::Raster(info);
  if (!surface) throw std::bad_alloc();
  SkCanvas& target = *surface->getCanvas();
  target.clear(SK_ColorTRANSPARENT);
  // A REPLACEMENT CARRIES THE PICTURE OVER, scaled from the extent it was
  // drawn at to the one it is kept at: a buffer is where earlier frames
  // accumulate, so moving the density or the size must not erase them.
  if (m_surface) {
    SkAutoCanvasRestore restore(&target, true);
    target.scale((float)extent.width() / (float)m_extent.width(),
                 (float)extent.height() / (float)m_extent.height());
    m_surface->draw(&target, 0, 0, SkSamplingOptions(SkFilterMode::kLinear),
                    nullptr);
  }
  m_surface = std::move(surface);
  m_extent = extent;
  m_scale = scale;
}

void Graphics::resize(float width, float height) {
  const float w = canvasDimension(width);
  const float h = canvasDimension(height);
  if (w == m_width && h == m_height) return;
  m_width = w;
  m_height = h;
  // The surface itself is re-formed by the next `begin`, which is where
  // the host's density is known; what stands on this one is carried into
  // it there.
}

Pen& Graphics::begin(Pen& host) {
  end();
  form(host);
  SkCanvas* canvas = m_surface->getCanvas();
  canvas->save();
  // The buffer's pen draws in canvas units whatever density the surface
  // was formed at, so a sketch writes one set of numbers.
  canvas->scale(m_scale.width(), m_scale.height());
  Frame frame;
  frame.width = m_width;
  frame.height = m_height;
  frame.seconds = host.millis() / 1000.0;
  frame.deltaSeconds = host.deltaTime / 1000.0;
  frame.frameCount = host.frameCount;
  frame.fonts = host.fonts();
  // The host's input as well as its clock: a program on the buffer reads
  // the pointer and the keys in the buffer's own units, which are the
  // host's canvas units where it is put down at the origin.
  frame.mouseX = host.mouseX;
  frame.mouseY = host.mouseY;
  frame.mouseIsPressed = host.mouseIsPressed;
  frame.keyIsPressed = host.keyIsPressed;
  frame.key = host.key;
  frame.keyCode = host.keyCode;
  frame.keysDown = host.keysDown();
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
