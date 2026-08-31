/** @file
 * Platform bring-up on the web thread: the resource directory resolved,
 * the handlers handed to ultralight::Platform, the GPU driver picked for
 * the host's device — the one backend-specific seam in the engine — and
 * the renderer created.
 */

#include <Ultralight/AppCore/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/Platform.h>
#include <sigilskia/device/GpuDevice.h>

#include "EngineImpl.h"
#include "ResourceDir.h"

#ifdef __APPLE__
#include "metal/MetalDriver.h"
#endif

namespace sigil::scry {

bool WebEngine::Impl::setupPlatform() {
  m_logger = std::make_unique<CallbackLogger>(config.logCallback);
  m_fileSystem =
      std::make_unique<PrefixFileSystem>(resolveResourceDir(config.resourceDir),
                                         config.fileSystemDir, m_logger.get());
  m_surfaceFactory = std::make_unique<SkiaSurfaceFactory>();

  ultralight::Config ulConfig;
  ulConfig.cache_path = config.cachePath.c_str();

  ultralight::Platform& platform = ultralight::Platform::instance();
  platform.set_config(ulConfig);
  platform.set_logger(m_logger.get());
  platform.set_file_system(m_fileSystem.get());
  platform.set_font_loader(ultralight::GetPlatformFontLoader());
  platform.set_surface_factory(m_surfaceFactory.get());

  // The only backend-specific seam: pick the GpuDriver implementation
  // for this platform. Everything downstream sees the neutral interface.
  if (config.gpuDevice) {
    sigil::skia::GraphiteContext* graphite = config.graphite;
    if (!graphite) {
      m_ownedGraphite = sigil::skia::GraphiteContext::create(*config.gpuDevice);
      graphite = m_ownedGraphite.get();
    }
#ifdef __APPLE__
    if (graphite && config.gpuDevice->backend() == sigil::skia::Backend::Metal)
      m_gpuDriver = MetalDriver::create(*config.gpuDevice, *graphite);
#endif
    if (m_gpuDriver)
      platform.set_gpu_driver(m_gpuDriver.get());
    else
      m_logger->log(LogLevel::Warning,
                    "GPU driver bring-up failed (no backend for this "
                    "platform yet, or shader compile failed); falling back "
                    "to the CPU renderer");
  }

  // Touch the ImageSourceProvider singleton before the renderer exists:
  // it is destroyed in reverse construction order, and engine teardown
  // (WebCore MemoryCache cleanup) locks its mutex — constructed lazily on
  // first AddImageSource it would die before an engine held in a static.
  ultralight::ImageSourceProvider::instance();

  m_renderer = ultralight::Renderer::Create();
  if (!m_renderer) {
    m_logger->log(LogLevel::Error, "ultralight::Renderer::Create() failed");
    return false;
  }
  return true;
}

}  // namespace sigil::scry
