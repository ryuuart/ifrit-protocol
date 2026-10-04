/** @file
 * The process's own values: the stock materials, the fonts and assets
 * every session shares, the flags file beside the binary, and the device
 * a set is lit on.
 */

#include "Startup.h"

#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#ifndef SIGILSKETCH_NO_DEVICE
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/render/device/Painter.h>
#include <sigilworld/diligent/Runtime.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <cstdint>
#include <cstdio>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

std::span<const sigil::material::Material> stockRecipes() {
  // Leaked deliberately, as the font context is: the recipes hold
  // Skia-backed programs, and a static destructor racing Skia teardown
  // is a class of crash worth not having.
  namespace surface = sigil::material::surface;
  static const auto* recipes = new std::vector<sigil::material::Material>{
      surface::program({}, surface::Reflection::SplitSum),
      surface::program({}, surface::Reflection::Additive), surface::unlit()};
  return *recipes;
}

sigil::material::WarmupResult warmStockMaterials() {
  return sigil::material::skia::warmup(stockRecipes());
}

void finishMaterialWarmup(std::future<sigil::material::WarmupResult>& loading) {
  const sigil::material::WarmupResult result = loading.get();
  if (result.ready != result.unique)
    std::fprintf(stderr,
                 "[sketchbook] material warm-up prepared %zu of %zu "
                 "programs\n",
                 result.ready, result.unique);
}

std::filesystem::path flagsFileNear(
    const std::filesystem::path& executableDirectory) {
  std::filesystem::path beside = executableDirectory / "sketch_flags.rsp";
  if (std::filesystem::exists(beside)) return beside;
  return executableDirectory.parent_path().parent_path().parent_path() /
         "sketch_flags.rsp";
}

std::filesystem::path executableDirectory(const char* argv0) {
#ifdef __APPLE__
  char buffer[4096];
  uint32_t size = sizeof buffer;
  if (_NSGetExecutablePath(buffer, &size) == 0)
    return std::filesystem::canonical(buffer).parent_path();
#endif
  std::error_code ec;
  auto canonical = std::filesystem::canonical(argv0, ec);
  return ec ? std::filesystem::current_path() : canonical.parent_path();
}

sigil::weave::FontContext& fonts() {
  // Leaked deliberately: it owns Skia-backed state, and a static
  // destructor racing Skia teardown is a class of crash worth not having.
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

sketch::Assets& assets() {
  static auto* store =
      new sketch::Assets(SIGIL_SKETCH_ASSET_DIR, SIGIL_SKETCH_DIR);
  return *store;
}

#ifndef SIGILSKETCH_NO_DEVICE
/** Held for the run: the runtime borrows the device, and every texture
 *  and pipeline it made goes when the device does. */
std::unique_ptr<sigil::geometry::device::Device> g_device;

/** Puts every set sketch on the device, and says whether it could. The
 *  sweep treats a false answer as fatal because drawing the CPU's
 *  picture under a name that asked for the device's would put two
 *  different pictures under one name; the live host carries on, because
 *  a window can say which tier it is showing. */
bool useDevice() {
  if (g_device) return true;
  std::string error;
  const sigil::geometry::device::DeviceConfig config;
  auto candidate = sigil::geometry::device::Device::create(config, &error);
  if (!candidate) {
    std::fprintf(stderr, "no device runtime (%s)\n", error.c_str());
    return false;
  }
  const auto frameRuntime = sigil::world::diligent::runtime(*candidate);
  const auto meshRuntime =
      sigil::geometry::mesh::render::deviceRuntime(*candidate);
  g_device = std::move(candidate);
  sketch::useRuntime(frameRuntime);
  // …and the 2D twin: a canvas sketch that stands a mesh up in space
  // reaches the same device through sketch::painterRuntime().
  sketch::usePainterRuntime(meshRuntime);
  // …and the device itself, for the calls no runtime can stand in for:
  // a foreign texture entering a material slot names the device it
  // already stands on.
  sketch::useDevice(g_device.get());
  return true;
}

/** Lets the device go while the process is still running. It outlives
 *  every frame that used it and must go BEFORE the process does:
 *  released after its own queue, the textures and pipelines it made take
 *  their teardown into static destruction, where the locks they want no
 *  longer exist. */
void releaseDevice() {
  sketch::useDevice(nullptr);
  sketch::useRuntime({});
  sketch::usePainterRuntime({});
  g_device.reset();
}
#else
bool useDevice() {
  std::fprintf(stderr,
               "no device runtime (this binary was built without one)\n");
  return false;
}

void releaseDevice() {}
#endif

/** True when the selection holds a sketch that draws through a device.
 *  The kind answers for itself, so a runtime added later is not a name
 *  this has to learn. Registry sweeps and montages ask before bringing
 *  up their shared device. */
bool selectionNeedsDevice(int only, const std::string& kind) {
  const auto& entries = sketch::registry();
  for (int index : sketch::selection(only, kind)) {
    const sketch::Kind entry = entries[index].kind();
    if (entry && entry->needsDevice()) return true;
  }
  return false;
}
