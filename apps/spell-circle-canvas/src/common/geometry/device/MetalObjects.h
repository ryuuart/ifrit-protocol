#pragma once
// The events a queue on Metal raises, brought over to the device as
// timeline semaphores its own queue can wait on. Private to the device
// feature; it spells no Vulkan type, so the translation unit that owns a
// device includes it without the loader.

#include <cstdint>
#include <mutex>
#include <vector>

namespace Diligent {
struct IRenderDevice;
}  // namespace Diligent

namespace sigil::geometry::device {

/** THE METAL EVENTS ONE DEVICE'S QUEUE WAITS ON, each brought over once
 *  as a timeline semaphore over the very same event, and kept until this
 *  goes. Safe from any thread. */
class MetalEvents {
 public:
  /** Brings events over onto @p device, which outlives this. */
  explicit MetalEvents(Diligent::IRenderDevice* device);
  /** Destroys every semaphore it made; nothing may still wait on one,
   *  so the queue is idle before this goes. */
  ~MetalEvents();
  MetalEvents(const MetalEvents&) = delete;
  MetalEvents& operator=(const MetalEvents&) = delete;

  /** Queues a wait on @p queue — the device's own VkQueue as void* — for
   *  @p mtlSharedEvent to reach @p value. False, with nothing queued,
   *  where the driver cannot bring the event over. */
  bool wait(void* queue, void* mtlSharedEvent, uint64_t value);

 private:
  /** One event and the semaphore over it, as the 64-bit word Vulkan
   *  defines a semaphore to be. */
  struct Brought {
    void* event = nullptr;
    uint64_t semaphore = 0;
  };
  Diligent::IRenderDevice* m_device = nullptr;
  std::mutex m_mutex;
  std::vector<Brought> m_brought;
};

}  // namespace sigil::geometry::device
