#pragma once

/** @file
 * @ingroup sketch-live
 *
 * The live host watches source files or externally built native modules
 * and swaps a successful replacement into the running session.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkRefCnt.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmeasure/advanced/FrameTimer.h>
#include <sigilmeasure/stats/Window.h>
#include <sigilmotion/advanced/ClockPolicy.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class SkImageInfo;
class SkPixmap;
class SkSurface;

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

/** THE BUILD IDENTITY OF THE IMAGE THAT IS RUNNING: the last-write time
 *  the executable carried when this process first asked.
 *
 *  It is what the reload skew guard compares framework headers against —
 *  a dylib compiled against headers newer than this stamp would load into
 *  a host whose structs have the old layout.
 *
 *  READ ONCE AND KEPT. The file on disk is replaced while the process
 *  that mapped it keeps running, and a stamp re-read after that rebuild
 *  postdates every header — which is exactly the moment the guard exists
 *  for, and exactly the moment a fresh stat would let pass. A zero stamp
 *  means the image could not be located. */
[[nodiscard]] std::filesystem::file_time_type hostBinaryTime();

/** The native runtime's public C++ boundary fingerprint, including its
 *  transitive public headers, dependency binaries and compile context.
 *  A plugin must match the runtime and every additional originating library
 *  it consumes before its factory can be called. */
[[nodiscard]] std::string_view hostBuildIdentity();

/** The Qt-free live host watches a native plugin artifact or sketch sources.
 *  An artifact is compiled by the caller's toolchain; source mode captures
 *  the framework's flags and builds changed units into a versioned module.
 *  A compatible replacement opens a candidate session before replacing the
 *  running one, so an unsuccessful build or load keeps the last session.
 *
 *  The executable exports the framework symbols that a guest resolves.
 *  Accepted libraries remain mapped for the process lifetime because
 *  sessions, values and callbacks can retain their code beyond this host.
 *  Each adopted generation therefore retains its mapped image; restarting
 *  the process reclaims those images. */
class Host {
 public:
  /** Selects artifact mode with pluginPath, or source mode with sketchPath.
   *  Other fields configure asset mounts, source compilation and watching.
   *  The options are read once when the host is constructed. */
  struct Options {
    /** The sketch's ENTRY: the file to watch, and the one whose
     *  directory says what else is built with it. A file standing in a
     *  directory of its own name is the entry of a directory sketch,
     *  and every other `.cpp` in that directory is a unit of it; any
     *  other file is a sketch of one unit. */
    std::filesystem::path sketchPath;
    /** An already compiled native sketch module to watch and load.
     *  Nonempty chooses the artifact path in place of source compilation;
     *  no compiler or flags file is needed. The matching .sigil-build sidecar
     *  binds its bytes to this host build before loading. Each replacement is
     *  copied into a unique runtime file before loading. Its containing
     *  directory supplies local assets, and its stem is the session's key. */
    std::filesystem::path pluginPath;
    /** Optional importer for Python entries. The host owns watching and
     *  adoption; the importer returns a native kind and owns its interpreter.
     *  Null leaves Python files unavailable without adding an interpreter
     *  dependency to the C++ host. */
    Kind (*pythonLoader)(const std::filesystem::path&) = nullptr;
    /** Prepares host-owned executors for a candidate kind before its
     *  availability probe and session setup. Called again on reload and
     *  restart. Throwing rejects the candidate and retains the running
     *  session. Resources used by accepted sessions must remain alive. */
    std::function<void(const Kind&)> prepareSession;
    /** What mounts at `res://`. For a file opened by path it defaults to
     *  `assets` beside that file; for a sketch this binary carries the
     *  process states its root, and empty mounts nothing. */
    std::filesystem::path assetsDirectory;
    /** THE DIRECTORY THE COMPILED-IN SKETCHES STAND IN, mounted at
     *  `sketch://` so a sketch this binary carries reaches its own files
     *  through `local()`; a file opened by path has its own directory
     *  mounted under its key instead. Empty mounts nothing. */
    std::filesystem::path sketchesDirectory;
    /** The compiler line the build captured, beside the executable. */
    std::filesystem::path flagsFile;
    /** THE COMMAND THAT COMPILES AND LINKS A GUEST, as a prefix rather
     *  than one path: its words each become an argument, so a launcher
     *  in front of a compiler or an interpreter in front of a script is
     *  spelled here with the spaces in it. A word holding a space of its
     *  own is written quoted. */
    std::string compiler = "clang++";
    /** WHO MOVES THE CLOCK the sessions this host opens are drawn at.
     *  Under any policy but the wall's a session is opened for a
     *  repeatable run: what the sketch measured about its own execution
     *  is pinned (`SketchContext::deterministic` reads true) and the
     *  runtime's own re-baking is held off, so a capture can be diffed. A
     *  host steps its session with whatever delta its caller states
     *  either way; the policy is what the session is opened for. */
    motion::ClockPolicy clock = motion::ClockPolicy::Wall;
    /** Start from the sketch already compiled into this binary rather
     *  than by building the file, and compile only once the file
     *  changes. Null loads a matching cached build or compiles the source. */
    const Entry* compiledIn = nullptr;
    /** THE IMAGE THIS HOST IS PART OF, as the skew guard's reference
     *  point: a dylib built against a framework header newer than this
     *  is refused. It defaults to the stamp the process read of its own
     *  executable at first ask, and is a field so that a test can state
     *  one. */
    std::filesystem::file_time_type hostStamp = hostBinaryTime();
    /** How long between re-reads of the directories the sketch is
     *  built from — the units beside the entry and their local headers. The
     *  entry itself is stamped every poll; the directories around it
     *  are not, because reading a directory is cheap but not free and
     *  a header is saved by hand a moment before the sketch is. Zero
     *  re-reads them on every poll. */
    std::chrono::milliseconds siblingScanInterval{250};
    /** HOW LONG A MODULE MAY DISAGREE WITH ITS SIDECAR BEFORE THAT IS A
     *  FAILURE, when the sidecar still names this host's build. A build
     *  that links the module in place and stamps the sidecar afterwards
     *  shows new bytes beside the old sidecar until it finishes, so the
     *  host treats the disagreement as a publication in progress: it says
     *  nothing and checks again on the next poll. The disagreement is
     *  reported once the same module and sidecar have been seen on
     *  consecutive polls spanning at least this long. Zero reports it on
     *  the second consecutive poll that sees them. */
    std::chrono::milliseconds pluginPublicationGrace{2000};
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
  [[nodiscard]] const std::filesystem::path& buildDirectory() const {
    return m_buildDirectory;
  }

  /** Removes the build directories of processes that are no longer
   *  running, beside the one this process builds in.
   *
   *  A run that was killed or that faulted never reached the removal
   *  above, and its directory carries a pid no later run can reuse, so
   *  nothing would ever clear it. The pid in the name is asked of the
   *  system directly, and only the answer that says NOBODY HOLDS IT
   *  removes anything: a directory whose process is alive — this
   *  process's own included — is left standing. Calling it walks, every
   *  time and from any thread. */
  static void sweepAbandonedBuildDirectories();

  /** TAKES THIS PROCESS'S ONE WALK, answering true to whoever took it
   *  and false to everyone after.
   *
   *  The walk is worth doing once per run and it reads a whole temporary
   *  directory, so an owner that wants it OFF the thread its first host
   *  is built on claims it here — synchronously, before launching the
   *  walk — and the first host then finds it claimed and walks nothing.
   *  A host that finds it unclaimed walks itself. */
  [[nodiscard]] static bool claimSweep();

  /** Drives reloads from artifact or source changes, completed compiles,
   *  and asset changes. Call once per frame. */
  void poll();

  /** Ticks and draws one frame. Returns false while nothing has loaded or
   *  after a callback failure, until a fresh session opens successfully.
   *  A negative @p fixedDt uses wall time. */
  bool frame(SkCanvas& canvas, double fixedDt = -1.0);

  /** Advances a frame whose output is discarded. Draw callbacks still run
   *  unless the sketch declares that capture steps need no painting.
   *  Invalid canvas dimensions fail with a message in errorLog(). */
  bool frame(double fixedDt);

  [[nodiscard]] bool compiling() const { return m_compile.valid(); }
  [[nodiscard]] bool live() const { return m_session != nullptr; }
  [[nodiscard]] int generation() const { return m_generation; }
  /** Reopens the current kind as a fresh runtime session without compiling
   *  it again. The host's watched source, loaded libraries and asset cache
   *  stay warm, while setup, clocks, Outputs and mount transitions all start
   *  over. Returns false when no kind has loaded yet. */
  bool restartSession();
  /** The running session, for a host that needs more than a frame from
   *  it — its counters, its viewpoint, its per-node costs. Null until
   *  something has loaded. */
  [[nodiscard]] Session* session() { return m_session.get(); }

  /** WHICH RUNTIME THE LOADED SKETCH DRAWS THROUGH — "canvas" or "set" —
   *  read off the kind the host is holding, or empty before one has
   *  loaded. A file opened by path is not known to draw through any
   *  runtime until it has been built, so this is what fills in the row a
   *  browser could not read off the file. */
  [[nodiscard]] std::string_view kind() const {
    return m_kind ? m_kind->runtime() : std::string_view{};
  }
  /** Whether the accepted kind uses the host's device executor. Graphite
   *  capture of a pure Canvas does not itself require that executor. */
  [[nodiscard]] bool needsDevice() const {
    return m_kind && m_kind->needsDevice();
  }

  /** WHETHER THE LOADED SKETCH DECLARED ITSELF A PLATE rather than a live
   *  scene — a sheet whose subject is its own size, judged on the cost of
   *  the still it is photographed as and not on holding 60 FPS. Read off
   *  the running session's declared canvas; false before one has loaded. */
  [[nodiscard]] bool plateOnly() const {
    return m_session && m_session->canvas().plateOnly;
  }

  /** The lifecycle a status display reads: Compiling wins even while a
   *  previous build keeps rendering underneath. */
  enum class State { Waiting, Compiling, Live, Failed };
  [[nodiscard]] State state() const {
    if (m_compile.valid()) return State::Compiling;
    if (!m_errorLog.empty()) return State::Failed;
    return m_session ? State::Live : State::Waiting;
  }

  /** Honest frame metrics over the last 120 frames: the work lane is the
   *  full frame body, timed by `frame()`, and the presentation lane is
   *  what the host reports through `markPresented()`, an interval of a
   *  second or more read as a pause rather than a frame. */
  [[nodiscard]] const measure::FrameTimer& frameTimes() const {
    return m_frameTimes;
  }
  /** The session's own PAINT phase over the same frames. It is inside the
   *  work lane, and it is worth having on its own because for a set drawn
   *  on a device it is where the readback and the blit onto the canvas
   *  land — the host cost a frame-time gate rendering onto a raster
   *  surface never pays. */
  [[nodiscard]] const measure::Window<measure::Duration>& drawTimes() const {
    return m_drawTimes;
  }
  void markPresented();
  /** HOW MANY FRAMES OF THIS SESSION HAVE REACHED THE SCREEN, counted
   *  from the moment it started running. A reader outside the render
   *  thread has no other way to tell a session that is merely selected
   *  from one that is being presented: the pointer to it is published
   *  when it opens, and its first frame can be seconds behind that. */
  [[nodiscard]] unsigned long long presentedFrames() const {
    return m_presentedFrames;
  }
  /** EMPTIES THE ROLLING WINDOWS, so that what is read after this
   *  describes what happened after this. A stretch a caller means to
   *  measure begins with the frames before it thrown away — the first
   *  frames of a session cost what a session costs once, and averaged in
   *  they are the sketch's steady cost misreported. */
  void resetMetrics();
  /** BEGINS PRESENTING AGAIN after a stretch in which something else
   *  held the window. That stretch is not a frame interval, so the next
   *  presentation starts one rather than extending the one this session
   *  was paused in the middle of — the rolling windows themselves stay,
   *  which is the point of a session outliving the look away from it. */
  void resume() { m_frameTimes.resume(); }

  /** Advances a newly opened session to a capture moment: the explicit
   *  interval, the declared moment, or 1.5 seconds when neither is supplied.
   *  Uses whole fixed steps followed by the fractional remainder; zero runs
   *  one update without advancing time. Steps longer than the session clock's
   *  maximum delta are subdivided. Does not restart an existing session.
   *  Returns the chosen interval and throws on invalid inputs or failed frames.
   *
   *  @p density is the pixels per canvas unit the still will be taken at,
   *  and every raster the session bakes is pinned to it before the first
   *  step, as a plate sweep pins its plate's: a bake or a pen's canvas
   *  formed on the way is drawn on the still's own grid rather than taken
   *  at one pixel per unit and magnified into it. */
  double prepareCapture(std::optional<double> at = std::nullopt,
                        double fps = 60.0, float density = 1.0f);

  /** THE STILL AS PIXELS IN HOST MEMORY: the CURRENT state (clock
   *  untouched) at @p scale times the sketch's canvas, repainted onto
   *  the capture backend's surface and read back.
   *
   *  The bitmap owns its pixels and names no device, so what a caller
   *  does with it afterwards — encode it, write it, hand it to another
   *  thread — needs neither this host nor the device the still was
   *  drawn on. That is the split the thumbnail store lives in: only the
   *  repaint and the readback have to happen where the frames are
   *  drawn. Null when nothing is loaded, when the extent is out of
   *  range, or when the readback failed, with the reason in errorLog().
   *  Fractional pixel extents round up; each dimension must be positive
   *  and at most 16384 pixels. */
  [[nodiscard]] SkBitmap still(float scale = 1.0f);

  /** THE STILL A PLATE IS TAKEN AS: the runtime's own `Session::still`,
   *  drawn at @p density pixels per canvas unit onto the capture
   *  backend's surface, cleared to the declared ground, and read back.
   *
   *  Unlike `still()` it MOVES THE SCENE where the runtime re-renders its
   *  still — one `Session::stillStep()` further, which this host's clock
   *  counts — because that is how the sweep photographs a plate. So a
   *  scene stepped to a moment by `prepareCapture()` and photographed
   *  here is the sweep's plate of that moment, byte for byte, and a
   *  protocol session under a moving clock takes its still through this
   *  same call. It is `plateExtent()` pixels, a fraction of a pixel
   *  dropped as the sweep drops it, and is drawn on the surface the
   *  capture backend makes — the raster the sweep draws on in a headless
   *  host, the device's in a window drawing on it. Null, with the reason
   *  in errorLog(), exactly where `still()` is; a still the sketch throws
   *  in fails the session as a frame that throws does. */
  [[nodiscard]] SkBitmap photograph(float density = 1.0f);

  /** `still()` encoded as a PNG and written to @p out: the window's save
   *  command, which photographs what the reader is looking at without
   *  moving it. Synchronous, because every caller of it reads the file
   *  back the moment it returns. */
  bool capture(const std::filesystem::path& out, float scale = 1.0f);

  /** `photograph()` encoded as a PNG and written to @p out: the still a
   *  headless `--frame` writes. Synchronous, as `capture()` is. */
  bool writePhotograph(const std::filesystem::path& out, float density = 1.0f);

  /** A host on the device must route capture through its own backend:
   *  once live frames render on the GPU, the runtime's caches hold
   *  device-backed images that cannot replay onto a raster canvas.
   *  makeSurface builds the capture target and readback fetches its
   *  pixels after the draw, both on whichever thread calls capture().
   *  Unset is the CPU raster path. */
  struct CaptureBackend {
    std::function<sk_sp<SkSurface>(const SkImageInfo&)> makeSurface;
    std::function<bool(SkSurface&, const SkPixmap&)> readback;
    /** The canvas the still is described through, given the surface
     *  makeSurface just built. A backend whose device needs the draws
     *  kept in order answers a canvas that keeps them; unset, and null,
     *  leave the surface's own canvas in place. The canvas belongs to
     *  the backend and must outlive the capture. */
    std::function<SkCanvas*(SkSurface&)> canvasOf;
  };
  void setCaptureBackend(CaptureBackend backend) {
    m_captureBackend = std::move(backend);
  }

  /** One line of state for a status bar. */
  [[nodiscard]] const std::string& status() const { return m_status; }
  /** Full compiler or loader output of the most recent failure; empty
   *  when the latest build is good. Python import, setup and frame failures
   *  include the traceback. What the hub's `problems()` lists — a shader
   *  file that does not compile, with the compiler's message and the
   *  line — stands here too, after any build output, while the sketch
   *  runs on with what its libraries keep painting, until the resource is
   *  mended or the sketch no longer asks for it. */
  [[nodiscard]] const std::string& errorLog() const { return m_errorLog; }

  [[nodiscard]] const std::filesystem::path& sketchPath() const {
    return m_options.sketchPath;
  }
  /** The canvas the running sketch declared; hosts letterbox to this
   *  size and clear with this colour. */
  [[nodiscard]] SkSize canvasSize() const;
  [[nodiscard]] sigil::material::Color background() const;
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
  bool runFrame(SkCanvas& canvas, double fixedDt, bool discarded);

  /** One translation unit on a build's compile line, and where its
   *  object goes. */
  struct Unit {
    std::filesystem::path source;
    std::filesystem::path object;
  };
  struct CompileResult {
    bool ok = false;
    bool cached = false;
    std::string cacheKey;
    std::filesystem::path library;
    std::string output;
    int compiled = 0;
    int units = 0;
  };

  void startCompile();
  bool adopt(const std::filesystem::path& library,
             std::string_view pluginIdentity = {});
  void loadPlugin();
  bool openSession(const Kind& kind, bool (*available)(std::string*) = nullptr);
  void loadPython();
  bool pythonChanged();
  void sessionFailed(const std::exception& error);
  /** The same failure for a callback that threw something other than a
   *  standard exception, which carries no message of its own. */
  void sessionFailed(std::string message);
  /** The one body of `still()` and `photograph()`: a surface of
   *  @p extent pixels from the capture backend, @p draw run onto it at
   *  @p scale, and the pixels read back. */
  SkBitmap drawStill(SkISize extent, float scale, const SkColor4f& ground,
                     const std::function<void(SkCanvas&)>& draw);
  /** Says what the hub's `problems()` lists the way a failed build is
   *  said, after any build output the log already holds, and takes back
   *  its own words once the list is empty. */
  void noteProblems();
  /** Empties the hub's problems before the sketch asks for its
   *  resources again, so a resource it no longer asks for leaves the
   *  log. */
  void beginDeclaration();
  /** THE NEWEST WRITE ACROSS EVERYTHING THE SKETCH IS BUILT FROM, or
   *  nothing when the entry itself is not there.
   *
   *  A sketch is more than one file: a helper beside it is reached by a
   *  quoted include, which resolves relative to the including file and
   *  needs no include path; a directory sketch has units beside its
   *  entry; local includes can reach headers in other directories. An edit to
   * any of them has to rebuild the sketch, or what stays on screen is the code
   * that stood before it. */
  [[nodiscard]] std::optional<std::filesystem::file_time_type> sourceStamp();
  /** Re-reads the directories the sketch is built from into the two
   *  stamps below. */
  void scanBeside();
  /** Every unit the next build compiles or reuses, in compile order:
   *  the entry, the sources beside it when it is a directory sketch,
   *  in name order. */
  [[nodiscard]] std::vector<std::filesystem::path> units() const;

  Options m_options;
  weave::FontContext& m_fonts;
  std::filesystem::path m_buildDirectory;
  /** WHICH HOST IN THIS PROCESS THIS IS, counted from one. Every host in
   *  a process links into one build directory, so the id is in the name
   *  of every dylib this one builds: without it two hosts building at
   *  once would write one path, and the file standing there when one of
   *  them dlopens would be whichever link finished last. */
  int m_hostId = 0;

  Assets m_assets;
  Kind m_kind;
  bool (*m_available)(std::string*) = nullptr;
  std::unique_ptr<Session> m_session;

  std::future<CompileResult> m_compile;
  struct PluginStamp {
    std::filesystem::file_time_type modified;
    uintmax_t bytes = 0;
    std::filesystem::file_time_type manifestModified;
    uintmax_t manifestBytes = 0;
    bool operator==(const PluginStamp&) const = default;
  };
  [[nodiscard]] std::optional<PluginStamp> pluginStamp() const;
  std::optional<PluginStamp> m_pluginStamp;
  /** The module and sidecar last seen disagreeing while the sidecar named
   *  this host's build, and when they were first seen that way; cleared
   *  when any poll sees other files. */
  std::optional<PluginStamp> m_unsettledPluginStamp;
  std::chrono::steady_clock::time_point m_unsettledPluginSince;
  /** Copies of the module taken so far, which names each copy: a refused
   *  copy that stays mapped keeps its path, and a later copy at that path
   *  would be handed the mapped image again. */
  int m_pluginCopies = 0;
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
  int m_unitsCompiled = 0;  // of the last adopted build, for its status line
  int m_unitsTotal = 0;
  bool m_everCompiled = false;
  bool m_runtimeFailed = false;
  std::filesystem::file_time_type m_pythonEntryStamp =
      std::filesystem::file_time_type::min();
  std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>>
      m_pythonInputs;
  int m_generation = 0;
  int m_frameIndex = -1;  // for the crash reporter's phase line
  /** How long this host has been running, in its own time — stated
   *  deltas under a fixed step, wall time when it is free-running. The
   *  asset poll and the crash reporter's frame line read it, and both
   *  want the same clock the session is stepped by. */
  motion::Engine m_clock;
  double m_lastAssetPoll = 0.0;
  std::chrono::steady_clock::time_point m_compileStart;
  unsigned long long m_presentedFrames = 0;
  measure::FrameTimer m_frameTimes{{.pause = std::chrono::seconds(1)}};
  measure::Window<measure::Duration> m_drawTimes{120};
  std::string m_status = "waiting for first build";
  std::string m_errorLog;
  /** What `noteProblems` last put in the error log, so that it takes
   *  back its own words and never a build's. */
  std::string m_problemLog;
  CaptureBackend m_captureBackend;
};

}  // namespace sigil::sketch
