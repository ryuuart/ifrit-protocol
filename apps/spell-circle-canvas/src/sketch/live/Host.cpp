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
#include <include/utils/SkNoDrawCanvas.h>
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "BuildCache.h"
#include "BuildDirectory.h"
#include "SkewGuard.h"
#include "sigilsketch/core/Crash.h"

namespace sigil::sketch {

namespace {

std::optional<SkISize> captureExtent(SkSize size, float scale = 1.0f) {
  const double width = std::ceil(double(size.width()) * scale);
  const double height = std::ceil(double(size.height()) * scale);
  if (!std::isfinite(scale) || scale <= 0 || !std::isfinite(width) ||
      !std::isfinite(height) || width < 1 || height < 1 || width > 16384 ||
      height > 16384)
    return std::nullopt;
  return SkISize::Make(int(width), int(height));
}

Host::Options withDefaults(Host::Options options) {
  // A sketch this binary carries takes the root the process stated; only
  // a file opened by path defaults to the assets beside it.
  if (options.assetsDirectory.empty() && !options.compiledIn) {
    // A SKETCH THAT IS A DIRECTORY keeps its other units beside its
    // entry, so the assets are NOT beside the entry: they are one level
    // further up, in the directory every sketch shares. The entry's stem
    // naming its own directory is what says which of the two forms this
    // is, and it is the same rule that decides what compiles with it.
    std::filesystem::path beside = options.sketchPath.parent_path();
    if (beside.filename() == options.sketchPath.stem())
      beside = beside.parent_path();
    options.assetsDirectory = beside / "assets";
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

// Compiler paths and authored paths are shell arguments, including apostrophes.
std::string shellArgument(std::string_view value) {
  std::string result = "'";
  for (char ch : value) result += ch == '\'' ? "'\"'\"'" : std::string(1, ch);
  return result + "'";
}

// Preprocessing asks the compiler itself to resolve every include and macro.
// A content key therefore covers angle includes, conditional includes, flags,
// and the native image this guest resolves its framework symbols from.
struct BuildInputs {
  std::string key;
  std::vector<std::string> units;
};

BuildInputs cacheInputs(const Host::Options& options,
                        const std::vector<std::filesystem::path>& sources) {
  if (options.hostStamp == std::filesystem::file_time_type{}) return {};
  std::string version;
  if (run(shellArgument(options.compiler) + " --version", version) != 0)
    return {};
  Dl_info image{};
  dladdr(reinterpret_cast<const void*>(&hostBinaryTime), &image);
  std::ifstream flags(options.flagsFile);
  if (!flags) return {};
  std::string identity =
      "sigil-build-2\n" + options.compiler + "\n" + version + "\n" +
      (image.dli_fname ? image.dli_fname : "") + "\n" +
      std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                         options.hostStamp.time_since_epoch())
                         .count()) +
      "\n" + std::string(std::istreambuf_iterator<char>(flags), {});
  BuildInputs inputs;
  std::string linked = identity;
  for (const auto& unit : sources) {
    std::string preprocessed;
    if (run(shellArgument(options.compiler) + " @" +
                shellArgument(options.flagsFile.string()) +
                " -fvisibility=hidden -fvisibility-inlines-hidden -E " +
                shellArgument(unit.string()),
            preprocessed) != 0)
      return {};
    inputs.units.push_back(
        buildDigest(identity + "\n" + unit.string() + "\n" + preprocessed));
    linked += "\n" + inputs.units.back();
  }
  inputs.key = buildDigest(linked);
  return inputs;
}

/** Where a unit's object goes: named for the unit, and for the whole of
 *  its path, so two units of one stem in different directories — two
 *  sketches' `tables.cpp` files — do not share one. */
std::filesystem::path objectFor(const std::filesystem::path& buildDirectory,
                                const std::filesystem::path& source) {
  char hash[24];
  std::snprintf(hash, sizeof hash, "%zx",
                std::hash<std::string>{}(source.string()));
  return buildDirectory /
         (std::string(hash) + "_" + source.stem().string() + ".o");
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
  cmd << shellArgument(options.compiler) << " @"
      << shellArgument(options.flagsFile.string())
      << " -fvisibility=hidden -fvisibility-inlines-hidden -c -o "
      << shellArgument(object.string()) << ' '
      << shellArgument(source.string());
  return cmd.str();
}

/** Every object into one dylib that resolves the framework out of the
 *  host. No flags file here: the objects already carry the
 *  architecture, and the rest of the captured line is for compiling. */
std::string linkLine(const Host::Options& options,
                     const std::vector<std::filesystem::path>& objects,
                     const std::filesystem::path& out) {
  std::ostringstream cmd;
  cmd << shellArgument(options.compiler)
#ifdef __APPLE__
      << " -shared -undefined dynamic_lookup -Wl,-dead_strip"
#else
      << " -shared"
#endif
      << " -o " << shellArgument(out.string());
  for (const std::filesystem::path& object : objects)
    cmd << ' ' << shellArgument(object.string());
  return cmd.str();
}

constexpr CanvasSpecification kUnloaded{};

}  // namespace

Host::Host(Options options, weave::FontContext& fonts)
    : m_options(withDefaults(std::move(options))),
      m_fonts(fonts),
      m_assets(m_options.assetsDirectory, m_options.sketchesDirectory) {
  // Before this process claims its own: the directories of runs that were
  // killed or that faulted are the ones nothing else will ever clear. An
  // owner that swept while its window was coming up claimed the walk
  // before this host existed, and this host walks nothing.
  if (claimSweep()) sweepAbandonedBuildDirectories();
  m_buildDirectory = acquireBuildDirectory();
  m_hostId = nextHostId();
  // Registered native bodies open directly. Python entries use the source
  // loader and record their input stamps before opening, so the first poll
  // does not import the same generation twice.
  if (m_options.compiledIn && m_options.compiledIn->kind) {
    if (m_options.sketchPath.extension() == ".py") {
      pythonChanged();
      loadPython();
      return;
    }
    m_kind = m_options.compiledIn->kind();
    if (!openSession(m_kind)) return;
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
  releaseBuildDirectory();
}

bool Host::openSession(const Kind& kind) {
  if (!kind) return false;
  const measure::Stopwatch opened;
  try {
    PhaseMark mark(Phase::Setup);
    // A compiled-in sketch is keyed by its entry; a workspace sketch by
    // its file's stem, with the files beside that file mounted as its own.
    std::string key;
    if (m_options.compiledIn) {
      key = m_options.compiledIn->key;
    } else if (!m_options.sketchPath.empty()) {
      key = m_options.sketchPath.stem().string();
      m_assets.mountSketch(key, m_options.sketchPath.parent_path());
    }
    // Setup may fail after allocating retained descriptions or callbacks.
    // Finish the candidate before releasing the last working session.
    auto candidate =
        kind->open(m_fonts, m_assets, m_options.deterministic, key);
    if (!candidate) throw std::runtime_error("the sketch opened no session");
    m_session = std::move(candidate);
    m_kind = kind;
  } catch (const std::exception& error) {
    m_errorLog = error.what();
    m_status =
        live() ? "setup failed — keeping previous sketch" : "setup failed";
    std::fprintf(stderr, "[sketch] %s\n%s\n", m_status.c_str(),
                 m_errorLog.c_str());
    return false;
  }
  std::fprintf(stderr, "[sketch] set up in %.0f ms\n", opened.elapsedMs());
  m_runtimeFailed = false;
  m_errorLog.clear();
  m_workMs.clear();
  m_drawMs.clear();
  m_presentedFrames = 0;
  return true;
}

bool Host::restartSession() {
  if (!m_kind) return false;
  if (!openSession(m_kind)) return false;
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
  // owns the sources around its entry. Local headers are reached by
  // quoted includes, including helpers owned by another sketch.
  // Nothing else in a directory is an input of the
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
  for (const auto& header : headersOf(m_options.sketchPath)) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(header, error);
    if (!error) m_headerStamp = std::max(m_headerStamp, time);
  }
}

std::vector<std::filesystem::path> Host::units() const {
  return unitsOf(m_options.sketchPath);
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
      m_buildDirectory / ("sketch_" + std::to_string(m_hostId) + "_" +
                          std::to_string(++m_generation) + ".dylib");
  // Each host owns its scratch objects; persistent objects are copied in.
  std::vector<std::filesystem::path> objects;
  std::vector<Unit> stale;
  auto sources = units();
  for (const std::filesystem::path& source : sources) {
    const auto object =
        m_buildDirectory /
        (std::to_string(m_hostId) + "_" +
         objectFor(m_buildDirectory, source).filename().string());
    objects.push_back(object);
    stale.push_back({source, object});
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
       stale = std::move(stale), total = (int)objects.size(), out,
       options = m_options, sources = std::move(sources)]() -> CompileResult {
        CompileResult result;
        result.library = out;
        const auto cache = buildCacheDirectory();
        const auto inputs =
            cache.empty() ? BuildInputs{} : cacheInputs(options, sources);
        result.cacheKey = inputs.key;
        if (restoreBuild(cache, inputs.key, out)) {
          result.ok = true;
          result.cached = true;
          return result;
        }

        result.units = total;
        // The stale units compile side by side, so a sketch of several
        // units takes as long as its slowest one — which is the entry
        // being edited. Each compile is a WAIT on a child process rather
        // than work this process does, so it goes to the fan-out that
        // exists for waiting, and each unit writes only its own two
        // elements.
        std::vector<unsigned char> restored(compiles.size(), 0);
        std::vector<std::string> outputs(compiles.size());
        std::vector<int> codes(compiles.size(), 0);
        core::schedule::concurrentIo(compiles.size(), [&](size_t unit) {
          if (!inputs.key.empty() &&
              restoreBuild(cache / "objects", inputs.units[unit],
                           stale[unit].object))
            restored[unit] = 1;
          else
            codes[unit] = run(compiles[unit], outputs[unit]);
        });
        // Failures in unit order, so the entry's errors read first.
        for (size_t i = 0; i < compiles.size(); ++i)
          if (codes[i] != 0) result.output += outputs[i];
        if (std::any_of(codes.begin(), codes.end(),
                        [](int code) { return code != 0; }))
          return result;
        for (size_t i = 0; i < stale.size(); ++i)
          if (!restored[i]) ++result.compiled;
        result.ok = run(link, result.output) == 0;
        if (result.ok && !inputs.key.empty() &&
            cacheInputs(options, sources).key == inputs.key) {
          for (size_t i = 0; i < stale.size(); ++i)
            if (!restored[i])
              storeBuild(cache / "objects", inputs.units[i], stale[i].object);
          storeBuild(cache, inputs.key, out);
        }
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
  if (!openSession(entry->kind())) return;
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

bool Host::pythonChanged() {
  bool changed = !m_everCompiled;
  const auto now = std::chrono::steady_clock::now();
  if (m_lastSiblingScan.time_since_epoch().count() == 0 ||
      now - m_lastSiblingScan >= m_options.siblingScanInterval) {
    m_lastSiblingScan = now;
    decltype(m_pythonInputs) inputs;
    for (const auto& path : pythonSourcesOf(m_options.sketchPath)) {
      if (path == m_options.sketchPath) continue;
      std::error_code ec;
      const auto stamp = std::filesystem::last_write_time(path, ec);
      if (!ec) inputs.emplace_back(path, stamp);
    }
    changed = changed || inputs != m_pythonInputs;
    m_pythonInputs = std::move(inputs);
  }
  // The entry is checked each frame; modules are checked on the directory
  // cadence. Comparing the path set also catches added and deleted modules.
  std::error_code ec;
  auto stamp = std::filesystem::last_write_time(m_options.sketchPath, ec);
  if (ec) stamp = std::filesystem::file_time_type::min();
  changed = changed || stamp != m_pythonEntryStamp;
  m_pythonEntryStamp = stamp;
  return changed;
}

void Host::loadPython() {
  m_everCompiled = true;
  ++m_generation;
  if (!m_options.pythonLoader) {
    m_errorLog =
        "Python sketches require a Python importer on this host. "
        "Link SigilSketchPython and set "
        "Host::Options::pythonLoader.";
    m_status = "Python unavailable";
    std::fprintf(stderr, "[sketch] %s\n", m_errorLog.c_str());
    return;
  }
  const measure::Stopwatch loaded;
  try {
    const Kind candidate = m_options.pythonLoader(m_options.sketchPath);
    if (!openSession(candidate)) return;
  } catch (const std::exception& error) {
    m_errorLog = error.what();
    m_status =
        live() ? "import failed — keeping previous sketch" : "import failed";
    std::fprintf(stderr, "[sketch] %s\n%s\n", m_status.c_str(),
                 m_errorLog.c_str());
    return;
  }
  char line[160];
  std::snprintf(line, sizeof line, "live · Python %d · loaded in %.0f ms",
                m_generation, loaded.elapsedMs());
  m_status = line;
  std::fprintf(stderr, "[sketch] %s\n", m_status.c_str());
}

void Host::sessionFailed(const std::exception& error) {
  m_runtimeFailed = true;
  m_errorLog = error.what();
  m_status = "sketch failed — waiting for an edit";
  std::fprintf(stderr, "[sketch] %s\n%s\n", m_status.c_str(),
               m_errorLog.c_str());
}

void Host::poll() {
  // Adopt a finished compile.
  if (m_compile.valid() && m_compile.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
    CompileResult result = m_compile.get();
    if (result.ok) {
      m_unitsCompiled = result.compiled;
      m_unitsTotal = result.units;
      adopt(result.library);
      if (result.cached) {
        if (m_errorLog.empty()) {
          m_status = "live · cached build";
          std::fprintf(stderr, "[sketch] %s\n", m_status.c_str());
        } else {
          // An unreadable artifact is disposable. Compile again without it.
          std::error_code error;
          std::filesystem::remove(
              buildCacheDirectory() / (result.cacheKey + ".bin"), error);
          m_everCompiled = false;
        }
      }
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
  if (m_options.sketchPath.extension() == ".py") {
    if (pythonChanged()) loadPython();
  } else if (!m_compile.valid()) {
    if (const auto stamp = sourceStamp();
        stamp && (!m_everCompiled || *stamp != m_compiledMtime))
      startCompile();
  }

  // Asset hot reload (twice a second is plenty for filesystem stats).
  if (m_session && !m_runtimeFailed &&
      m_clock.elapsed() - m_lastAssetPoll > 0.5) {
    m_lastAssetPoll = m_clock.elapsed();
    if (m_assets.poll()) {
      PhaseMark mark(Phase::Setup);
      if (m_options.sketchPath.extension() == ".py") {
        loadPython();
      } else {
        try {
          m_session->redeclare();
        } catch (const std::exception& error) {
          sessionFailed(error);
        }
      }
    }
  }
}

bool Host::frame(SkCanvas& canvas, double fixedDt) {
  if (!m_session || m_runtimeFailed) return false;
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
    try {
      m_session->frame(canvas, fixedDt);
    } catch (const std::exception& error) {
      sessionFailed(error);
      return false;
    }
  }
  m_workMs.add(watch.elapsedMs());
  m_drawMs.add(m_session->timing().drawMs);
  return true;
}

bool Host::frame(double fixedDt) {
  const auto extent = captureExtent(canvasSize());
  if (!extent) {
    m_errorLog = "Canvas dimensions are invalid";
    return false;
  }
  SkNoDrawCanvas scratch(extent->width(), extent->height());
  if (!frame(scratch, fixedDt)) return false;
  if (!captureExtent(canvasSize())) {
    m_errorLog = "Canvas dimensions are invalid";
    return false;
  }
  return true;
}

double Host::prepareCapture(std::optional<double> at, double fps) {
  const double declared = captureSeconds();
  const double seconds = at.value_or(declared >= 0 ? declared : 1.5);
  if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(fps) ||
      fps <= 0 || !std::isfinite(1.0 / fps))
    throw std::invalid_argument("Capture time or frame rate is invalid");
  const double rate = std::max(fps, 1.0 / motion::FrameClockOptions{}.maxDelta);
  if (seconds * rate > double(std::numeric_limits<int>::max()))
    throw std::invalid_argument("Capture time or frame rate is invalid");
  const auto advance = [&](double dt) {
    if (!frame(dt))
      throw std::runtime_error(m_errorLog.empty() ? "No sketch is loaded"
                                                  : m_errorLog);
  };
  const int frames = int(std::floor(seconds * rate));
  for (int index = 0; index < frames; ++index) advance(1.0 / rate);
  const double remainder = seconds - double(frames) / rate;
  if (frames == 0 || remainder > 1e-12) advance(remainder);
  return seconds;
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

SkBitmap Host::still(float scale) {
  if (!m_session || m_runtimeFailed) return {};
  const CanvasSpecification& specification = m_session->canvas();
  const auto extent = captureExtent(specification.size, scale);
  if (!extent) {
    m_errorLog = "Capture dimensions or scale are invalid";
    return {};
  }
  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(extent->width(), extent->height());
  sk_sp<SkSurface> surface = m_captureBackend.makeSurface
                                 ? m_captureBackend.makeSurface(info)
                                 : SkSurfaces::Raster(info);
  if (!surface) return {};
  SkCanvas* through =
      m_captureBackend.canvasOf ? m_captureBackend.canvasOf(*surface) : nullptr;
  SkCanvas& canvas = through ? *through : *surface->getCanvas();
  canvas.clear(specification.background.toSkColor());
  canvas.scale(scale, scale);
  try {
    m_session->repaint(canvas);
  } catch (const std::exception& error) {
    sessionFailed(error);
    return {};
  }
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(surface->imageInfo())) return {};
  if (m_captureBackend.readback) {
    if (!m_captureBackend.readback(*surface, bitmap.pixmap())) return {};
  } else {
    if (!surface->readPixels(bitmap.pixmap(), 0, 0)) return {};
  }
  // Nothing after the readback names the device, and a caller may hand
  // these pixels to another thread: immutable is what makes that safe.
  bitmap.setImmutable();
  return bitmap;
}

bool Host::capture(const std::filesystem::path& out, float scale) {
  const SkBitmap bitmap = still(scale);
  if (bitmap.isNull()) return false;
  // The format the capture path is named for; the directories above the
  // file are the sink's business, not this one's.
  const sk_sp<SkData> png =
      image::encodeImage(bitmap.pixmap(), image::Format::Png);
  return png && io::writeBytes(out, png->data(), png->size());
}

}  // namespace sigil::sketch
