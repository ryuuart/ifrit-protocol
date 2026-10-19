// A Vulkan device whose driver runs over Metal, and the three doors that
// make its objects Metal's: a texture made here answers the Metal texture
// behind it, a Metal texture comes in as an image over its own memory,
// and a Metal event comes in as a fence this device's queue waits on.
// The device is the test's own, brought up from the loader the way a
// renderer that owns the API would, and adopted. Every case SKIPS where
// there is no Vulkan runtime or the driver does not run over Metal.

#import <Metal/Metal.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
// After the core header, whose types it names.
#include <vulkan/vulkan_metal.h>

#include <dlfcn.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilcore/hardware/Handle.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace sigil::core::hardware;

namespace {

/** WHERE THE LOADER IS: the library names a process finds it by, and the
 *  places a Homebrew or a manual install puts it. */
void *openLoader() {
  const char *candidates[] = {
      "libvulkan.1.dylib",
      "libvulkan.dylib",
      "/opt/homebrew/lib/libvulkan.1.dylib",
      "/usr/local/lib/libvulkan.1.dylib",
      "/opt/homebrew/lib/libMoltenVK.dylib",
      "/usr/local/lib/libMoltenVK.dylib",
  };
  for (const char *candidate : candidates)
    if (void *module = dlopen(candidate, RTLD_NOW | RTLD_LOCAL)) return module;
  return nullptr;
}

/** Whether @p name is among @p available. */
template <typename Properties>
bool offers(const std::vector<Properties> &available, const char *name) {
  for (const Properties &each : available)
    if (std::strcmp(each.extensionName, name) == 0) return true;
  return false;
}

/**
 * A VULKAN DEVICE THE TEST OWNS: an instance on the loader, the first
 * physical device, one graphics queue, timeline semaphores and the
 * metal-objects extension — what a renderer owning the API brings up —
 * and the `GpuDevice` that adopts it. The adopted device goes first, then
 * the Vulkan device, then the instance.
 */
class VulkanDevice {
 public:
  /** Null, with the reason in @p why, where any step is refused. */
  static std::unique_ptr<VulkanDevice> bringUp(std::string *why) {
    std::unique_ptr<VulkanDevice> out(new VulkanDevice());
    if (!out->start(why)) return nullptr;
    return out;
  }

  ~VulkanDevice() {
    gpu.reset();
    if (m_device) m_destroyDevice(m_device, nullptr);
    if (m_instance) m_destroyInstance(m_instance, nullptr);
    if (m_loader) dlclose(m_loader);
  }
  VulkanDevice(const VulkanDevice &) = delete;
  VulkanDevice &operator=(const VulkanDevice &) = delete;

  /** The driver's own answer for the Metal texture behind @p image. */
  void *metalTextureOf(uint64_t image) const {
    const auto exportObjects = reinterpret_cast<PFN_vkExportMetalObjectsEXT>(
        m_getDeviceProcAddr(m_device, "vkExportMetalObjectsEXT"));
    VkExportMetalTextureInfoEXT wanted{};
    wanted.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT;
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    wanted.image = reinterpret_cast<VkImage>(image);
    wanted.plane = VK_IMAGE_ASPECT_PLANE_0_BIT;
    VkExportMetalObjectsInfoEXT objects{};
    objects.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
    objects.pNext = &wanted;
    exportObjects(m_device, &objects);
    return (void *)wanted.mtlTexture;
  }

  std::unique_ptr<GpuDevice> gpu;

 private:
  VulkanDevice() = default;

  bool start(std::string *why) {
    // The Homebrew loader finds the driver through its own configuration
    // directory; a process run with a stripped environment is pointed at
    // it, and a caller's own setting is left alone.
    setenv("VK_DRIVER_FILES", "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json",
           /*overwrite=*/0);
    m_loader = openLoader();
    if (!m_loader) return refuse(why, "no Vulkan loader");
    auto *getInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(m_loader, "vkGetInstanceProcAddr"));
    if (!getInstanceProcAddr) return refuse(why, "the loader has no vkGetInstanceProcAddr");

    const auto enumerateInstanceExtensions =
        reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
            getInstanceProcAddr(nullptr, "vkEnumerateInstanceExtensionProperties"));
    const auto createInstance =
        reinterpret_cast<PFN_vkCreateInstance>(getInstanceProcAddr(nullptr, "vkCreateInstance"));
    if (!enumerateInstanceExtensions || !createInstance)
      return refuse(why, "the loader cannot make an instance");
    uint32_t count = 0;
    enumerateInstanceExtensions(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> instanceExtensions(count);
    enumerateInstanceExtensions(nullptr, &count, instanceExtensions.data());

    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = VK_API_VERSION_1_2;
    std::vector<const char *> wantedInstance;
    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &application;
    // A driver that is not conformant on every point lists itself only to
    // an instance that asks for such drivers.
    if (offers(instanceExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
      wantedInstance.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
      instanceInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    instanceInfo.enabledExtensionCount = (uint32_t)wantedInstance.size();
    instanceInfo.ppEnabledExtensionNames = wantedInstance.data();
    if (createInstance(&instanceInfo, nullptr, &m_instance) != VK_SUCCESS)
      return refuse(why, "no Vulkan instance");

#define SIGIL_VK_INSTANCE(type, name) \
  const auto name = reinterpret_cast<PFN_##type>(getInstanceProcAddr(m_instance, #type))
    SIGIL_VK_INSTANCE(vkEnumeratePhysicalDevices, enumeratePhysicalDevices);
    SIGIL_VK_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties, queueFamilies);
    SIGIL_VK_INSTANCE(vkEnumerateDeviceExtensionProperties, enumerateDeviceExtensions);
    SIGIL_VK_INSTANCE(vkCreateDevice, createDevice);
    SIGIL_VK_INSTANCE(vkGetDeviceProcAddr, getDeviceProcAddr);
#undef SIGIL_VK_INSTANCE
    m_destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        getInstanceProcAddr(m_instance, "vkDestroyInstance"));
    m_getDeviceProcAddr = getDeviceProcAddr;

    uint32_t physicalCount = 1;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    const VkResult listed = enumeratePhysicalDevices(m_instance, &physicalCount, &physical);
    if ((listed != VK_SUCCESS && listed != VK_INCOMPLETE) || !physical)
      return refuse(why, "no Vulkan physical device");

    uint32_t familyCount = 0;
    queueFamilies(physical, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    queueFamilies(physical, &familyCount, families.data());
    uint32_t family = UINT32_MAX;
    for (uint32_t i = 0; i < familyCount && family == UINT32_MAX; ++i)
      if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) family = i;
    if (family == UINT32_MAX) return refuse(why, "no graphics queue");

    count = 0;
    enumerateDeviceExtensions(physical, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> deviceExtensions(count);
    enumerateDeviceExtensions(physical, nullptr, &count, deviceExtensions.data());
    if (!offers(deviceExtensions, VK_EXT_METAL_OBJECTS_EXTENSION_NAME))
      return refuse(why, "the driver does not run over Metal");
    std::vector<const char *> wantedDevice = {VK_EXT_METAL_OBJECTS_EXTENSION_NAME};
    // A driver that offers the portability subset must be told it is
    // known about.
    if (offers(deviceExtensions, "VK_KHR_portability_subset"))
      wantedDevice.push_back("VK_KHR_portability_subset");
    if (offers(deviceExtensions, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
      wantedDevice.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline{};
    timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timeline.timelineSemaphore = VK_TRUE;
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &timeline;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = (uint32_t)wantedDevice.size();
    deviceInfo.ppEnabledExtensionNames = wantedDevice.data();
    if (createDevice(physical, &deviceInfo, nullptr, &m_device) != VK_SUCCESS)
      return refuse(why, "no Vulkan device with timeline semaphores over Metal");
    m_destroyDevice =
        reinterpret_cast<PFN_vkDestroyDevice>(getDeviceProcAddr(m_device, "vkDestroyDevice"));
    const auto getQueue =
        reinterpret_cast<PFN_vkGetDeviceQueue>(getDeviceProcAddr(m_device, "vkGetDeviceQueue"));
    VkQueue queue = VK_NULL_HANDLE;
    getQueue(m_device, family, 0, &queue);

    NativeDevice native;
    native.backend = Backend::Vulkan;
    native.vulkan.instance = m_instance;
    native.vulkan.physicalDevice = physical;
    native.vulkan.device = m_device;
    native.vulkan.queue = queue;
    native.vulkan.queueFamilyIndex = family;
    native.vulkan.apiVersion = VK_API_VERSION_1_2;
    native.vulkan.getInstanceProcAddr =
        reinterpret_cast<VulkanHandles::GetInstanceProcAddr>(getInstanceProcAddr);
    std::string error;
    gpu = GpuDevice::adopt(native, &error);
    if (!gpu) return refuse(why, ("the device was not adopted: " + error).c_str());
    return true;
  }

  static bool refuse(std::string *why, const char *reason) {
    *why = reason;
    return false;
  }

  void *m_loader = nullptr;
  VkInstance m_instance = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  PFN_vkDestroyInstance m_destroyInstance = nullptr;
  PFN_vkDestroyDevice m_destroyDevice = nullptr;
  PFN_vkGetDeviceProcAddr m_getDeviceProcAddr = nullptr;
};

/** A Metal texture on @p device, released by the caller. */
id<MTLTexture> metalTexture(id<MTLDevice> device, MTLPixelFormat format, int width, int height,
                            int levels, MTLTextureUsage usage) {
  MTLTextureDescriptor *descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                         width:(NSUInteger)width
                                                        height:(NSUInteger)height
                                                     mipmapped:levels > 1];
  descriptor.mipmapLevelCount = (NSUInteger)levels;
  descriptor.usage = usage;
  descriptor.storageMode = MTLStorageModePrivate;
  return [device newTextureWithDescriptor:descriptor];
}

}  // namespace

/** THE DEVICE AND THE METAL DEVICE UNDER IT, or a skip saying which is
 *  missing. */
class HardwareVulkanOverMetal : public testing::Test {
 protected:
  void SetUp() override {
    std::string why;
    vulkan = VulkanDevice::bringUp(&why);
    if (!vulkan) GTEST_SKIP() << why;
    device = vulkan->gpu.get();
    metal = (id<MTLDevice>)device->native().mtlDevice;
    if (!metal) GTEST_SKIP() << "the driver names no Metal device under it";
  }

  std::unique_ptr<VulkanDevice> vulkan;
  GpuDevice *device = nullptr;
  id<MTLDevice> metal = nil;
};

TEST_F(HardwareVulkanOverMetal, TheDeviceNamesTheMetalDeviceItRunsOnAndNoQueue) {
  EXPECT_EQ(device->backend(), Backend::Vulkan);
  EXPECT_NE(device->native().mtlDevice, nullptr);
  EXPECT_EQ(device->native().mtlCommandQueue, nullptr) << "the driver's queue stays the driver's";
}

// THE EXPORT: a texture made here is also the Metal texture the driver
// keeps behind it, one object however often it is asked for, on the
// device the driver runs on, and nothing once the handle is stale.
TEST_F(HardwareVulkanOverMetal, ATextureMadeHereIsReachedOnMetal) {
  TextureDescription description;
  description.width = 16;
  description.height = 8;
  description.format = TextureFormat::BGRA8Unorm;
  const TextureHandle texture = device->createTexture(description);
  ASSERT_TRUE(device->isValid(texture));
  const NativeTexture native = device->exportNative(texture);
  EXPECT_EQ(native.backend, Backend::Vulkan);
  EXPECT_NE(native.vkImage, 0u);
  ASSERT_NE(native.mtlTexture, nullptr);
  EXPECT_EQ(device->exportNative(texture).mtlTexture, native.mtlTexture);
  id<MTLTexture> behind = (id<MTLTexture>)native.mtlTexture;
  EXPECT_EQ((void *)behind.device, (void *)metal);
  EXPECT_EQ((int)behind.width, 16);
  EXPECT_EQ((int)behind.height, 8);
  EXPECT_EQ(behind.pixelFormat, MTLPixelFormatBGRA8Unorm);
  device->destroy(texture);
  EXPECT_FALSE(device->exportNative(texture)) << "a stale handle";
}

// THE IMPORT: a texture Metal made is named by an image over its very
// memory — the driver, asked for the Metal object behind the image,
// answers that texture — with the size, format and levels the texture
// was made with. The image is the handle's and the texture its maker's,
// whether or not ownership was asked for.
TEST_F(HardwareVulkanOverMetal, AMetalTextureIsNamedByAnImageOverItsOwnMemory) {
  id<MTLTexture> texture = metalTexture(metal, MTLPixelFormatRGBA16Float, 64, 32, 4,
                                        MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget);
  // Released at the end of the test. The one path that skips that
  // release is a failed assertion, which ends the run.
  // NOLINTNEXTLINE(clang-analyzer-osx.cocoa.RetainCount)
  ASSERT_NE(texture, nil);
  const NSUInteger before = CFGetRetainCount((CFTypeRef)texture);

  // Whatever the driver hands back for the caller to let go of is let go
  // of before the counts are read.
  TextureHandle imported;
  @autoreleasepool {
    NativeTexture given;
    given.backend = Backend::Metal;
    given.mtlTexture = (void *)texture;
    imported = device->importNative(given, /*takeOwnership=*/true);
    ASSERT_TRUE(device->isValid(imported));
    const NativeTexture native = device->exportNative(imported);
    EXPECT_EQ(native.backend, Backend::Vulkan);
    EXPECT_NE(native.vkImage, 0u);
    EXPECT_EQ(native.vkMemory, 0u) << "the texture is the memory";
    EXPECT_EQ(native.vkFormat, 97u /*R16G16B16A16_SFLOAT*/);
    EXPECT_EQ(native.width, 64);
    EXPECT_EQ(native.height, 32);
    EXPECT_EQ(native.mipLevels, 4);
    EXPECT_EQ(native.mtlTexture, (void *)texture);
    EXPECT_EQ(vulkan->metalTextureOf(native.vkImage), (void *)texture)
        << "the driver names the very texture behind the image";
  }

  // The image goes at retirement and holds nothing of the texture after
  // it, ownership asked for or not: the texture is as its maker left it.
  device->destroy(imported);
  for (uint64_t frame = 0; frame < GpuDevice::kFramesInFlight; ++frame) device->beginFrame();
  EXPECT_EQ(device->pendingDestroys(), 0u);
  EXPECT_EQ(CFGetRetainCount((CFTypeRef)texture), before) << "the image went, the texture stays";
  CFRelease((CFTypeRef)texture);
}

// A TEXTURE MADE FOR READING ALONE is named for reading alone: the image
// over it asks nothing of it that it was not made for, so the driver
// takes it rather than deriving a usage it lacks.
TEST_F(HardwareVulkanOverMetal, ATextureMadeOnlyToBeReadIsNamedAllTheSame) {
  id<MTLTexture> texture =
      metalTexture(metal, MTLPixelFormatRGBA8Unorm, 8, 8, 1, MTLTextureUsageShaderRead);
  // NOLINTNEXTLINE(clang-analyzer-osx.cocoa.RetainCount)
  ASSERT_NE(texture, nil);
  NativeTexture given;
  given.backend = Backend::Metal;
  given.mtlTexture = (void *)texture;
  const TextureHandle imported = device->importNative(given);
  EXPECT_TRUE(device->isValid(imported));
  EXPECT_EQ(device->exportNative(imported).vkFormat, 37u /*R8G8B8A8_UNORM*/);
  device->destroy(imported);
  CFRelease((CFTypeRef)texture);
}

TEST_F(HardwareVulkanOverMetal, ATextureInAFormatNoTextureHereHasIsRefused) {
  id<MTLTexture> texture =
      metalTexture(metal, MTLPixelFormatR8Unorm, 8, 8, 1, MTLTextureUsageShaderRead);
  // NOLINTNEXTLINE(clang-analyzer-osx.cocoa.RetainCount)
  ASSERT_NE(texture, nil);
  NativeTexture given;
  given.backend = Backend::Metal;
  given.mtlTexture = (void *)texture;
  EXPECT_FALSE(device->importNative(given));
  given.mtlTexture = nullptr;
  EXPECT_FALSE(device->importNative(given)) << "no texture";
  CFRelease((CFTypeRef)texture);
}

// THE FENCE BROUGHT OVER: a wait on an event a queue on Metal raises
// holds everything submitted here after it — a signal of the device's own
// fence among them — until that queue raises the event. Nothing here
// raises it, so the signal standing still is the wait and not a slow
// queue.
TEST_F(HardwareVulkanOverMetal, AMetalEventHoldsThisQueueUntilMetalRaisesIt) {
  // THE EVENT STAYS ITS MAKER'S, proven by its going: once the fence is
  // destroyed and the maker lets go, a weak reference to the event reads
  // nothing. A retain count read here would not prove it, because other
  // processes driving the same Metal device move what the count reads
  // while the claim holds.
  NSHashTable *watched = [NSHashTable weakObjectsHashTable];
  @autoreleasepool {
    id<MTLSharedEvent> event = [metal newSharedEvent];
    // NOLINTNEXTLINE(clang-analyzer-osx.cocoa.RetainCount)
    ASSERT_NE(event, nil);
    [watched addObject:event];
    NativeFence given;
    given.backend = Backend::Metal;
    given.mtlSharedEvent = (void *)event;
    const FenceHandle painted = device->importNative(given);
    ASSERT_TRUE(device->isValid(painted));
    const NativeFence native = device->exportNative(painted);
    EXPECT_EQ(native.backend, Backend::Vulkan);
    EXPECT_NE(native.vkSemaphore, 0u);
    EXPECT_EQ(native.mtlSharedEvent, (void *)event) << "the very event it was brought over from";
    EXPECT_EQ(device->signal(painted), kFenceInitialValue) << "the other queue numbers it";

    const FenceHandle after = device->createFence();
    device->waitGpu(painted, 1);
    const FenceValue reached = device->signal(after);
    EXPECT_EQ(device->waitCpu(after, reached, std::chrono::milliseconds(50)), FenceWait::TimedOut)
        << "the queue ran past a wait nothing had raised";

    id<MTLCommandQueue> queue = [metal newCommandQueue];
    id<MTLCommandBuffer> raise = [queue commandBuffer];
    [raise encodeSignalEvent:event value:1];
    [raise commit];
    EXPECT_EQ(device->waitCpu(after, reached), FenceWait::Reached);
    EXPECT_EQ(device->waitCpu(painted, 1), FenceWait::Reached);
    EXPECT_GE(device->completedValue(painted), 1u);
    // The buffer that raised it holds what it encoded until it completes.
    [raise waitUntilCompleted];

    device->destroyFence(painted);
    device->destroyFence(after);
    EXPECT_FALSE(device->importNative(NativeFence{}));
    CFRelease((CFTypeRef)queue);
    CFRelease((CFTypeRef)event);
  }
  // Metal lets go of a completed buffer's references on a thread of its
  // own, so the reference is read until it reads nothing for as long as
  // that can take; a reference the device kept never would.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  bool released = false;
  while (!released && std::chrono::steady_clock::now() < deadline) {
    @autoreleasepool {
      released = [watched anyObject] == nil;
    }
    if (!released) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_TRUE(released) << "the device kept a reference to the maker's event";
}

// …and the other way round: a fence made here is the event a queue on
// Metal waits on, and the device's signal lets that queue through.
TEST_F(HardwareVulkanOverMetal, AFenceMadeHereHoldsAQueueOnMetal) {
  const FenceHandle fence = device->createFence();
  const NativeFence native = device->exportNative(fence);
  EXPECT_NE(native.vkSemaphore, 0u);
  ASSERT_NE(native.mtlSharedEvent, nullptr);
  id<MTLSharedEvent> event = (id<MTLSharedEvent>)native.mtlSharedEvent;

  id<MTLCommandQueue> queue = [metal newCommandQueue];
  id<MTLCommandBuffer> held = [queue commandBuffer];
  [held encodeWaitForEvent:event value:1];
  [held commit];
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_NE(held.status, MTLCommandBufferStatusCompleted) << "Metal ran past an unraised fence";
  EXPECT_EQ(device->signal(fence), 1u);
  [held waitUntilCompleted];
  EXPECT_EQ(held.status, MTLCommandBufferStatusCompleted);
  device->destroyFence(fence);
  CFRelease((CFTypeRef)queue);
}

// THE DEVICE GOING AWAY IS THE LAST FRAME: a wait still queued on an
// event brought over is let finish before the semaphore over it is
// released, so the device cannot be gone before the event is raised.
TEST_F(HardwareVulkanOverMetal, ADeviceGoingAwayLetsAQueuedWaitFinishFirst) {
  id<MTLSharedEvent> event = [metal newSharedEvent];
  // NOLINTNEXTLINE(clang-analyzer-osx.cocoa.RetainCount)
  ASSERT_NE(event, nil);
  NativeFence given;
  given.backend = Backend::Metal;
  given.mtlSharedEvent = (void *)event;
  const FenceHandle painted = device->importNative(given);
  ASSERT_TRUE(device->isValid(painted));
  device->waitGpu(painted, 1);

  std::atomic<bool> raised{false};
  std::thread raiser([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    raised = true;
    event.signaledValue = 1;
  });
  vulkan->gpu.reset();
  EXPECT_TRUE(raised.load()) << "the device went while a wait on it was still queued";
  raiser.join();
  CFRelease((CFTypeRef)event);
}
