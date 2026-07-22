#pragma once
// Free-form / organic showcase: the "nothing here is a rectangle"
// scene. Seeded blobs scattered chaotically (deterministic per seed),
// a rounded-star centerpiece ringed by an element-stamp border
// (ContourWalk stamps a nested composed ornament), a radial rune ring
// and beads riding a star contour (Layouts.h schemes), a plaque
// dressed per-edge (Shapes.h edges()), routed connectors
// (Routers.h), and a wandering hit probe reading
// Composer::hitTest() live. Catalog: #5 #9 #10 #12 + the shape kit.

#include "GalleryCore.h"

#include <sigilcompose/Layouts.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Shapes.h>

namespace compose_gallery {

struct OrganicScene final : Scene {
  choreograph::Output<float> spin{0.0f};
  choreograph::Output<float> pulse{1.0f};
  std::string hitLabel = "—";
  SkPoint probe{0, 0};

  const char *name() const override { return "organic"; }

  static Fill emberGradient(float w, float h) {
    SkPoint pts[2] = {{0, 0}, {w, h}};
    SkColor4f colors[2] = {
        SkColor4f::FromColor(SkColorSetRGB(0x1a, 0x10, 0x2e)),
        SkColor4f::FromColor(SkColorSetRGB(0x43, 0x14, 0x2c))};
    return Fill::shader(SkShaders::LinearGradient(
        pts, SkGradient({colors, SkTileMode::kClamp}, {})));
  }

  Element describe() {
    const float w = kSceneSize.width(), h = kSceneSize.height();

    // -- chaos bed: seeded blobs, scatter-laid, no two runs different --
    auto chaos = layout(layouts::Scatter{.seed = 77, .jitter = 0.9f})
                     .inset(0).zIndex(0);
    for (int i = 0; i < 14; ++i) {
      const float hue = (float)i / 14.0f;
      const SkColor4f tint{0.25f + 0.5f * hue, 0.2f,
                           0.45f - 0.25f * hue, 0.5f};
      chaos.child(box().width(70 + (float)(i % 5) * 26)
                      .height(60 + (float)(i % 4) * 24)
                      .outline(shapes::blob((uint32_t)(100 + i), 0.35f,
                                            5 + i % 6))
                      .fill(Fill::color(tint))
                      .blend(SkBlendMode::kPlus));
    }

    // -- centerpiece: rounded star, stamped element border, breathing --
    ContourWalk stampWalk;
    stampWalk.spacing = 46.0f;
    stampWalk.stamp = box().width(18).height(18)
                          .outline(shapes::star(4, 0.4f))
                          .fill(Fill::color({1.0f, 0.71f, 0.42f, 0.9f}))
                          .foreground(util::stroke(
                              1.2f, Fill::color({1, 0.9f, 0.75f, 1})));
    auto centerpiece =
        box().key("sigil").width(300).height(300)
            .inset((w - 300) / 2, (h - 300) / 2, (w - 300) / 2,
                   (h - 300) / 2)
            .zIndex(3)
            .outline(shapes::rounded(shapes::star(7, 0.62f), 14))
            .fill(Fill::color({0.96f, 0.42f, 0.29f, 0.92f}))
            .rotate(&spin).scale(&pulse)
            .foreground(stampWalk)
            .child(layout(layouts::Radial{.radiusFraction = 0.52f})
                       .inset(0)
                       .children([&] {
                         std::vector<Element> runes;
                         const char8_t *glyphs[] = {u8"ᚠ", u8"ᚢ", u8"ᚦ",
                                                    u8"ᚨ", u8"ᚱ", u8"ᚲ",
                                                    u8"ᚷ", u8"ᚹ", u8"ᚺ"};
                         for (int i = 0; i < 9; ++i)
                           runes.push_back(text(
                               std::u8string(glyphs[(size_t)i]),
                               styleAt(22, SkColorSetARGB(
                                               0xff, 0x2a, 0x10, 0x1c))));
                         return runes;
                       }()));

    // -- satellites: blob moons the connectors route between ----------
    auto moon = [&](const char *key, uint32_t seed, float l, float t) {
      return box().key(key).width(90).height(90)
          .inset(l, t, w - l - 90, h - t - 90).zIndex(2)
          .outline(shapes::blob(seed, 0.28f, 7))
          .fill(Fill::color({0.36f, 0.62f, 0.66f, 0.95f}))
          .foreground(util::stroke(2, Fill::color({0.8f, 1, 1, 0.6f})));
    };

    // -- plaque: per-edge chrome (#9) ---------------------------------
    PathFormat topDash;
    topDash.width = 5;
    topDash.strokeFill = Fill::color({1.0f, 0.71f, 0.42f, 1});
    topDash.dashIntervals = {14, 6};
    PathFormat bottomStamp;
    bottomStamp.width = 3;
    bottomStamp.strokeFill = Fill::color({0.62f, 0.83f, 1.0f, 1});
    {
      SkPathBuilder zig;
      zig.moveTo(0, 3);
      zig.lineTo(4, -3);
      zig.lineTo(8, 3);
      bottomStamp.stampPath = zig.detach();
      bottomStamp.stampAdvance = 9;
    }
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
        box().key("plaque").width(250).height(120)
            .inset(40, h - 170, w - 290, 50).zIndex(2)
            .corners({0, 26, 0, 26})
            .fill(Fill::color({0.11f, 0.12f, 0.2f, 0.96f}))
            .foreground(shapes::onEdges(shapes::Edge::Top, topDash))
            .foreground(shapes::onEdges(shapes::Edge::Bottom, bottomStamp))
            .foreground(shapes::onEdges(shapes::Edge::Left, leftDots))
            .padding(18, 16)
            .child(text(u8"per-edge chrome:", styleAt(15, 0xff9aa4bb)))
            .child(text(u8"dash / zigzag / dots / bare",
                        styleAt(17, 0xffe8d9c2)));

    // -- routed connectors (#12) --------------------------------------
    PathFormat wire;
    wire.width = 3;
    wire.strokeFill = Fill::color({1.0f, 0.84f, 0.5f, 0.85f});
    wire.dashIntervals = {10, 7};
    PathFormat bow;
    bow.width = 2.4f;
    bow.strokeFill = Fill::color({0.62f, 0.9f, 0.9f, 0.8f});

    // The probe marker and hit readout live in SLOTS: independent
    // update domains — the world above is described ONCE and its
    // caches stay warm while these two mount points churn per frame.
    return stack().fill(emberGradient(w, h))
        .child(chaos)
        .child(std::move(centerpiece))
        .child(moon("moon-a", 21, 90, 80))
        .child(moon("moon-b", 22, w - 170, 120))
        .child(std::move(plaque))
        .child(connector("plaque", "sigil", routers::orthogonal(18))
                   .inset(0).foreground(wire).zIndex(1))
        .child(connector("moon-a", "moon-b", routers::arc(0.35f))
                   .inset(0).foreground(bow).zIndex(1))
        .child(slot("probe").inset(0).zIndex(6))
        .child(slot("hud").inset(16, 12, 16, h - 44).zIndex(6));
  }

  Element probeDot() const {
    const SkPoint p = probe;
    return custom([p](SkCanvas &c, const PaintContext &) {
             SkPaint paint;
             paint.setAntiAlias(true);
             paint.setColor(0xffffffff);
             c.drawCircle(p.x(), p.y(), 5, paint);
             paint.setStyle(SkPaint::kStroke_Style);
             paint.setStrokeWidth(1.5f);
             c.drawCircle(p.x(), p.y(), 10, paint);
           }).inset(0).cache(Cache::None);
  }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
    spin = 0.0f;
    pulse = 1.0f;
    hitLabel = "—";
    probe = {kSceneSize.width() / 2, kSceneSize.height() / 2};
    composer.render(describe());
    composer.renderSlot("probe", probeDot());
    composer.renderSlot("hud", box().child(text(toU8("hit: " + hitLabel),
                                                styleAt(16, 0xffffe0b0))));
  }

  void update(double elapsed, Composer &composer) override {
    spin = (float)std::fmod(elapsed * 9.0, 360.0);
    pulse = 1.0f + 0.05f * (float)std::sin(elapsed * 2.2);

    // Lissajous probe; hitTest names what it wanders over. Only the
    // two slots re-render — the scene itself never re-describes.
    const float w = kSceneSize.width(), h = kSceneSize.height();
    SkPoint next{w * 0.5f + w * 0.42f * (float)std::sin(elapsed * 0.9),
                 h * 0.5f + h * 0.4f * (float)std::sin(elapsed * 0.53 + 1)};
    std::string label = composer.hitTest(next).value_or("—");
    probe = next;
    composer.renderSlot("probe", probeDot());
    if (label != hitLabel) {
      hitLabel = std::move(label);
      composer.renderSlot("hud",
                          box().child(text(toU8("hit: " + hitLabel),
                                           styleAt(16, 0xffffe0b0))));
    }
  }
};

} // namespace compose_gallery
