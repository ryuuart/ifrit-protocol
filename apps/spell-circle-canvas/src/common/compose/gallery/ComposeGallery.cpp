// ComposeGallery — the interactive vehicle for the stress catalog, and
// the practical-usage proof: every scene is built with the public API
// exactly as a host app would, and the FPS overlay is itself an
// IfritCompose composer layered over each scene (multi-composer usage).
//
// Keys: 1..7 scenes · Space pause · -/= time scale · F toggle stats ·
// Esc quit.  `ComposeGallery --headless <outdir>` renders each scene
// mid-animation to PNG, measures unthrottled frame times over 120
// frames, prints a per-scene FPS table, and exits.

#include <ifritcompose/Compose.h>
#include <ifritcompose/Decorations.h>
#include <ifritcompose/Util.h>

#include <textflow/FontContext.h>
#include <textflow/ports/SystemFontManager.h>

#include <entt/entt.hpp>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/encode/SkPngEncoder.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QRasterWindow>

#include <QtCore/QTimer>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using namespace ifrit::compose;
using ifrit::compose::util::toU8;
using namespace std::chrono_literals;

namespace {

textflow::FontContext &fonts() {
  static auto *context =
      new textflow::FontContext(textflow::ports::systemFontManager());
  return *context;
}

textflow::TextStyle styleAt(float size, SkColor color = SK_ColorWHITE) {
  textflow::TextStyle s;
  s.shaping.fontSize = size;
  s.paint.foreground.setColor(color);
  return s;
}

constexpr SkSize kSceneSize = {900, 640};

// ---------------------------------------------------------------------------
// Frame statistics (the FPS measurement)

struct FrameStats {
  std::deque<double> frameMs; // rolling window of FULL frame work
  std::deque<double> presentMs; // wall deltas between presented frames
  static constexpr size_t kWindow = 120;

  void add(double ms) {
    frameMs.push_back(ms);
    if (frameMs.size() > kWindow)
      frameMs.pop_front();
  }
  void addPresent(double ms) {
    presentMs.push_back(ms);
    if (presentMs.size() > kWindow)
      presentMs.pop_front();
  }
  double presentedFps() const {
    if (presentMs.empty())
      return 0;
    const double avg = std::accumulate(presentMs.begin(), presentMs.end(),
                                       0.0) / (double)presentMs.size();
    return avg > 0 ? 1000.0 / avg : 0;
  }
  double average() const {
    if (frameMs.empty())
      return 0;
    return std::accumulate(frameMs.begin(), frameMs.end(), 0.0) /
           (double)frameMs.size();
  }
  double percentile(double p) const {
    if (frameMs.empty())
      return 0;
    std::vector<double> sorted(frameMs.begin(), frameMs.end());
    std::sort(sorted.begin(), sorted.end());
    return sorted[std::min(sorted.size() - 1,
                           (size_t)((double)sorted.size() * p))];
  }
  double fps() const {
    const double avg = average();
    return avg > 0 ? 1000.0 / avg : 0;
  }
};

// ---------------------------------------------------------------------------

struct Scene {
  virtual ~Scene() = default;
  virtual const char *name() const = 0;
  virtual void setup(Composer &composer, ifrit::tick::Ticker &ticker) = 0;
  virtual void update(double, Composer &) {}
};

// ---- 1: live scoreboard (reconcile + transitions) ------------------------

struct ScoreboardScene final : Scene {
  struct Row {
    std::string name;
    int score;
    bool highlighted;
    bool operator==(const Row &) const = default;
  };
  std::vector<Row> rows;
  std::mt19937 rng{7};
  double nextMutation = 0.0;

  const char *name() const override { return "scoreboard"; }

  static Element rowElement(const Row &row) {
    return box().row().gap(12).padding(10).corners({8})
        .transition({350ms})
        .fill(Fill::color(row.highlighted
                              ? SkColor4f{0.35f, 0.20f, 0.52f, 1}
                              : SkColor4f{0.10f, 0.11f, 0.16f, 1}))
        .child(text(toU8(row.name), styleAt(20, 0xffe8ecf8)).grow(1))
        .child(text(toU8(std::to_string(row.score)),
                    styleAt(20, 0xff7ee8ff)));
  }

  Element describe() {
    auto list = box().column().gap(8).padding(28)
                    .fill(Fill::color({0.06f, 0.05f, 0.12f, 1}));
    list.child(text(u8"LIVE STANDINGS", styleAt(30, 0xffffb46b)));
    for (const Row &row : rows)
      list.child(memo(row, rowElement).key(row.name));
    return list;
  }

  void setup(Composer &composer, ifrit::tick::Ticker &) override {
    rows = {{"ember", 128, false}, {"sigil", 96, true},
            {"cinder", 77, false}, {"ash", 64, false},
            {"flare", 51, false},  {"soot", 12, false}};
    nextMutation = 0.0;
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextMutation)
      return;
    nextMutation = elapsed + 0.9;
    for (Row &row : rows) {
      row.score += (int)(rng() % 9);
      row.highlighted = false;
    }
    rows[rng() % rows.size()].highlighted = true;
    std::stable_sort(rows.begin(), rows.end(),
                     [](const Row &a, const Row &b) {
                       return a.score > b.score;
                     });
    composer.render(describe());
  }
};

// ---- 2: choreographed headline (bindings + live custom leaf) -------------

struct HeadlineScene final : Scene {
  choreograph::Output<float> drop{-40.0f}, fade{0.0f}, spin{0.0f};

  const char *name() const override { return "headline"; }

  void setup(Composer &composer, ifrit::tick::Ticker &ticker) override {
    drop = -40.0f;
    fade = 0.0f;
    ticker.timeline().apply(&drop).then<choreograph::RampTo>(
        0.0f, 0.9f, &choreograph::easeOutQuint);
    ticker.timeline().apply(&fade).then<choreograph::RampTo>(1.0f, 0.7f);
    ticker.add([this](double dt) {
      spin = spin.value() + (float)(dt * 40.0);
      return true;
    });

    composer.render(
        box().column().padding(56).gap(24)
            .fill(Fill::color({0.05f, 0.08f, 0.14f, 1}))
            .child(text(u8"CHOREOGRAPH", styleAt(84, 0xff7ee8ff))
                       .key("headline")
                       .translateY(&drop).opacity(&fade))
            .child(text(u8"outputs bound as style values",
                        styleAt(22, 0xff9aa4bb)).opacity(&fade))
            .child(custom([this](SkCanvas &c, const PaintContext &ctx) {
                     SkPaint ring;
                     ring.setAntiAlias(true);
                     ring.setStyle(SkPaint::kStroke_Style);
                     ring.setStrokeWidth(3);
                     const float cx = ctx.size.width() / 2;
                     const float cy = ctx.size.height() / 2;
                     for (int i = 0; i < 6; ++i) {
                       const float a = spin.value() * 3.14159f / 180.0f +
                                       (float)i * 3.14159f / 3.0f;
                       ring.setColor(i % 2 ? 0x88b18cff : 0xcc7ee8ff);
                       c.drawCircle(cx + std::cos(a) * 90,
                                    cy + std::sin(a) * 90, 34, ring);
                     }
                   })
                       .height(280)
                       .cache(Cache::None)));
  }
};

// ---- 3: blend stack ------------------------------------------------------

struct BlendScene final : Scene {
  choreograph::Output<float> slide{0.0f};

  const char *name() const override { return "blend"; }

  static Element table(const char *title, SkColor4f base) {
    auto t = box().column().gap(6).padding(18).width(380).corners({12})
                 .fill(Fill::color(base));
    t.child(text(toU8(title), styleAt(22, 0xffffffff)));
    for (int i = 0; i < 4; ++i)
      t.child(box().row().gap(10).padding(8).corners({6})
                  .fill(Fill::color({0, 0, 0, 0.35f}))
                  .child(text(toU8("row " + std::to_string(i)),
                              styleAt(16, 0xffdde4f2)).grow(1))
                  .child(text(toU8(std::to_string(37 * (i + 1))),
                              styleAt(16, 0xff7ee8ff))));
    return t;
  }

  void setup(Composer &composer, ifrit::tick::Ticker &ticker) override {
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      slide = (float)(std::sin(t * 0.8) * 140.0);
      return true;
    });
    composer.render(
        stack().fill(Fill::color({0.08f, 0.05f, 0.13f, 1}))
            .child(table("ALPHA", {0.16f, 0.10f, 0.28f, 1})
                       .inset(70, 90, 450, 130).zIndex(0))
            .child(table("BETA", {0.10f, 0.20f, 0.30f, 1})
                       .inset(300, 170, 220, 50).zIndex(1)
                       .translateX(&slide)
                       .opacity(0.85f)
                       .blend(SkBlendMode::kScreen)));
  }
};

// ---- 4: chrome (decoration primitives) -----------------------------------

struct ChromeScene final : Scene {
  const char *name() const override { return "chrome"; }

  void setup(Composer &composer, ifrit::tick::Ticker &) override {
    SkPathBuilder leaf;
    leaf.moveTo(0, 0);
    leaf.quadTo(6, -7, 14, 0);
    leaf.quadTo(6, 7, 0, 0);

    PathFormat stamped;
    stamped.width = 2;
    stamped.strokeFill = Fill::color({0.69f, 0.55f, 1.0f, 1});
    stamped.stampPath = leaf.detach();
    stamped.stampAdvance = 20;

    PathFormat dashed;
    dashed.width = 4;
    dashed.strokeFill = Fill::color({0.49f, 0.91f, 1.0f, 1});
    dashed.dashIntervals = {14, 8};

    ContourWalk orbit;
    orbit.spacing = 26;
    orbit.animatedWalk = true; // dots breathe with the clock
    orbit.draw = [](SkCanvas &c, const PathSample &s,
                    const PaintContext &ctx) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setColor(SkColorSetARGB(0xff, 0xff, 0xb4, 0x6b));
      const float wave =
          std::sin(s.fraction * 6.28318f + (float)ctx.elapsedSeconds * 2);
      c.drawCircle(0, -10, 2.0f + 1.8f * wave, p);
    };

    composer.render(
        stack().fill(Fill::color({0.10f, 0.06f, 0.16f, 1}))
            .child(box().inset(60, 70, 60, 70).corners({24})
                       .fill(Fill::color({0.07f, 0.08f, 0.14f, 0.92f}))
                       .foreground(stamped)
                       .foreground(orbit)
                       .child(box().column().padding(40).gap(14)
                                  .child(text(u8"CHROME",
                                              styleAt(56, 0xffe8ecf8)))
                                  .child(text(u8"stamped leaves, breathing walk dots, dash data",
                                              styleAt(17, 0xff9aa4bb)))
                                  .child(box().height(30)
                                             .foreground(dashed)))));
  }
};

// ---- 5: CRT + bloom (effects) --------------------------------------------

struct CrtScene final : Scene {
  const char *name() const override { return "crt_bloom"; }

  static Element scanlines(float phase, SkColor4f tint) {
    return custom([phase, tint](SkCanvas &c, const PaintContext &ctx) {
             SkPaint p;
             p.setColor4f(tint, nullptr);
             const float shift =
                 phase + (float)std::fmod(ctx.elapsedSeconds * 8.0, 6.0);
             for (float y = shift; y < ctx.size.height(); y += 6.0f)
               c.drawRect(SkRect::MakeXYWH(0, y, ctx.size.width(), 2.4f), p);
           })
        .absolute().inset(0)
        .cache(Cache::None) // reads the clock: declared volatility
        .blend(SkBlendMode::kPlus);
  }

  void setup(Composer &composer, ifrit::tick::Ticker &) override {
    composer.render(
        stack().fill(Fill::color({0.02f, 0.03f, 0.05f, 1}))
            .child(box().column().padding(70).gap(10).inset(0).zIndex(2)
                       .child(text(u8"PHOSPHOR", styleAt(96, 0xff9df2ff))))
            .child(box().column().padding(70).inset(0).zIndex(1)
                       .effect(Effect::filter(
                           SkImageFilters::Blur(14, 14, nullptr)))
                       .blend(SkBlendMode::kPlus)
                       .cache(Cache::Texture) // bloom baked once
                       .child(text(u8"PHOSPHOR", styleAt(96, 0xff2a7f96))))
            .child(scanlines(0.0f, {0.10f, 0.22f, 0.16f, 1}).zIndex(0))
            .child(scanlines(2.0f, {0.16f, 0.10f, 0.20f, 1}).zIndex(0))
            .child(scanlines(4.0f, {0.05f, 0.12f, 0.24f, 1}).zIndex(0)));
  }
};

// ---- 6: particles (EnTT SoA + one drawAtlas leaf) ------------------------

struct ParticleScene final : Scene {
  static constexpr size_t kCount = 50000;
  entt::registry registry;
  sk_sp<SkImage> sprite;
  std::vector<SkRSXform> xforms;
  std::vector<SkRect> texRects;

  struct Pos { float x, y; };
  struct Vel { float dx, dy; };

  const char *name() const override { return "particles"; }

  void setup(Composer &composer, ifrit::tick::Ticker &ticker) override {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
    s->getCanvas()->clear(SK_ColorTRANSPARENT);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(0xff7ee8ff);
    s->getCanvas()->drawCircle(4, 4, 3.0f, p);
    sprite = s->makeImageSnapshot();

    registry.clear();
    std::mt19937 rng{11};
    auto unit = [&] { return (float)(rng() % 10000) / 10000.0f; };
    for (size_t i = 0; i < kCount; ++i) {
      entt::entity e = registry.create();
      registry.emplace<Pos>(e, unit() * kSceneSize.width(),
                            unit() * kSceneSize.height());
      registry.emplace<Vel>(e, unit() * 90 - 45, unit() * 90 - 45);
    }
    xforms.reserve(kCount);
    texRects.assign(kCount, SkRect::MakeWH(8, 8));

    ticker.add([this](double dt) {
      registry.view<Pos, const Vel>().each([dt](Pos &p, const Vel &v) {
        p.x += v.dx * (float)dt;
        p.y += v.dy * (float)dt;
        if (p.x < 0) p.x += kSceneSize.width();
        else if (p.x > kSceneSize.width()) p.x -= kSceneSize.width();
        if (p.y < 0) p.y += kSceneSize.height();
        else if (p.y > kSceneSize.height()) p.y -= kSceneSize.height();
      });
      return true;
    });

    composer.render(
        stack().fill(Fill::color({0.02f, 0.02f, 0.06f, 1}))
            .child(custom([this](SkCanvas &c, const PaintContext &) {
                     xforms.clear();
                     registry.view<const Pos>().each([this](const Pos &p) {
                       xforms.push_back(SkRSXform::Make(1, 0, p.x, p.y));
                     });
                     c.drawAtlas(sprite.get(),
                                 SkSpan(xforms.data(), xforms.size()),
                                 SkSpan(texRects.data(), texRects.size()),
                                 {}, SkBlendMode::kPlus,
                                 SkSamplingOptions(SkFilterMode::kNearest),
                                 nullptr, nullptr);
                   })
                       .inset(0)
                       .cache(Cache::None))
            .child(text(u8"50,000 particles — EnTT SoA + one drawAtlas",
                        styleAt(18, 0xff9aa4bb))
                       .absolute().inset(24, 24, 24, 590).zIndex(1)));
  }
};

// ---- 7: reflow + connector (derive phase, live) --------------------------

struct DeriveScene final : Scene {
  int slotIndex = 0;
  double nextMove = 0.0;

  const char *name() const override { return "derive"; }

  Element describe() {
    const float lefts[3] = {520, 300, 640};
    const float tops[3] = {80, 240, 380};
    PathFormat wire;
    wire.width = 3;
    wire.strokeFill = Fill::color({1.0f, 0.71f, 0.42f, 1});
    wire.dashIntervals = {8, 6};

    return stack().fill(Fill::color({0.06f, 0.07f, 0.12f, 1}))
        .child(box().inset(40, 40, 40, 40)
                   .child(text(
                       u8"the quick brown fox jumps over the lazy dog and "
                       u8"keeps running through the tall summer grass until "
                       u8"the river bend appears and the evening light "
                       u8"settles over the water in long amber bands while "
                       u8"the reeds keep time against the slow current and "
                       u8"the ferry rope hums under a traveler's hand",
                       styleAt(26, 0xffdde4f2))
                            .key("body")
                            .flowAround("float", 14))
                   .zIndex(1))
        .child(box().key("anchor").width(16).height(16)
                   .inset(60, 560, 824, 64).absolute()
                   .corners({8}).fill(Fill::color({0.49f, 0.91f, 1, 1})))
        .child(box().key("float").width(200).height(150)
                   .inset(lefts[slotIndex], tops[slotIndex],
                          kSceneSize.width() - lefts[slotIndex] - 200,
                          kSceneSize.height() - tops[slotIndex] - 150)
                   .absolute().corners({14})
                   .fill(Fill::color({0.20f, 0.13f, 0.33f, 1}))
                   .child(text(u8"float", styleAt(18, 0xff9aa4bb))
                              .absolute().inset(12, 12, 12, 110)))
        .child(connector("anchor", "float").inset(0).foreground(wire));
  }

  void setup(Composer &composer, ifrit::tick::Ticker &) override {
    slotIndex = 0;
    nextMove = 0.0;
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextMove)
      return;
    nextMove = elapsed + 1.6;
    slotIndex = (slotIndex + 1) % 3;
    composer.render(describe());
  }
};

// ---------------------------------------------------------------------------
// Stage + FPS overlay (itself an IfritCompose composer)

struct GalleryStage {
  ifrit::tick::FrameClock clock;
  std::unique_ptr<ifrit::tick::Ticker> ticker;
  std::unique_ptr<Composer> composer;
  std::unique_ptr<Scene> scene;

  // Overlay: a second composer layered over the scene.
  ifrit::tick::Ticker overlayTicker;
  Composer overlay{overlayTicker, fonts()};
  FrameStats stats;
  bool showStats = true;

  GalleryStage() {
    overlay.setSize(kSceneSize);
    reset();
  }

  void reset() {
    composer.reset();
    ticker = std::make_unique<ifrit::tick::Ticker>();
    composer = std::make_unique<Composer>(*ticker, fonts());
    composer->setClock(&clock);
    composer->setSize(kSceneSize);
  }

  void activate(std::unique_ptr<Scene> next) {
    reset();
    stats = {};
    scene = std::move(next);
    scene->setup(*composer, *ticker);
  }

  double pendingOverlayMs = 0.0; // charged to the next frame's sample
  std::chrono::steady_clock::time_point lastPresent{};

  /** One full frame of work: tick (step + scene update + reconcile) and
   *  the scene draw are measured together — reconcile and relayout are
   *  real per-frame costs, not free. Returns nothing; stats accumulate. */
  void frame(SkCanvas &canvas, double fixedDt = -1.0) {
    const auto start = std::chrono::steady_clock::now();
    const double dt = fixedDt >= 0 ? fixedDt : clock.tick();
    if (fixedDt >= 0)
      clock.tick(); // keep elapsed advancing for scene update schedules
    ticker->tick(dt);
    scene->update(clock.elapsed(), *composer);
    composer->draw(canvas);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    stats.add(ms + pendingOverlayMs);
    pendingOverlayMs = 0.0;
    drawOverlay(canvas);
  }

  void markPresented() {
    const auto now = std::chrono::steady_clock::now();
    if (lastPresent.time_since_epoch().count() != 0)
      stats.addPresent(std::chrono::duration<double, std::milli>(
                           now - lastPresent)
                           .count());
    lastPresent = now;
  }

  void drawOverlay(SkCanvas &canvas) {
    if (!showStats)
      return;
    const auto overlayStart = std::chrono::steady_clock::now();
    const Composer::Stats &cs = composer->stats();
    char line[160];
    const double presented = stats.presentedFps();
    std::snprintf(line, sizeof(line),
                  "%s   work %5.2f ms (p99 %5.2f)   headroom ~%.0f fps%s%.0f%s",
                  scene->name(), stats.average(), stats.percentile(0.99),
                  stats.fps(), presented > 0 ? "   presented " : "",
                  presented > 0 ? presented : 0.0,
                  presented > 0 ? " fps" : "");
    char line2[160];
    std::snprintf(line2, sizeof(line2),
                  "instances %zu   pictures %zu   textures %zu   live %zu",
                  cs.instances, cs.picturesLive, cs.texturesLive,
                  cs.nodesPainted);
    overlay.render(
        box().child(box().row()
                        .inset(12, kSceneSize.height() - 64, 12, 12)
                        .absolute().corners({8}).padding(10, 6)
                        .fill(Fill::color({0, 0, 0, 0.55f}))
                        .child(box().column().gap(2)
                                   .child(text(toU8(line),
                                               styleAt(15, 0xff7ee8ff)))
                                   .child(text(toU8(line2),
                                               styleAt(13, 0xff9aa4bb))))));
    overlay.draw(canvas);
    pendingOverlayMs = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - overlayStart)
                           .count();
  }
};

std::unique_ptr<Scene> makeScene(int index) {
  switch (index) {
  case 1: return std::make_unique<HeadlineScene>();
  case 2: return std::make_unique<BlendScene>();
  case 3: return std::make_unique<ChromeScene>();
  case 4: return std::make_unique<CrtScene>();
  case 5: return std::make_unique<ParticleScene>();
  case 6: return std::make_unique<DeriveScene>();
  default: return std::make_unique<ScoreboardScene>();
  }
}
constexpr int kSceneCount = 7;

class GalleryWindow final : public QRasterWindow {
public:
  GalleryWindow() {
    setTitle(
        "ComposeGallery — 1..7 scenes · Space pause · -/= speed · F stats");
    resize((int)kSceneSize.width(), (int)kSceneSize.height());
    m_stage.activate(makeScene(0));
    m_timer.setInterval(16);
    QObject::connect(&m_timer, &QTimer::timeout, [this] { update(); });
    m_timer.start();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QImage image((int)kSceneSize.width(), (int)kSceneSize.height(),
                 QImage::Format_RGBA8888_Premultiplied);
    sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
        SkImageInfo::Make(image.width(), image.height(),
                          kRGBA_8888_SkColorType, kPremul_SkAlphaType),
        image.bits(), image.bytesPerLine());
    surface->getCanvas()->clear(SK_ColorBLACK);
    m_stage.frame(*surface->getCanvas());
    QPainter painter(this);
    painter.drawImage(0, 0, image);
    m_stage.markPresented();
  }

  void keyPressEvent(QKeyEvent *event) override {
    const int key = event->key();
    if (key >= Qt::Key_1 && key < Qt::Key_1 + kSceneCount) {
      m_stage.activate(makeScene(key - Qt::Key_1));
      return;
    }
    switch (key) {
    case Qt::Key_Space:
      m_stage.clock.setPaused(!m_stage.clock.paused());
      break;
    case Qt::Key_Minus:
      m_stage.clock.setTimeScale(m_stage.clock.timeScale() * 0.5);
      break;
    case Qt::Key_Equal:
      m_stage.clock.setTimeScale(m_stage.clock.timeScale() * 2.0);
      break;
    case Qt::Key_F:
      m_stage.showStats = !m_stage.showStats;
      break;
    case Qt::Key_Escape:
      close();
      break;
    }
  }

private:
  GalleryStage m_stage;
  QTimer m_timer;
};

int runHeadless(const std::string &outDir) {
  std::filesystem::create_directories(outDir);
  std::printf("%-12s %12s %10s %14s\n", "scene", "work ms", "p99 ms",
              "headroom fps");
  for (int i = 0; i < kSceneCount; ++i) {
    GalleryStage stage;
    stage.activate(makeScene(i));
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)kSceneSize.width(), (int)kSceneSize.height()));

    // 120 frames at a fixed 60 Hz step. The measured span is the FULL
    // frame body — ticker step, scene update, reconcile, scene draw,
    // and the overlay (charged to the following sample). "Headroom" is
    // 1000/work-ms: the unthrottled ceiling, NOT a presented rate.
    for (int f = 0; f < 120; ++f) {
      surface->getCanvas()->clear(SK_ColorBLACK);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
    }
    std::printf("%-12s %12.2f %10.2f %14.0f\n", stage.scene->name(),
                stage.stats.average(), stage.stats.percentile(0.99),
                stage.stats.fps());

    SkBitmap bm;
    bm.allocPixels(surface->imageInfo());
    surface->readPixels(bm.pixmap(), 0, 0);
    const std::string path =
        outDir + "/gallery_" + stage.scene->name() + ".png";
    SkFILEWStream stream(path.c_str());
    if (!stream.isValid() || !SkPngEncoder::Encode(&stream, bm.pixmap(), {}))
      return 1;
  }
  std::printf("wrote %d gallery scenes to %s\n", kSceneCount,
              outDir.c_str());
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc >= 2 && std::string(argv[1]) == "--headless")
    return runHeadless(argc >= 3 ? argv[2] : "compose_gallery_out");

  QGuiApplication app(argc, argv);
  GalleryWindow window;
  window.show();
  return app.exec();
}
