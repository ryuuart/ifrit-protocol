/** @file
 * The reload loop: watch, compile, dlopen, swap — and keep the last good
 * sketch running while a build is broken.
 */

#include "sigilsketch/live/Host.h"

#include <dlfcn.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkSurface.h>
#include <sigilcore/schedule/ConcurrentIo.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>
#include <sigilsketch/core/Sources.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string_view>

#include "BuildDir.h"
#include "SkewGuard.h"
#include "sigilsketch/core/Crash.h"

namespace sigil::sketch {

namespace {

Host::Options withDefaults(Host::Options options) {
  if (options.assetsDir.empty()) {
    // A SKETCH THAT IS A DIRECTORY keeps its other units beside its
    // entry, so the assets are NOT beside the entry: they are one level
    // further up, in the directory every sketch shares. The entry's stem
    // naming its own directory is what says which of the two forms this
    // is, and it is the same rule that decides what compiles with it.
    std::filesystem::path beside = options.sketchPath.parent_path();
    if (beside.filename() == options.sketchPath.stem())
      beside = beside.parent_path();
    options.assetsDir = beside / "assets";
  }
  return options;
}

/** Runs a shell command, capturing stdout+stderr; returns the exit code. */
int run(const std::string& command, std::string& output) {
  // the command is the host's own compiler line, never user text
  // NOLINTNEXTLINE(bugprone-command-processor)
  FILE* pipe = popen((command + " 2>&1").c_str(), "r");
  if (!pipe) {
    output = "failed to spawn: " + command;
    return -1;
  }
  char buffer[4096];
  while (size_t n = fread(buffer, 1, sizeof buffer, pipe))
    output.append(buffer, n);
  return pclose(pipe);
}

/** Where a unit's object goes: named for the unit, and for the whole of
 *  its path, so two units of one stem in different directories — a
 *  sketch's `tables.cpp` and the shared layer's — do not share one. */
std::filesystem::path objectFor(const std::filesystem::path& buildDir,
                                const std::filesystem::path& source) {
  char hash[24];
  std::snprintf(hash, sizeof hash, "%zx",
                std::hash<std::string>{}(source.string()));
  return buildDir / (std::string(hash) + "_" + source.stem().string() + ".o");
}

/** One unit to its object, with the flags the build captured — and the
 *  one difference between a compiled-in sketch and a guest one.
 *
 *  HIDDEN VISIBILITY IS WHAT MAKES A GUEST RUN ITS OWN CODE. A sketch's
 *  body reaches the host through weak definitions: the class's vtable
 *  and typeinfo when every virtual is inline, and the function templates
 *  the registration macro takes the address of. Weak definitions
 *  COALESCE across a flat namespace — every image that exports one names
 *  the same symbol, and the loader binds them all to whichever image
 *  came first. The host is loaded before any dylib and exports its own
 *  copy of every sketch it was built with, so a guest's definitions
 *  would lose to the host's and the picture would be the host's; between
 *  two generations of the same guest, build 1 would beat build 2 and an
 *  edit would never appear.
 *
 *  Compiled hidden, a guest's definitions are private to its image and
 *  join no coalescing set in either direction. Its UNDEFINED references
 *  are untouched by this — they still resolve into the host through the
 *  link line's dynamic lookup — and the entry points the registration
 *  macro exports carry default visibility explicitly, so `dlsym` finds
 *  them. The cost is that a guest gets its own copy of every inline the
 *  host also has, which is correct for code and would be wrong only for
 *  a mutable static inside one. */
std::string compileLine(const Host::Options& options,
                        const std::filesystem::path& source,
                        const std::filesystem::path& object) {
  std::ostringstream cmd;
  cmd << options.compiler << " @" << options.flagsFile
      << " -fvisibility=hidden -fvisibility-inlines-hidden -c -o " << object
      << ' ' << source;
  return cmd.str();
}

/** Every object into one dylib that resolves the framework out of the
 *  host. No flags file here: the objects already carry the
 *  architecture, and the rest of the captured line is for compiling. */
std::string linkLine(const Host::Options& options,
                     const std::vector<std::filesystem::path>& objects,
                     const std::filesystem::path& out) {
  std::ostringstream cmd;
  cmd << options.compiler
#ifdef __APPLE__
      << " -shared -undefined dynamic_lookup -Wl,-dead_strip"
#else
      << " -shared"
#endif
      << " -o " << out;
  for (const std::filesystem::path& object : objects) cmd << ' ' << object;
  return cmd.str();
}

constexpr CanvasSpec kUnloaded{};

}  // namespace

Host::Host(Options options, weave::FontContext& fonts)
    : m_options(withDefaults(std::move(options))),
      m_fonts(fonts),
      m_assets(m_options.assetsDir) {
  // Before this process claims its own: the directories of runs that were
  // killed or that faulted are the ones nothing else will ever clear. An
  // owner that swept while its window was coming up claimed the walk
  // before this host existed, and this host walks nothing.
  if (claimSweep()) sweepAbandonedBuildDirs();
  m_buildDir = acquireBuildDir();
  m_hostId = nextHostId();
  // A sketch this binary already carries opens instantly, and the file is
  // watched from where it stands: an edit builds, an unedited file never
  // does. A file the binary does not carry has to be built to be seen.
  if (m_options.compiledIn && m_options.compiledIn->kind) {
    m_kind = m_options.compiledIn->kind();
    openSession(m_kind);
    if (const auto stamp = sourceStamp()) {
      m_compiledMtime = *stamp;
      m_everCompiled = true;
    }
    m_status = "live · compiled in";
  }
}

Host::~Host() {
  if (m_compile.valid()) m_compile.wait();
  // A session's retained descriptions and running motions may point into
  // sketch-owned state; release it before the libraries it came from.
  // Loaded dylibs intentionally remain mapped.
  m_session.reset();
  // …and the files behind them go with the last host in this process.
  // An unlinked file that is mapped stays readable until the last
  // mapping goes, so removing the directory takes nothing out from
  // under a library still in use.
  releaseBuildDir();
}

void Host::openSession(const Kind& kind) {
  m_session.reset();
  if (!kind) return;
  {
    PhaseMark mark(Phase::Setup);
    m_session = kind->open(m_fonts, m_assets, m_options.deterministic);
  }
  m_workMs.clear();  // fresh sketch, fresh numbers
  m_drawMs.clear();
  m_presentedFrames = 0;
}

bool Host::restartSession() {
  if (!m_kind) return false;
  openSession(m_kind);
  // The new Session owns its own fresh clock and ticker. Reset the host-side
  // clock as well so asset polling and crash-report frame coordinates describe
  // the same new run, not the session that was just released.
  m_clock = motion::FrameClock{};
  m_lastAssetPoll = 0.0;
  m_frameIndex = -1;
  m_presentSince.reset();
  m_presentMs.clear();
  return m_session != nullptr;
}

void Host::resetMetrics() {
  m_workMs.clear();
  m_drawMs.clear();
  m_presentMs.clear();
  // The interval running when this was called spans the boundary, so it
  // is not one of the intervals that follow it: the next presentation
  // seeds a new one.
  m_presentSince.reset();
}

SkSize Host::canvasSize() const {
  return m_session ? m_session->canvas().size : kUnloaded.size;
}

SkColor4f Host::background() const {
  return m_session ? m_session->canvas().background : kUnloaded.background;
}

double Host::captureSeconds() const {
  return m_session ? m_session->canvas().captureSeconds
                   : kUnloaded.captureSeconds;
}

std::optional<std::filesystem::file_time_type> Host::sourceStamp() {
  std::error_code ec;
  const auto self = std::filesystem::last_write_time(m_options.sketchPath, ec);
  if (ec) return std::nullopt;
  const auto now = std::chrono::steady_clock::now();
  if (m_lastSiblingScan.time_since_epoch().count() == 0 ||
      now - m_lastSiblingScan >= m_options.siblingScanInterval) {
    m_lastSiblingScan = now;
    scanBeside();
  }
  return std::max({self, m_headerStamp, m_unitStamp});
}

void Host::scanBeside() {
  m_headerStamp = {};
  m_unitStamp = {};
  // Beside a bare sketch the other sources are OTHER SKETCHES, and an
  // edit to one of them is nothing to this one; only a directory sketch
  // owns the sources around its entry. Headers are shared either way,
  // reached by a quoted include. The shared layer is units and headers
  // of every sketch. Nothing else in a directory is an input of the
  // compile: a capture landing beside the sketch is not an edit.
  const auto stamp = [this](const std::filesystem::path& dir, bool sources) {
    std::error_code scan;
    for (auto it = std::filesystem::directory_iterator(dir, scan);
         !scan && it != std::filesystem::directory_iterator();
         it.increment(scan)) {
      const std::filesystem::path& p = it->path();
      const std::filesystem::path extension = p.extension();
      const bool header = extension == ".h" || extension == ".hpp";
      const bool unit =
          sources && extension == ".cpp" && p != m_options.sketchPath;
      if (!header && !unit) continue;
      std::error_code stat;
      const auto t = std::filesystem::last_write_time(p, stat);
      if (stat) continue;
      std::filesystem::file_time_type& newest =
          header ? m_headerStamp : m_unitStamp;
      if (t > newest) newest = t;
    }
  };
  stamp(m_options.sketchPath.parent_path(),
        directorySketch(m_options.sketchPath));
  if (!m_options.sharedDir.empty()) stamp(m_options.sharedDir, true);
}

std::vector<std::filesystem::path> Host::units() const {
  std::vector<std::filesystem::path> all = unitsOf(m_options.sketchPath);
  if (!m_options.sharedDir.empty()) {
    std::vector<std::filesystem::path> shared =
        sourcesUnder(m_options.sharedDir);
    all.insert(all.end(), std::make_move_iterator(shared.begin()),
               std::make_move_iterator(shared.end()));
  }
  return all;
}

void Host::startCompile() {
  m_compiledMtime = sourceStamp().value_or(std::filesystem::file_time_type{});
  m_everCompiled = true;
  m_compileStart = std::chrono::steady_clock::now();

  // Skew guard: never hand a dylib built against newer framework headers
  // to an older host — the crash it prevents is unattributable. The
  // reference point is the image that is RUNNING, not the file on disk,
  // which a rebuild replaces underneath it.
  if (m_options.hostStamp != std::filesystem::file_time_type{}) {
    if (const std::string stale =
            newerHeaderThanHost(m_options.flagsFile, m_options.hostStamp);
        !stale.empty()) {
      m_errorLog =
          "framework headers are newer than this host (" + stale +
          ").\nA sketch built against them would load into a host whose "
          "structs have the old layout, so this build is refused: rebuild "
          "Sketchbook and restart it.";
      m_status = "stale host — waiting for a rebuild";
      return;  // keep the previous sketch alive
    }
  }
  m_status = "compiling build " + std::to_string(m_generation + 1) + "…";

  const std::filesystem::path out =
      m_buildDir / ("sketch_" + std::to_string(m_hostId) + "_" +
                    std::to_string(++m_generation) + ".dylib");
  // Every unit is on the link line; only the stale ones are compiled. A
  // unit is stale when it has never been built, when its source is not
  // the one its object came from, or when any header around the sketch
  // has been written since — the one conservative rule that needs no
  // dependency scan, and the one that makes a table in its own unit
  // free to every edit of the entry.
  std::vector<std::filesystem::path> objects;
  std::vector<Unit> stale;
  const std::filesystem::file_time_type headers = m_headerStamp;
  for (const std::filesystem::path& source : units()) {
    std::error_code ec;
    const auto sourceTime = std::filesystem::last_write_time(source, ec);
    const std::filesystem::path object = objectFor(m_buildDir, source);
    objects.push_back(object);
    const auto built = m_built.find(source);
    const bool fresh = !ec && built != m_built.end() &&
                       built->second.source == sourceTime &&
                       built->second.headers == headers &&
                       std::filesystem::exists(built->second.object, ec);
    if (!fresh) stale.push_back({source, object, sourceTime});
  }
  std::vector<std::string> compiles;
  compiles.reserve(stale.size());
  for (const Unit& unit : stale)
    compiles.push_back(compileLine(m_options, unit.source, unit.object));
  std::string link = linkLine(m_options, objects, out);

  m_compile = std::async(
      std::launch::async,
      // copying the captures can fail only on allocation
      // NOLINTNEXTLINE(bugprone-exception-escape)
      [compiles = std::move(compiles), link = std::move(link),
       stale = std::move(stale), headers, total = (int)objects.size(),
       out]() -> CompileResult {
        CompileResult result;
        result.library = out;
        result.compiled = stale;
        result.headers = headers;
        result.units = total;
        // The stale units compile side by side, so a sketch of several
        // units takes as long as its slowest one — which is the entry
        // being edited. Each compile is a WAIT on a child process rather
        // than work this process does, so it goes to the fan-out that
        // exists for waiting, and each unit writes only its own two
        // elements.
        std::vector<std::string> outputs(compiles.size());
        std::vector<int> codes(compiles.size(), 0);
        core::schedule::concurrentIo(compiles.size(), [&](size_t unit) {
          codes[unit] = run(compiles[unit], outputs[unit]);
        });
        // Failures in unit order, so the entry's errors read first.
        for (size_t i = 0; i < compiles.size(); ++i)
          if (codes[i] != 0) result.output += outputs[i];
        if (!result.output.empty()) return result;
        result.ok = run(link, result.output) == 0;
        return result;
      });
}

void Host::adopt(const std::filesystem::path& library) {
  void* handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    m_errorLog = dlerror();
    m_status = "load failed";
    return;
  }
  auto abi = reinterpret_cast<unsigned (*)()>(dlsym(handle, "sigilSketchAbi"));
  auto exported =
      reinterpret_cast<const Entry* (*)()>(dlsym(handle, "sigilSketchEntry"));
  // A REFUSED IMAGE IS CLOSED. Nothing in it is referenced — no session
  // was opened, no vtable and no string literal of its is held — which
  // is what separates it from the images below, none of which is ever
  // closed.
  if (!abi || !exported || abi() != kAbiVersion) {
    dlclose(handle);
    m_errorLog =
        "sketch ABI mismatch — is SIGIL_SKETCH(...) present? "
        "(after framework changes, restart the host)";
    m_status = "load failed";
    return;
  }

  const Entry* entry = exported();
  if (!entry || !entry->kind) {
    dlclose(handle);
    m_errorLog = "the sketch exported no kind";
    m_status = "load failed";
    return;
  }
  m_libraries.push_back(handle);
  m_kind = entry->kind();
  openSession(m_kind);
  m_errorLog.clear();
  const double seconds = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - m_compileStart)
                             .count();
  char line[160];
  if (m_unitsTotal > 1)
    std::snprintf(line, sizeof line,
                  "live · build %d · %d of %d units compiled in %.1fs",
                  m_generation, m_unitsCompiled, m_unitsTotal, seconds);
  else
    std::snprintf(line, sizeof line, "live · build %d · compiled in %.1fs",
                  m_generation, seconds);
  m_status = line;
  std::fprintf(stderr, "[sketch] %s\n", m_status.c_str());
}

void Host::poll() {
  // Adopt a finished compile.
  if (m_compile.valid() && m_compile.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
    CompileResult result = m_compile.get();
    if (result.ok) {
      for (const Unit& unit : result.compiled)
        m_built[unit.source] =
            Built{unit.object, unit.sourceTime, result.headers};
      m_unitsCompiled = (int)result.compiled.size();
      m_unitsTotal = result.units;
      adopt(result.library);
    } else {
      m_errorLog = result.output;
      m_status = live() ? "build " + std::to_string(m_generation) +
                              " failed — keeping build " +
                              std::to_string(m_generation - 1)
                        : "compile failed";
      std::fprintf(stderr, "[sketch] %s\n%s\n", m_status.c_str(),
                   m_errorLog.c_str());
    }
  }

  // Source changed (or never built) → kick a compile.
  if (!m_compile.valid()) {
    if (const auto stamp = sourceStamp();
        stamp && (!m_everCompiled || *stamp != m_compiledMtime))
      startCompile();
  }

  // Asset hot reload (twice a second is plenty for filesystem stats).
  if (m_session && m_clock.elapsed() - m_lastAssetPoll > 0.5) {
    m_lastAssetPoll = m_clock.elapsed();
    if (m_assets.poll()) {
      PhaseMark mark(Phase::Setup);
      m_session->redeclare();
    }
  }
}

bool Host::frame(SkCanvas& canvas, double fixedDt) {
  if (!m_session) return false;
  const measure::Stopwatch watch;
  // A stated step and a wall-clock one are the same clock here as
  // everywhere else. It matters beyond tidiness: the asset poll below
  // measures its half-second against this reading, and a free-running
  // host whose reading never moved would poll once and never again.
  if (fixedDt >= 0)
    m_clock.advance(fixedDt);
  else
    m_clock.tick();
  noteFrame(++m_frameIndex, m_clock.elapsed());
  {
    PhaseMark mark(Phase::Update);
    m_session->frame(canvas, fixedDt);
  }
  m_workMs.add(watch.elapsedMs());
  m_drawMs.add(m_session->timing().drawMs);
  return true;
}

double Host::workMsAverage() const { return m_workMs.mean(); }

double Host::drawMsAverage() const { return m_drawMs.mean(); }

double Host::workMsP99() const { return m_workMs.percentile(0.99); }

double Host::presentedFps() const {
  if (m_presentMs.size() < 2) return 0.0;
  const double mean = m_presentMs.mean();
  return mean > 0 ? 1000.0 / mean : 0.0;
}

void Host::markPresented() {
  ++m_presentedFrames;
  if (!m_presentSince) {
    m_presentSince.emplace();  // seeds the cadence; nothing to measure yet
    return;
  }
  const double ms = m_presentSince->elapsedMs();
  if (ms < 1000.0)  // ignore stalls (window drags, sleeps)
    m_presentMs.add(ms);
  m_presentSince->reset();
}

bool Host::capture(const std::filesystem::path& out, float scale) {
  if (!m_session) return false;
  const CanvasSpec& spec = m_session->canvas();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(
      std::max(1, (int)(spec.size.width() * scale)),
      std::max(1, (int)(spec.size.height() * scale)));
  sk_sp<SkSurface> surface = m_captureBackend.makeSurface
                                 ? m_captureBackend.makeSurface(info)
                                 : SkSurfaces::Raster(info);
  if (!surface) return false;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(spec.background.toSkColor());
  canvas.scale(scale, scale);
  m_session->repaint(canvas);
  SkBitmap bitmap;
  bitmap.allocPixels(surface->imageInfo());
  if (m_captureBackend.readback) {
    if (!m_captureBackend.readback(*surface, bitmap.pixmap())) return false;
  } else {
    surface->readPixels(bitmap.pixmap(), 0, 0);
  }
  // The format the capture path is named for; the directories above the
  // file are the sink's business, not this one's.
  const sk_sp<SkData> png =
      image::encodeImage(bitmap.pixmap(), image::Format::Png);
  return png && io::writeBytes(out, png->data(), png->size());
}

}  // namespace sigil::sketch
