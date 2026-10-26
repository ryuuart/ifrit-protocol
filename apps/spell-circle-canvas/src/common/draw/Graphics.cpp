/** @file
 * The offscreen buffer: a surface formed at the host's density, a pen
 * over it, and the two ways a frame puts it back down.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSamplingOptions.h>
#if defined(SK_GRAPHITE)
#include <include/gpu/graphite/Recorder.h>
#include <src/gpu/graphite/RecorderPriv.h>
#endif
#include <sigildraw/Graphics.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>

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

/** WHICH DEVICE @p canvas RECORDS ONTO, as an identity: the capabilities
 *  of a Graphite recorder's shared context, which every recorder on one
 *  context shares and no other context does, or a Ganesh recording
 *  context; null for a canvas on no device. */
const void* deviceOf(SkCanvas* canvas) {
  if (!canvas) return nullptr;
#if defined(SK_GRAPHITE)
  if (skgpu::graphite::Recorder* recorder = canvas->recorder())
    return recorder->priv().caps();
#endif
  return canvas->recordingContext();
}

}  // namespace

Graphics::Graphics(float width, float height)
    : m_width(canvasDimension(width)), m_height(canvasDimension(height)) {}

void Graphics::setDensityFloor(float devicePixelsPerUnit) {
  if (!std::isfinite(devicePixelsPerUnit))
    throw std::invalid_argument("Graphics density must be finite");
  m_densityFloor = std::max(0.0f, devicePixelsPerUnit);
}

bool Graphics::standsWhere(Pen& host) const {
  if (!m_surface) return false;
  // A host that records onto no device — one that draws nothing, or a
  // raster canvas — forms nothing, so the buffer stands where it is.
  const void* const device = deviceOf(host.canvas());
  if (!device) return true;
  // A device that refused the buffer is not asked again at every frame:
  // the buffer stays on the raster it took instead.
  if (!m_device && device == m_refusedBy) return true;
  // A device's recorder belongs to the thread that made it, so a buffer on
  // the device is drawn into on the thread it was formed on and no other.
  return m_device == device && m_thread == std::this_thread::get_id();
}

void Graphics::follow(Pen& host) {
  if (!m_surface || standsWhere(host)) return;
  form(host, m_extent);
}

void Graphics::form(Pen& host) {
  const float density = std::max(host.contentScale(), m_densityFloor);
  if (!std::isfinite(density) || density < 0)
    throw std::invalid_argument(
        "Graphics host density must be finite and nonnegative");
  form(host,
       {pixelDimension(m_width, density), pixelDimension(m_height, density)});
}

void Graphics::form(Pen& host, SkISize extent) {
  const SkSize scale{(float)extent.width() / m_width,
                     (float)extent.height() / m_height};
  const bool stands = standsWhere(host);
  if (m_surface && m_extent == extent && stands) {
    // The units the buffer's pen draws in are its canvas size whatever
    // extent the surface was formed at, so a `resize` too small to move
    // the rounded extent still moves the scale.
    m_scale = scale;
    return;
  }
  const SkImageInfo info = SkImageInfo::MakeN32Premul(extent);
  // FORMED WHERE THE HOST DRAWS, through the host's canvas, so a buffer
  // begun under a device stands on that device. A re-form at another size
  // is made BESIDE ITSELF while the buffer stands where its host draws;
  // a buffer its host's device cannot reach — on raster, on another
  // context, or on another thread's recorder — FOLLOWS the host there. A
  // host whose canvas makes no surface — one that draws nothing, or a
  // device refusing the extent — forms it on raster.
  SkCanvas* const hostCanvas = host.canvas();
  sk_sp<SkSurface> surface;
  if (m_surface && stands) {
    surface = m_surface->makeSurface(info);
  } else {
    if (hostCanvas) surface = hostCanvas->makeSurface(info);
    m_refusedBy = surface ? nullptr : deviceOf(hostCanvas);
    if (!surface) surface = SkSurfaces::Raster(info);
  }
  if (!surface) throw std::bad_alloc();
  SkCanvas& target = *surface->getCanvas();
  target.clear(SK_ColorTRANSPARENT);
  // A REPLACEMENT CARRIES THE PICTURE OVER, scaled from the extent it was
  // drawn at to the one it is kept at: a buffer is where earlier frames
  // accumulate, so moving the density, the size, the thread or the
  // device's recorder must not erase them. A picture on raster is drawn
  // onto any device; one standing on another device's context cannot be
  // read from this one, and the buffer starts there transparent.
  const void* const device = deviceOf(&target);
  sk_sp<SkImage> held = image();
  if (held && held->isTextureBacked() && m_device != device) held.reset();
  if (held) {
    SkAutoCanvasRestore restore(&target, true);
    target.scale((float)extent.width() / (float)held->width(),
                 (float)extent.height() / (float)held->height());
    target.drawImage(held, 0, 0, SkSamplingOptions(SkFilterMode::kLinear));
  }
  m_device = device;
  m_thread = std::this_thread::get_id();
  m_surface = std::move(surface);
  m_snapshot.reset();
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
  // Put down at the host's origin in canvas units, the buffer stands in
  // the host's root where the host's own canvas does.
  frame.rootWidth = host.m_rootWidth;
  frame.rootHeight = host.m_rootHeight;
  frame.toRoot = host.m_toRoot;
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
  // Taken on the thread that drew, so a host on another thread reads the
  // picture without reaching into the recorder that drew it.
  m_snapshot = m_surface->makeImageSnapshot();
}

sk_sp<SkImage> Graphics::image() const {
  if (!m_surface) return nullptr;
  if (!m_open && m_snapshot) return m_snapshot;
  return m_surface->makeImageSnapshot();
}

void Pen::image(const Graphics& buffer, float x, float y) {
  // Placed by the buffer's CANVAS size, never by its pixel count: the
  // two differ by the host's density, and a buffer drawn at its pixel
  // count would land at the wrong size on a doubled screen.
  image(buffer.image(), x, y, buffer.width(), buffer.height());
}

void Pen::image(const Graphics& buffer, float x, float y, float width,
                float height) {
  image(buffer.image(), x, y, width, height);
}

}  // namespace sigil::draw
