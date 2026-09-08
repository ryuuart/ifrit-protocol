#pragma once

/** @file
 * What this process stands up once, before any sketch draws and whatever
 * lane is running: the one web engine a page sketch borrows, the stock
 * materials, the font context and the asset store every session is handed,
 * the compile flags a reloaded sketch is built with, and the one device a
 * set is lit on.
 */

#include <sigilmaterial/stock/Stock.h>
#ifdef SIGILSKETCH_BOOK_SCRY
#include <sigilsketch/scry/SharedEngine.h>
#endif

#include <filesystem>
#include <future>
#include <string>

namespace sigil::sketch {
class Assets;
}

namespace sigil::weave {
class FontContext;
}

#ifdef SIGILSKETCH_BOOK_SCRY
/** THIS HOST OPTS INTO ONE LAZY WEB ENGINE for every sketch it opens.
 *
 * SigilScry's ordinary path remains explicit caller ownership through
 * WebEngine::create(config). Sketchbook is the exceptional host whose live
 * and resident sketches must borrow one renderer across selection and reload,
 * so it chooses that renderer's configuration before any sketch can ask for
 * it and releases it after every session is gone. */
class SharedWebEngineScope {
 public:
  SharedWebEngineScope() {
    if (!sigil::sketch::scry::configureSharedEngine({}))
      std::fprintf(stderr,
                   "[sketchbook] shared web engine was already configured\n");
  }
  ~SharedWebEngineScope() { sigil::sketch::scry::shutdownSharedEngine(); }

  SharedWebEngineScope(const SharedWebEngineScope&) = delete;
  SharedWebEngineScope& operator=(const SharedWebEngineScope&) = delete;

  void shutdown() { sigil::sketch::scry::shutdownSharedEngine(); }
};
#else
class SharedWebEngineScope {
 public:
  void shutdown() {}
};
#endif

/** THE STOCK MATERIALS, COMPILED BEFORE THE FIRST SKETCH DRAWS: the
 *  backend this host draws through, and then the material library's own
 *  warm-up over every recipe it ships. How many catalogues that is, where
 *  each reads its shader files from and how they are read side by side
 *  are the library's business, not this host's. */
sigil::material::WarmupResult warmStockMaterials();

/** Waits for that warm-up and says on stderr how much of it landed. */
void finishMaterialWarmup(std::future<sigil::material::WarmupResult>& loading);

/** The compiler line the build captured, which lands beside the binaries
 *  rather than inside the bundle: a macOS application is a directory, and
 *  its executable sits three levels down inside it. */
std::filesystem::path flagsFileNear(const std::filesystem::path& exeDir);

/** The directory this binary stands in, whatever the path it was invoked
 *  through. */
std::filesystem::path executableDir(const char* argv0);

/** THE PROCESS'S ONE FONT CONTEXT, shaped through the system's fonts:
 *  every session, still and headless lane shares it, so the shaping and
 *  glyph caches are filled once. */
sigil::weave::FontContext& fonts();

/** THE PROCESS'S ONE ASSET STORE, rooted at this repository's own
 *  sketch assets. A sketch opened from anywhere else mounts `assets/`
 *  beside its own file instead, which is the host's option and not this
 *  store. */
sigil::sketch::Assets& assets();

/** Puts every set sketch on the device, and says whether it could. The
 *  sweep treats a false answer as fatal because drawing the CPU's
 *  picture under a name that asked for the device's would put two
 *  different pictures under one name; the live host carries on, because
 *  a window can say which tier it is showing. */
bool useDevice();

/** Lets the device go while the process is still running. It outlives
 *  every frame that used it and must go BEFORE the process does:
 *  released after its own queue, the textures and pipelines it made take
 *  their teardown into static destruction, where the locks they want no
 *  longer exist. */
void releaseDevice();

/** True when the selection holds a sketch that draws through a device.
 *  The kind answers for itself, so a runtime added later is not a name
 *  this has to learn. It is not what decides whether a device is brought
 *  up — a `--gpu` run brings one up whatever it holds, because the
 *  surface a canvas is photographed on comes off that same device — but
 *  it is what a montage asks before spending one. */
bool selectionNeedsDevice(int only, const std::string& kind);
