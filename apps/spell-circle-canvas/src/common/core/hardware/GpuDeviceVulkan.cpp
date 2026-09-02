// The Vulkan backend of GpuDevice: images as VkImage with their memory,
// fences as timeline semaphores, both signalled and waited on through
// the device's one queue. Every entry point is resolved at run time from
// the host's own vkGetInstanceProcAddr, so the library links no Vulkan
// and a machine without a driver simply reports that. Compiles
// everywhere; the Vulkan headers are the only build-time need.
//
// A VULKAN DEVICE IS ONLY EVER ADOPTED HERE. Whoever owns the Vulkan API
// in a process creates the device — a renderer that cannot attach to one
// someone else made has no choice about that — and hands over instance,
// physical device, device, queue and its own loader entry point. Two
// instances in one process would mean two loaders, two queues and a copy
// between them, which is exactly what adopting exists to avoid.

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "DeviceBackend.h"

namespace sigil::core::hardware {

namespace {

using GetInstanceProcAddr = VulkanHandles::GetInstanceProcAddr;

/** Every Vulkan call the backend makes, as resolved function pointers. */
struct Api {
  GetInstanceProcAddr getInstanceProcAddr = nullptr;
  PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
  // Instance.
  PFN_vkGetPhysicalDeviceMemoryProperties getMemoryProperties = nullptr;
  // Device.
  PFN_vkQueueSubmit queueSubmit = nullptr;
  PFN_vkCreateImage createImage = nullptr;
  PFN_vkDestroyImage destroyImage = nullptr;
  PFN_vkGetImageMemoryRequirements getImageMemoryRequirements = nullptr;
  PFN_vkAllocateMemory allocateMemory = nullptr;
  PFN_vkFreeMemory freeMemory = nullptr;
  PFN_vkBindImageMemory bindImageMemory = nullptr;
  PFN_vkCreateSemaphore createSemaphore = nullptr;
  PFN_vkDestroySemaphore destroySemaphore = nullptr;
  PFN_vkWaitSemaphores waitSemaphores = nullptr;
  PFN_vkGetSemaphoreCounterValue getSemaphoreCounterValue = nullptr;

  template <typename Fn>
  static Fn instanceProc(GetInstanceProcAddr gipa, VkInstance instance,
                         const char* name) {
    return reinterpret_cast<Fn>(gipa(instance, name));
  }

  bool loadInstance(VkInstance instance) {
    auto gipa = getInstanceProcAddr;
#define SIGIL_VK_INSTANCE(field, name) \
  field = instanceProc<decltype(field)>(gipa, instance, name)
    SIGIL_VK_INSTANCE(getDeviceProcAddr, "vkGetDeviceProcAddr");
    SIGIL_VK_INSTANCE(getMemoryProperties,
                          "vkGetPhysicalDeviceMemoryProperties");
#undef SIGIL_VK_INSTANCE
    return getDeviceProcAddr && getMemoryProperties;
  }

  bool loadDevice(VkDevice device) {
#define SIGIL_VK_DEVICE(field, name) \
  field = reinterpret_cast<decltype(field)>(getDeviceProcAddr(device, name))
    SIGIL_VK_DEVICE(queueSubmit, "vkQueueSubmit");
    SIGIL_VK_DEVICE(createImage, "vkCreateImage");
    SIGIL_VK_DEVICE(destroyImage, "vkDestroyImage");
    SIGIL_VK_DEVICE(getImageMemoryRequirements,
                        "vkGetImageMemoryRequirements");
    SIGIL_VK_DEVICE(allocateMemory, "vkAllocateMemory");
    SIGIL_VK_DEVICE(freeMemory, "vkFreeMemory");
    SIGIL_VK_DEVICE(bindImageMemory, "vkBindImageMemory");
    SIGIL_VK_DEVICE(createSemaphore, "vkCreateSemaphore");
    SIGIL_VK_DEVICE(destroySemaphore, "vkDestroySemaphore");
    SIGIL_VK_DEVICE(waitSemaphores, "vkWaitSemaphores");
    SIGIL_VK_DEVICE(getSemaphoreCounterValue, "vkGetSemaphoreCounterValue");
#undef SIGIL_VK_DEVICE
    return queueSubmit && createImage && destroyImage && allocateMemory &&
           freeMemory && bindImageMemory && createSemaphore &&
           destroySemaphore && waitSemaphores && getSemaphoreCounterValue;
  }
};

// ---------------------------------------------------------------------------
// Format and usage mapping.

VkFormat toVulkan(TextureFormat format) {
  switch (format) {
    case TextureFormat::RGBA8Unorm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::BGRA8Unorm:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::RGBA16Float:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  return VK_FORMAT_R8G8B8A8_UNORM;
}

/** Every image can be sampled, drawn into and copied both ways — the
 *  set a Graphite wrap promises — with storage added for ShaderWrite. */
VkImageUsageFlags toVulkan(TextureUsage usage) {
  VkImageUsageFlags flags =
      static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
      static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
      static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) |
      static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
      static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  if (has(usage, TextureUsage::ShaderWrite))
    flags |= static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT);
  return flags;
}

template <typename T>
uint64_t toHandle(T object) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}
template <typename T>
T fromHandle(uint64_t value) {
  // A NativeTexture carries Vulkan objects as the 64-bit words the API
  // itself defines them to be, so the dispatchable ones make the trip
  // back through a pointer.
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  return reinterpret_cast<T>(static_cast<uintptr_t>(value));
}

// ---------------------------------------------------------------------------

class VulkanBackend final : public GpuDevice::Backend_ {
 public:
  VulkanBackend(Api api, NativeDevice native)
      : m_api(api), m_native(native) {
    m_api.getMemoryProperties(physicalDevice(), &m_memory);
  }

  // Nothing is torn down: every Vulkan object behind an adopted device
  // is the host's, and it outlives this.
  ~VulkanBackend() override = default;

  const NativeDevice& native() const override { return m_native; }

  NativeTexture createTexture(const TextureDesc& desc) override {
    // Vulkan's create-info structs are zero-filled and then written field
    // by field; every field this call depends on, `samples` included, is
    // given its value below.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = toVulkan(desc.format);
    info.extent = {static_cast<uint32_t>(desc.width),
                   static_cast<uint32_t>(desc.height), 1};
    info.mipLevels =
        static_cast<uint32_t>(clampedMipLevels(desc));
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = toVulkan(desc.usage);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    if (m_api.createImage(device(), &info, nullptr, &image) != VK_SUCCESS)
      return {};

    VkMemoryRequirements requirements{};
    m_api.getImageMemoryRequirements(device(), image, &requirements);
    // CPU-accessible asks for host-visible, coherent memory when a type
    // offers it for this image and falls back to device-local otherwise;
    // the image keeps optimal tiling either way, which is what a Graphite
    // wrap expects.
    const VkMemoryPropertyFlags wanted =
        desc.cpuAccessible ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                           : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    int typeIndex = memoryType(requirements.memoryTypeBits, wanted);
    if (typeIndex < 0) typeIndex = memoryType(requirements.memoryTypeBits, 0);
    if (typeIndex < 0) {
      m_api.destroyImage(device(), image, nullptr);
      return {};
    }
    VkMemoryAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = static_cast<uint32_t>(typeIndex);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (m_api.allocateMemory(device(), &allocate, nullptr, &memory) !=
            VK_SUCCESS ||
        m_api.bindImageMemory(device(), image, memory, 0) != VK_SUCCESS) {
      if (memory) m_api.freeMemory(device(), memory, nullptr);
      m_api.destroyImage(device(), image, nullptr);
      return {};
    }

    NativeTexture out;
    out.backend = Backend::Vulkan;
    out.vkImage = toHandle(image);
    out.vkMemory = toHandle(memory);
    out.vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    out.vkFormat = info.format;
    out.width = desc.width;
    out.height = desc.height;
    out.mipLevels = static_cast<int>(info.mipLevels);
    return out;
  }

  // Vulkan objects carry no reference count: ownership is who calls
  // destroy, so retaining is nothing and releasing is the destroy.
  void retainTexture(const NativeTexture&) override {}

  void releaseTexture(const NativeTexture& texture) override {
    if (texture.vkImage)
      m_api.destroyImage(device(), fromHandle<VkImage>(texture.vkImage),
                         nullptr);
    if (texture.vkMemory)
      m_api.freeMemory(device(), fromHandle<VkDeviceMemory>(texture.vkMemory),
                       nullptr);
  }

  void* createFence() override {
    VkSemaphoreTypeCreateInfo type{};
    type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type.initialValue = kFenceInitialValue;
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = &type;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    if (m_api.createSemaphore(device(), &info, nullptr, &semaphore) !=
        VK_SUCCESS)
      return nullptr;
    return fromHandle<void*>(toHandle(semaphore));
  }

  void destroyFence(void* fence) override {
    m_api.destroySemaphore(device(), semaphore(fence), nullptr);
  }

  void signalFence(void* fence, FenceValue value) override {
    submitTimeline(fence, value, /*signal=*/true);
  }

  void waitFenceGpu(void* fence, FenceValue value) override {
    submitTimeline(fence, value, /*signal=*/false);
  }

  bool waitFenceCpu(void* fence, FenceValue value,
                    std::chrono::milliseconds timeout) override {
    VkSemaphore handle = semaphore(fence);
    VkSemaphoreWaitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    info.semaphoreCount = 1;
    info.pSemaphores = &handle;
    info.pValues = &value;
    const auto nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count();
    return m_api.waitSemaphores(device(), &info,
                                static_cast<uint64_t>(nanoseconds)) ==
           VK_SUCCESS;
  }

  FenceValue completedValue(void* fence) override {
    uint64_t value = kFenceInitialValue;
    m_api.getSemaphoreCounterValue(device(), semaphore(fence), &value);
    return value;
  }

 private:
  VkInstance instance() const {
    return static_cast<VkInstance>(m_native.vulkan.instance);
  }
  VkPhysicalDevice physicalDevice() const {
    return static_cast<VkPhysicalDevice>(m_native.vulkan.physicalDevice);
  }
  VkDevice device() const {
    return static_cast<VkDevice>(m_native.vulkan.device);
  }
  VkQueue queue() const { return static_cast<VkQueue>(m_native.vulkan.queue); }
  static VkSemaphore semaphore(void* fence) {
    return fromHandle<VkSemaphore>(toHandle(fence));
  }

  int memoryType(uint32_t allowed, VkMemoryPropertyFlags wanted) const {
    for (uint32_t i = 0; i < m_memory.memoryTypeCount; ++i)
      if ((allowed & (1u << i)) &&
          (m_memory.memoryTypes[i].propertyFlags & wanted) == wanted)
        return static_cast<int>(i);
    return -1;
  }

  /** An empty submission that signals or waits on the timeline: queued
   *  behind everything submitted so far, ahead of everything after. */
  void submitTimeline(void* fence, FenceValue value, bool signal) {
    VkSemaphore handle = semaphore(fence);
    VkTimelineSemaphoreSubmitInfo timeline{};
    timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = &timeline;
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    if (signal) {
      timeline.signalSemaphoreValueCount = 1;
      timeline.pSignalSemaphoreValues = &value;
      submit.signalSemaphoreCount = 1;
      submit.pSignalSemaphores = &handle;
    } else {
      timeline.waitSemaphoreValueCount = 1;
      timeline.pWaitSemaphoreValues = &value;
      submit.waitSemaphoreCount = 1;
      submit.pWaitSemaphores = &handle;
      submit.pWaitDstStageMask = &stage;
    }
    m_api.queueSubmit(queue(), 1, &submit, VK_NULL_HANDLE);
  }

  Api m_api;
  NativeDevice m_native;
  VkPhysicalDeviceMemoryProperties m_memory{};
};

}  // namespace

std::unique_ptr<GpuDevice::Backend_> createVulkanBackend(
    const NativeDevice& native, std::string* error) {
  NativeDevice resolved = native;
  resolved.backend = Backend::Vulkan;
  const VulkanHandles& handles = native.vulkan;
  if (!handles.instance || !handles.physicalDevice || !handles.device ||
      !handles.queue) {
    if (error)
      *error =
          "a Vulkan instance, physical device, device and queue are all "
          "required";
    return nullptr;
  }
  // The HOST'S loader, always: it already opened one, and dispatching
  // through a second copy of the same library is what makes two APIs
  // stop being one device.
  if (!handles.getInstanceProcAddr) {
    if (error)
      *error = "a Vulkan device is adopted with the loader that made it";
    return nullptr;
  }
  Api api;
  api.getInstanceProcAddr = handles.getInstanceProcAddr;
  if (!api.loadInstance(static_cast<VkInstance>(handles.instance)) ||
      !api.loadDevice(static_cast<VkDevice>(handles.device))) {
    if (error) *error = "the loader lacks the entry points this device needs";
    return nullptr;
  }
  if (!resolved.vulkan.apiVersion)
    resolved.vulkan.apiVersion = VK_API_VERSION_1_2;
  return std::make_unique<VulkanBackend>(api, resolved);
}

}  // namespace sigil::core::hardware
