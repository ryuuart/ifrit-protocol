// Lain's NAVI, late at night: a Copland OS desktop seen through the
// curved glass of a tube in a dark room. Translucent windows stand over
// one another — a terminal scrolling a MIPS listing while a command is
// typed, a map of the Wired, a mail that should not have come — and the
// cursor goes between them at the pace of someone thinking. A dialog
// opens onto the Wired and the Wired comes through: PRESENT DAY, PRESENT
// TIME in the layers, a face, voices in Japanese, then a notice, and the
// desktop is as it was.
//
// The listing's first sixteen lines are verbatim off the Layer 04 frame;
// the rest continue the same gcc -S output so the scroll has material.
// The prose under the wallpaper is transcribed where the frame is
// legible and filled in the same register where the tube ate it.
//
// The desktop that stands still is ONE texture, lit by the tube once:
// bloom on what is bright, then the beam's convergence and grain. What
// moves — the scroll, the typing, the cursor, the windows opening, the
// Wired's layers, the rolling band — stands over it, each baked where it
// holds still and moved by a bound value. The shadow mask and the glass
// are painted once and laid over everything.

// TAGS: Interfaces/Film

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilweave/layout/ParagraphStyle.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#include "CrtBeam.h"
#include "Tube.h"

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;
using material::Color;
using material::Filter;
using material::hexColor;
using Json = sigil::data::Json;

namespace {

// The source frames are 1016 x 720.
constexpr float kWidth = 1016, kHeight = 720;
// The tube's face inside the bezel, and the corners it turns on.
constexpr glm::vec4 kScreen{40, 34, 936, 648};
constexpr float kCorner = 38;

// One pass of the evening, after which the desktop is as it began.
constexpr float kLoop = 26;
// The listing steps a line at a time on this pitch and this beat.
constexpr float kPitch = 19, kLineEvery = 0.42f;
// The dialog onto the Wired, and the notice that follows it.
constexpr float kDialogOpens = 6.4f, kDialogCloses = 12.6f;
constexpr float kNoticeOpens = 19.0f, kNoticeCloses = 23.6f;
constexpr float kFaceFrom = 13.6f, kFaceUntil = 20.2f;

/** The Copland registers. The windows are cool glass over a navy ground,
 *  their chrome a pale steel; every word is the thin condensed sans
 *  except the listing, the lockup and what the Wired says in Japanese. */
StyleSheet copland() {
  return StyleSheet{
      rule(":root")
          .var("ground", hexColor(0x050818))
          .var("ink", hexColor(0xd2e2ea))
          .var("dim", hexColor(0x8ea6b8))
          .var("phosphor", hexColor(0x5fe0b0))
          .var("wired", hexColor(0x9cc4ff))
          .var("pane", hexColor(0x14223a, 0.62f))
          .var("edge", hexColor(0xa8c8dc, 0.55f))
          .var("chrome", hexColor(0xc4d4e0, 0.9f))
          .var("chromeInk", hexColor(0x0c1626))
          .fontFamily("Avenir Next Condensed, Hiragino Sans, sans-serif")
          .fontWeight(300)
          .fontSize(14)
          .letterSpacing(0.6)
          .ink(var("ink")),
      rule(".window")
          .fill(Fill::var("pane"))
          .stroke(stroke(1, Fill::var("edge"))),
      rule(".chrome")
          .height(20)
          .paddingLeft(8)
          .paddingRight(6)
          .fill(Fill::var("chrome"))
          .fontSize(12)
          .fontWeight(600)
          .letterSpacing(1.8)
          .ink(var("chromeInk")),
      rule(".button").width(10).height(10).marginLeft(4).stroke(
          stroke(1, Fill::var("chromeInk"))),
      rule(".listing, .prompt")
          .fontFamily("JetBrains Mono, Andale Mono, Menlo, monospace")
          .fontWeight(300)
          .fontSize(13)
          .letterSpacing(0)
          .ink(var("phosphor")),
      rule(".host").ink(var("dim")),
      rule(".menu").fontSize(13).fontWeight(400).letterSpacing(1.2),
      rule(".label").fontSize(11).letterSpacing(1.4).ink(var("dim")),
      rule(".japanese")
          .fontFamily("Hiragino Sans, Hiragino Kaku Gothic ProN, sans-serif"),
      rule(".prose")
          .fontFamily("Hiragino Mincho ProN, YuMincho, serif")
          .fontSize(30)
          .letterSpacing(2)
          .ink(hexColor(0x1a2442)),
      rule(".lockup")
          .fontFamily("Times New Roman, Times, serif")
          .fontStyle(FontStyle::Italic)
          .fontWeight(700)
          .ink(hexColor(0x2c3d5c)),
      rule(".announce")
          .fontSize(84)
          .fontWeight(200)
          .letterSpacing(22)
          .ink(var("wired")),
      rule(".whisper")
          .fontFamily("Hiragino Sans, Hiragino Kaku Gothic ProN, sans-serif")
          .fontWeight(300)
          .letterSpacing(4)
          .ink(var("wired")),
      rule(".lyric")
          .fontFamily("Times New Roman, Times, serif")
          .fontSize(44)
          .fontWeight(400)
          .letterSpacing(1)
          .ink(hexColor(0xb6c4d0)),
  };
}

/** A straight rule from @p from to @p to. */
Element segment(glm::vec2 from, glm::vec2 to, float thickness, Fill fill) {
  return pathFigure(path::toPath(path::Polyline{.points = {from, to}}),
                    thickness)
      .stroke(stroke(thickness, fill));
}

/** A Copland window: the steel strip with its title and two buttons, and
 *  under it a translucent pane that holds @p body. */
Element window(std::string_view title, float x, float y, float width,
               float height, std::initializer_list<Children> body) {
  return box()
      .rect(x, y, width, height)
      .styleClass("window")
      .column()
      .children({
          box()
              .styleClass("chrome")
              .row()
              .alignItems(Align::Center)
              .children({
                  text(std::string(title)).flexGrow(),
                  box().styleClass("button"),
                  box().styleClass("button"),
              }),
          box().flexGrow().padding(10).column().gap(3).children(body),
      });
}

/** The cursor, pointing up and to the left from its hot spot. */
path::Outline arrow() {
  return path::toPath(path::Polyline{.points = {{0, 0},
                                                {0, 17},
                                                {4.5f, 13.2f},
                                                {7.6f, 20},
                                                {10.2f, 18.9f},
                                                {7.2f, 12.3f},
                                                {12.4f, 12.3f}},
                                     .closed = true});
}

/** Eased from 0 to 1 as @p value crosses [from, to]. */
float between(float value, float from, float to) {
  const float share = std::clamp((value - from) / (to - from), 0.0f, 1.0f);
  return share * share * (3 - 2 * share);
}

}  // namespace

struct LainNavi {
  sketch::kit::Document content;
  /** THE ONE CLOCK: seconds into the evening's pass. */
  motion::Animatable<float> clock = motion::animatable(0.0f);
  /** Written each frame: the listing's scroll and where the cursor is. */
  motion::Animatable<float> scroll = motion::animatable(0.0f);
  motion::Animatable<float> cursorX = motion::animatable(0.0f);
  motion::Animatable<float> cursorY = motion::animatable(0.0f);
  size_t typed = static_cast<size_t>(-1);

  /** Up over @p rise from @p from, held, and down over @p fall from
   *  @p until, on the evening's clock. */
  motion::Animatable<float> shown(float from, float until, float rise,
                                  float fall, motion::Range to = {0, 1}) const {
    return motion::bind(clock, {.from = {0, kLoop},
                                .envelope = motion::envelope::trapezoid(
                                    from / kLoop, (from + rise) / kLoop,
                                    until / kLoop, (until + fall) / kLoop),
                                .ease = motion::ease::inOutSine,
                                .to = to});
  }

  // ---- The desktop: everything that stands still, lit once -------------

  /** The wallpaper: a navy ground ruled faintly, the prose the Wired left
   *  across it, the Copland eye and the lockup. */
  Element wallpaper() const {
    return box().inset(0).children({
        box().inset(0).fill(
            material::radialGradient({0.56f, 0.44f}, 0.9f,
                                     {{0, hexColor(0x0d1638)},
                                      {0.6f, hexColor(0x070b22)},
                                      {1, hexColor(0x03040e)}})),
        each(24,
             [](size_t column) {
               return box()
                   .rect(kScreen.x + column * 40.0f, 0, 1, kHeight)
                   .fill(hexColor(0x1a2a52, 0.35f));
             }),
        each(17,
             [](size_t row) {
               return box()
                   .rect(0, kScreen.y + row * 40.0f, kWidth, 1)
                   .fill(hexColor(0x1a2a52, 0.35f));
             }),
        box()
            .inset(0)
            .filter(Filter::blur(0.8f))
            .children({each(content["prose"].array(),
                            [](const Json& line, size_t index) {
                              return text(std::string(line.string()))
                                  .styleClass("prose")
                                  .left(20 + 26 * std::sin(index * 1.7f))
                                  .top(54 + 44.0f * index);
                            })}),
        coplandEye({740, 470}, 104),
        box()
            .rect(590, 590, 340, 70)
            .column()
            .alignItems(Align::End)
            .styleClass("lockup")
            .children({
                text("Copland OS Enterprise").fontSize(30),
                text("Produced By Tachibana Lab")
                    .fontSize(13)
                    .letterSpacing(0.8),
            }),
    });
  }

  /** The Copland eye: two lids, four satellites and a stem, blurred until
   *  it is a pedestal of light more than a mark. */
  static Element coplandEye(glm::vec2 eye, float radius) {
    const Fill line = Fill::color(hexColor(0x2a3f6e));
    PathFormat lid = stroke(14, line);
    return box()
        .inset(0)
        .filter(Filter::blur(5))
        .children({
            // The pedestal of light the eye stands in.
            box()
                .rect(eye.x - radius * 2, eye.y - radius * 2, radius * 4,
                      radius * 4)
                .fill(material::radialGradient({0.5f, 0.5f}, 0.5f,
                                               {{0, hexColor(0x1c2c5a, 0.8f)},
                                                {1, hexColor(0x1c2c5a, 0)}})),
            // The lids: each half an ellipse, opening toward the other.
            box()
                .rect(eye.x + radius * 0.12f, eye.y - radius * 0.78f, radius,
                      radius * 1.56f)
                .shape(shapes::arc(270, 180))
                .fill(Fill::none())
                .stroke(lid),
            box()
                .rect(eye.x - radius * 1.12f, eye.y - radius * 0.78f, radius,
                      radius * 1.56f)
                .shape(shapes::arc(90, 180))
                .fill(Fill::none())
                .stroke(lid),
            // Four satellites at the corners of the eye's square.
            each(4,
                 [&](size_t corner) {
                   const glm::vec2 side{corner % 2 ? 1.0f : -1.0f,
                                        corner / 2 ? 1.0f : -1.0f};
                   const glm::vec2 centre = eye + side * radius * 0.92f;
                   const float size = radius * 0.3f;
                   return box()
                       .rect(centre.x - size / 2, centre.y - size / 2, size,
                             size)
                       .shape(shapes::ellipse())
                       .fill(line);
                 }),
            segment({eye.x, eye.y + radius * 0.95f},
                    {eye.x, eye.y + radius * 1.5f}, 14, line),
        });
  }

  /** The menu bar across the top of the face. */
  Element menuBar() const {
    return box()
        .rect(kScreen.x, kScreen.y + 4, kScreen.z, 22)
        .row()
        .alignItems(Align::Center)
        .paddingLeft(48)
        .paddingRight(48)
        .gap(22)
        .fill(hexColor(0x9fb3c4, 0.16f))
        .styleClass("menu")
        .children({
            each(content["menu"].array(),
                 [](const Json& item) {
                   return text(std::string(item.string()));
                 }),
            box().flexGrow(),
            text("Layer:07").styleClass("label"),
            text("23:42"),
        });
  }

  /** The icons down the left edge: a hollow tile and its name. */
  Element icons() const {
    return box()
        .rect(58, 84, 56, 300)
        .column()
        .gap(22)
        .alignItems(Align::Center)
        .children({each(content["icons"].array(), [](const Json& name) {
          return box()
              .column()
              .gap(5)
              .alignItems(Align::Center)
              .children({
                  box()
                      .width(30)
                      .height(30)
                      .fill(hexColor(0x9fb3c4, 0.12f))
                      .stroke(stroke(1, Fill::var("edge"))),
                  text(std::string(name.string())).styleClass("label"),
              });
        })});
  }

  /** The Wired as the NAVI draws it: named nodes and the links between. */
  Element wiredMap() const {
    const Json& map = content["wired"];
    const auto nodes = map["nodes"].array();
    auto at = [&](size_t index) {
      return glm::vec2{nodes[index]["x"].number(), nodes[index]["y"].number()};
    };
    return window("Wired / Protocol 7 — node map", 530, 80, 410, 250,
                  {box().inset(0).children({
                      each(map["links"].array(),
                           [&](const Json& link) {
                             return segment(
                                 at(static_cast<size_t>(link[0].number())),
                                 at(static_cast<size_t>(link[1].number())), 1,
                                 Fill::color(hexColor(0x7fa8d8, 0.5f)));
                           }),
                      each(nodes,
                           [&](const Json& node, size_t index) {
                             const glm::vec2 point = at(index);
                             return box()
                                 .left(point.x - 4)
                                 .top(point.y - 4)
                                 .row()
                                 .gap(6)
                                 .alignItems(Align::Center)
                                 .children({
                                     box()
                                         .width(8)
                                         .height(8)
                                         .shape(shapes::ellipse())
                                         .fill(Fill::var("wired")),
                                     text(std::string(node["name"].string()))
                                         .styleClass("label"),
                                 });
                           }),
                  })});
  }

  /** The mail that should not have come. */
  Element mail() const {
    const Json& letter = content["mail"];
    return window(
        "mail — 1 of 1", 470, 356, 440, 226,
        {
            each(letter["header"].array(),
                 [](const Json& line) {
                   return text(std::string(line.string())).styleClass("label");
                 }),
            box().height(10),
            each(letter["body"].array(),
                 [](const Json& line) {
                   return text(std::string(line.string()))
                       .styleClass("japanese")
                       .fontSize(17);
                 }),
        });
  }

  /** The terminal's frame; its listing and its prompt move over it. */
  Element terminal() const {
    return window("NAVI :: Layer 04 — gcc -S _3D.c", 128, 84, 420, 424, {});
  }

  Element desktop() const {
    return box()
        .inset(0)
        .key("desktop")
        .cache(Cache::Texture)
        .filter(tube())
        .children(
            {wallpaper(), menuBar(), icons(), terminal(), wiredMap(), mail()});
  }

  /** The tube over the still desktop: what is lit blooms, then the three
   *  guns converge a little apart and the signal carries its grain. The
   *  raster is the shadow mask's, laid over everything that moves too. */
  static Filter tube() {
    return Filter::phosphorBloom(8, 0.42f, 0.42f, 0.5f)
        .then(Filter::of(lain_navi::crtBeam({.uBounds = {0, 0, kWidth, kHeight},
                                             .uRgbShift = 0.9f,
                                             .uNoise = 0.03f}),
                         0.9f));
  }

  // ---- What moves ------------------------------------------------------

  /** The listing, a line at a time, behind the terminal's pane. It is
   *  set twice over so the scroll wraps without a seam. */
  Element listing() const {
    std::string passage;
    for (int pass = 0; pass < 2; ++pass)
      for (const Json& line : content["listing"].array())
        passage += std::string(line.string()) + "\n";
    return box()
        .rect(140, 116, 400, 350)
        .overflow(Overflow::Clip)
        .children({
            box()
                .key("listing")
                .cache(Cache::Texture)
                .translateY(scroll)
                .children(
                    {text(passage)
                         .styleClass("listing")
                         .width(720)
                         .lineHeight(weave::Leading::absolute(kPitch))
                         .filter(Filter::glow(hexColor(0x5fe0b0, 0.5f), 3))}),
        });
  }

  /** The command line, as far as it has been typed, and the caret. */
  Element prompt(size_t count) const {
    const std::string_view words = content["command"]["words"].string();
    return box()
        .row()
        .alignItems(Align::Center)
        .children({
            text("lain@navi ~ %").styleClass("prompt host").marginRight(8),
            text(std::string(words.substr(0, count))).styleClass("prompt"),
            box()
                .width(7)
                .height(14)
                .fill(Fill::var("phosphor"))
                .opacity(motion::bind(
                    clock, {.from = {0, 1.06f},
                            .envelope = motion::envelope::square(0.5f)})),
        });
  }

  /** What the Wired says through the layers, each held as one texture
   *  and faded on the clock. */
  Element voices() const {
    return box().inset(0).children(
        {each(content["voices"].array(), [&](const Json& voice) {
          const float at = voice["at"].number(),
                      until = voice["until"].number();
          return box()
              .key(std::string(voice["words"].string()))
              .cache(Cache::Texture)
              .opacity(shown(at, until, 1.6f, 1.8f, {0, 0.8f}))
              .centerAt({voice["x"].number(), voice["y"].number()})
              .children(
                  {text(std::string(voice["words"].string()))
                       .styleClass(std::string(voice["role"].string()))
                       .fontSize(voice["size"].number())
                       .filter(Filter::glow(hexColor(0x6f9cff, 0.55f), 9))});
        })});
  }

  /** A face in the Wired, too large and too faint, its signal
   *  unsteady. */
  Element face() const {
    const Fill socket = Fill::color(hexColor(0x02040c, 0.7f));
    const Fill iris = Fill::color(hexColor(0xb8dcff));
    return box()
        .rect(170, 120, 340, 440)
        .opacity(shown(kFaceFrom, kFaceUntil, 2.2f, 2.4f, {0, 0.9f}))
        .children(
            {box()
                 .inset(0)
                 // The signal wavers, about twice a second.
                 .opacity(
                     motion::bind(clock, {.from = {0, kLoop},
                                          .to = {0.84f, 0.84f},
                                          .wiggle = {.amount = 0.16f,
                                                     .frequency = kLoop * 2,
                                                     .seed = 7,
                                                     .octaves = 2}}))
                 .children({box()
                                .inset(0)
                                .key("face")
                                .cache(Cache::Texture)
                                .filter(Filter::blur(4))
                                .children({
                                    box()
                                        .rect(30, 10, 280, 400)
                                        .shape(shapes::ellipse())
                                        .fill(material::radialGradient(
                                            {0.5f, 0.42f}, 0.55f,
                                            {{0, hexColor(0x8fb0f0, 0.55f)},
                                             {0.7f, hexColor(0x6d8fd8, 0.22f)},
                                             {1, hexColor(0x6d8fd8, 0)}})),
                                    box()
                                        .rect(80, 172, 84, 30)
                                        .shape(shapes::ellipse())
                                        .fill(socket),
                                    box()
                                        .rect(176, 172, 84, 30)
                                        .shape(shapes::ellipse())
                                        .fill(socket),
                                    kit::dot({122, 187}, 8, iris),
                                    kit::dot({218, 187}, 8, iris),
                                    box()
                                        .rect(136, 318, 68, 4)
                                        .fill(hexColor(0x02040c, 0.6f)),
                                })})});
  }

  /** The dialog onto the Wired: it zooms open the way a Copland window
   *  does, fills its bar while the connection is made, and closes. */
  Element dialog() const {
    return box()
        .rect(300, 250, 420, 150)
        .opacity(shown(kDialogOpens, kDialogCloses, 0.45f, 0.4f))
        .scale(shown(kDialogOpens, kDialogCloses, 0.45f, 0.4f, {0.88f, 1}))
        .children({
            box()
                .inset(0)
                .key("dialog")
                .cache(Cache::Texture)
                .filter(Filter::glow(hexColor(0x8fb8e0, 0.3f), 10))
                .children({
                    window(
                        "Wired — Protocol 7", 0, 0, 420, 150,
                        {
                            text("接続しています")
                                .styleClass("japanese")
                                .fontSize(18),
                            text("CONNECTING TO THE WIRED").styleClass("label"),
                        }),
                    box()
                        .rect(10, 118, 400, 8)
                        .stroke(stroke(1, Fill::var("edge"))),
                }),
            box()
                .rect(10, 118, 400, 8)
                .fill(Fill::var("wired"))
                .transformOrigin(pct(0), pct(50))
                .scaleX(motion::bind(
                    clock, {.from = {kDialogOpens + 0.6f, kDialogCloses - 0.9f},
                            .clampFrom = true,
                            .ease = motion::ease::inOutSine})),
        });
  }

  /** The notice that a mail has come. */
  Element notice() const {
    return box()
        .rect(726, 60, 236, 70)
        .key("notice")
        .cache(Cache::Texture)
        .opacity(shown(kNoticeOpens, kNoticeCloses, 0.4f, 0.4f))
        .scale(shown(kNoticeOpens, kNoticeCloses, 0.4f, 0.4f, {0.9f, 1}))
        .filter(Filter::glow(hexColor(0x8fb8e0, 0.3f), 8))
        .children({window("NAVI", 0, 0, 236, 70,
                          {
                              text("新着メール 1通 — 1 new message")
                                  .styleClass("japanese")
                                  .fontSize(13),
                          })});
  }

  Element cursor() const {
    return box()
        .rect(0, 0, 14, 22)
        .key("cursor")
        .cache(Cache::Texture)
        .translateX(cursorX)
        .translateY(cursorY)
        .children({pathFigure(arrow(), 1)
                       .fill(hexColor(0xf2f7ff))
                       .stroke(stroke(1, Fill::color(hexColor(0x050818))))
                       .filter(Filter::glow(hexColor(0xbcd8ff, 0.6f), 4))});
  }

  /** The hum bar: a band of light rolling down the face four times an
   *  evening. */
  Element scanBand() const {
    return box()
        .rect(kScreen.x, -160, kScreen.z, 160)
        .key("band")
        .cache(Cache::Texture)
        .fill(material::linearGradient({0, 0}, {0, 1},
                                       {{0, hexColor(0xbfd8ff, 0)},
                                        {0.7f, hexColor(0xbfd8ff, 0.05f)},
                                        {1, hexColor(0xbfd8ff, 0)}}))
        .translateY(motion::bind(clock, {.from = {0, kLoop / 4},
                                         .to = {0, kHeight + 160},
                                         .wrap = kHeight + 160}));
  }

  // ---- The glass -------------------------------------------------------

  Element glass() const {
    const lain_navi::TubeParameters face{.uScreen = kScreen,
                                         .uCorner = kCorner};
    return box().inset(0).children({
        box()
            .inset(0)
            .key("mask")
            .cache(Cache::Texture)
            .fill(lain_navi::shadowMask(face))
            .blendMode(material::BlendMode::Multiply),
        box()
            .inset(0)
            .key("glass")
            .cache(Cache::Texture)
            .fill(lain_navi::glass(face)),
        // The power lamp and the maker's name in the bezel below the face.
        box()
            .rect(0, kScreen.y + kScreen.w, kWidth,
                  kHeight - kScreen.y - kScreen.w)
            .key("bezel")
            .cache(Cache::Texture)
            .row()
            .alignItems(Align::Center)
            .justifyContent(Justify::Center)
            .children({
                text("NAVI").styleClass("label").letterSpacing(6).ink(
                    hexColor(0x3a404c)),
                box()
                    .rect(930, 16, 6, 6)
                    .shape(shapes::ellipse())
                    .fill(hexColor(0x6dffa8))
                    .filter(Filter::glow(hexColor(0x3dff90, 0.8f), 4)),
            }),
    });
  }

  Element describe() const {
    return box().inset(0).applyStyleSheet(copland()).children({
        desktop(),
        listing(),
        box().rect(140, 474, 400, 24).children({slot("prompt")}),
        voices(),
        face(),
        dialog(),
        notice(),
        cursor(),
        scanBand(),
        glass(),
    });
  }

  /** Where the cursor is at @p seconds: eased between the stops. */
  glm::vec2 cursorAt(float seconds) const {
    const auto stops = content["cursor"].array();
    glm::vec2 where{stops.front()["x"].number(), stops.front()["y"].number()};
    for (size_t index = 1; index < stops.size(); ++index) {
      const glm::vec2 next{stops[index]["x"].number(),
                           stops[index]["y"].number()};
      where +=
          (next - where) * between(seconds, stops[index - 1]["at"].number(),
                                   stops[index]["at"].number());
    }
    return where;
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight);
    context.background(hexColor(0x050818));
    // The dialog is filling its bar and PRESENT DAY, PRESENT TIME stand
    // at full in the layers.
    context.captureAt(11.0);
    content = sketch::kit::Document(context, "data/content.json");
    context.composer.render(describe());
    update(0, context);
  }

  void update(double elapsed, sketch::SketchContext& context) {
    const float seconds =
        static_cast<float>(std::fmod(elapsed, static_cast<double>(kLoop)));
    clock = seconds;
    const long long line = static_cast<long long>(elapsed / kLineEvery);
    scroll = -kPitch *
             static_cast<float>(line % static_cast<long long>(
                                           content["listing"].array().size()));
    const glm::vec2 point = cursorAt(seconds);
    cursorX = point.x;
    cursorY = point.y;

    const Json& command = content["command"];
    const float since = seconds - static_cast<float>(command["at"].number());
    const size_t count =
        std::min(command["words"].string().size(),
                 static_cast<size_t>(std::max(0.0f, since) *
                                     command["perSecond"].number()));
    if (count != typed) {
      typed = count;
      context.composer.renderSlot("prompt", prompt(count));
    }
  }
};

SIGIL_SKETCH(LainNavi, "Study · Film",
             "Lain's NAVI at night — a Copland desktop of translucent windows "
             "through a curved tube, and the Wired coming through it")
