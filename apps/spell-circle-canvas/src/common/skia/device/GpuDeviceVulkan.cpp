// The Vulkan backend of GpuDevice: images as VkImage with their memory,
// fences as timeline semaphores, both signalled and waited on through
// the device's one queue. Every entry point is resolved at run time from
// the loader — found through the platform's library search, or the
// host's own vkGetInstanceProcAddr for an adopted device — so the
// library links no Vulkan and a machine without a driver simply reports
// that. Compiles everywhere; the Vulkan headers are the only build-time
// need.

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

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace sigil::skia {

namespace {

// ---------------------------------------------------------------------------
// The loader: the few entry points this backend needs, resolved through
// vkGetInstanceProcAddr and, for device-level calls, vkGetDeviceProcAddr.

using GetInstanceProcAddr = VulkanHandles::GetInstanceProcAddr;

/** Opens the Vulkan loader library and hands back its
 *  vkGetInstanceProcAddr; null with a reason when none is found. */
GetInstanceProcAddr openLoader(std::string* error) {
  static GetInstanceProcAddr cached = nullptr;
  if (cached) return cached;
  const char* candidates[] = {
      std::getenv("SIGILSKIA_VULKAN_LIBRARY"),
#if defined(_WIN32)
      "vulkan-1.dll",
#elif defined(__APPLE__)
      "libvulkan.dylib",
      "libvulkan.1.dylib",
      "/opt/homebrew/lib/libvulkan.dylib",
      "/opt/homebrew/lib/libvulkan.1.dylib",
      "/usr/local/lib/libvulkan.dylib",
      "/opt/homebrew/lib/libMoltenVK.dylib",
      "/usr/local/lib/libMoltenVK.dylib",
      "libMoltenVK.dylib",
#else
      "libvulkan.so.1",
      "libvulkan.so",
#endif
  };
  for (const char* candidate : candidates) {
    if (!candidate) continue;
#if defined(_WIN32)
    HMODULE module = LoadLibraryA(candidate);
    if (!module) continue;
    auto proc = reinterpret_cast<GetInstanceProcAddr>(
        GetProcAddress(module, "vkGetInstanceProcAddr"));
#else
    void* module = dlopen(candidate, RTLD_NOW | RTLD_LOCAL);
    if (!module) continue;
    auto proc = reinterpret_cast<GetInstanceProcAddr>(
        dlsym(module, "vkGetInstanceProcAddr"));
#endif
    if (!proc) continue;
#if defined(__APPLE__)
    // The Homebrew loader discovers the MoltenVK driver through its own
    // sysconfdir; a process running with a stripped environment is told
    // where that is. A caller's own configuration is never overridden.
    setenv("VK_DRIVER_FILES",
           "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json",
           /*overwrite=*/0);
    setenv("VK_ICD_FILENAMES",
           "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json",
           /*overwrite=*/0);
#endif
    cached = proc;
    return cached;
  }
  if (error)
    *error =
        "no Vulkan loader library (on macOS: brew install molten-vk "
        "vulkan-loader; or point SIGILSKIA_VULKAN_LIBRARY at one)";
  return nullptr;
}

/** Every Vulkan call the backend makes, as resolved function pointers. */
struct Api {
  GetInstanceProcAddr getInstanceProcAddr = nullptr;
  PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
  // Global.
  PFN_vkCreateInstance createInstance = nullptr;
  PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensions =
      nullptr;
  // Instance.
  PFN_vkDestroyInstance destroyInstance = nullptr;
  PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices = nullptr;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilyProperties =
      nullptr;
  PFN_vkGetPhysicalDeviceMemoryProperties getMemoryProperties = nullptr;
  PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties = nullptr;
  PFN_vkGetPhysicalDeviceFeatures2 getPhysicalDeviceFeatures2 = nullptr;
  PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensions = nullptr;
  PFN_vkCreateDevice createDevice = nullptr;
  // Device.
  PFN_vkDestroyDevice destroyDevice = nullptr;
  PFN_vkGetDeviceQueue getDeviceQueue = nullptr;
  PFN_vkDeviceWaitIdle deviceWaitIdle = nullptr;
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

  bool loadGlobal(GetInstanceProcAddr gipa) {
    getInstanceProcAddr = gipa;
    createInstance =
        instanceProc<PFN_vkCreateInstance>(gipa, nullptr, "vkCreateInstance");
    enumerateInstanceExtensions =
        instanceProc<PFN_vkEnumerateInstanceExtensionProperties>(
            gipa, nullptr, "vkEnumerateInstanceExtensionProperties");
    return createInstance != nullptr;
  }

  bool loadInstance(VkInstance instance) {
    auto gipa = getInstanceProcAddr;
#define SIGILSKIA_VK_INSTANCE(field, name) \
  field = instanceProc<decltype(field)>(gipa, instance, name)
    SIGILSKIA_VK_INSTANCE(getDeviceProcAddr, "vkGetDeviceProcAddr");
    SIGILSKIA_VK_INSTANCE(destroyInstance, "vkDestroyInstance");
    SIGILSKIA_VK_INSTANCE(enumeratePhysicalDevices,
                          "vkEnumeratePhysicalDevices");
    SIGILSKIA_VK_INSTANCE(getQueueFamilyProperties,
                          "vkGetPhysicalDeviceQueueFamilyProperties");
    SIGILSKIA_VK_INSTANCE(getMemoryProperties,
                          "vkGetPhysicalDeviceMemoryProperties");
    SIGILSKIA_VK_INSTANCE(getPhysicalDeviceProperties,
                          "vkGetPhysicalDeviceProperties");
    SIGILSKIA_VK_INSTANCE(getPhysicalDeviceFeatures2,
                          "vkGetPhysicalDeviceFeatures2");
    SIGILSKIA_VK_INSTANCE(enumerateDeviceExtensions,
                          "vkEnumerateDeviceExtensionProperties");
    SIGILSKIA_VK_INSTANCE(createDevice, "vkCreateDevice");
#undef SIGILSKIA_VK_INSTANCE
    return getDeviceProcAddr && enumeratePhysicalDevices &&
           getQueueFamilyProperties && getMemoryProperties && createDevice;
  }

  bool loadDevice(VkDevice device) {
#define SIGILSKIA_VK_DEVICE(field, name) \
  field = reinterpret_cast<decltype(field)>(getDeviceProcAddr(device, name))
    SIGILSKIA_VK_DEVICE(destroyDevice, "vkDestroyDevice");
    SIGILSKIA_VK_DEVICE(getDeviceQueue, "vkGetDeviceQueue");
    SIGILSKIA_VK_DEVICE(deviceWaitIdle, "vkDeviceWaitIdle");
    SIGILSKIA_VK_DEVICE(queueSubmit, "vkQueueSubmit");
    SIGILSKIA_VK_DEVICE(createImage, "vkCreateImage");
    SIGILSKIA_VK_DEVICE(destroyImage, "vkDestroyImage");
    SIGILSKIA_VK_DEVICE(getImageMemoryRequirements,
                        "vkGetImageMemoryRequirements");
    SIGILSKIA_VK_DEVICE(allocateMemory, "vkAllocateMemory");
    SIGILSKIA_VK_DEVICE(freeMemory, "vkFreeMemory");
    SIGILSKIA_VK_DEVICE(bindImageMemory, "vkBindImageMemory");
    SIGILSKIA_VK_DEVICE(createSemaphore, "vkCreateSemaphore");
    SIGILSKIA_VK_DEVICE(destroySemaphore, "vkDestroySemaphore");
    SIGILSKIA_VK_DEVICE(waitSemaphores, "vkWaitSemaphores");
    SIGILSKIA_VK_DEVICE(getSemaphoreCounterValue, "vkGetSemaphoreCounterValue");
#undef SIGILSKIA_VK_DEVICE
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
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
      VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (has(usage, TextureUsage::ShaderWrite))
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  return flags;
}

bool hasExtension(const std::vector<VkExtensionProperties>& available,
                  const char* name) {
  for (const VkExtensionProperties& extension : available)
    if (std::strcmp(extension.extensionName, name) == 0) return true;
  return false;
}

template <typename T>
uint64_t toHandle(T object) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}
template <typename T>
T fromHandle(uint64_t value) {
  return reinterpret_cast<T>(static_cast<uintptr_t>(value));
}

// ---------------------------------------------------------------------------

class VulkanBackend final : public GpuDevice::Backend_ {
 public:
  VulkanBackend(Api api, NativeDevice native, bool owned)
      : m_api(api), m_native(native), m_owned(owned) {
    m_api.getMemoryProperties(physicalDevice(), &m_memory);
  }

  ~VulkanBackend() override {
    if (!m_owned) return;
    // Everything the device still references has been released by the
    // owner; the queue is drained before the objects under it go.
    if (m_api.deviceWaitIdle) m_api.deviceWaitIdle(device());
    if (m_api.destroyDevice) m_api.destroyDevice(device(), nullptr);
    if (m_api.destroyInstance) m_api.destroyInstance(instance(), nullptr);
  }

  const NativeDevice& native() const override { return m_native; }

  NativeTexture createTexture(const TextureDesc& desc) override {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = toVulkan(desc.format);
    info.extent = {static_cast<uint32_t>(desc.width),
                   static_cast<uint32_t>(desc.height), 1};
    info.mipLevels = 1;
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
  bool m_owned;
  VkPhysicalDeviceMemoryProperties m_memory{};
};

/** Instance, physical device, device and queue of this library's own. */
bool createOwnedHandles(Api& api, NativeDevice& native, std::string* error) {
  // Instance: 1.2 for timeline semaphores in core, and on Apple the
  // portability enumeration that lets MoltenVK show up at all.
  std::vector<const char*> instanceExtensions;
  VkInstanceCreateFlags instanceFlags = 0;
  if (api.enumerateInstanceExtensions) {
    uint32_t count = 0;
    api.enumerateInstanceExtensions(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    api.enumerateInstanceExtensions(nullptr, &count, available.data());
    if (hasExtension(available, "VK_KHR_portability_enumeration")) {
      instanceExtensions.push_back("VK_KHR_portability_enumeration");
      instanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    if (hasExtension(available, "VK_KHR_get_physical_device_properties2"))
      instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");
  }
  VkApplicationInfo application{};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.pApplicationName = "SigilSkia";
  application.apiVersion = VK_API_VERSION_1_2;
  VkInstanceCreateInfo instanceInfo{};
  instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceInfo.flags = instanceFlags;
  instanceInfo.pApplicationInfo = &application;
  instanceInfo.enabledExtensionCount =
      static_cast<uint32_t>(instanceExtensions.size());
  instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();
  VkInstance instance = VK_NULL_HANDLE;
  if (api.createInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
    if (error) *error = "vkCreateInstance failed (no Vulkan driver?)";
    return false;
  }
  if (!api.loadInstance(instance)) {
    if (error) *error = "the loader lacks instance entry points";
    return false;
  }

  // The first physical device with a graphics queue.
  uint32_t deviceCount = 0;
  api.enumeratePhysicalDevices(instance, &deviceCount, nullptr);
  std::vector<VkPhysicalDevice> devices(deviceCount);
  api.enumeratePhysicalDevices(instance, &deviceCount, devices.data());
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  uint32_t family = 0;
  for (VkPhysicalDevice candidate : devices) {
    uint32_t familyCount = 0;
    api.getQueueFamilyProperties(candidate, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    api.getQueueFamilyProperties(candidate, &familyCount, families.data());
    for (uint32_t i = 0; i < familyCount; ++i) {
      if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        physical = candidate;
        family = i;
        break;
      }
    }
    if (physical) break;
  }
  if (!physical) {
    if (error) *error = "no Vulkan physical device with a graphics queue";
    api.destroyInstance(instance, nullptr);
    return false;
  }

  // The device: timeline semaphores are what fences are built on, so a
  // driver without them is refused; the portability subset is enabled
  // where the driver requires it (MoltenVK).
  VkPhysicalDeviceVulkan12Features features12{};
  features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &features12;
  if (api.getPhysicalDeviceFeatures2)
    api.getPhysicalDeviceFeatures2(physical, &features);
  if (!features12.timelineSemaphore) {
    if (error) *error = "the Vulkan device has no timeline semaphores";
    api.destroyInstance(instance, nullptr);
    return false;
  }
  VkPhysicalDeviceVulkan12Features enable12{};
  enable12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  enable12.timelineSemaphore = VK_TRUE;
  VkPhysicalDeviceFeatures2 enable{};
  enable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  enable.pNext = &enable12;

  std::vector<const char*> deviceExtensions;
  if (api.enumerateDeviceExtensions) {
    uint32_t count = 0;
    api.enumerateDeviceExtensions(physical, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    api.enumerateDeviceExtensions(physical, nullptr, &count, available.data());
    if (hasExtension(available, "VK_KHR_portability_subset"))
      deviceExtensions.push_back("VK_KHR_portability_subset");
  }
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queueInfo{};
  queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfo.queueFamilyIndex = family;
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = &priority;
  VkDeviceCreateInfo deviceInfo{};
  deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceInfo.pNext = &enable;
  deviceInfo.queueCreateInfoCount = 1;
  deviceInfo.pQueueCreateInfos = &queueInfo;
  deviceInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
  VkDevice device = VK_NULL_HANDLE;
  if (api.createDevice(physical, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
    if (error) *error = "vkCreateDevice failed";
    api.destroyInstance(instance, nullptr);
    return false;
  }
  if (!api.loadDevice(device)) {
    if (error) *error = "the loader lacks device entry points";
    api.destroyInstance(instance, nullptr);
    return false;
  }
  VkQueue queue = VK_NULL_HANDLE;
  api.getDeviceQueue(device, family, 0, &queue);

  VkPhysicalDeviceProperties properties{};
  if (api.getPhysicalDeviceProperties)
    api.getPhysicalDeviceProperties(physical, &properties);

  native.vulkan.instance = instance;
  native.vulkan.physicalDevice = physical;
  native.vulkan.device = device;
  native.vulkan.queue = queue;
  native.vulkan.queueFamilyIndex = family;
  native.vulkan.apiVersion =
      properties.apiVersion
          ? std::min(properties.apiVersion,
                     static_cast<uint32_t>(VK_API_VERSION_1_2))
          : VK_API_VERSION_1_2;
  native.vulkan.getInstanceProcAddr = api.getInstanceProcAddr;
  return true;
}

}  // namespace

std::unique_ptr<GpuDevice::Backend_> createVulkanBackend(
    const NativeDevice& native, bool owned, std::string* error) {
  NativeDevice resolved = native;
  resolved.backend = Backend::Vulkan;
  Api api;
  if (owned) {
    GetInstanceProcAddr gipa = openLoader(error);
    if (!gipa || !api.loadGlobal(gipa)) {
      if (error && error->empty()) *error = "the loader lacks vkCreateInstance";
      return nullptr;
    }
    if (!createOwnedHandles(api, resolved, error)) return nullptr;
  } else {
    const VulkanHandles& handles = native.vulkan;
    if (!handles.instance || !handles.physicalDevice || !handles.device ||
        !handles.queue) {
      if (error)
        *error =
            "a Vulkan instance, physical device, device and queue are all "
            "required";
      return nullptr;
    }
    GetInstanceProcAddr gipa = handles.getInstanceProcAddr;
    if (!gipa) gipa = openLoader(error);
    if (!gipa) return nullptr;
    api.loadGlobal(gipa);
    if (!api.loadInstance(static_cast<VkInstance>(handles.instance)) ||
        !api.loadDevice(static_cast<VkDevice>(handles.device))) {
      if (error) *error = "the loader lacks the entry points this device needs";
      return nullptr;
    }
    resolved.vulkan.getInstanceProcAddr = gipa;
    if (!resolved.vulkan.apiVersion)
      resolved.vulkan.apiVersion = VK_API_VERSION_1_2;
  }
  return std::make_unique<VulkanBackend>(api, resolved, owned);
}

}  // namespace sigil::skia
