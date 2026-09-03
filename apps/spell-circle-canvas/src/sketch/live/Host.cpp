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
#include <sigilimage/encode/Encode.h>
#include <sigilloader/source/Sink.h>
#include <sigilsketch/core/Sources.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>

#include "sigilsketch/core/Crash.h"

namespace sigil::sketch {

namespace {

Host::Options withDefaults(Host::Options options) {
  if (options.assetsDir.empty())
    options.assetsDir = options.sketchPath.parent_path() / "assets";
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

/** One unit to its object, with the flags the build captured. */
std::string compileLine(const Host::Options& options,
                        const std::filesystem::path& source,
                        const std::filesystem::path& object) {
  std::ostringstream cmd;
  cmd << options.compiler << " @" << options.flagsFile << " -c -o " << object
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

// ---- header/host skew guard ----------------------------------------------
// A sketch dylib compiled against framework headers NEWER than the host
// binary loads into a host whose structs have the OLD layout: dispatch
// corrupts and the crash points nowhere near the cause. The ABI version
// guards deliberate changes to the sketch surface; this guards every
// header edit, by refusing to compile while any repository header on the
// include path postdates the running binary.

std::filesystem::file_time_type hostBinaryTime() {
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(&hostBinaryTime), &info) &&
      info.dli_fname) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(info.dli_fname, ec);
    if (!ec) return t;
  }
  return {};
}

/** True when @p p is an ABI-BOUNDARY header: one whose types cross the
 *  host/dylib line.
 *
 *  EVERY PUBLIC HEADER OF A FRAMEWORK LIBRARY IS ONE, and the line is
 *  drawn there rather than at a list of file names. A sketch constructs
 *  the libraries' objects and the host mutates them — a pool filled in the
 *  dylib and resized by the host, an element built in the dylib and
 *  reconciled by the host — so any header that changes a layout either
 *  side reads changes it on ONE side only, and the corruption surfaces
 *  wherever the object is next touched rather than where it was caused. A
 *  header believed harmless is exactly the header that gets one wrong. */
bool abiBoundaryHeader(const std::filesystem::path& p) {
  return p.generic_string().find("/include/sigil") != std::string::npos;
}

/** The first ABI-boundary repository header on the flags file's -I paths
 *  that is newer than @p hostTime; empty when none is. */
std::string newerHeaderThanHost(const std::filesystem::path& flagsFile,
                                std::filesystem::file_time_type hostTime) {
  std::ifstream flags(flagsFile);
  std::string token;
  std::error_code ec;
  while (flags >> token) {
    if (token.size() > 2 && token.compare(0, 2, "-I") == 0)
      token.erase(0, 2);
    else
      continue;
    if (!token.empty() && token.front() == '"')
      token = token.substr(1, token.size() - 2);
    // Repository headers only — dependency trees are immutable in
    // practice and huge to scan.
    if (token.find("/src/") == std::string::npos) continue;
    for (auto it = std::filesystem::recursive_directory_iterator(token, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); ++it) {
      const std::filesystem::path& p = it->path();
      if (p.extension() != ".h" && p.extension() != ".hpp") continue;
      if (!abiBoundaryHeader(p)) continue;
      auto t = std::filesystem::last_write_time(p, ec);
      if (!ec && t > hostTime) return p.string();
    }
  }
  return {};
}

constexpr CanvasSpec kUnloaded{};

}  // namespace

Host::Host(Options options, weave::FontContext& fonts)
    : m_options(withDefaults(std::move(options))),
      m_fonts(fonts),
      m_assets(m_options.assetsDir) {
  m_buildDir = std::filesystem::temp_directory_path() /
               ("sigil_sketch_" + std::to_string(getpid()));
  std::filesystem::create_directories(m_buildDir);
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
    std::vector<std::filesystem::path> shared = sourcesUnder(m_options.sharedDir);
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
  // to an older host — the crash it prevents is unattributable.
  if (const auto hostTime = hostBinaryTime();
      hostTime != std::filesystem::file_time_type{}) {
    if (const std::string stale =
            newerHeaderThanHost(m_options.flagsFile, hostTime);
        !stale.empty()) {
      m_errorLog =
          "framework headers are NEWER than this host binary (" + stale +
          ").\n"
          "A sketch compiled against skewed headers would corrupt the host "
          "ABI, so this build is refused rather than risked.\n\n"
          "If you are ONE AGENT IN A SHARED SESSION: this is normal and "
          "expected. Someone changed the library and the host is being "
          "rebuilt. WAIT A MOMENT AND RE-RUN THIS EXACT COMMAND. Do not run "
          "cmake or ninja yourself — the build directory is shared and a "
          "second build will corrupt it. If it persists past a few "
          "minutes, say so rather than working around it.\n\n"
          "If you OWN this checkout: rebuild the host.";
      m_status = "stale host — waiting for a rebuild";
      return;  // keep the previous sketch alive
    }
  }
  m_status = "compiling build " + std::to_string(m_generation + 1) + "…";

  const std::filesystem::path out =
      m_buildDir / ("sketch_" + std::to_string(++m_generation) + ".dylib");
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
        // The stale units compile side by side, as many at once as the
        // machine has cores: a sketch of several units then takes as
        // long as its slowest one, which is the entry being edited.
        std::vector<std::string> outputs(compiles.size());
        std::vector<int> codes(compiles.size(), 0);
        std::atomic<size_t> next{0};
        const auto work = [&] {
          for (size_t i; (i = next.fetch_add(1)) < compiles.size();)
            codes[i] = run(compiles[i], outputs[i]);
        };
        const size_t workers = std::min(
            compiles.size(),
            std::max<size_t>(1, std::thread::hardware_concurrency()));
        std::vector<std::thread> pool;
        for (size_t w = 1; w < workers; ++w) pool.emplace_back(work);
        work();
        for (std::thread& worker : pool) worker.join();
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
  if (!abi || !exported || abi() != kAbiVersion) {
    m_errorLog =
        "sketch ABI mismatch — is SIGIL_SKETCH(...) present? "
        "(after framework changes, restart the host)";
    m_status = "load failed";
    return;
  }
  m_libraries.push_back(handle);

  const Entry* entry = exported();
  if (!entry || !entry->kind) {
    m_errorLog = "the sketch exported no kind";
    m_status = "load failed";
    return;
  }
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
        m_built[unit.source] = Built{unit.object, unit.sourceTime,
                                     result.headers};
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
  return png && loader::writeBytes(out, png->data(), png->size());
}

}  // namespace sigil::sketch
