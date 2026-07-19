// ComposeGallery — the interactive vehicle for the stress catalog's
// motion items: live data mutations with transitions, Choreograph
// bindings, and blend stacks, drawn through IfritCompose into a Qt
// raster window. (The full Qt Quick gallery in the TextFlowGallery mold
// arrives once scenes stabilize; this window is deliberately minimal.)
//
// Keys: 1..3 scenes · Space pause · -/= time scale · Esc quit.
// `ComposeGallery --headless <outdir>` renders each scene mid-animation
// to PNG and exits (CI smoke / visual review).

#include <ifritcompose/Compose.h>
#include <ifritcompose/Util.h>

#include <textflow/FontContext.h>
#include <textflow/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/encode/SkPngEncoder.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QRasterWindow>

#include <QtCore/QTimer>

#include <algorithm>
#include <cmath>
#include <memory>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace ifrit::compose;
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

struct Scene {
  virtual ~Scene() = default;
  virtual const char *name() const = 0;
  /** Called on activation and whenever data should change. */
  virtual void setup(Composer &composer, ifrit::tick::Ticker &ticker) = 0;
  /** Called every frame with scaled elapsed time; re-render as needed. */
  virtual void update(double elapsed, Composer &composer) = 0;
};

// ---- Scene 1: live scoreboard (transitions on data mutations) ------------

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
        .child(text(ifrit::compose::util::toU8(row.name), styleAt(20, 0xffe8ecf8)).grow(1))
        .child(text(ifrit::compose::util::toU8(std::to_string(row.score)),
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

// ---- Scene 2: choreographed headline (bindings + custom leaf) ------------

struct HeadlineScene final : Scene {
  choreograph::Output<float> drop{-40.0f}, fade{0.0f}, spin{0.0f};

  const char *name() const override { return "headline"; }

  void setup(Composer &composer, ifrit::tick::Ticker &ticker) override {
    drop = -40.0f;
    fade = 0.0f;
    ticker.timeline().apply(&drop).then<choreograph::RampTo>(
        0.0f, 0.9f, &choreograph::easeOutQuint);
    ticker.timeline().apply(&fade).then<choreograph::RampTo>(1.0f, 0.7f);
    // Endless spin as a steppable (loops don't self-terminate).
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
                       const float a =
                           spin.value() * 3.14159f / 180.0f +
                           i * 3.14159f / 3.0f;
                       ring.setColor(i % 2 ? 0x88b18cff : 0xcc7ee8ff);
                       c.drawCircle(cx + std::cos(a) * 90,
                                    cy + std::sin(a) * 90, 34, ring);
                     }
                   })
                       .height(280)
                       .cache(Cache::None))); // per-frame by declaration
  }

  void update(double, Composer &) override {}
};

// ---- Scene 3: blend stack (binding-driven overlap) -----------------------

struct BlendScene final : Scene {
  choreograph::Output<float> slide{0.0f};

  const char *name() const override { return "blend"; }

  static Element table(const char *title, SkColor4f base) {
    auto t = box().column().gap(6).padding(18).width(380).corners({12})
                 .fill(Fill::color(base));
    t.child(text(ifrit::compose::util::toU8(title), styleAt(22, 0xffffffff)));
    for (int i = 0; i < 4; ++i)
      t.child(box().row().gap(10).padding(8).corners({6})
                  .fill(Fill::color({0, 0, 0, 0.35f}))
                  .child(text(ifrit::compose::util::toU8("row " + std::to_string(i)),
                              styleAt(16, 0xffdde4f2)).grow(1))
                  .child(text(ifrit::compose::util::toU8(std::to_string(37 * (i + 1))),
                              styleAt(16, 0xff7ee8ff))));
    return t;
  }

  void setup(Composer &composer, ifrit::tick::Ticker &ticker) override {
    ticker.add([this](double dt) {
      static double t = 0;
      t += dt;
      slide = (float)(std::sin(t * 0.8) * 140.0);
      return true;
    });

    composer.render(
        stack().fill(Fill::color({0.08f, 0.05f, 0.13f, 1}))
            .child(table("ALPHA", {0.16f, 0.10f, 0.28f, 1})
                       .inset(70, 90, 450, 130).zIndex(0))
            .child(table("BETA", {0.10f, 0.20f, 0.30f, 1})
                       .inset(300, 170, 220, 50)
                       .zIndex(1)
                       .translateX(&slide)
                       .opacity(0.85f)
                       .blend(SkBlendMode::kScreen)));
  }

  void update(double, Composer &) override {}
};

// ---------------------------------------------------------------------------

struct Stage {
  ifrit::tick::FrameClock clock;
  std::unique_ptr<ifrit::tick::Ticker> ticker;
  std::unique_ptr<Composer> composer;
  std::unique_ptr<Scene> scene;

  Stage() { reset(); }

  void reset() {
    // Fresh ticker/composer per scene so steppables and motions die
    // with the scene they belong to (order: composer references ticker).
    composer.reset();
    ticker = std::make_unique<ifrit::tick::Ticker>();
    composer = std::make_unique<Composer>(*ticker, fonts());
    composer->setClock(&clock);
    composer->setSize(kSceneSize);
  }

  void activate(std::unique_ptr<Scene> next) {
    reset();
    scene = std::move(next);
    scene->setup(*composer, *ticker);
  }

  /** Ticks and returns whether anything needs painting. */
  bool frame() {
    const double dt = clock.tick();
    bool animating = ticker->tick(dt);
    scene->update(clock.elapsed(), *composer);
    return animating || composer->dirty() || ticker->active();
  }
};

std::unique_ptr<Scene> makeScene(int index) {
  switch (index) {
  case 1: return std::make_unique<HeadlineScene>();
  case 2: return std::make_unique<BlendScene>();
  default: return std::make_unique<ScoreboardScene>();
  }
}

class GalleryWindow final : public QRasterWindow {
public:
  GalleryWindow() {
    setTitle("ComposeGallery — 1..3 scenes · Space pause · -/= speed");
    resize((int)kSceneSize.width(), (int)kSceneSize.height());
    m_stage.activate(makeScene(0));
    m_timer.setInterval(16);
    QObject::connect(&m_timer, &QTimer::timeout, [this] {
      m_stage.frame();
      update(); // gallery: always repaint; hosts would gate on frame()
    });
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
    m_stage.composer->draw(*surface->getCanvas());
    QPainter painter(this);
    painter.drawImage(0, 0, image);
  }

  void keyPressEvent(QKeyEvent *event) override {
    switch (event->key()) {
    case Qt::Key_1: case Qt::Key_2: case Qt::Key_3:
      m_stage.activate(makeScene(event->key() - Qt::Key_1));
      break;
    case Qt::Key_Space:
      m_stage.clock.setPaused(!m_stage.clock.paused());
      break;
    case Qt::Key_Minus:
      m_stage.clock.setTimeScale(m_stage.clock.timeScale() * 0.5);
      break;
    case Qt::Key_Equal:
      m_stage.clock.setTimeScale(m_stage.clock.timeScale() * 2.0);
      break;
    case Qt::Key_Escape:
      close();
      break;
    }
  }

private:
  Stage m_stage;
  QTimer m_timer;
};

int runHeadless(const std::string &outDir) {
  std::filesystem::create_directories(outDir);
  for (int i = 0; i < 3; ++i) {
    Stage stage;
    stage.activate(makeScene(i));
    // Advance ~0.6 s in fixed steps so animations are mid-flight.
    for (int f = 0; f < 36; ++f) {
      stage.ticker->tick(1.0 / 60.0);
      stage.scene->update(f / 60.0, *stage.composer);
    }
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)kSceneSize.width(), (int)kSceneSize.height()));
    surface->getCanvas()->clear(SK_ColorBLACK);
    stage.composer->draw(*surface->getCanvas());
    SkBitmap bm;
    bm.allocPixels(surface->imageInfo());
    surface->readPixels(bm.pixmap(), 0, 0);
    const std::string path =
        outDir + "/gallery_" + stage.scene->name() + ".png";
    SkFILEWStream stream(path.c_str());
    if (!stream.isValid() || !SkPngEncoder::Encode(&stream, bm.pixmap(), {}))
      return 1;
  }
  std::printf("wrote 3 gallery scenes to %s\n", outDir.c_str());
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
