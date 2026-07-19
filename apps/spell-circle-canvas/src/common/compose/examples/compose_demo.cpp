// Headless IfritCompose gallery: renders phase-1 stress-catalog panels
// (STRESS_TESTS.md items 1–3) to PNG. Out dir: first argument, default
// ./compose_demo_out. The interactive Qt ComposeGallery follows later.

#include <ifritcompose/Compose.h>
#include <ifritcompose/Decorations.h>
#include <include/core/SkPathBuilder.h>

#include <textflow/FontContext.h>
#include <textflow/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <include/encode/SkPngEncoder.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace ifrit::compose;

namespace {
std::u8string toU8(const std::string &s) {
  return std::u8string(s.begin(), s.end());
}
} // namespace

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

bool writePanel(Composer &composer, SkSize size,
                const std::filesystem::path &path) {
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul((int)size.width(), (int)size.height()));
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  surface->readPixels(bm.pixmap(), 0, 0);
  SkFILEWStream stream(path.string().c_str());
  return stream.isValid() && SkPngEncoder::Encode(&stream, bm.pixmap(), {});
}

Fill backdropGradient(SkSize size) {
  SkPoint pts[2] = {{0, 0}, {size.width(), size.height()}};
  SkColor4f colors[2] = {SkColor4f::FromColor(SkColorSetRGB(0x14, 0x0e, 0x26)),
                         SkColor4f::FromColor(SkColorSetRGB(0x5b, 0x21, 0x38))};
  return Fill::shader(SkShaders::LinearGradient(
      pts, SkGradient({colors, SkTileMode::kClamp}, {})));
}

// ---- Panel 1: static typographic poster (stress #1) ----------------------

Element poster(SkSize size) {
  return box().column().padding(64).gap(20).fill(backdropGradient(size))
      .child(text(u8"IFRIT", styleAt(120, SkColorSetRGB(0xff, 0xb4, 0x6b)))
                 .key("title").zIndex(2))
      .child(text(u8"PROTOCOL", styleAt(120, SkColorSetRGB(0xff, 0x5e, 0x8a)))
                 .zIndex(2))
      .child(box().row().gap(14).alignItems(Align::Baseline)
                 .child(text(u8"vol. 4", styleAt(16, 0xff9aa4bb)))
                 .child(text(u8"summer program",
                             styleAt(26, 0xffc9d1e8))))
      .child(custom([](SkCanvas &c, const PaintContext &ctx) {
               SkPaint ring;
               ring.setAntiAlias(true);
               ring.setStyle(SkPaint::kStroke_Style);
               for (int i = 0; i < 3; ++i) {
                 ring.setStrokeWidth(4.0f - i);
                 ring.setColor(i % 2 ? 0x88b18cff : 0xcc7ee8ff);
                 c.drawCircle(ctx.size.width() / 2, ctx.size.height() / 2,
                              ctx.size.width() * (0.42f - 0.1f * i), ring);
               }
             }).absolute().inset(660, 920, 100, 110).zIndex(0));
}

// ---- Panel 2: data-driven scoreboard (stress #2 snapshot) ----------------

struct Row {
  std::string name;
  int score;
  bool highlighted;
  bool operator==(const Row &) const = default;
};

Element scoreRow(const Row &row) {
  return box().row().gap(12).padding(10).corners({8})
      .fill(Fill::color(row.highlighted
                            ? SkColor4f{0.32f, 0.18f, 0.45f, 1}
                            : SkColor4f{0.10f, 0.11f, 0.16f, 1}))
      .child(text(toU8(row.name), styleAt(18, 0xffe8ecf8)).grow(1))
      .child(text(toU8(std::to_string(row.score)),
                  styleAt(18, 0xff7ee8ff)));
}

Element scoreboard(const std::vector<Row> &rows, SkSize size) {
  auto list = box().column().gap(6).padding(24).fill(backdropGradient(size));
  list.child(text(u8"STANDINGS", styleAt(28, 0xffffb46b)));
  for (const Row &row : rows)
    list.child(memo(row, scoreRow).key(row.name));
  return list;
}

// ---- Panel 3: layered tables with blend (stress #3) ----------------------

Element blendPanel(SkSize size) {
  std::vector<Row> a = {{"alpha", 42, true}, {"beta", 17, false},
                        {"gamma", 8, false}};
  std::vector<Row> b = {{"delta", 99, false}, {"epsilon", 64, true},
                        {"zeta", 31, false}};
  auto tableA = box().column().gap(6).padding(20).width(360)
                    .children(std::vector<Element>{}); // filled below
  for (const Row &r : a)
    tableA.child(scoreRow(r));
  auto tableB = box().column().gap(6).padding(20).width(360);
  for (const Row &r : b)
    tableB.child(scoreRow(r));

  return stack().fill(backdropGradient(size))
      .child(std::move(tableA).inset(40, 60, 240, 60).zIndex(0))
      .child(std::move(tableB)
                 .inset(240, 140, 40, 20)
                 .zIndex(1)
                 .opacity(0.85f)
                 .blend(SkBlendMode::kScreen));
}

// ---- Panel 4: chrome — per-edge decoration primitives (stress #9/10) ----

Element chromePanel(SkSize size) {
  // A leaf-ish stamp path for the vine walk.
  SkPathBuilder leaf;
  leaf.moveTo(0, 0);
  leaf.quadTo(6, -7, 14, 0);
  leaf.quadTo(6, 7, 0, 0);

  PathFormat dashedBottom;
  dashedBottom.width = 4;
  dashedBottom.strokeFill = Fill::color({0.49f, 0.91f, 1.0f, 1});
  dashedBottom.dashIntervals = {14, 8};

  PathFormat stampedFrame;
  stampedFrame.width = 2;
  stampedFrame.strokeFill = Fill::color({0.69f, 0.55f, 1.0f, 1});
  stampedFrame.stampPath = leaf.detach();
  stampedFrame.stampAdvance = 20;

  ContourWalk dotWalk;
  dotWalk.spacing = 26;
  dotWalk.draw = [](SkCanvas &c, const PathSample &s, const PaintContext &) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(SkColorSetARGB(0xff, 0xff, 0xb4, 0x6b));
    c.drawCircle(0, -10, 2.0f + 1.6f * std::sin(s.fraction * 6.28318f), p);
  };

  return stack().fill(backdropGradient(size))
      .child(box().inset(60, 70, 60, 70)
                 .corners({24})
                 .fill(Fill::color({0.07f, 0.08f, 0.14f, 0.92f}))
                 .foreground(stampedFrame)
                 .foreground(dotWalk)
                 .child(box().column().padding(40).gap(14)
                            .child(text(u8"CHROME", styleAt(56, 0xffe8ecf8)))
                            .child(text(u8"PathFormat stamps a leaf around the frame,",
                                        styleAt(17, 0xff9aa4bb)))
                            .child(text(u8"a ContourWalk orbits dots along the outline,",
                                        styleAt(17, 0xff9aa4bb)))
                            .child(text(u8"and the divider below is two numbers of dash data.",
                                        styleAt(17, 0xff9aa4bb)))
                            .child(box().height(30)
                                       .foreground(dashedBottom))));
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path outDir = argc > 1 ? argv[1] : "compose_demo_out";
  std::filesystem::create_directories(outDir);

  ifrit::tick::Ticker ticker;

  {
    Composer composer(ticker, fonts());
    SkSize size = {1080, 1350};
    composer.setSize(size);
    composer.render(poster(size));
    if (!writePanel(composer, size, outDir / "poster.png"))
      return 1;
    std::printf("poster: %zu instances, %zu pictures\n",
                composer.stats().instances, composer.stats().picturesLive);
  }
  {
    Composer composer(ticker, fonts());
    SkSize size = {560, 640};
    composer.setSize(size);
    std::vector<Row> rows = {{"ember", 128, false},   {"sigil", 96, true},
                             {"cinder", 77, false},   {"ash", 64, false},
                             {"flare", 51, false},    {"soot", 12, false}};
    composer.render(scoreboard(rows, size));
    if (!writePanel(composer, size, outDir / "scoreboard.png"))
      return 1;
  }
  {
    Composer composer(ticker, fonts());
    SkSize size = {640, 480};
    composer.setSize(size);
    composer.render(blendPanel(size));
    if (!writePanel(composer, size, outDir / "stack_blend.png"))
      return 1;
  }

  {
    Composer composer(ticker, fonts());
    SkSize size = {760, 520};
    composer.setSize(size);
    composer.render(chromePanel(size));
    if (!writePanel(composer, size, outDir / "chrome.png"))
      return 1;
  }

  std::printf("wrote 4 panels to %s\n", outDir.string().c_str());
  return 0;
}
