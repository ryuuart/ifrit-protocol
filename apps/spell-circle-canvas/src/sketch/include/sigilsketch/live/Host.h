#pragma once

/** @file
 * The live-reload host: a sketch watched, rebuilt into a dylib on save,
 * and hot-swapped into the running session.
 */

#include <include/core/SkRefCnt.h>
#include <sigilmeasure/stats/Samples.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class SkImageInfo;
class SkPixmap;
class SkSurface;

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

/** THE LIVE HOST, and it is Qt-free on purpose: it watches the sketch's
 *  sources, rebuilds them into a versioned dylib with the compiler flags
 *  the build captured, dlopens the result and swaps the running session
 *  — keeping the previous one alive on a compile error, which is the
 *  behaviour that makes live coding usable.
 *
 *  The host executable exports the framework's symbols, so a sketch
 *  dylib links with `-undefined dynamic_lookup` and builds in a couple
 *  of seconds: a few small translation units, nothing linked against
 *  the static libraries. A sketch that is a directory is the entry and
 *  every source beside it, compiled apart and linked once — and a unit
 *  whose source and headers are the ones it was last compiled from is
 *  not compiled again, so a table standing in its own unit costs a
 *  reload of the entry nothing but the link.
 *
 *  Old libraries are never dlclosed. Their statics stay valid — a
 *  running session may hold a vtable, a string literal or a function
 *  pointer that lives in one — and one small leak per reload is the
 *  trade every host in this family makes. */
class Host {
 public:
  struct Options {
    /** The sketch's ENTRY: the file to watch, and the one whose
     *  directory says what else is built with it. A file standing in a
     *  directory of its own name is the entry of a directory sketch,
     *  and every other `.cpp` in that directory is a unit of it; any
     *  other file is a sketch of one unit. */
    std::filesystem::path sketchPath;
    /** Where the sketch looks for what it did not generate. Defaults to
     *  `assets` beside the sketch file. */
    std::filesystem::path assetsDir;
    /** The compiler line the build captured, beside the executable. */
    std::filesystem::path flagsFile;
    std::string compiler = "clang++";
    /** THE SHARED LAYER: a directory whose sources are units of every
     *  sketch this host builds, and whose headers a sketch spells as
     *  `<shared/Name.h>`. Watched with the sketch's own files. Empty
     *  for none. */
    std::filesystem::path sharedDir;
    /** Pin anything a sketch measured about its own execution, so a
     *  capture can be diffed. */
    bool deterministic = false;
    /** Start from the sketch already compiled into this binary rather
     *  than by building the file, and compile only once the file
     *  changes. Null means always build. */
    const Entry* compiledIn = nullptr;
    /** How long between re-reads of the directories the sketch is
     *  built from — the one beside the entry and the shared layer. The
     *  entry itself is stamped every poll; the directories around it
     *  are not, because reading a directory is cheap but not free and
     *  a header is saved by hand a moment before the sketch is. Zero
     *  re-reads them on every poll. */
    std::chrono::milliseconds siblingScanInterval{250};
  };

  Host(Options options, weave::FontContext& fonts);
  ~Host();

  /** WHERE THIS HOST BUILDS: one object per unit and one dylib per
   *  build, under a directory named for the running process.
   *
   *  Every host in a process shares it — a window keeps three sketches
   *  resident, each with a host of its own — so it is made with the
   *  first of them and removed with the last, and again on normal exit
   *  for a process that ends without unwinding that far. Nothing on
   *  disk survives usefully past the run: the freshness table that
   *  decides a rebuild is in memory, so no later process reads a byte
   *  of it. */
  [[nodiscard]] const std::filesystem::path& buildDir() const {
    return m_buildDir;
  }

  /** Removes the build directories of processes that are no longer
   *  running, beside the one this process builds in.
   *
   *  A run that was killed or that faulted never reached the removal
   *  above, and its directory carries a pid no later run can reuse, so
   *  nothing would ever clear it. The pid in the name is asked of the
   *  system directly, and only the answer that says NOBODY HOLDS IT
   *  removes anything: a directory whose process is alive — this
   *  process's own included — is left standing. Runs once by itself
   *  before the first host makes its directory; calling it again is
   *  harmless. */
  static void sweepAbandonedBuildDirs();

  /** Drives the reload machinery: source mtime, finished compiles, asset
   *  changes. Call once per frame. */
  void poll();

  /** Ticks and draws one frame. Returns false while nothing has ever
   *  loaded. A negative @p fixedDt uses wall time. */
  bool frame(SkCanvas& canvas, double fixedDt = -1.0);

  [[nodiscard]] bool compiling() const { return m_compile.valid(); }
  [[nodiscard]] bool live() const { return m_session != nullptr; }
  [[nodiscard]] int generation() const { return m_generation; }
  /** The running session, for a host that needs more than a frame from
   *  it — its counters, its viewpoint, its per-node costs. Null until
   *  something has loaded. */
  [[nodiscard]] Session* session() { return m_session.get(); }

  /** The lifecycle a status display reads: Compiling wins even while a
   *  previous build keeps rendering underneath. */
  enum class State { Waiting, Compiling, Live, Failed };
  [[nodiscard]] State state() const {
    if (m_compile.valid()) return State::Compiling;
    if (!m_errorLog.empty()) return State::Failed;
    return m_session ? State::Live : State::Waiting;
  }

  /** Honest frame metrics over a rolling window: the full frame body is
   *  timed, presentation is what the host reports via markPresented(). */
  [[nodiscard]] double workMsAverage() const;
  [[nodiscard]] double workMsP99() const;
  /** The session's own PAINT phase over the same rolling window. It is
   *  inside the work above, and it is worth having on its own because
   *  for a set drawn on a device it is where the readback and the blit
   *  onto the canvas land — the host cost a frame-time gate rendering
   *  onto a raster surface never pays. */
  [[nodiscard]] double drawMsAverage() const;
  [[nodiscard]] double presentedFps() const;
  void markPresented();
  /** BEGINS PRESENTING AGAIN after a stretch in which something else
   *  held the window. That stretch is not a frame interval, so the next
   *  presentation starts one rather than extending the one this session
   *  was paused in the middle of — the rolling windows themselves stay,
   *  which is the point of a session outliving the look away from it. */
  void resume() { m_presentSince.reset(); }

  /** Renders the CURRENT state (clock untouched) into a PNG at @p scale
   *  times the sketch's canvas. The capture path for both the windowed
   *  save command and headless asset generation. */
  bool capture(const std::filesystem::path& out, float scale = 1.0f);

  /** A host on the device must route capture through its own backend:
   *  once live frames render on the GPU, the runtime's caches hold
   *  device-backed images that cannot replay onto a raster canvas.
   *  makeSurface builds the capture target and readback fetches its
   *  pixels after the draw, both on whichever thread calls capture().
   *  Unset is the CPU raster path. */
  struct CaptureBackend {
    std::function<sk_sp<SkSurface>(const SkImageInfo&)> makeSurface;
    std::function<bool(SkSurface&, const SkPixmap&)> readback;
  };
  void setCaptureBackend(CaptureBackend backend) {
    m_captureBackend = std::move(backend);
  }

  /** One line of state for a status bar. */
  [[nodiscard]] const std::string& status() const { return m_status; }
  /** Full compiler or loader output of the most recent failure; empty
   *  when the latest build is good. */
  [[nodiscard]] const std::string& errorLog() const { return m_errorLog; }

  [[nodiscard]] const std::filesystem::path& sketchPath() const {
    return m_options.sketchPath;
  }
  /** The canvas the running sketch declared; hosts letterbox to this
   *  size and clear with this colour. */
  [[nodiscard]] SkSize canvasSize() const;
  [[nodiscard]] SkColor4f background() const;
  /** THE SCENE TIME THE RUNNING SKETCH DECLARED a still of itself should
   *  be taken at, or a negative number where it declared none — the
   *  moment a capture steps to unless the caller names another.
   *
   *  A body declares it from inside its own setup, so it is only
   *  truthful once something has loaded; before that it is negative,
   *  which reads as "no preference" exactly as an undeclaring sketch
   *  does. */
  [[nodiscard]] double captureSeconds() const;

 private:
  /** One translation unit on a build's compile line, and where its
   *  object goes. */
  struct Unit {
    std::filesystem::path source;
    std::filesystem::path object;
    std::filesystem::file_time_type sourceTime;
  };
  struct CompileResult {
    bool ok = false;
    std::filesystem::path library;
    std::string output;
    /** The units this build compiled — recorded as built once the
     *  build is adopted, under the header stamp they were compiled
     *  against — and how many the sketch has in all. */
    std::vector<Unit> compiled;
    std::filesystem::file_time_type headers;
    int units = 0;
  };
  /** WHAT A UNIT WAS LAST COMPILED FROM. Its object is reused while its
   *  source and the headers around the sketch are the ones it was
   *  compiled against. The headers are ONE stamp for every unit rather
   *  than a dependency list per unit: any header beside the sketch or
   *  in the shared layer may be included by any unit, and re-reading
   *  two small directories is cheaper than asking the compiler which
   *  unit includes what. */
  struct Built {
    std::filesystem::path object;
    std::filesystem::file_time_type source;
    std::filesystem::file_time_type headers;
  };

  void startCompile();
  void adopt(const std::filesystem::path& library);
  void openSession(const Kind& kind);
  /** THE NEWEST WRITE ACROSS EVERYTHING THE SKETCH IS BUILT FROM, or
   *  nothing when the entry itself is not there.
   *
   *  A sketch is more than one file: a helper beside it is reached by a
   *  quoted include, which resolves relative to the including file and
   *  needs no include path; a directory sketch has units beside its
   *  entry; the shared layer has both. An edit to any of them has to
   *  rebuild the sketch, or what stays on screen is the code that stood
   *  before it. */
  [[nodiscard]] std::optional<std::filesystem::file_time_type> sourceStamp();
  /** Re-reads the directories the sketch is built from into the two
   *  stamps below. */
  void scanBeside();
  /** Every unit the next build compiles or reuses, in compile order:
   *  the entry, the sources beside it when it is a directory sketch,
   *  then the shared layer's. */
  [[nodiscard]] std::vector<std::filesystem::path> units() const;

  Options m_options;
  weave::FontContext& m_fonts;
  std::filesystem::path m_buildDir;
  /** WHICH HOST IN THIS PROCESS THIS IS, counted from one. Every host in
   *  a process links into one build directory, so the id is in the name
   *  of every dylib this one builds: without it two hosts building at
   *  once would write one path, and the file standing there when one of
   *  them dlopens would be whichever link finished last. */
  int m_hostId = 0;

  Assets m_assets;
  Kind m_kind;
  std::unique_ptr<Session> m_session;
  std::vector<void*> m_libraries;  // never dlclosed (statics stay valid)

  std::future<CompileResult> m_compile;
  std::filesystem::file_time_type m_compiledMtime;
  // The directories around the sketch, re-read on the cadence the
  // options name rather than every poll: reading a directory is not
  // per-frame work, and the file being typed into is where
  // responsiveness is wanted. One stamp for the headers, which decide
  // whether a cached object is still good, and one for the other
  // sources, which only ever mean a rebuild.
  std::filesystem::file_time_type m_headerStamp;
  std::filesystem::file_time_type m_unitStamp;
  std::chrono::steady_clock::time_point m_lastSiblingScan;
  std::map<std::filesystem::path, Built> m_built;
  int m_unitsCompiled = 0;  // of the last adopted build, for its status line
  int m_unitsTotal = 0;
  bool m_everCompiled = false;
  int m_generation = 0;
  int m_frameIndex = -1;  // for the crash reporter's phase line
  /** How long this host has been running, in its own time — stated
   *  deltas under a fixed step, wall time when it is free-running. The
   *  asset poll and the crash reporter's frame line read it, and both
   *  want the same clock the session is stepped by. */
  motion::FrameClock m_clock;
  double m_lastAssetPoll = 0.0;
  std::chrono::steady_clock::time_point m_compileStart;
  // Absent until the first presentation: there is no interval to measure
  // from before one, and resume() empties it for the same reason.
  std::optional<measure::Stopwatch> m_presentSince;
  measure::Samples m_workMs{120};    // rolling frame-body cost window
  measure::Samples m_drawMs{120};    // …and the paint phase inside it
  measure::Samples m_presentMs{60};  // rolling present-interval window
  std::string m_status = "waiting for first build";
  std::string m_errorLog;
  CaptureBackend m_captureBackend;
};

}  // namespace sigil::sketch
