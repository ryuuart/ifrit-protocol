/** @file
 * The source a foreign texture reaches a material slot as: a handle in
 * the device's table, let go when the last value naming it is.
 */

#include "sigilworld/diligent/Import.h"

#include <sigilgeometry/device/Device.h>

#include <chrono>
#include <memory>
#include <utility>

namespace sigil::world::diligent {

using ::sigil::geometry::device::Device;

namespace {

/** THE HANDLE, HELD. The value a caller carries is copyable, so what
 *  owns the handle is shared state behind it: the last copy to go is
 *  what tells the device to let go. */
struct Held {
  core::hardware::GpuDevice* device = nullptr;
  core::hardware::TextureHandle handle;

  ~Held() {
    if (device && handle) device->destroy(handle);
  }
};

/** A SOURCE WITH NO HOST IMAGE. Nothing here reads the pixels back —
 *  that is the whole point of the door — so its frame carries no image
 *  and no binding, and a renderer that cannot bind the device texture
 *  draws the body undressed rather than something it invented. */
class NativeSource {
 public:
  explicit NativeSource(std::shared_ptr<Held> held) : m_held(std::move(held)) {}

  media::Frame frameAt(std::chrono::duration<double>) const {
    media::Frame frame;
    if (!m_held || !m_held->device || !m_held->handle) return frame;
    const core::hardware::NativeTexture native =
        m_held->device->exportNative(m_held->handle);
    if (!native) return frame;
    frame.device.kind = media::DeviceFrame::Kind::Texture;
    frame.device.device = m_held->device;
    frame.device.pointer = native.mtlTexture;
    frame.device.handle = native.vkImage;
    frame.device.format = native.vkFormat;
    frame.device.layout = native.vkLayout;
    frame.device.width = native.width;
    frame.device.height = native.height;
    return frame;
  }
  bool isRunning() const { return false; }

  /** One import is one texture: two values are the same map when they
   *  name the same handle, which is what a reconciler asking whether a
   *  surface changed reads. */
  bool operator==(const NativeSource& other) const {
    return m_held == other.m_held;
  }

 private:
  std::shared_ptr<Held> m_held;
};

}  // namespace

material::Texture importNative(Device& device,
                               const core::hardware::NativeTexture& native,
                               bool takeOwnership) {
  core::hardware::GpuDevice* gpu = device.gpu();
  if (!gpu) return {};
  const core::hardware::TextureHandle handle =
      gpu->importNative(native, takeOwnership);
  if (!handle) return {};
  auto held = std::make_shared<Held>();
  held->device = gpu;
  held->handle = handle;
  return material::Texture(NativeSource{std::move(held)});
}

}  // namespace sigil::world::diligent
