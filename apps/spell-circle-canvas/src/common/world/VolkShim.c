/* SigilWorld's Vulkan bootstrap. DiligentCore (from the
 * sigil-vcpkg-registry port) is built against volk but does not archive
 * volk's objects, so this TU supplies them — with one change: stock
 * volk only dlopens leaf names plus /usr/local/lib, which misses the
 * Homebrew MoltenVK/loader install at /opt/homebrew on Apple Silicon
 * (and macOS dlopen does not match leaf names against already-loaded
 * images, so pre-loading cannot fix it from outside). volkInitialize is
 * therefore renamed away during the include and re-implemented below
 * with an absolute-path candidate list; everything else is verbatim
 * vendored volk (thirdparty/volk, MIT, pinned vulkan-sdk-1.4.321.0).
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
