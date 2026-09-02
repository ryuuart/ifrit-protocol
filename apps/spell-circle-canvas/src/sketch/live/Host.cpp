/** @file
 * The reload loop: watch, compile, dlopen, swap — and keep the last good
 * sketch running while a build is broken.
 */

#include "sigilsketch/live/Host.h"

#include <dlfcn.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/encode/SkPngEncoder.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "sigilsketch/live/Crash.h"

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
 *  host/dylib line. Extension headers compile fresh into every dylib and
 *  stay self-contained — the host often does not even link them, so
 *  their mtimes must not wedge the guard. */
bool abiBoundaryHeader(const std::filesystem::path& p) {
  const std::string s = p.generic_string();
  if (s.find("include/sigilsketch/") != std::string::npos) return true;
  if (s.find("include/sigilweave/") != std::string::npos) return true;
  if (s.find("include/sigilmotion/") != std::string::npos) return true;
  if (s.find("include/sigilworld/") != std::string::npos) return true;
  if (s.find("include/sigilcompose/") != std::string::npos) {
    const std::string name = p.filename().string();
    return name == "Compose.h" || name == "Material.h";
  }
  return false;
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
}

SkSize Host::canvasSize() const {
  return m_session ? m_session->canvas().size : kUnloaded.size;
}

SkColor4f Host::background() const {
  return m_session ? m_session->canvas().background : kUnloaded.background;
}

std::optional<std::filesystem::file_time_type> Host::sourceStamp() {
  std::error_code ec;
  const auto self = std::filesystem::last_write_time(m_options.sketchPath, ec);
  if (ec) return std::nullopt;
  // The headers standing beside the sketch are part of it — a quoted
  // include resolves relative to the including file, so a directory of
  // sketches shares helpers with none of them on the include path. They
  // are re-read a few times a second rather than every poll: a directory
  // read is cheap but it is not free, and a header is saved by hand
  // while the sketch is saved by the same hand a moment later.
  const auto now = std::chrono::steady_clock::now();
  if (m_lastSiblingScan.time_since_epoch().count() == 0 ||
      now - m_lastSiblingScan > std::chrono::milliseconds(250)) {
    m_lastSiblingScan = now;
    m_siblingStamp = {};
    std::error_code scan;
    for (auto it = std::filesystem::directory_iterator(
             m_options.sketchPath.parent_path(), scan);
         !scan && it != std::filesystem::directory_iterator(); ++it) {
      const std::filesystem::path& p = it->path();
      if (p.extension() != ".h" && p.extension() != ".hpp") continue;
      std::error_code stat;
      const auto t = std::filesystem::last_write_time(p, stat);
      if (!stat && t > m_siblingStamp) m_siblingStamp = t;
    }
  }
  return std::max(self, m_siblingStamp);
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
  std::ostringstream cmd;
  cmd << m_options.compiler << " @" << m_options.flagsFile
#ifdef __APPLE__
      << " -shared -undefined dynamic_lookup -Wl,-dead_strip"
#else
      << " -shared"
#endif
      << " -o " << out << ' ' << m_options.sketchPath;

  m_compile = std::async(std::launch::async,
                         // copying the captures can fail only on allocation
                         // NOLINTNEXTLINE(bugprone-exception-escape)
                         [command = cmd.str(), out]() -> CompileResult {
                           CompileResult result;
                           result.library = out;
                           result.ok = run(command, result.output) == 0;
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
  char line[128];
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
  if (m_session && m_elapsed - m_lastAssetPoll > 0.5) {
    m_lastAssetPoll = m_elapsed;
    if (m_assets.poll()) {
      PhaseMark mark(Phase::Setup);
      m_session->redeclare();
    }
  }
}

bool Host::frame(SkCanvas& canvas, double fixedDt) {
  if (!m_session) return false;
  const auto start = std::chrono::steady_clock::now();
  m_elapsed += fixedDt >= 0 ? fixedDt : 0.0;
  noteFrame(++m_frameIndex, m_elapsed);
  {
    PhaseMark mark(Phase::Update);
    m_session->frame(canvas, fixedDt);
  }
  const double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  if (m_workMs.size() >= 120) m_workMs.erase(m_workMs.begin());
  m_workMs.push_back(ms);
  return true;
}

double Host::workMsAverage() const {
  if (m_workMs.empty()) return 0.0;
  double sum = 0.0;
  for (double v : m_workMs) sum += v;
  return sum / (double)m_workMs.size();
}

double Host::workMsP99() const {
  if (m_workMs.empty()) return 0.0;
  std::vector<double> sorted = m_workMs;
  std::sort(sorted.begin(), sorted.end());
  return sorted[(size_t)((double)(sorted.size() - 1) * 0.99)];
}

double Host::presentedFps() const {
  if (m_presentMs.size() < 2) return 0.0;
  double sum = 0.0;
  for (double v : m_presentMs) sum += v;
  const double mean = sum / (double)m_presentMs.size();
  return mean > 0 ? 1000.0 / mean : 0.0;
}

void Host::markPresented() {
  const auto now = std::chrono::steady_clock::now();
  if (m_lastPresent.time_since_epoch().count() != 0) {
    const double ms =
        std::chrono::duration<double, std::milli>(now - m_lastPresent).count();
    if (ms < 1000.0) {  // ignore stalls (window drags, sleeps)
      if (m_presentMs.size() >= 60) m_presentMs.erase(m_presentMs.begin());
      m_presentMs.push_back(ms);
    }
  }
  m_lastPresent = now;
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
  std::error_code ec;
  std::filesystem::create_directories(out.parent_path(), ec);
  SkFILEWStream stream(out.string().c_str());
  return stream.isValid() && SkPngEncoder::Encode(&stream, bitmap.pixmap(), {});
}

}  // namespace sigil::sketch
