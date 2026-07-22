#include "SketchHost.h"

#include "SketchCrash.h"

#include <sigilmotion/Ticker.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/encode/SkPngEncoder.h>

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace sigil::compose::sketch {

namespace {

SketchHost::Options withDefaults(SketchHost::Options options) {
  if (options.assetsDir.empty())
    options.assetsDir = options.sketchPath.parent_path() / "assets";
  return options;
}

/** Runs a shell command, capturing stdout+stderr; returns exit code. */
int run(const std::string &command, std::string &output) {
  FILE *pipe = popen((command + " 2>&1").c_str(), "r");
  if (!pipe) {
    output = "failed to spawn: " + command;
    return -1;
  }
  char buffer[4096];
  while (size_t n = fread(buffer, 1, sizeof buffer, pipe))
    output.append(buffer, n);
  return pclose(pipe);
}

// ---- header/host skew guard ------------------------------------------------
// A sketch dylib compiled against framework headers NEWER than this host
// binary loads into a host whose structs have the OLD layout — variant
// dispatch corrupts and the crash points nowhere near the cause (it cost a
// study agent an lldb session). kAbiVersion only guards deliberate
// SketchContext changes; this guards every header edit: refuse to compile
// while any repo header on the include path postdates the running binary.

std::filesystem::file_time_type hostBinaryTime() {
  Dl_info info{};
  if (dladdr(reinterpret_cast<void *>(&hostBinaryTime), &info) &&
      info.dli_fname) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(info.dli_fname, ec);
    if (!ec)
      return t;
  }
  return {};
}

/** True when @p p is an ABI-BOUNDARY header: one whose types cross the
 *  host/dylib line (Element/Composer/Material wrappers, SketchContext,
 *  weave/motion values embedded in descriptions). Extension headers
 *  (Layouts/Lines/Brushes/Shapes/styles/patterns…) compile fresh into
 *  every dylib and stay self-contained — the host often does not even
 *  link them, so their mtimes must not wedge the guard. */
bool abiBoundaryHeader(const std::filesystem::path &p) {
  const std::string s = p.generic_string();
  if (s.find("include/sigilsketch/") != std::string::npos)
    return true;
  if (s.find("include/sigilweave/") != std::string::npos)
    return true;
  if (s.find("include/sigilmotion/") != std::string::npos)
    return true;
  if (s.find("include/sigilcompose/") != std::string::npos) {
    const std::string name = p.filename().string();
    return name == "Compose.h" || name == "Material.h";
  }
  return false;
}

/** First ABI-boundary repo header on the flags file's -I paths newer than
 *  @p hostTime (empty string when none). */
std::string newerHeaderThanHost(const std::filesystem::path &flagsFile,
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
    // Repo headers only — vcpkg/system trees are immutable in practice
    // and huge to scan.
    if (token.find("/src/") == std::string::npos)
      continue;
    for (auto it = std::filesystem::recursive_directory_iterator(token, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); ++it) {
      const std::filesystem::path &p = it->path();
      if (p.extension() != ".h" && p.extension() != ".hpp")
        continue;
      if (!abiBoundaryHeader(p))
        continue;
      auto t = std::filesystem::last_write_time(p, ec);
      if (!ec && t > hostTime)
        return p.string();
    }
  }
  return {};
}

} // namespace

SketchHost::SketchHost(Options options, sigil::weave::FontContext &fonts)
    : m_options(withDefaults(std::move(options))), m_fonts(fonts),
      m_assets(m_options.assetsDir) {
  m_buildDir = std::filesystem::temp_directory_path() /
               ("compose_sketch_" + std::to_string(getpid()));
  std::filesystem::create_directories(m_buildDir);
}

SketchHost::~SketchHost() {
  if (m_compile.valid())
    m_compile.wait();
  // Retained descriptions and animations may point into sketch-owned state.
  // Release consumers first; loaded dylibs intentionally remain mapped.
  m_composer.reset();
  m_ticker.reset();
  m_sketch.reset();
  std::error_code ec;
  std::filesystem::remove_all(m_buildDir, ec);
}

void SketchHost::startCompile() {
  std::error_code ec;
  m_compiledMtime =
      std::filesystem::last_write_time(m_options.sketchPath, ec);
  m_everCompiled = true;
  m_compileStart = std::chrono::steady_clock::now();

  // Skew guard: never hand a dylib built against newer framework headers
  // to this (older) host — the crash it prevents is unattributable.
  if (const auto hostTime = hostBinaryTime();
      hostTime != std::filesystem::file_time_type{}) {
    if (const std::string stale =
            newerHeaderThanHost(m_options.flagsFile, hostTime);
        !stale.empty()) {
      m_errorLog =
          "framework headers are NEWER than this host binary (" + stale +
          ") — rebuild ComposeSketch before iterating; a sketch compiled "
          "against skewed headers would corrupt the host ABI";
      m_status = "stale host — rebuild ComposeSketch";
      return; // keep the previous sketch alive, p5 style
    }
  }
  m_status = "compiling build " + std::to_string(m_generation + 1) +
             "…";

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
                         [command = cmd.str(), out]() -> CompileResult {
                           CompileResult result;
                           result.library = out;
                           result.ok =
                               run(command, result.output) == 0;
                           return result;
                         });
}

void SketchHost::adopt(const std::filesystem::path &library) {
  void *handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    m_errorLog = dlerror();
    m_status = "load failed";
    return;
  }
  auto abi =
      reinterpret_cast<unsigned (*)()>(dlsym(handle, "sigilSketchAbi"));
  auto create = reinterpret_cast<Sketch *(*)()>(
      dlsym(handle, "sigilSketchCreate"));
  if (!abi || !create || abi() != kAbiVersion) {
    m_errorLog = "sketch ABI mismatch — is SIGIL_SKETCH(...) present? "
                 "(after framework changes, restart the host)";
    m_status = "load failed";
    return;
  }
  m_libraries.push_back(handle);

  // Fresh world for the new sketch; the clock keeps running so time is
  // continuous across reloads. The canvas spec resets to defaults —
  // each sketch declares its own via ctx.canvas()/ctx.background().
  // The old Composer retains programs/bindings supplied by the old sketch;
  // destroy it (and its motions) before destroying their owner.
  m_composer.reset();
  m_ticker.reset();
  m_sketch.reset();
  m_canvasSpec = CanvasSpec{};
  m_ticker = std::make_unique<sigil::motion::Ticker>();
  m_composer = std::make_unique<Composer>(*m_ticker, m_fonts);
  m_composer->setSize(m_canvasSpec.size);
  m_appliedSize = m_canvasSpec.size;
  m_composer->setClock(&m_clock);
  m_sketch.reset(create());
  SketchContext ctx = makeContext();
  {
    PhaseMark mark(Phase::Setup);
    m_sketch->setup(ctx);
  }
  applyCanvasSpec();
  m_errorLog.clear();
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                    m_compileStart)
          .count();
  char line[128];
  std::snprintf(line, sizeof line, "live · build %d · compiled in %.1fs",
                m_generation, seconds);
  m_status = line;
  m_workMs.clear(); // fresh sketch, fresh numbers
  std::fprintf(stderr, "[sketch] %s\n", m_status.c_str());
}

void SketchHost::poll() {
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
    std::error_code ec;
    const auto mtime =
        std::filesystem::last_write_time(m_options.sketchPath, ec);
    if (!ec && (!m_everCompiled || mtime != m_compiledMtime))
      startCompile();
  }

  // Asset hot reload (twice a second is plenty for filesystem stats).
  if (m_sketch && m_clock.elapsed() - m_lastAssetPoll > 0.5) {
    m_lastAssetPoll = m_clock.elapsed();
    if (m_assets.poll()) {
      SketchContext ctx = makeContext();
      {
        PhaseMark mark(Phase::Setup);
        m_sketch->setup(ctx);
      }
      applyCanvasSpec();
    }
  }
}

SketchContext SketchHost::makeContext() {
  return SketchContext{*m_composer,      *m_ticker,     m_assets,
                       m_canvasSpec.size, &m_canvasSpec, &m_fonts};
}

void SketchHost::applyCanvasSpec() {
  if (m_composer && m_appliedSize != m_canvasSpec.size) {
    m_composer->setSize(m_canvasSpec.size);
    m_appliedSize = m_canvasSpec.size;
  }
}

bool SketchHost::capture(const std::filesystem::path &out,
                         float scale) {
  if (!m_composer)
    return false;
  const SkSize size = m_canvasSpec.size;
  const SkImageInfo info = SkImageInfo::MakeN32Premul(
      std::max(1, (int)(size.width() * scale)),
      std::max(1, (int)(size.height() * scale)));
  sk_sp<SkSurface> surface = m_captureBackend.makeSurface
                                 ? m_captureBackend.makeSurface(info)
                                 : SkSurfaces::Raster(info);
  if (!surface)
    return false;
  SkCanvas &canvas = *surface->getCanvas();
  canvas.clear(m_canvasSpec.background.toSkColor());
  canvas.scale(scale, scale);
  m_composer->draw(canvas);
  SkBitmap bitmap;
  bitmap.allocPixels(surface->imageInfo());
  if (m_captureBackend.readback) {
    if (!m_captureBackend.readback(*surface, bitmap.pixmap()))
      return false;
  } else {
    surface->readPixels(bitmap.pixmap(), 0, 0);
  }
  std::error_code ec;
  std::filesystem::create_directories(out.parent_path(), ec);
  SkFILEWStream stream(out.string().c_str());
  return stream.isValid() &&
         SkPngEncoder::Encode(&stream, bitmap.pixmap(), {});
}

bool SketchHost::frame(SkCanvas &canvas, double fixedDt) {
  if (!m_sketch)
    return false;
  const auto start = std::chrono::steady_clock::now();
  double dt;
  if (fixedDt >= 0) { // deterministic stepping (headless capture)
    m_syntheticNow += fixedDt;
    dt = m_clock.tick(m_syntheticNow);
    if (dt <= 0.0) // first tick primes the clock and reports zero
      dt = fixedDt;
  } else {
    dt = m_clock.tick();
  }
  m_ticker->tick(dt);
  noteFrame(++m_frameIndex, m_clock.elapsed());
  SketchContext ctx = makeContext();
  {
    PhaseMark mark(Phase::Update);
    m_sketch->update(m_clock.elapsed(), ctx);
  }
  applyCanvasSpec(); // ctx.canvas() mid-run = p5's resizeCanvas
  {
    PhaseMark mark(Phase::Draw);
    m_composer->draw(canvas);
  }
  const double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  if (m_workMs.size() >= 120)
    m_workMs.erase(m_workMs.begin());
  m_workMs.push_back(ms);
  return true;
}

double SketchHost::workMsAverage() const {
  if (m_workMs.empty())
    return 0.0;
  double sum = 0.0;
  for (double v : m_workMs)
    sum += v;
  return sum / (double)m_workMs.size();
}

double SketchHost::workMsP99() const {
  if (m_workMs.empty())
    return 0.0;
  std::vector<double> sorted = m_workMs;
  std::sort(sorted.begin(), sorted.end());
  return sorted[(size_t)((double)(sorted.size() - 1) * 0.99)];
}

double SketchHost::presentedFps() const {
  if (m_presentMs.size() < 2)
    return 0.0;
  double sum = 0.0;
  for (double v : m_presentMs)
    sum += v;
  const double mean = sum / (double)m_presentMs.size();
  return mean > 0 ? 1000.0 / mean : 0.0;
}

void SketchHost::markPresented() {
  const auto now = std::chrono::steady_clock::now();
  if (m_lastPresent.time_since_epoch().count() != 0) {
    const double ms =
        std::chrono::duration<double, std::milli>(now - m_lastPresent)
            .count();
    if (ms < 1000.0) { // ignore stalls (window drags, sleeps)
      if (m_presentMs.size() >= 60)
        m_presentMs.erase(m_presentMs.begin());
      m_presentMs.push_back(ms);
    }
  }
  m_lastPresent = now;
}

} // namespace sigil::compose::sketch
