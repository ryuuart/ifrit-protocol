// MAGI-01's Danang Type-B protection display. Three radial registers stand
// inside concentric defense rings; four fixed panels read the moving field.
// TAGS: Interfaces/Film, Typography/Paths, Drawing/Primitives

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/kit/Crt.h>
#include <sigilmaterial/skia/Bloom.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <array>
#include <cmath>
#include <string>

#include "EvangelionUi.h"

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace motion = sigil::motion;
namespace weave = sigil::weave;
using namespace sigil::compose;
using sigil::draw::Pen;
using sigil::material::skia::Paint;

namespace {
constexpr float kWidth = 1428, kHeight = 800;
constexpr SkPoint kCentre{712, 400};
const material::Color kGround = hexColor(0x020704);
const material::Color kOrange = hexColor(0xFF951F);
const material::Color kGreen = hexColor(0x77E9A3);
const material::Color kDarkGreen = hexColor(0x407C42);

struct EvaMagiInterior {
  weave::FontContext* fonts = nullptr;
  choreograph::Output<float> turn{0};
  int second = -1;

  Text label(Utf8 words, SkPoint origin, float capHeight, float measure,
             material::Color ink = kOrange) const {
    const auto face = evangelion::condensedBold();
    const auto probe =
        metrics(weave::textStyle({.face = face, .size = 100}), *fonts);
    weave::Type type{.face = face, .size = capHeight * 100 / probe.capHeight};
    const float width = intrinsicSize(text(words).font(type), *fonts).width();
    if (measure > 0 && width > 0) type.condense = measure / width;
    const float slack = metrics(weave::textStyle(type), *fonts).capSlack();
    return text(words).font(type).ink(ink).at({origin.fX, origin.fY - slack});
  }

  Element rings() const {
    return pen(
               "protection.rings",
               [](Pen& p) {
                 p.noFill();
                 p.strokeWeight(4);
                 p.stroke(Paint::linear({0, 40}, {0, 760},
                                        {{0, kDarkGreen},
                                         {0.55f, hexColor(0x8D9954)},
                                         {1, hexColor(0xAF402D)}}));
                 for (int i = 0; i < 12; ++i) p.circle(kCentre, 88 + i * 56.0f);
               },
               Cache::Texture)
        .opacity(0.5f);
  }

  Element codeField() {
    Element group = box().inset(0);
    std::string code;
    for (int i = 0; i < 45; ++i) code += i % 3 == 0 ? "110 " : "001 ";
    for (int band = 0; band < 13; ++band) {
      const float r = 383 + band * 29.0f;
      group.children({text(code)
                          .font({.face = evangelion::condensedRegular(),
                                 .size = 34,
                                 .color = kGreen,
                                 .track = 1.3f,
                                 .condense = 0.70f})
                          .width(r * 2)
                          .height(r * 2)
                          .centerAt(kCentre)
                          .textOnPath({.path = shapes::circle(),
                                       .at = motion::bind(&turn).target(
                                           band * 0.037f,
                                           band * 0.037f + (band % 2 ? -1 : 1)),
                                       .align = TextPath::Align::Start,
                                       .autoFlip = false})});
    }
    group.children({pen(
        "protection.channels",
        [](Pen& p) {
          p.noStroke();
          p.fill(kGround);
          p.rect(0, 361, 357, 74);
          p.rect(1067, 361, kWidth - 1067, 74);
          p.noFill();
          p.stroke(hexColor(0x9AA654));
          p.strokeWeight(5);
          for (float y : {370.0f, 391.0f, 413.0f, 431.0f}) {
            p.line(0, y, 324, y);
            p.line(1100, y, kWidth, y);
            p.bezier(324, y, 353, y, 353, y - 15, 344, y - 15);
            p.bezier(1100, y, 1069, y, 1069, y - 15, 1078, y - 15);
          }
        },
        Cache::Texture)});
    return group;
  }

  Element registerArm(float angle, const char* name) const {
    const std::array<const char*, 4> names{"CELEBRUM", "CELEBELLUM", "CALLOSUM",
                                           "OBLONGATE"};
    Element arm = box()
                      .inset(0)
                      .transformOrigin(pct(100 * kCentre.fX / kWidth), pct(50))
                      .rotate(angle);
    arm.children({pen(
        "protection.register",
        [](Pen& p) {
          p.fill(kGround);
          p.stroke(kOrange);
          p.strokeWeight(3);
          p.quad(786, 343, 1015, 343, 941, 470, 712, 470);
          p.line(811, 343, 737, 470);
          for (int i = 1; i < 4; ++i) {
            const float y = 343 + i * 31.75f, shift = i * 18.5f;
            p.line(811 - shift, y, 1015 - shift, y);
          }
        },
        Cache::Texture)});
    for (int i = 0; i < 4; ++i)
      arm.children(
          {label(names[i], {824 - i * 18.5f, 349 + i * 31.75f}, 20, 166)});
    arm.children({label(name, {0, 0}, 16, 120)
                      .width(120)
                      .height(21)
                      .centerAt({764, 404})
                      .rotate(-60)});
    return arm;
  }

  Element core() const {
    return box().inset(0).children(
        {registerArm(0, "MELCHIOR-2"), registerArm(-120, "MELCHIOR-1"),
         registerArm(120, "MELCHIOR-3"),
         pen(
             "protection.hub",
             [](Pen& p) {
               p.fill(kGround);
               p.stroke(kOrange);
               p.strokeWeight(3);
               p.triangle(637, 343, 786, 343, 712, 470);
             },
             Cache::Texture),
         label("MAGI", {666, 358}, 32, 93), label("01", {683, 395}, 33, 57)});
  }

  Element panel(float x, float y, float width, Utf8 heading, Utf8 reading,
                bool timer) const {
    Element plate = box()
                        .rect(SkRect::MakeXYWH(x, y, width, 112))
                        .fill(kGround)
                        .borderRadius({8})
                        .stroke(stroke(4, Paint::solid(kOrange)));
    if (timer) {
      plate.children({box().rect({8, 39, width - 8, 42}).fill(kOrange),
                      label(heading, {10, 9}, 24, width - 20),
                      label(reading, {15, 52}, 47, width - 100),
                      label("sec.", {width - 77, 77}, 23, 63)});
    } else {
      plate.children({label(heading, {9, 12}, 50, width - 18),
                      box().rect({6, 72, width - 6, 75}).fill(kOrange),
                      label(reading, {width * 0.33f, 82}, 21, width * 0.62f)});
    }
    return box().inset(0).children(
        {std::move(plate),
         box()
             .rect(SkRect::MakeXYWH(x - 31, y - 2, 17, 116))
             .fill(kOrange)
             .borderRadius({9}),
         box()
             .rect(SkRect::MakeXYWH(x + width + 15, y - 2, 17, 116))
             .fill(kOrange)
             .borderRadius({9})});
  }

  Element timers(int elapsed) const {
    const int remaining = 223238 - elapsed;
    return box().inset(0).children(
        {panel(1014, 12, 365, "TIME REMAINING TO COLLAPSE",
               kit::formatted("%03d,%03d", remaining / 1000, remaining % 1000),
               true),
         panel(1014, 667, 365, "TIME SINCE SCREEN RAISED",
               kit::formatted("%03d,%03d", elapsed / 1000, elapsed % 1000),
               true)});
  }

  void setup(sketch::SketchContext& ctx) {
    fonts = ctx.fonts;
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = 9,
                             .background = kGround,
                             .nonlinearPicture = true});
    auto tube = material::kit::crt(SkRect::MakeWH(kWidth, kHeight));
    tube.set("uBloom", 0.22f);
    const auto phosphor = material::skia::bloom({.sigma = 1.6f,
                                                .strength = 0.24f,
                                                .spread = 2.2f,
                                                .tail = 0.12f,
                                                .threshold = 0.12f,
                                                .knee = 0.18f,
                                                .softness = 0.3f,
                                                .whitening = 0.06f,
                                                .dilation = 0.35f,
                                                .deepening = 1.5f});
    ctx.composer.render(box().inset(0).fill(kGround).children(
        {box()
             .inset(0)
             .overflow(Overflow::Clip)
             .children({codeField(), rings(), core(),
                        panel(59, 12, 365, "DANANG TYPE-B DEFENSE SCREEN",
                              "on MAGI-01 ORIGINAL", false),
                        panel(59, 667, 365, "PROTECT NO.666",
                              "on MAGI-01 ORIGINAL", false),
                        slot("timers")})
             .filter(phosphor.then(
                 material::skia::Effect::recipe(tube, 82.0f)))}));
    second = 0;
    ctx.composer.renderSlot("timers", timers(second));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    turn = static_cast<float>(std::fmod(elapsed * 0.006, 1.0));
    const int now = static_cast<int>(elapsed) % 223238;
    if (now == second) return;
    second = now;
    ctx.composer.renderSlot("timers", timers(second));
  }
};
}  // namespace

SIGIL_SKETCH(
    EvaMagiInterior, "Study · Film",
    "The End of Evangelion — MAGI-01 Danang Type-B defense, Protect No.666")
