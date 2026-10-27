// The device signal on Vulkan: the submit that carries it signals a
// semaphore, and a submit of its own on the same queue waits on that
// semaphore and raises a fence, which the host waits on. A semaphore
// Graphite signals is a binary one, which no host can wait on; the fence
// the second submit raises is.

#include <sigilskia/graphite/GraphiteContext.h>

#include <memory>

#include "Waits.h"

#ifdef SK_VULKAN

#include <gpu/graphite/BackendSemaphore.h>
#include <gpu/graphite/vk/VulkanGraphiteTypes.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace sigil::skia {

namespace {

/** The entry points the signal calls, answered by the device. */
struct VulkanCalls {
  PFN_vkCreateSemaphore createSemaphore = nullptr;
  PFN_vkDestroySemaphore destroySemaphore = nullptr;
  PFN_vkCreateFence createFence = nullptr;
  PFN_vkDestroyFence destroyFence = nullptr;
  PFN_vkQueueSubmit queueSubmit = nullptr;
  PFN_vkWaitForFences waitForFences = nullptr;

  bool complete() const {
    return createSemaphore && destroySemaphore && createFence && destroyFence &&
           queueSubmit && waitForFences;
  }
};

/** THE SIGNAL ON VULKAN: the semaphore the carrying submit signals and
 *  the fence the following submit raises once that semaphore is. */
class VulkanSignal final : public DeviceSignal::Form {
 public:
  VulkanSignal(VkDevice device, VkQueue queue, const VulkanCalls& calls,
               VkSemaphore semaphore, VkFence fence)
      : m_device(device),
        m_queue(queue),
        m_calls(calls),
        m_semaphore(semaphore),
        m_fence(fence),
        m_backendSemaphore(
            skgpu::graphite::BackendSemaphores::MakeVulkan(semaphore)) {}

  ~VulkanSignal() override {
    if (m_raising) {
      // The queue still holds the semaphore and the fence until the fence
      // is raised, so neither goes before it.
      m_calls.waitForFences(m_device, 1, &m_fence, VK_TRUE, UINT64_MAX);
      m_calls.destroySemaphore(m_device, m_semaphore, nullptr);
    } else if (!m_attached) {
      m_calls.destroySemaphore(m_device, m_semaphore, nullptr);
    }
    // An attached semaphore whose submit was never followed may still be
    // signalled by work the queue holds, so it is left to the device
    // rather than destroyed under it.
    m_calls.destroyFence(m_device, m_fence, nullptr);
  }

  void attach(skgpu::graphite::InsertRecordingInfo& insert) override {
    insert.fNumSignalSemaphores = 1;
    insert.fSignalSemaphores = &m_backendSemaphore;
    m_attached = true;
  }

  void afterSubmit() override {
    if (!m_attached || m_raising) return;
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_semaphore;
    submit.pWaitDstStageMask = &stage;
    m_raising = m_calls.queueSubmit(m_queue, 1, &submit, m_fence) == VK_SUCCESS;
  }

  bool wait(std::chrono::steady_clock::time_point deadline) const override {
    if (!m_raising) return false;
    const auto left = std::chrono::duration_cast<std::chrono::nanoseconds>(
        deadline - std::chrono::steady_clock::now());
    const uint64_t timeout =
        static_cast<uint64_t>(std::max<int64_t>(left.count(), 0));
    return m_calls.waitForFences(m_device, 1, &m_fence, VK_TRUE, timeout) ==
           VK_SUCCESS;
  }

 private:
  VkDevice m_device;
  VkQueue m_queue;
  VulkanCalls m_calls;
  VkSemaphore m_semaphore;
  VkFence m_fence;
  skgpu::graphite::BackendSemaphore m_backendSemaphore;
  bool m_attached = false;
  bool m_raising = false;
};

}  // namespace

std::unique_ptr<DeviceSignal::Form> vulkanSignal(void* device, void* queue,
                                                 void* getDeviceProcAddr) {
  if (!device || !queue || !getDeviceProcAddr) return nullptr;
  const auto vulkanDevice = static_cast<VkDevice>(device);
  const auto resolve =
      reinterpret_cast<PFN_vkGetDeviceProcAddr>(getDeviceProcAddr);
  VulkanCalls calls;
  calls.createSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
      resolve(vulkanDevice, "vkCreateSemaphore"));
  calls.destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
      resolve(vulkanDevice, "vkDestroySemaphore"));
  calls.createFence = reinterpret_cast<PFN_vkCreateFence>(
      resolve(vulkanDevice, "vkCreateFence"));
  calls.destroyFence = reinterpret_cast<PFN_vkDestroyFence>(
      resolve(vulkanDevice, "vkDestroyFence"));
  calls.queueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
      resolve(vulkanDevice, "vkQueueSubmit"));
  calls.waitForFences = reinterpret_cast<PFN_vkWaitForFences>(
      resolve(vulkanDevice, "vkWaitForFences"));
  if (!calls.complete()) return nullptr;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkSemaphore semaphore = VK_NULL_HANDLE;
  if (calls.createSemaphore(vulkanDevice, &semaphoreInfo, nullptr,
                            &semaphore) != VK_SUCCESS)
    return nullptr;
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence = VK_NULL_HANDLE;
  if (calls.createFence(vulkanDevice, &fenceInfo, nullptr, &fence) !=
      VK_SUCCESS) {
    calls.destroySemaphore(vulkanDevice, semaphore, nullptr);
    return nullptr;
  }
  return std::make_unique<VulkanSignal>(
      vulkanDevice, static_cast<VkQueue>(queue), calls, semaphore, fence);
}

}  // namespace sigil::skia

#else  // !SK_VULKAN

namespace sigil::skia {

// A Skia without the Vulkan backend has no Vulkan recorder to signal for.
std::unique_ptr<DeviceSignal::Form> vulkanSignal(void*, void*, void*) {
  return nullptr;
}

}  // namespace sigil::skia

#endif
