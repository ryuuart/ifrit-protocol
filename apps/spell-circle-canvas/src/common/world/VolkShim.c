/* SigilWorld's Vulkan bootstrap, for Diligent alone.
 *
 * DiligentCore resolves every Vulkan entry point through volk. Stock
 * volk dlopens leaf names plus /usr/local/lib, which misses the Homebrew
 * MoltenVK and loader install at /opt/homebrew on Apple Silicon — and
 * macOS dlopen matches neither that directory nor an already-loaded
 * image by leaf name, so nothing outside the process can correct it.
 * This translation unit therefore compiles vendored volk
 * (thirdparty/volk, MIT, pinned vulkan-sdk-1.4.321.0) with
 * volkInitialize renamed away and re-implemented below over an
 * absolute-path candidate list. Defining every volk symbol here is also
 * what keeps the copy DiligentCore archives from being pulled in beside
 * it: the linker has nothing left to resolve from that member.
 *
 * SigilSkia needs none of this. It resolves its own entry points and
 * links no Vulkan; AdoptDevice.cpp hands it the vkGetInstanceProcAddr
 * resolved here, so both APIs dispatch through the one loader this
 * process opened.
 */

#define volkInitialize volkInitializeStockDisabled
#include "thirdparty/volk/volk.c"
#undef volkInitialize

#if defined(__APPLE__)
#include <dlfcn.h>
#include <stdlib.h>

VkResult volkInitialize(void) {
  const char* candidates[] = {
      getenv("SIGILWORLD_VULKAN_LIBRARY"), /* explicit override first */
      "libvulkan.dylib",
      "libvulkan.1.dylib",
      "/opt/homebrew/lib/libvulkan.dylib",
      "/opt/homebrew/lib/libvulkan.1.dylib",
      "/usr/local/lib/libvulkan.dylib",
      "/opt/homebrew/lib/libMoltenVK.dylib",
      "/usr/local/lib/libMoltenVK.dylib",
      "libMoltenVK.dylib",
  };
  void* module = NULL;
  for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    if (!candidates[i]) continue;
    module = dlopen(candidates[i], RTLD_NOW | RTLD_LOCAL);
    if (module) break;
  }
  if (!module) return VK_ERROR_INITIALIZATION_FAILED;

  PFN_vkGetInstanceProcAddr proc =
      (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr");
  if (!proc) return VK_ERROR_INITIALIZATION_FAILED;

  /* The Homebrew loader discovers the MoltenVK ICD through its
   * sysconfdir; when the process runs with a stripped environment help
   * it along — env is read at vkCreateInstance time, so setenv here
   * still lands. Never override a caller's own configuration. */
  setenv("VK_DRIVER_FILES", "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json",
         /*overwrite=*/0);
  setenv("VK_ICD_FILENAMES", "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json",
         /*overwrite=*/0);

  volkInitializeCustom(proc);
  return VK_SUCCESS;
}
#else
VkResult volkInitialize(void) { return volkInitializeStockDisabled(); }
#endif
