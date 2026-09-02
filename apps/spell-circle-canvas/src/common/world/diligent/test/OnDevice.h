#pragma once

/** @file
 * THE ONE DEVICE THIS BINARY BRINGS UP, the seam values that stand on
 * it, and the small set of things every case here photographs.
 *
 * A device is made ONCE for the process. Creating one costs more the
 * more of them a process has already made, and a case that made its own
 * would be paying that for every case after it; nothing here needs a
 * device nobody else has touched. A case that needs the device and finds
 * none SKIPS rather than fails, so a machine with no Vulkan runtime
 * stays green — and says so through the binary's `gpu` label rather than
 * through a passing test.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilworld/diligent/Painter.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <memory>
#include <string>

namespace sigil::world::diligent {

/** The device every case in this binary shares, or null with @p error
 *  saying why there is none. */
inline geometry::device::Device* sharedDevice(std::string* error) {
  static std::string reason;
  static const std::unique_ptr<geometry::device::Device> device = [] {
    const geometry::device::DeviceConfig config;
    return geometry::device::Device::create(config, &reason);
  }();
  if (error) *error = reason;
  return device.get();
}

/** A DEVICE AND ONE SEAM VALUE STANDING ON IT, or the reason there is
 *  neither. The runtime type is the parameter because two seams stand on
 *  the same device: the one that performs a frame's passes and the one
 *  that draws a mesh onto a canvas. */
template <class R>
struct OnDevice {
  geometry::device::Device* device = nullptr;
  R runtime;
  std::string error;
  explicit operator bool() const { return device != nullptr; }
};

/** The runtime that performs a frame's passes. */
inline OnDevice<::sigil::world::Runtime> onDevice() {
  OnDevice<::sigil::world::Runtime> out;
  out.device = sharedDevice(&out.error);
  if (out.device) out.runtime = ::sigil::world::diligent::runtime(*out.device);
  return out;
}

/** The runtime that draws a mesh onto a canvas. */
inline OnDevice<::sigil::geometry::mesh::render::Runtime> onPainterDevice() {
  OnDevice<::sigil::geometry::mesh::render::Runtime> out;
  out.device = sharedDevice(&out.error);
  if (out.device)
    out.runtime = ::sigil::world::diligent::painterRuntime(*out.device);
  return out;
}

/** LOOKING DOWN ON A SET from above and in front, which is where a body
 *  standing on a plate has both of them in view. */
inline Camera raisedEye() {
  Camera camera;
  camera.eye = {0, 90, 240};
  camera.target = {0, 0, 0};
  return camera;
}

/** SQUARE ON TO A CARD, so what varies across the card is the surface
 *  and not the angle it is seen at. */
inline Camera levelEye() {
  Camera camera;
  camera.eye = {0, 0, 200};
  camera.target = {0, 0, 0};
  return camera;
}

/** A card facing the camera, wearing @p surface, lit by one sun. */
inline Frame card(const material::Material& surface, SkISize extent) {
  Element root =
      Element()
          .key("set")
          .child(Element().key("sun").light(light::sun({-0.2f, -0.3f, -1.0f})))
          .child(Element()
                     .key("card")
                     .mesh(::sigil::geometry::mesh::quad(120, 120))
                     .fill(surface));
  Frame frame(root);
  frame.extent(extent)
      .camera(levelEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

/** ONE FRAME, rendered on @p runtime and photographed from @p camera at
 *  @p extent. The clock is stepped once, so a plate is a function of the
 *  declaration and not of how long anything took. */
inline SkBitmap photograph(const Frame& frame,
                           const ::sigil::world::Runtime& executor,
                           SkISize extent, const Camera& camera) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame copy = frame;
  copy.runtime(executor);
  ticker.tick(1.0 / 60.0);
  scene.render(copy);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(extent.width(), extent.height()));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  scene.draw(canvas, camera);
  return bitmap;
}

/** A pixel of @p plate at fractions of its width and height, so a case
 *  names where it is looking rather than where a projection put it. */
inline SkColor4f at(const SkBitmap& plate, float x, float y) {
  return plate.getColor4f((int)(x * (float)plate.width()),
                          (int)(y * (float)plate.height()));
}

/** WHAT THE 2D PATH PAINTED ON THIS DEVICE: the handle it was painted
 *  through, and the same texture as the graphics API's own object. Both
 *  are empty when the device carries no adopted 2D side. The caller owns
 *  the handle and destroys it. */
struct PaintedTexture {
  core::hardware::TextureHandle handle;
  core::hardware::NativeTexture native;
};

/** Paint a square of @p colour into a texture on @p device through the
 *  2D path, and hand back the graphics API's own object for it — the
 *  pixels a 3D pass then binds where they already stand. */
inline PaintedTexture paintOnDevice(geometry::device::Device& device,
                                    SkColor4f colour, int side = 16) {
  PaintedTexture out;
  core::hardware::GpuDevice* gpu = device.gpu();
  skia::GraphiteContext* graphite = device.graphite();
  if (!gpu || !graphite) return out;

  core::hardware::TextureDesc desc;
  desc.width = desc.height = side;
  desc.format = core::hardware::TextureFormat::RGBA8Unorm;
  out.handle = gpu->createTexture(desc);
  if (!out.handle) return out;
  {
    // The surface has to be gone before the texture is exported, and the
    // queue is shared with every other runtime on this device.
    const geometry::device::Device::QueueLock lock(device);
    skia::OffscreenSurface surface(*graphite, *gpu, out.handle);
    if (!surface.canvas()) return out;
    surface.canvas()->clear(colour);
    surface.submit();
  }
  out.native = gpu->exportNative(out.handle);
  return out;
}

}  // namespace sigil::world::diligent
