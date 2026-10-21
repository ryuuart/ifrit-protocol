/** @file
 * tonhalle_posters — two Swiss concert posters hung side by side on one
 * wall: Müller-Brockmann's measured arc table revealed by span masks, and
 * a Weingart-style phototype collage cut into register strips over
 * halftone fields with lettering running round a disc.
 */

// THE LEFT PLATE is Josef Müller-Brockmann's "beethoven" poster (Tonhalle
// Zürich, 1955), reconstructed rather than traced. Every number in its
// arc table comes from pixel analysis of an archival scan: arc centre at
// (0.2693W, 0.7156H), ring widths doubling 1:2:4:8:16, and every arc end
// landing on an exact multiple of the poster's 11.25° angular module. Each
// arc run is a full circle OUTLINE whose path starts at that run's start
// angle, stroked at the ring's width and revealed with a span mask, so the
// mask's parameter maps directly onto the measured sweep. Ring durations
// double from the inside out and sweeps run linearly, so the reveal is a
// geometric progression rather than a decorative ease. The geometry is
// static and only the mask parameters animate: the reveal is one mount
// transition per arc with a shared delay.
//
// THE RIGHT PLATE is a fictional concert poster after Wolfgang Weingart's
// "Kunstkredit Basel 1976/77": an asymmetric black ground, interrupted
// text strips, colliding scales, overlapping dot screens and diagonal
// framing round a vector diaphragm. Five clipped strips carry one headline
// and shift its printing register; a shaped run travels round the
// diaphragm; a red dot screen slides in multiply over the sheet. The
// printed sheet and its grain are retained as textures, so the live pieces
// move at paint without relayout.
//
// Both are Weltformat-like portraits (the √2 rectangle and 5:7) hung at
// one height. Every fraction of the arc table is a fraction of its PLATE
// box and the collage is drawn in its own 1000 × 1400 sheet scaled onto
// its plate, so each poster's own geometry survives the hanging.
//
// EDIT THESE FIRST
//   kRuns                 — the measured arc table: inner and outer radius
//                           as a fraction of the poster width, and the two
//                           math-convention angles.
//   kRingBaseMilliseconds — the innermost ring's reveal, which every ring
//                           outward doubles.
//   kPlateHeight          — how large both reproductions hang on the wall.

// TAGS: Studies/Graphic Design, Typography/Posters, Typography/Lettering,
// Typography/Paths, Materials/Print, Motion/Bindings

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

// ---------------------------------------------------------------------------
// The wall

constexpr float kWallWidth = 1280, kWallHeight = 900;
/** Both plates hang this tall, their tops on one line. */
constexpr float kPlateHeight = 760, kPlateTop = 40, kPlateGap = 64;

constexpr material::Color kWall{0.235f, 0.230f, 0.222f, 1};
constexpr material::Color kLabel{0.760f, 0.745f, 0.715f, 1};

/** The shadow both plates throw on the wall. */
material::Material hung(material::Color ground) {
  return material::from(ground).effects(material::Filter::shadow(
      {0, 0, 0, 0.45f}, {.blur = 22, .offset = {0, 8}}));
}

// ---------------------------------------------------------------------------
// The arc table

namespace arcs {

constexpr float kPosterWidth = 720.0f;
constexpr float kPosterHeight = kPosterWidth * 1.4144f;
constexpr float kPlateWidth = kPlateHeight * (kPosterWidth / kPosterHeight);
/** Poster units to plate pixels. */
constexpr float kScale = kPlateWidth / kPosterWidth;

constexpr material::Color kPaper{0.961f, 0.953f, 0.933f, 1};
constexpr material::Color kInk{0.066f, 0.062f, 0.058f, 1};

/** The beat of blank paper the reveal starts from. */
constexpr std::chrono::milliseconds kRevealDelay{150};
/** The innermost ring's sweep; every ring outward doubles it. */
constexpr unsigned kRingBaseMilliseconds = 120;

/** One measured run. Angles are in MATH convention (0° = +x,
 *  counter-clockwise positive, y up); the canvas is y-down and
 *  clockwise-positive, so a canvas angle is the negated math angle. */
struct Run {
  float innerRadius, outerRadius;  // × poster width
  float startDegrees, endDegrees;  // math convention
};
inline constexpr auto kRuns = std::to_array<Run>({
    {0.3480f, 0.3607f, -33.75f, 22.50f},
    {0.3480f, 0.3607f, -146.25f, -135.00f},
    {0.3640f, 0.3913f, -22.50f, 45.00f},
    {0.3640f, 0.3913f, -157.50f, -90.00f},
    {0.3940f, 0.4473f, 0.00f, 78.75f},
    {0.3940f, 0.4473f, -135.00f, -45.00f},
    {0.4500f, 0.5560f, -150.00f, 90.00f},
    {0.5600f, 0.7770f, -150.00f, 45.00f},
    {0.7820f, 1.2070f, -150.00f, 101.25f},
});
/** The ring each run belongs to, for the doubling reveal durations. */
inline constexpr auto kRings = std::to_array<int>({0, 0, 1, 1, 2, 2, 3, 4, 5});

/** The registers: the poster's title over the sheet's own setting. */
StyleSheet look() {
  return StyleSheet{rule("h1").font({.size = 38 * kScale})};
}

/** One arc run: a disc centred on the poster's arc centre, sized to the
 *  run's mid-radius circle, whose outline STARTS at the run's canvas start
 *  angle, so masking it from 0 to sweep/360 reveals exactly the measured
 *  run. */
Element arcRun(const Run& run, int ring) {
  const SkPoint centre{0.2693f * kPlateWidth, 0.7156f * kPlateHeight};
  const float middleRadius =
      (run.innerRadius + run.outerRadius) * 0.5f * kPlateWidth;
  const float width = (run.outerRadius - run.innerRadius) * kPlateWidth;
  const float sweep = run.endDegrees - run.startDegrees;
  const float span = std::min(sweep / 360.0f, 0.9995f);

  Element arc = kit::disc(geometry::path::fromSk(centre), middleRadius)
                    .shape(shapes::arc(-run.endDegrees))
                    .stroke(PathFormat{.width = width,
                                       .strokeFill = Fill::color(kInk)});
  // A transition that starts from nothing needs a moment of nothing to
  // start from, and that moment is the mount delay.
  const auto duration = std::chrono::milliseconds(
      kRingBaseMilliseconds << (unsigned)std::min(ring, 5));
  arc.mask(by::spans(
      spans::upTo(motion::animate({.from = 0.0001f,
                                   .to = span,
                                   .duration = duration,
                                   .delay = kRevealDelay,
                                   .ease = motion::ease::linear}))));
  return arc;
}

/** The imprint: groups of a hanging label against its own block of
 *  entries. The label column is a fixed measure set flush RIGHT against
 *  the gutter and the entries set flush left from it, so every entry
 *  starts on one line whatever its label's length. */
Element imprint() {
  struct Group {
    const char* label;
    std::vector<const char*> lines;
  };
  const std::vector<Group> groups = {
      {"tonhalle",
       {"grosser saal", "dienstag, den 22. februar 1955,", "20.15 uhr",
        "4. extrakonzert", "der tonhalle-gesellschaft"}},
      {"leitung", {"carl schuricht"}},
      {"solist", {"wolfgang schneiderhan"}},
      {"beethoven",
       {"ouverture zu «coriolan», op. 62", "violinkonzert in d-dur, op. 61",
        "siebente sinfonie in a-dur, op. 92"}},
      {"vorverkauf",
       {"tonhalle-kasse, hug, jecklin,", "kuoni",
        "karten zu fr. 3.50 bis 9.50"}}};

  return box()
      .column()
      .gap(9 * kScale)
      .children(each(groups, [](const Group& group, size_t index) {
        return box()
            .key("group" + std::to_string(index))
            .row()
            .gap(7 * kScale)
            .children({
                box()
                    .width(56 * kScale)
                    .row()
                    .justifyContent(Justify::End)
                    .children({text(group.label)}),
                box().column().gap(1 * kScale).children(
                    each(group.lines,
                         [](const char* line) { return text(line); })),
            });
      }));
}

/** The poster, in plate coordinates: the runs, then the type block as the
 *  sheet sets it — "beethoven" alone at the left margin at about 62%
 *  depth, and the imprint below it as a two-column table. */
Element plate() {
  return stack()
      .applyStyleSheet(look())
      .fill(hung(kPaper))
      .overflow(Overflow::Clip)
      // Every line on the sheet sends its colour to the paint through the
      // 8-bit ladder the poster's palette was read in.
      .font({.size = 11.5f * kScale, .track = 0.2f * kScale, .color8 = true})
      .ink(kInk)
      .children({
          each(kRuns,
               [](const Run& run, size_t index) {
                 return arcRun(run, kRings[index])
                     .key("arc" + std::to_string(index));
               }),
          document::h1("beethoven")
              .key("title")
              .at({0.055f * kPlateWidth, 0.600f * kPlateHeight}),
          imprint().key("imprint").at(
              {0.175f * kPlateWidth, 0.690f * kPlateHeight}),
      });
}

}  // namespace arcs

// ---------------------------------------------------------------------------
// The collage

namespace collage {

constexpr float kSheetWidth = 1000, kSheetHeight = 1400;
constexpr float kPlateWidth = kPlateHeight * (kSheetWidth / kSheetHeight);

const auto kPaper = material::hexColor(0xEAE6D8);
const auto kBlack = material::hexColor(0x17201F);
const auto kRed = material::hexColor(0xD94B31);

Text type(std::string words, float size, material::Color colour = kPaper,
          bool heavy = true) {
  return text(std::move(words))
      .font({.face = weave::ports::face(
                 {"Helvetica Neue", "Helvetica", "Arial"}, heavy ? 700 : 400),
             .size = size,
             .track = heavy ? -size * 0.045f : 0.1f})
      .ink(colour);
}

Element strip(std::string words, float size, float left, float top,
              float width, material::Color ground = kPaper,
              material::Color ink = kBlack) {
  return kit::at(type(std::move(words), size, ink), left, top, width,
                 size * 1.17f)
      .fill(ground);
}

Element polygon(const char* path, float left, float top, float width,
                float height, const material::Material& fill) {
  return kit::at(left, top, width, height)
      .shape(shapes::svg(path))
      .fill(fill);
}

Element disc(float left, float top, float size) {
  return kit::at(left, top, size, size).shape(shapes::circle());
}

/** THE PRINTED SHEET: everything that does not move, retained as one
 *  texture. */
Element printedSheet() {
  auto dots =
      material::field::halftoneRamp(5.5f, 0.5f, 2.45f, kBlack, 45, 0.0f, 0.8f);
  auto fineDots = material::field::halftoneRamp(3.7f, 0.4f, 1.6f, kBlack, 15);
  std::vector<Element> fixed{
      kit::at(0, 0, kSheetWidth, kSheetHeight).fill(kBlack),
      polygon("M0 0H850L940 104V760H640L610 1000H60L0 910Z", 153, 170, 847,
              1050, kPaper),
      kit::at(type("tonhalle\nzürich", 37), 35, 24, 510, 97),
      kit::at(type("NEUE MUSIK\n18—20 OKTOBER\nKONZERTREIHE 04", 14, kPaper,
                   false),
              731, 33, 230, 66),
      kit::at(type("ZEIT WIRD FORM", 65), 309, 91, 664, 85),
      kit::at(type("FORM", 138, kBlack)
                  .textStroke(1.6f, Fill::color(kBlack))
                  .ink(material::Color{0, 0, 0, 0}),
              559, 206, 425, 172),
      kit::at(153, 246, 623, 641).fill(dots),
      kit::at(555, 164, 445, 90).fill(fineDots),
      polygon("M0 0H260L390 640H0Z", 618, 567, 365, 610,
              material::from(kPaper).layer(fineDots)),
      strip("DREI TAGE / EIN TAKT", 33, 209, 457, 668),
      strip("percussion. elektronik. kammerorchester.", 19, 239, 495, 690),
      strip("04", 252, 664, 936, 310, kRed, kPaper),
      strip("PULS", 73, 30, 1044, 231, kBlack, kPaper).zIndex(1),
      strip("IMPULS", 71, 30, 1118, 309, kBlack, kPaper).zIndex(1),
      kit::at(type("EINTRITT / 19.30 UHR", 17, kBlack).rotate(-90), 836, 785,
              294, 31),
  };

  // The concentric bands are the collage's percussion diaphragm; their
  // hard alternating screens stay registered to the printed sheet.
  fixed.push_back(disc(54, 518, 580).fill(kBlack));
  fixed.push_back(disc(59, 523, 570)
                      .fill(Fill::none())
                      .stroke(stroke(2, Fill::color(kPaper))));
  for (int band = 0; band < 32; ++band) {
    const float radius = 237 - band * 5.4f;
    fixed.push_back(
        disc(344 - radius, 808 - radius, radius * 2)
            .fill(Fill::none())
            .stroke(stroke(band % 4 == 0 ? 3.0f : 1.0f, Fill::color(kPaper))));
  }
  fixed.push_back(disc(282, 746, 124).fill(kRed));
  fixed.push_back(disc(319, 783, 50).fill(kBlack));
  fixed.push_back(disc(339, 803, 10).fill(kPaper));
  for (int spoke = 0; spoke < 12; ++spoke) {
    const float angle = spoke * 0.5235988f;
    fixed.push_back(
        disc(339.5f + 181 * std::cos(angle), 803.5f + 181 * std::sin(angle), 9)
            .fill(kPaper));
  }

  // A perspective score crosses the circular image like ruled acetate.
  for (int line = 0; line < 13; ++line)
    fixed.push_back(
        kit::at(639 - line * 7.3f, 614 + line * 23.5f, 305 + line * 7.3f, 2)
            .fill(kBlack));
  for (int line = 0; line < 8; ++line)
    fixed.push_back(
        kit::at(665 + line * 43.0f, 612, 1.5f, 313).fill(kBlack).skewX(-15));
  fixed.push_back(strip("RAUM", 64, 665, 652, 278, kBlack, kPaper));
  fixed.push_back(strip("KLANG", 64, 624, 741, 321, kPaper, kBlack));
  fixed.push_back(strip("KÖRPER", 64, 596, 828, 351, kBlack, kPaper));
  fixed.push_back(
      polygon("M0 0H110L440 420H330Z", -120, 903, 457, 465, kPaper));
  fixed.push_back(polygon("M0 0H70L450 410H370Z", -41, 1053, 345, 347, kRed));
  fixed.push_back(kit::at(346, 1190, 627, 181).fill(kPaper));
  const std::array<const char*, 3> titles{"01 / PULS", "02 / RAUM",
                                          "03 / ZEIT"};
  const std::array<const char*, 3> details{
      "18. OKTOBER\nPERCUSSION\nENSEMBLE\n19.30 UHR",
      "19. OKTOBER\nKAMMER-\nORCHESTER\n20.00 UHR",
      "20. OKTOBER\nELEKTRONISCHES\nSTUDIO\n18.00 UHR"};
  for (int column = 0; column < 3; ++column) {
    const float left = 363 + 201 * column;
    fixed.push_back(
        kit::at(type(titles[column], 21, kBlack), left, 1203, 194, 31));
    fixed.push_back(kit::at(left, 1239, 174, 3).fill(kRed));
    fixed.push_back(kit::at(type(details[column], 14, kBlack, false), left,
                            1256, 194, 99));
  }
  fixed.push_back(kit::at(
      type("RHYTHMUS / EINE TYPOGRAFISCHE PARTITUR", 11, kPaper, false), 348,
      1377, 617, 19));
  return positioned()
      .width(kSheetWidth)
      .height(kSheetHeight)
      .children(std::move(fixed))
      .cache(Cache::Texture);
}

/** The moving registers: the orbit of the ring lettering, the red
 *  screen's slide, and each headline strip's offset. */
struct Registers {
  motion::Animatable<float> orbit = motion::animatable(0.0f);
  motion::Animatable<float> screen = motion::animatable(0.0f);
  std::array<motion::Animatable<float>, 5> strips;
};

/** THE POSTER on its own 1000 × 1400 sheet: the printed sheet, the
 *  ring lettering, the red screen and the five clipped strips of one
 *  shaped headline, whose offsets expose the printed registration rather
 *  than reflowing text, under one retained grain layer. */
Element poster(Registers& registers) {
  std::vector<Element> live{
      printedSheet(),
      kit::at(type("PULS / IMPULS / RAUM / KLANG / ZEIT / KÖRPER / "
                   "PULS / IMPULS / RAUM / KLANG / ZEIT / KÖRPER /",
                   22)
                  .textOnPath({.path = shapes::circle(),
                               .at = registers.orbit}),
              76, 540, 536, 536),
      kit::at(178, 268, 775, 74)
          .fill(material::field::halftoneRamp(6.5f, 1.8f, 2.9f, kRed, -15))
          .blendMode(material::BlendMode::Multiply)
          .translateX(registers.screen),
  };
  for (int index = 0; index < 5; ++index) {
    registers.strips[index] = motion::animatable(0.0f);
    live.push_back(
        kit::at(positioned()
                    .children({kit::at(type("rhythmus", 201, kBlack), 3,
                                       -index * 37.0f - 37, 960, 243)})
                    .fill(kPaper)
                    .overflow(Overflow::Clip)
                    .translateX(registers.strips[index]),
                65, 281 + index * 37.0f, 900, 37));
  }
  live.push_back(kit::at(0, 0, kSheetWidth, kSheetHeight)
                     .fill(material::noise(0.7f, {.seed = 29, .grain = true}))
                     .cache(Cache::Texture)
                     .blendMode(material::BlendMode::SoftLight)
                     .opacity(0.12f));
  return positioned()
      .width(kSheetWidth)
      .height(kSheetHeight)
      .children(std::move(live));
}

/** The registers at @p seconds: a slow orbit, a slide of the screen, and
 *  the strips swinging out of register, the outer two least. */
void move(Registers& registers, float seconds) {
  registers.orbit = seconds * 0.015f;
  registers.screen = 6.0f * std::sin(seconds * 0.62f);
  for (int index = 0; index < 5; ++index)
    registers.strips[index] = (index == 0 || index == 4 ? 2.0f : 9.0f) *
                              std::sin(seconds * 1.1f + index * 0.8f);
}

}  // namespace collage

/** A museum label under a plate: the maker, then two lines. */
Element label(const char* maker, const char* title, const char* note) {
  return box().column().gap(4).ink(kLabel).children({
      text(maker).font({.size = 14.0f, .track = 0.6f}),
      text(title).font({.size = 12.0f, .track = 0.4f}),
      text(note).font({.size = 12.0f, .track = 0.4f}),
  });
}

struct TonhallePosters {
  collage::Registers registers;

  void setup(sketch::Scene2DContext& ctx) {
    // The arc table's outermost ring runs 120 ms doubled five times, so
    // its last sweep lands before four seconds and the plate is the poster
    // rather than a frame of its assembly.
    sketch::kit::stage(ctx, {.size = {kWallWidth, kWallHeight},
                             .captureAt = 4.5,
                             .background = material::Color{0, 0, 0, 1}});
    const float left =
        (kWallWidth - arcs::kPlateWidth - collage::kPlateWidth - kPlateGap) /
        2;
    const float right = left + arcs::kPlateWidth + kPlateGap;
    const float labelTop = kPlateTop + kPlateHeight + 18;
    ctx.composer.render(stack().fill(Fill::color(kWall)).children({
        arcs::plate().key("arc table").rect(left, kPlateTop,
                                            arcs::kPlateWidth, kPlateHeight),
        kit::at(right, kPlateTop, collage::kPlateWidth, kPlateHeight)
            .key("collage")
            .fill(hung(collage::kBlack))
            // The sheet's diagonal bands run off its edges and are trimmed
            // there, as the print is.
            .overflow(Overflow::Clip)
            .children({collage::poster(registers)
                           .scale(kPlateHeight / collage::kSheetHeight)
                           .transformOrigin(Dimension(0.0f),
                                            Dimension(0.0f))}),
        kit::at(label("josef müller-brockmann",
                      "beethoven — tonhalle zürich, 1955",
                      "measured arc table · rings double 1:2:4:8:16"),
                left, labelTop, arcs::kPlateWidth, 64),
        kit::at(label("after wolfgang weingart",
                      "rhythmus — kunstkredit basel 1976/77",
                      "cut register strips · layered dot screens"),
                right, labelTop, collage::kPlateWidth, 64),
    }));
  }

  void update(double elapsed) { collage::move(registers, float(elapsed)); }
};

}  // namespace

SIGIL_SKETCH(TonhallePosters, "Study · Type",
             "Two Swiss concert posters as one spread: Brockmann's measured "
             "arc table revealed by span masks beside a Weingart-style "
             "collage of cut register strips, dot screens and ring lettering")
