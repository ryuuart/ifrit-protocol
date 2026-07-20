// Headless SigilCompose gallery: renders phase-1 stress-catalog panels
// (STRESS_TESTS.md items 1–3) to PNG. Out dir: first argument, default
// ./compose_demo_out. The interactive Qt ComposeGallery follows later.

#include <sigilcompose/Compose.h>
#include <sigilcompose/Util.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Layouts.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Shapes.h>
#include <sigilimage/ImageAsset.h>
#include <include/effects/SkImageFilters.h>
#include <include/core/SkPathBuilder.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

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

using namespace sigil::compose;


namespace {

sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

sigil::weave::TextStyle styleAt(float size, SkColor color = SK_ColorWHITE) {
  sigil::weave::TextStyle s;
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
      .child(text(sigil::compose::util::toU8(row.name), styleAt(18, 0xffe8ecf8)).grow(1))
      .child(text(sigil::compose::util::toU8(std::to_string(row.score)),
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

// ---- Panel 5: CRT stack + bloom (stress #13/#14) -------------------------

Element scanlines(float phase, SkColor4f tint) {
  return custom([phase, tint](SkCanvas &c, const PaintContext &ctx) {
           SkPaint p;
           p.setColor4f(tint, nullptr);
           for (float y = phase; y < ctx.size.height(); y += 6.0f)
             c.drawRect(SkRect::MakeXYWH(0, y, ctx.size.width(), 2.4f), p);
         })
      .absolute().inset(0)
      .blend(SkBlendMode::kPlus); // stacked layers accumulate brightness
}

Element crtBloomPanel(SkSize) {
  return stack().fill(Fill::color({0.02f, 0.03f, 0.05f, 1}))
      // Bloom: a blurred bright copy beneath the sharp headline, both
      // over the CRT glow field.
      .child(box().column().padding(70).gap(10).inset(0).zIndex(2)
                 .child(text(u8"PHOSPHOR", styleAt(96, 0xff9df2ff)))
                 .child(text(u8"plus-blended scanline layers brighten where they stack;",
                             styleAt(17, 0xff9aa4bb)))
                 .child(text(u8"the halo is the same headline, blurred and plus-blended.",
                             styleAt(17, 0xff9aa4bb))))
      .child(box().column().padding(70).inset(0).zIndex(1)
                 .effect(Effect::filter(SkImageFilters::Blur(14, 14, nullptr)))
                 .blend(SkBlendMode::kPlus)
                 .cache(Cache::Texture) // bloom baked once
                 .child(text(u8"PHOSPHOR", styleAt(96, 0xff2a7f96))))
      .child(scanlines(0.0f, {0.10f, 0.22f, 0.16f, 1}).zIndex(0))
      .child(scanlines(2.0f, {0.16f, 0.10f, 0.20f, 1})
                 .inset(0, 0, 120, 0).zIndex(0))
      .child(scanlines(4.0f, {0.05f, 0.12f, 0.24f, 1})
                 .inset(140, 40, 0, 0).zIndex(0));
}

} // namespace

// ---- Panel 6: organic chaos — the free-form kit in one frame -------------
// Shapes.h blobs/stars/squircles, Layouts.h scatter/radial/along-path,
// an element-stamp border (ContourWalk.stamp), per-edge chrome
// (shapes::edges), and routed connectors (Routers.h).

Element organicPanel(SkSize size) {
  const float w = size.width(), h = size.height();

  auto chaos = layout(layouts::Scatter{.seed = 77, .jitter = 0.9f})
                   .inset(0).zIndex(0);
  for (int i = 0; i < 12; ++i)
    chaos.child(box().width(80 + (float)(i % 5) * 24)
                    .height(64 + (float)(i % 4) * 22)
                    .outline(shapes::blob((uint32_t)(300 + i), 0.34f,
                                          5 + i % 6))
                    .fill(Fill::color({0.3f + 0.04f * (float)i, 0.18f,
                                       0.42f - 0.02f * (float)i, 0.55f}))
                    .blend(SkBlendMode::kPlus));

  ContourWalk stampWalk;
  stampWalk.spacing = 44.0f;
  stampWalk.stamp = box().width(16).height(16)
                        .outline(shapes::star(4, 0.4f))
                        .fill(Fill::color({1.0f, 0.72f, 0.4f, 0.95f}));
  auto sigilStar =
      box().key("sigil").width(280).height(280)
          .inset((w - 280) / 2, (h - 280) / 2 - 20, (w - 280) / 2,
                 (h - 280) / 2 + 20)
          .absolute().zIndex(3)
          .outline(shapes::rounded(shapes::star(7, 0.62f), 12))
          .fill(Fill::color({0.95f, 0.42f, 0.28f, 0.95f}))
          .foreground(stampWalk)
          .child(layout(layouts::Radial{.radiusFraction = 0.5f}).inset(0)
                     .children([&] {
                       std::vector<Element> beads;
                       for (int i = 0; i < 10; ++i)
                         beads.push_back(
                             box().width(14).height(14)
                                 .outline(shapes::polygon(3 + i % 4))
                                 .fill(Fill::color(
                                     {0.2f, 0.1f, 0.14f, 0.9f})));
                       return beads;
                     }()));

  PathFormat topDash;
  topDash.width = 5;
  topDash.strokeFill = Fill::color({1.0f, 0.71f, 0.42f, 1});
  topDash.dashIntervals = {14, 6};
  ContourWalk leftDots;
  leftDots.spacing = 12.0f;
  leftDots.draw = [](SkCanvas &c, const PathSample &,
                     const PaintContext &) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(0xffffd9a0);
    c.drawCircle(0, 0, 2.4f, p);
  };
  auto plaque =
      box().key("plaque").width(240).height(100)
          .inset(36, h - 140, w - 276, 40).absolute().zIndex(2)
          .corners({0, 24, 0, 24})
          .fill(Fill::color({0.11f, 0.12f, 0.2f, 0.97f}))
          .foreground(shapes::onEdges(shapes::Edge::Top, topDash))
          .foreground(shapes::onEdges(shapes::Edge::Left, leftDots))
          .padding(16, 14)
          .child(text(u8"per-edge chrome", styleAt(15, 0xff9aa4bb)))
          .child(text(u8"stamped-star border above",
                      styleAt(16, 0xffe8d9c2)));

  auto moon = [&](const char *key, uint32_t seed, float l, float t) {
    return box().key(key).width(84).height(84)
        .inset(l, t, w - l - 84, h - t - 84).absolute().zIndex(2)
        .outline(shapes::blob(seed, 0.28f, 7))
        .fill(Fill::color({0.36f, 0.62f, 0.66f, 0.95f}))
        .foreground(util::stroke(2, Fill::color({0.8f, 1, 1, 0.6f})));
  };

  PathFormat wire;
  wire.width = 3;
  wire.strokeFill = Fill::color({1.0f, 0.84f, 0.5f, 0.85f});
  wire.dashIntervals = {10, 7};
  PathFormat bow;
  bow.width = 2.4f;
  bow.strokeFill = Fill::color({0.62f, 0.9f, 0.9f, 0.8f});

  return stack().fill(backdropGradient(size))
      .child(chaos)
      .child(std::move(sigilStar))
      .child(moon("moon-a", 21, 70, 60))
      .child(moon("moon-b", 22, w - 150, 100))
      .child(std::move(plaque))
      .child(connector("plaque", "sigil", routers::orthogonal(16))
                 .inset(0).foreground(wire).zIndex(1))
      .child(connector("moon-a", "moon-b", routers::arc(0.35f))
                 .inset(0).foreground(bow).zIndex(1));
}

// ---- Panel 7: tile maze — atlas regions in chunks (stress #15) -----------

std::shared_ptr<sigil::image::ImageAsset> mazeAtlas() {
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
  SkCanvas &c = *s->getCanvas();
  SkPaint p;
  p.setColor(SkColorSetRGB(0x18, 0x16, 0x24)); // 0 floor
  c.drawRect(SkRect::MakeXYWH(0, 0, 16, 16), p);
  p.setColor(SkColorSetRGB(0x5a, 0x33, 0x2c)); // 1 wall
  c.drawRect(SkRect::MakeXYWH(16, 0, 16, 16), p);
  p.setColor(SkColorSetRGB(0x3a, 0x1f, 0x1c));
  for (int row = 0; row < 4; ++row)
    c.drawRect(SkRect::MakeXYWH(16, (float)row * 4 + 3, 16, 1), p);
  p.setColor(SkColorSetRGB(0x1c, 0x2a, 0x1e)); // 2 moss
  c.drawRect(SkRect::MakeXYWH(32, 0, 16, 16), p);
  p.setColor(SkColorSetRGB(0x2f, 0x49, 0x2c));
  c.drawRect(SkRect::MakeXYWH(36, 5, 4, 3), p);
  p.setColor(SkColorSetRGB(0x2a, 0x12, 0x18)); // 3 ember
  c.drawRect(SkRect::MakeXYWH(48, 0, 16, 16), p);
  p.setColor(SkColorSetRGB(0xff, 0x7a, 0x33));
  c.drawRect(SkRect::MakeXYWH(54, 6, 4, 4), p);
  SkBitmap bm;
  bm.allocPixels(s->imageInfo());
  s->readPixels(bm.pixmap(), 0, 0);
  SkDynamicMemoryWStream stream;
  SkPngEncoder::Encode(&stream, bm.pixmap(), {});
  return std::make_shared<sigil::image::ImageAsset>(
      *sigil::image::ImageAsset::decode(stream.detachAsData()));
}

Element tileMazePanel(SkSize size) {
  static auto atlas = mazeAtlas();
  constexpr float kTile = 24.0f;
  const int cols = (int)(size.width() / kTile);
  const int rows = (int)((size.height() - 60) / kTile);
  auto tiles = box().width((float)cols * kTile)
                   .height((float)rows * kTile);
  for (int y = 0; y < rows; ++y)
    for (int x = 0; x < cols; ++x) {
      const uint32_t hsh =
          (uint32_t)(x * 73856093 ^ y * 19349663 ^ 0x9e3779b9);
      int id = 0;
      const bool border = x == 0 || y == 0 || x == cols - 1 || y == rows - 1;
      if (border || (x % 2 == 0 && (hsh & 7) < 5 && y % 4 != 2))
        id = 1;
      else if ((hsh % 13) == 4)
        id = 2;
      else if ((hsh % 29) == 9)
        id = 3;
      tiles.child(image(atlas)
                      .region(SkRect::MakeXYWH((float)id * 16, 0, 16, 16))
                      .absolute()
                      .inset((float)x * kTile, (float)y * kTile, 0, 0)
                      .width(kTile).height(kTile));
    }
  return box().padding(20).fill(Fill::color({0.03f, 0.03f, 0.07f, 1}))
      .child(text(u8"maze tileset — image(atlas).region(cell) per tile",
                  styleAt(16, 0xff9aa4bb)))
      .child(box().height(10))
      .child(std::move(tiles));
}

int main(int argc, char **argv) {
  std::filesystem::path outDir = argc > 1 ? argv[1] : "compose_demo_out";
  std::filesystem::create_directories(outDir);

  sigil::motion::Ticker ticker;

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

  {
    Composer composer(ticker, fonts());
    SkSize size = {860, 520};
    composer.setSize(size);
    composer.render(crtBloomPanel(size));
    if (!writePanel(composer, size, outDir / "crt_bloom.png"))
      return 1;
  }

  {
    Composer composer(ticker, fonts());
    SkSize size = {900, 640};
    composer.setSize(size);
    composer.render(organicPanel(size));
    if (!writePanel(composer, size, outDir / "organic.png"))
      return 1;
    std::printf("organic: %zu instances, %zu pictures\n",
                composer.stats().instances, composer.stats().picturesLive);
  }

  {
    Composer composer(ticker, fonts());
    SkSize size = {720, 560};
    composer.setSize(size);
    composer.render(tileMazePanel(size));
    if (!writePanel(composer, size, outDir / "tile_maze.png"))
      return 1;
  }

  {
    // Query surround (stress #5): the HOST draws around resolved nodes
    // — a ring from bounds(), and a probe row colored by hitTest().
    Composer composer(ticker, fonts());
    SkSize size = {720, 420};
    composer.setSize(size);
    composer.render(
        stack().fill(Fill::color({0.05f, 0.06f, 0.1f, 1}))
            .child(box().key("hero").width(180).height(180)
                       .inset(80, 100, 460, 140).absolute()
                       .outline(shapes::star(5))
                       .fill(Fill::color({0.95f, 0.42f, 0.28f, 1})))
            .child(box().key("side").width(140).height(90)
                       .inset(420, 150, 160, 180).absolute().corners({16})
                       .fill(Fill::color({0.36f, 0.62f, 0.66f, 1}))));

    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul((int)size.width(), (int)size.height()));
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());

    SkCanvas &canvas = *surface->getCanvas();
    if (auto hero = composer.bounds("hero")) {
      SkPaint ring;
      ring.setAntiAlias(true);
      ring.setStyle(SkPaint::kStroke_Style);
      ring.setStrokeWidth(3);
      ring.setColor(0xffffb46b);
      canvas.drawRoundRect(hero->makeOutset(10, 10), 18, 18, ring);
    }
    for (int x = 20; x < (int)size.width(); x += 24) {
      const SkPoint probe{(float)x, 190.0f};
      auto hit = composer.hitTest(probe);
      SkPaint dot;
      dot.setAntiAlias(true);
      dot.setColor(!hit ? 0xff3a3f52
                        : (*hit == "hero" ? 0xffff5e8a : 0xff7ee8ff));
      canvas.drawCircle(probe.x(), probe.y(), 4, dot);
    }
    SkBitmap bm;
    bm.allocPixels(surface->imageInfo());
    surface->readPixels(bm.pixmap(), 0, 0);
    SkFILEWStream stream((outDir / "query_hits.png").string().c_str());
    if (!stream.isValid() || !SkPngEncoder::Encode(&stream, bm.pixmap(), {}))
      return 1;
  }

  std::printf("wrote 8 panels to %s\n", outDir.string().c_str());
  return 0;
}
