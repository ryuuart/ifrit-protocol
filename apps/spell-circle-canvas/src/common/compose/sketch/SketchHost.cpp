#include "SketchHost.h"

#include <ifrittick/Ticker.h>

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace ifrit::compose::sketch {

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

} // namespace

SketchHost::SketchHost(Options options, textflow::FontContext &fonts)
    : m_options(withDefaults(std::move(options))), m_fonts(fonts),
      m_assets(m_options.assetsDir) {
  m_buildDir = std::filesystem::temp_directory_path() /
               ("compose_sketch_" + std::to_string(getpid()));
  std::filesystem::create_directories(m_buildDir);
}

SketchHost::~SketchHost() {
  if (m_compile.valid())
    m_compile.wait();
  m_sketch.reset(); // destroy before its library would go away
  std::error_code ec;
  std::filesystem::remove_all(m_buildDir, ec);
}

void SketchHost::startCompile() {
  std::error_code ec;
  m_compiledMtime =
      std::filesystem::last_write_time(m_options.sketchPath, ec);
  m_everCompiled = true;
  m_compileStart = std::chrono::steady_clock::now();
  m_status = "compiling build " + std::to_string(m_generation + 1) +
             "…";

  const std::filesystem::path out =
      m_buildDir / ("sketch_" + std::to_string(++m_generation) + ".dylib");
  std::ostringstream cmd;
  cmd << m_options.compiler << " @" << m_options.flagsFile
#ifdef __APPLE__
      << " -shared -undefined dynamic_lookup"
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
      reinterpret_cast<unsigned (*)()>(dlsym(handle, "ifritSketchAbi"));
  auto create = reinterpret_cast<Sketch *(*)()>(
      dlsym(handle, "ifritSketchCreate"));
  if (!abi || !create || abi() != kAbiVersion) {
    m_errorLog = "sketch ABI mismatch — is IFRIT_SKETCH(...) present? "
                 "(after framework changes, restart the host)";
    m_status = "load failed";
    return;
  }
  m_libraries.push_back(handle);

  // Fresh world for the new sketch; the clock keeps running so time is
  // continuous across reloads.
  m_sketch.reset();
  m_ticker = std::make_unique<ifrit::tick::Ticker>();
  m_composer = std::make_unique<Composer>(*m_ticker, m_fonts);
  m_composer->setSize(kCanvasSize);
  m_composer->setClock(&m_clock);
  m_sketch.reset(create());
  SketchContext ctx{*m_composer, *m_ticker, m_assets, kCanvasSize};
  m_sketch->setup(ctx);
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
      SketchContext ctx{*m_composer, *m_ticker, m_assets, kCanvasSize};
      m_sketch->setup(ctx);
    }
  }
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
  SketchContext ctx{*m_composer, *m_ticker, m_assets, kCanvasSize};
  m_sketch->update(m_clock.elapsed(), ctx);
  m_composer->draw(canvas);
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

} // namespace ifrit::compose::sketch
