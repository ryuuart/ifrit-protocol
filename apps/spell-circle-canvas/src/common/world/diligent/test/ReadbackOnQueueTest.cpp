/** @file
 * A read back from Graphite on the device's own Vulkan queue waits for the
 * device and no longer: held behind work that waits on a semaphore only
 * the host raises, the read stays out while the host holds it, and
 * returns the pixels once the host lets the work run. The wait the read
 * sleeps on is a fence the device raises, so a read that returned early
 * would answer pixels the device had not drawn.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendSemaphore.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/vk/VulkanGraphiteTypes.h>
#include <sigilgpu/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>
#include <vulkan/vulkan_core.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "DeviceSeams.h"

using namespace sigil;

namespace {

/** The entry points the case calls on the device. */
struct HostCalls {
  PFN_vkCreateSemaphore createSemaphore = nullptr;
  PFN_vkDestroySemaphore destroySemaphore = nullptr;
  PFN_vkQueueSubmit queueSubmit = nullptr;
  PFN_vkQueueWaitIdle queueWaitIdle = nullptr;
  PFN_vkSignalSemaphore signalSemaphore = nullptr;

  bool complete() const {
    return createSemaphore && destroySemaphore && queueSubmit &&
           queueWaitIdle && signalSemaphore;
  }
};

HostCalls hostCalls(const gpu::VulkanHandles& handles) {
  HostCalls calls;
  const auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      handles.getInstanceProcAddr(handles.instance, "vkGetDeviceProcAddr"));
  if (!getDeviceProcAddr) return calls;
  const auto device = static_cast<VkDevice>(handles.device);
  const auto resolve = [&](const char* name) {
    return getDeviceProcAddr(device, name);
  };
  calls.createSemaphore =
      reinterpret_cast<PFN_vkCreateSemaphore>(resolve("vkCreateSemaphore"));
  calls.destroySemaphore =
      reinterpret_cast<PFN_vkDestroySemaphore>(resolve("vkDestroySemaphore"));
  calls.queueSubmit =
      reinterpret_cast<PFN_vkQueueSubmit>(resolve("vkQueueSubmit"));
  calls.queueWaitIdle =
      reinterpret_cast<PFN_vkQueueWaitIdle>(resolve("vkQueueWaitIdle"));
  calls.signalSemaphore =
      reinterpret_cast<PFN_vkSignalSemaphore>(resolve("vkSignalSemaphore"));
  if (!calls.signalSemaphore)
    calls.signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(
        resolve("vkSignalSemaphoreKHR"));
  return calls;
}

}  // namespace

TEST(ReadbackOnQueue, AVulkanReadReturnsOnlyOnceTheHostLetsTheDeviceRun) {
  const auto on = world::diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  skia::GraphiteContext* own = world::diligent::vulkanGraphite(*on.device);
  if (!own) GTEST_SKIP() << "the device carries no Graphite on its own queue";
  const gpu::NativeDevice& native = on.device->gpu()->native();
  ASSERT_EQ(native.backend, gpu::Backend::Vulkan);
  const gpu::VulkanHandles& handles = native.vulkan;
  const HostCalls calls = hostCalls(handles);
  if (!calls.complete()) GTEST_SKIP() << "the device answers no host signal";
  const auto device = static_cast<VkDevice>(handles.device);
  const auto queue = static_cast<VkQueue>(handles.queue);

  // THE HOST'S HOLD: a timeline semaphore only the host raises, and a
  // submit that waits on it before it signals the semaphore the drawing
  // waits on.
  VkSemaphoreTypeCreateInfo timeline{};
  timeline.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  timeline.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  VkSemaphoreCreateInfo held{};
  held.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  held.pNext = &timeline;
  VkSemaphore hostRaised = VK_NULL_HANDLE;
  if (calls.createSemaphore(device, &held, nullptr, &hostRaised) != VK_SUCCESS)
    GTEST_SKIP() << "the device makes no timeline semaphore";
  VkSemaphoreCreateInfo binary{};
  binary.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkSemaphore released = VK_NULL_HANDLE;
  ASSERT_EQ(calls.createSemaphore(device, &binary, nullptr, &released),
            VK_SUCCESS);
  {
    geometry::device::Device::QueueLock lock(*on.device);
    const uint64_t raisedValue = 1;
    const uint64_t binaryValue = 0;
    VkTimelineSemaphoreSubmitInfo values{};
    values.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    values.waitSemaphoreValueCount = 1;
    values.pWaitSemaphoreValues = &raisedValue;
    values.signalSemaphoreValueCount = 1;
    values.pSignalSemaphoreValues = &binaryValue;
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = &values;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &hostRaised;
    submit.pWaitDstStageMask = &stage;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &released;
    ASSERT_EQ(calls.queueSubmit(queue, 1, &submit, VK_NULL_HANDLE), VK_SUCCESS);
  }

  // THE DRAWING, submitted behind the hold.
  const SkImageInfo info = SkImageInfo::MakeN32Premul(4, 4);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(own->recorder(), info);
  ASSERT_NE(surface, nullptr);
  surface->getCanvas()->clear(SK_ColorBLUE);
  {
    geometry::device::Device::QueueLock lock(*on.device);
    std::unique_ptr<skgpu::graphite::Recording> recording =
        own->recorder()->snap();
    ASSERT_NE(recording, nullptr);
    const std::unique_lock<std::mutex> context = own->lockContext();
    skgpu::graphite::BackendSemaphore wait =
        skgpu::graphite::BackendSemaphores::MakeVulkan(released);
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    insert.fNumWaitSemaphores = 1;
    insert.fWaitSemaphores = &wait;
    ASSERT_EQ(own->context()->insertRecording(insert),
              skgpu::graphite::InsertStatus::kSuccess);
    ASSERT_TRUE(own->context()->submit(
        skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kNo)));
  }

  SkBitmap pixels;
  pixels.allocPixels(info);
  std::atomic<bool> returned{false};
  bool read = false;
  std::thread reader([&] {
    geometry::device::Device::QueueLock lock(*on.device);
    read = skia::readbackPixels(*own, *surface, pixels.pixmap());
    returned.store(true, std::memory_order_release);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_FALSE(returned.load(std::memory_order_acquire))
      << "the read returned while the device still waited on the host";

  VkSemaphoreSignalInfo raise{};
  raise.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
  raise.semaphore = hostRaised;
  raise.value = 1;
  ASSERT_EQ(calls.signalSemaphore(device, &raise), VK_SUCCESS);
  reader.join();
  EXPECT_TRUE(read);
  EXPECT_EQ(pixels.getColor(0, 0), SK_ColorBLUE);
  EXPECT_EQ(pixels.getColor(3, 3), SK_ColorBLUE);

  {
    geometry::device::Device::QueueLock lock(*on.device);
    calls.queueWaitIdle(queue);
  }
  calls.destroySemaphore(device, released, nullptr);
  calls.destroySemaphore(device, hostRaised, nullptr);
}
