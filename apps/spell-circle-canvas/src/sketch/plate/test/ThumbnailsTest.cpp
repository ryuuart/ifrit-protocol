/** @file
 * The thumbnail store: when a still on disk is the one a sketch is owed,
 * what a run does with a sketch it cannot draw inside its budget, and
 * that a walk let go answers without finishing.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Runtime.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/plate/Thumbnails.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Runtime.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

#include "ScratchDir.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::test::ScratchDir;

/** HOW MANY FRAMES THE WALK ACTUALLY DREW, which is what every case
 *  below reads instead of a clock: a still is a fixed number of frames,
 *  so a run that ended early drew fewer of them and by exactly how many
 *  says where it ended. */
std::atomic_int g_frames{0};
/** Raised by the probe below once it has drawn kStopAfter frames — the
 *  stand-in for a window closing mid-render, and deterministic where a
 *  timer would not be. */
std::atomic_bool g_stop{false};
constexpr int kStopAfter = 5;

/** The rate a still is walked at, and a moment far enough out that the
 *  walk is thousands of frames — long enough that any of the endings
 *  below is separated from a finished walk by more than rounding. */
constexpr double kRate = 60.0;
constexpr double kLateMoment = 40.0;
constexpr int kWholeWalk = (int)(kLateMoment * kRate);

/** A sketch that counts the frames it is walked for and stops itself
 *  partway. */
struct Counting {
  void setup(SketchContext& ctx) {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(kLateMoment);
  }
  void update(double /*elapsed*/, SketchContext& ctx) {
    if (g_frames.fetch_add(1) + 1 >= kStopAfter) g_stop.store(true);
    ctx.composer.render(
        box().width(16).height(16).fill(Fill::color({1, 0, 0, 1})));
  }
};

/** The same walk, counted, with nothing that stops it — what a budget
 *  and a plate declaration are read against. */
struct Long {
  void setup(SketchContext& ctx) {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(kLateMoment);
  }
  void update(double /*elapsed*/, SketchContext& ctx) {
    g_frames.fetch_add(1);
    ctx.composer.render(
        box().width(16).height(16).fill(Fill::color({0, 1, 0, 1})));
  }
};

/** A sketch that declares itself a plate: judged on the cost of its
 *  still rather than on holding a frame rate, which is the declaration
 *  a fill stands down on. */
struct Plate {
  void setup(SketchContext& ctx) {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
    ctx.plate();
  }
  void update(double /*elapsed*/, SketchContext& ctx) {
    g_frames.fetch_add(1);
    ctx.composer.render(
        box().width(16).height(16).fill(Fill::color({0, 0, 1, 1})));
  }
};

namespace world = sigil::world;

/** AN EXECUTOR THAT DRAWS NOTHING AND REMEMBERS BEING REACHED — the
 *  stand-in for the device runtime a host installs, so a case can assert
 *  that a still never went through it. */
struct Reached : world::Executor {
  std::atomic_int* reached = nullptr;
  void execute(const world::PassWork&, const world::View&,
               world::Targets&) const override {
    if (reached) reached->fetch_add(1);
  }
  bool operator==(const Reached& other) const {
    return reached == other.reached;
  }
};

namespace render = sigil::geometry::mesh::render;

/** THE 2D TWIN: a mesh executor that draws nothing and remembers being
 *  reached, so a case can assert that a canvas sketch standing geometry
 *  up in space never drew its still through the painter a host
 *  installed. */
struct PainterReached : render::Executor {
  std::atomic_int* reached = nullptr;
  void drawMesh(SkCanvas&, const sigil::geometry::mesh::Mesh&, const glm::mat4&,
                const sigil::geometry::mesh::camera::Camera&, SkSize,
                const render::MeshStyle&) const override {
    if (reached) reached->fetch_add(1);
  }
  void drawPanel(SkCanvas&, const glm::mat4&,
                 const sigil::geometry::mesh::camera::Camera&, SkSize,
                 const std::function<void(SkCanvas&)>&) const override {
    if (reached) reached->fetch_add(1);
  }
  bool operator==(const PainterReached& other) const {
    return reached == other.reached;
  }
};

/** A 2D sketch that stands one mesh up in space through whichever
 *  painter it was opened on — the shape `floating_panels` and
 *  `painter_gpu` are, with nothing else on the sheet. */
struct Standing {
  void setup(SketchContext& ctx) {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.05);
  }
  void update(double /*elapsed*/, SketchContext& ctx) {
    // The painter is read where a sketch reads it: from inside the body,
    // as it declares.
    ctx.composer.render(box().width(32).height(24).children(
        {custom("mesh", [painter = painterRuntime()](
                            SkCanvas& canvas, const PaintContext& paint) {
          const sigil::geometry::mesh::camera::Camera camera;
          painter.get()->drawPanel(canvas, glm::mat4(1.0f), camera, paint.size,
                                   [](SkCanvas&) {});
        })}));
  }
};

/** A set with nothing in it: what is being asserted is which runtime the
 *  session opened on, which needs no subject. */
struct Lit {
  void setup(SetContext& ctx) {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.05);
  }
  world::Frame describe(float /*seconds*/) {
    return world::Element().key("empty");
  }
};

template <class SketchType>
Entry entryOf(const char* name) {
  return Entry{name, name, "Test", "a thumbnail fixture", &kindOf<SketchType>};
}

ThumbnailRun runInto(const std::filesystem::path& out, std::string stem) {
  ThumbnailRun run;
  run.outputPath = out;
  run.stem = std::move(stem);
  run.maxDimension = 64;
  return run;
}

enum class Fault { None, Factory, Open, Frame, Still };
std::atomic_int g_openSessions{0};

template <Fault fault>
struct ThrowingSession final : Session {
  ThrowingSession() { ++g_openSessions; }
  ~ThrowingSession() override { --g_openSessions; }
  CanvasSpecification specification{.size = {48, 32}, .captureSeconds = 0.05};
  const CanvasSpecification& canvas() const override { return specification; }
  void frame(SkCanvas& canvas, double) override {
    if constexpr (fault == Fault::Frame)
      throw std::logic_error("thumbnail frame failed");
    canvas.clear(SK_ColorGREEN);
  }
  void repaint(SkCanvas& canvas) override { canvas.clear(SK_ColorGREEN); }
  void still(SkCanvas& canvas) override {
    if constexpr (fault == Fault::Still)
      throw std::out_of_range("thumbnail still failed");
    repaint(canvas);
  }
  Timing timing() const override { return {}; }
};

template <Fault fault>
struct ThrowingKind final : KindOperations {
  std::string_view runtime() const override { return "canvas"; }
  std::unique_ptr<Session> open(sigil::weave::FontContext&, Assets&, bool,
                                std::string_view) const override {
    if constexpr (fault == Fault::Open)
      throw std::runtime_error("thumbnail open failed");
    return std::make_unique<ThrowingSession<fault>>();
  }
};

template <Fault fault>
Kind throwingKind() {
  if constexpr (fault == Fault::Factory)
    throw std::runtime_error("thumbnail factory failed");
  return Kind(ThrowingKind<fault>{});
}

/** A STILL IS CPU-ONLY WHATEVER THE PROCESS HOLDS. The worker that draws
 *  one runs beside a window that is presenting, and a device is one
 *  device and one queue: a background walk driving the queue the render
 *  thread draws with is two threads inside one graphics context. */
TEST(ThumbnailStore, ASetIsDrawnOnTheCpuWhateverTheProcessInstalled) {
  const ScratchDir dir("sigil_thumbnail_set");
  std::atomic_int reached{0};
  Reached installed;
  installed.reached = &reached;
  useRuntime(world::Runtime(installed));

  const Entry entry = entryOf<Lit>("lit");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "lit", "aaaa"), "lit");
  const ThumbnailOutcome outcome =
      renderThumbnail(entry, fonts(), assets(), run);
  useRuntime({});

  EXPECT_EQ(ThumbnailOutcome::Wrote, outcome);
  EXPECT_TRUE(std::filesystem::exists(run.outputPath));
  EXPECT_EQ(reached.load(), 0)
      << "the still went through the runtime the process installed";
}

/** A STILL IS CPU-ONLY FOR EVERY RUNTIME, not only a set's. A 2D sketch
 *  that stands geometry up in space hands `painterRuntime()` to a mesh
 *  style, and the still it is drawn for must reach the CPU executor
 *  whatever the process installed — the same one device, one queue that
 *  keeps a set's still off the device. */
TEST(ThumbnailStore, ACanvasIsPaintedOnTheCpuWhateverTheProcessInstalled) {
  const ScratchDir dir("sigil_thumbnail_canvas");
  std::atomic_int reached{0};
  PainterReached installed;
  installed.reached = &reached;
  usePainterRuntime(render::Runtime(installed));

  const Entry entry = entryOf<Standing>("standing");
  ThumbnailRun run =
      runInto(thumbnailFile(dir.path, "standing", "aaaa"), "standing");
  const ThumbnailOutcome outcome =
      renderThumbnail(entry, fonts(), assets(), run);
  usePainterRuntime({});

  EXPECT_EQ(ThumbnailOutcome::Wrote, outcome);
  EXPECT_TRUE(std::filesystem::exists(run.outputPath));
  EXPECT_EQ(reached.load(), 0)
      << "the still went through the painter the process installed";
}

/** A file with @p text in it, so a key has something to hash. */
void write(const std::filesystem::path& path, std::string_view text) {
  std::ofstream stream(path);
  stream << text;
}

// ---------------------------------------------------------------------------
// The key, and what makes a still on disk the wrong one

TEST(ThumbnailStore, ASourceThatChangedIsANewKey) {
  const ScratchDir dir("sigil_thumbnail_key");
  const std::filesystem::path source = dir.path / "probe.cpp";
  write(source, "// one");
  const std::string first = thumbnailKey(source);
  EXPECT_FALSE(first.empty());
  EXPECT_EQ(first, thumbnailKey(source)) << "the same file keyed twice";

  write(source, "// one, and a second line that makes it longer");
  EXPECT_NE(first, thumbnailKey(source));
}

TEST(ThumbnailStore, TheKeyIsTheSourceAndNothingElse) {
  // A KEY THAT CAME BACK is the whole claim: the key is a function of the
  // source's own size and time, so a source restored to what it was is
  // owed the still it had. No host binary or clock may enter, or a source
  // put back would answer differently.
  const ScratchDir dir("sigil_thumbnail_pure");
  const std::filesystem::path source = dir.path / "probe.cpp";
  write(source, "// one");
  const auto when = std::filesystem::last_write_time(source);
  const std::string first = thumbnailKey(source);

  write(source, "// one, and a second line that makes it longer");
  ASSERT_NE(first, thumbnailKey(source));

  write(source, "// one");
  std::filesystem::last_write_time(source, when);
  EXPECT_EQ(first, thumbnailKey(source))
      << "the source is back, so the still it had is fresh again";
}

/** A SKETCH THAT IS A DIRECTORY IS BUILT FROM EVERY SOURCE BESIDE ITS
 *  ENTRY, so its still is stale when any of them changed — and the answer
 *  cannot depend on the order a directory happens to be read in, which no
 *  filesystem promises. */
TEST(ThumbnailStore, ADirectorySketchIsKeyedOnEverySourceBesideItsEntry) {
  const ScratchDir dir("sigil_thumbnail_directory");
  const std::filesystem::path root = dir.path / "rain";
  std::filesystem::create_directories(root);
  const std::filesystem::path entry = root / "rain.cpp";
  write(entry, "// the entry");
  write(root / "tables.cpp", "// a unit beside it");
  write(root / "palette.h", "// a header beside it");
  const std::string first = thumbnailKey(entry);
  EXPECT_EQ(first, thumbnailKey(entry)) << "the same directory keyed twice";

  // A unit the entry does not name is still a unit of the sketch.
  write(root / "tables.cpp", "// a unit beside it, longer than it was");
  const std::string moved = thumbnailKey(entry);
  EXPECT_NE(first, moved);

  // …and so is a header beside it.
  write(root / "palette.h", "// a header beside it, longer than it was");
  EXPECT_NE(moved, thumbnailKey(entry));

  // A file that is neither is nothing to the sketch.
  const std::string standing = thumbnailKey(entry);
  write(root / "notes.txt", "not a source");
  EXPECT_EQ(standing, thumbnailKey(entry));
}

TEST(ThumbnailStore, OwnedHeadersInvalidateBareAndDirectoryThumbnails) {
  const ScratchDir dir("sigil_thumbnail_owners");
  const auto owner = dir.path / "owner";
  std::filesystem::create_directories(owner);
  std::filesystem::create_directories(dir.path / "rain");
  write(owner / "Palette.h", "#include \"Tone.h\"\n");
  for (const char* name : {"bare.cpp", "rain/rain.cpp"}) {
    const auto entry = dir.path / name;
    const std::string prefix = directorySketch(entry) ? "../" : "";
    write(entry, "#include \"" + prefix + "owner/Palette.h\"\n");
    write(owner / "Tone.h", "// one\n");
    const std::string first = thumbnailKey(entry);
    write(owner / "Tone.h", "// a different pigment\n");
    const std::string changed = thumbnailKey(entry);
    EXPECT_NE(first, changed) << name;
    write(owner / "Unused.h", "// an unrelated helper\n");
    EXPECT_EQ(changed, thumbnailKey(entry)) << name;
  }
}

TEST(ThumbnailStore, PythonHelpersAndPackagesInvalidateTheKey) {
  const ScratchDir dir("sigil_thumbnail_python");
  const auto entry = dir.path / "scene.py";
  const auto helper = dir.path / "palette.py";
  const auto package = dir.path / "shapes";
  std::filesystem::create_directories(package);
  write(entry, "from .palette import ink\nfrom .shapes import leaf\n");
  write(helper, "ink = '#ff0000'\n");
  write(package / "__init__.py", "from .leaf import leaf\n");
  write(package / "leaf.py", "leaf = 1\n");
  const auto first = thumbnailKey(entry);
  EXPECT_EQ(first, thumbnailKey(entry));

  const auto when = std::filesystem::last_write_time(helper);
  write(helper, "ink = '#00ff00'\n");
  std::filesystem::last_write_time(helper, when + std::chrono::seconds(1));
  const auto siblingEdit = thumbnailKey(entry);
  EXPECT_NE(first, siblingEdit);

  write(package / "leaf.py", "leaf = 100\n");
  const auto packageEdit = thumbnailKey(entry);
  EXPECT_NE(siblingEdit, packageEdit);

  std::filesystem::rename(package, dir.path / "renamed");
  const auto renamed = thumbnailKey(entry);
  EXPECT_NE(packageEdit, renamed)
      << "a package's path matters even when its files keep size and time";

  write(dir.path / "notes.txt", "not a Python source");
  write(dir.path / "neighbor.cpp", "// a separate native sketch");
  std::filesystem::create_directories(dir.path / "__pycache__");
  write(dir.path / "__pycache__" / "cached.pyc", "compiled bytecode");
  std::filesystem::create_directories(dir.path / "unrelated");
  write(dir.path / "unrelated" / "elsewhere.py", "unrelated = True\n");
  EXPECT_EQ(renamed, thumbnailKey(entry));

  std::filesystem::remove(helper);
  EXPECT_NE(renamed, thumbnailKey(entry));
}

TEST(ThumbnailStore, AStillIsFreshOnlyUnderTheKeyItWasWrittenAt) {
  const ScratchDir dir("sigil_thumbnail_fresh");
  EXPECT_TRUE(freshThumbnail(dir.path, "probe", "aaaa").empty())
      << "nothing is on disk yet";

  const std::filesystem::path at = thumbnailFile(dir.path, "probe", "aaaa");
  write(at, "not really a png");
  EXPECT_EQ(at, freshThumbnail(dir.path, "probe", "aaaa"));
  EXPECT_TRUE(freshThumbnail(dir.path, "probe", "bbbb").empty())
      << "a still under another key is the answer to another question";
}

TEST(ThumbnailStore, ANoteStandsInForAStillUnderTheSameKey) {
  const ScratchDir dir("sigil_thumbnail_note");
  EXPECT_TRUE(thumbnailNote(dir.path, "probe", "aaaa").empty());
  ASSERT_TRUE(noteThumbnail(dir.path, "probe", "aaaa", "ran past its budget"));
  EXPECT_EQ("ran past its budget", thumbnailNote(dir.path, "probe", "aaaa"));
  EXPECT_TRUE(thumbnailNote(dir.path, "probe", "bbbb").empty())
      << "a note is asked and answered at one key only";
}

TEST(ThumbnailStore, WhatTheKeyNoLongerNamesIsRemoved) {
  const ScratchDir dir("sigil_thumbnail_prune");
  write(thumbnailFile(dir.path, "probe", "aaaa"), "an old still");
  ASSERT_TRUE(noteThumbnail(dir.path, "probe", "aaaa", "an old answer"));
  EXPECT_TRUE(freshThumbnail(dir.path, "probe", "aaaa").empty())
      << "writing the note took the still of the same key with it";

  const std::filesystem::path kept = thumbnailFile(dir.path, "probe", "bbbb");
  write(kept, "the still now");
  pruneThumbnails(dir.path, "probe", kept);
  EXPECT_EQ(kept, freshThumbnail(dir.path, "probe", "bbbb"));
  EXPECT_TRUE(thumbnailNote(dir.path, "probe", "aaaa").empty())
      << "the note of a spent key went with its still";
}

// ---------------------------------------------------------------------------
// What one render does with a sketch it cannot draw in the time it has

TEST(ThumbnailRender, ExceptionsFailOneSketchAndTheNextRenderStillSucceeds) {
  const ScratchDir dir("sigil_thumbnail_exception");
  const Entry failures[] = {
      {"factory", "factory", "Test", "", &throwingKind<Fault::Factory>},
      {"open", "open", "Test", "", &throwingKind<Fault::Open>},
      {"frame", "frame", "Test", "", &throwingKind<Fault::Frame>},
      {"still", "still", "Test", "", &throwingKind<Fault::Still>},
  };
  const Entry healthy{"healthy", "healthy", "Test", "",
                      &throwingKind<Fault::None>};
  for (const auto& entry : failures) {
    SCOPED_TRACE(entry.key);
    auto run = runInto(thumbnailFile(dir.path, entry.key, "broken"), entry.key);
    run.budget = std::chrono::milliseconds::zero();
    ThumbnailOutcome outcome = ThumbnailOutcome::Wrote;
    testing::internal::CaptureStderr();
    EXPECT_NO_THROW(outcome = renderThumbnail(entry, fonts(), assets(), run));
    const auto diagnostic = testing::internal::GetCapturedStderr();
    EXPECT_EQ(outcome, ThumbnailOutcome::Failed);
    EXPECT_NE(diagnostic.find(std::string("[thumbnail] ") + entry.key),
              std::string::npos);
    EXPECT_NE(diagnostic.find("failed"), std::string::npos);
    EXPECT_EQ(g_openSessions.load(), 0);
    EXPECT_FALSE(std::filesystem::exists(run.outputPath));

    run.outputPath = thumbnailFile(dir.path, entry.key, "recovered");
    EXPECT_EQ(renderThumbnail(healthy, fonts(), assets(), run),
              ThumbnailOutcome::Wrote);
    EXPECT_EQ(g_openSessions.load(), 0);
    EXPECT_TRUE(std::filesystem::exists(run.outputPath));
  }
}

TEST(ThumbnailRender, AWalkLetGoAnswersWithoutFinishingIt) {
  const ScratchDir dir("sigil_thumbnail_stopped");
  g_frames.store(0);
  g_stop.store(false);
  const Entry entry = entryOf<Counting>("thumbnail_counting");
  ThumbnailRun run =
      runInto(thumbnailFile(dir.path, "counting", "aaaa"), "counting");
  run.budget = std::chrono::milliseconds::zero();  // only the stop ends it
  run.stop = &g_stop;

  EXPECT_EQ(ThumbnailOutcome::Stopped,
            renderThumbnail(entry, fonts(), assets(), run));
  // The stop is read between frames, so the frame that raised it is the
  // last one drawn — and every one of the thousands after it is not.
  EXPECT_EQ(kStopAfter, g_frames.load());
  EXPECT_LT(g_frames.load(), kWholeWalk);
  EXPECT_FALSE(std::filesystem::exists(run.outputPath))
      << "a walk let go leaves no still behind";
}

TEST(ThumbnailRender, AWalkPastItsBudgetIsAbandoned) {
  const ScratchDir dir("sigil_thumbnail_budget");
  g_frames.store(0);
  const Entry entry = entryOf<Long>("thumbnail_long");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "long", "aaaa"), "long");
  // The smallest budget there is: whatever this machine's speed, one
  // frame of anything outlasts it, and what is asserted is that the walk
  // ended far short of its whole rather than when.
  run.budget = std::chrono::milliseconds(1);

  EXPECT_EQ(ThumbnailOutcome::OverBudget,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_LT(g_frames.load(), kWholeWalk);
  EXPECT_FALSE(std::filesystem::exists(run.outputPath));
}

TEST(ThumbnailRender, ASketchThatDeclaredItselfAPlateIsNotWalked) {
  const ScratchDir dir("sigil_thumbnail_heavy");
  g_frames.store(0);
  const Entry entry = entryOf<Plate>("thumbnail_plate");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "plate", "aaaa"), "plate");

  EXPECT_EQ(ThumbnailOutcome::Heavy,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_EQ(0, g_frames.load()) << "setup ran; the walk did not";
  EXPECT_FALSE(std::filesystem::exists(run.outputPath));

  // …and a run asked for the heavy ones draws it like any other.
  run.heavy = true;
  EXPECT_EQ(ThumbnailOutcome::Wrote,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_TRUE(std::filesystem::exists(run.outputPath));
}

TEST(ThumbnailRender, AWalkInsideItsBudgetWritesTheStill) {
  const ScratchDir dir("sigil_thumbnail_wrote");
  g_frames.store(0);
  const Entry entry = entryOf<Plate>("thumbnail_plate_again");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "plate", "cccc"), "plate");
  run.heavy = true;
  run.budget = std::chrono::milliseconds::zero();

  EXPECT_EQ(ThumbnailOutcome::Wrote,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_GT(g_frames.load(), 0);
  EXPECT_TRUE(std::filesystem::exists(run.outputPath));
  EXPECT_TRUE(thumbnailNote(dir.path, "plate", "cccc").empty())
      << "a still that landed leaves nothing standing in for it";
}

}  // namespace
