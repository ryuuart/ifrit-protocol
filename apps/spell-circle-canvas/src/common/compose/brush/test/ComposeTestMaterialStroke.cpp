/** @file
 * A material keyline's ends and corners: the band a `StrokeOptions` states
 * is the silhouette Skia strokes for every cap and join, at fractional
 * widths and miter limits, on open and closed contours and either side of
 * a closed one; a mark that reaches past the node's box keeps its ends;
 * and a relief and a lit surface stand on that same band.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/verbs/Decoration.h>
#include <sigilgeometry/path/Stroke.h>

#include <cmath>
#include <string>
#include <vector>

#include "support/BrushTestSupport.h"
#include "support/Rows.h"

namespace {

using geometry::path::Cap;
using geometry::path::Join;

constexpr int kSide = 200;
const material::Color kRed = {1, 0, 0, 1};

/** An open zigzag with a sharp turn and a shallow one, ending well inside
 *  the host so both of its caps show. */
SkPath zigzag() {
  return SkPathBuilder()
      .moveTo(30, 150)
      .lineTo(70, 50)
      .lineTo(110, 150)
      .lineTo(170, 110)
      .detach();
}

/** A closed triangle, one corner sharp. */
SkPath triangle() {
  return SkPathBuilder()
      .moveTo(40, 160)
      .lineTo(100, 30)
      .lineTo(160, 160)
      .close()
      .detach();
}

const char* nameOf(Cap cap) {
  return cap == Cap::Butt ? "butt" : cap == Cap::Round ? "round" : "square";
}

const char* nameOf(Join join) {
  return join == Join::Round   ? "round"
         : join == Join::Miter ? "miter"
                               : "bevel";
}

/** @p outline keylined in @p finish as @p options state, by a node that
 *  covers the whole host, so the node's local pixels are the host's: the
 *  host's pixels once drawn. */
SkBitmap keylined(const SkPath& outline, const material::Material& finish,
                  const StrokeOptions& options,
                  std::optional<material::texture::Role> map = {}) {
  Host host(kSide, kSide);
  if (map) host.composer.setSurfaceMap(*map);
  host.composer.render(
      box().children({box()
                          .absolute()
                          .rect(0, 0, kSide, kSide)
                          .shape(heldPath(geometry::path::fromSk(outline)))
                          .stroke(finish, options)}));
  host.frame();
  return host.pixels();
}

/** What Skia draws for @p outline stroked red as @p options state,
 *  standing on the side of a closed contour they name as the keyline does:
 *  stroked at twice the width and clipped to that side. */
SkBitmap skiaStroke(const SkPath& outline, const StrokeOptions& options) {
  SkBitmap pixels;
  pixels.allocN32Pixels(kSide, kSide);
  SkCanvas canvas(pixels);
  canvas.clear(SK_ColorBLACK);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorRED);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeCap(geometry::path::toSk(options.cap));
  paint.setStrokeJoin(geometry::path::toSk(options.join));
  paint.setStrokeMiter(options.miterLimit);
  paint.setStrokeWidth(options.width);
  if (options.position != material::StrokePosition::Center) {
    paint.setStrokeWidth(options.width * 2);
    canvas.clipPath(outline,
                    options.position == material::StrokePosition::Inside
                        ? SkClipOp::kIntersect
                        : SkClipOp::kDifference,
                    true);
  }
  canvas.drawPath(outline, paint);
  return pixels;
}

/** How many pixels of @p host differ from @p expected by more than a level
 *  of rounding in red, and how many @p expected inks at all. */
struct Agreement {
  int differing = 0;
  int inked = 0;
};

Agreement against(const SkBitmap& drawn, const SkBitmap& expected) {
  Agreement agreement;
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x) {
      const int painted = static_cast<int>(SkColorGetR(drawn.getColor(x, y)));
      const int stroked =
          static_cast<int>(SkColorGetR(expected.getColor(x, y)));
      agreement.differing += std::abs(painted - stroked) > 2;
      agreement.inked += stroked > 0;
    }
  return agreement;
}

int inkOf(const SkBitmap& drawn) {
  int inked = 0;
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x)
      inked += drawn.getColor(x, y) != SK_ColorBLACK;
  return inked;
}

/** Whether two drawings agree in every byte. */
bool identical(const SkBitmap& one, const SkBitmap& other) {
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x)
      if (one.getColor(x, y) != other.getColor(x, y)) return false;
  return true;
}

}  // namespace

TEST(ComposeMaterialKeyline, TheBandIsTheSilhouetteSkiaStrokes) {
  sigil::compose::test::checkRows({
      {"EveryCapAndJoin",
       [&] {
         for (const Cap cap : {Cap::Butt, Cap::Round, Cap::Square})
           for (const Join join : {Join::Round, Join::Miter, Join::Bevel})
             for (const bool open : {true, false}) {
               SCOPED_TRACE(std::string(nameOf(cap)) + " cap, " + nameOf(join) +
                            " join, " + (open ? "open" : "closed"));
               const StrokeOptions options{
                   .width = 14,
                   .position = material::StrokePosition::Center,
                   .cap = cap,
                   .join = join};
               const SkPath outline = open ? zigzag() : triangle();
               const Agreement agreement =
                   against(keylined(outline, kRed, options),
                           skiaStroke(outline, options));
               EXPECT_GT(agreement.inked, 2000);
               EXPECT_EQ(agreement.differing, 0);
             }
       }},
      {"ACapOrAJoinChangesTheBandWhereItShould",
       [&] {
         // The comparison above holds only if the options reach the stroker: a
         // square cap reaches past the end a butt cap stops at, and a miter
         // fills the corner a bevel cuts.
         const auto at = [](const StrokeOptions& options, int x, int y) {
           return SkColorGetR(keylined(zigzag(), kRed, options).getColor(x, y));
         };
         StrokeOptions options{.width = 14,
                               .position = material::StrokePosition::Center};
         // Five pixels past the far end, along the last leg.
         options.cap = Cap::Butt;
         EXPECT_EQ(at(options, 174, 107), 0u);
         options.cap = Cap::Square;
         EXPECT_GT(at(options, 174, 107), 200u);
         // Just beyond the bevel's chord above the sharp corner at (70, 50).
         options.cap = Cap::Butt;
         options.join = Join::Bevel;
         EXPECT_EQ(at(options, 70, 39), 0u);
         options.join = Join::Miter;
         EXPECT_GT(at(options, 70, 39), 200u);
       }},
      {"FractionalWidths",
       [&] {
         for (const float width : {0.5f, 1.25f, 2.75f}) {
           SCOPED_TRACE(width);
           const StrokeOptions options{
               .width = width,
               .position = material::StrokePosition::Center,
               .cap = Cap::Round,
               .join = Join::Miter};
           const Agreement agreement =
               against(keylined(zigzag(), kRed, options),
                       skiaStroke(zigzag(), options));
           EXPECT_GT(agreement.inked, 200);
           EXPECT_EQ(agreement.differing, 0);
         }
         // Skia strokes a zero width as a hairline; a keyline of no width is
         // none.
         for (const float width : {0.0f, -3.0f, std::nanf("")})
           for (const auto position : {material::StrokePosition::Inside,
                                       material::StrokePosition::Center,
                                       material::StrokePosition::Outside}) {
             SCOPED_TRACE(width);
             EXPECT_EQ(inkOf(keylined(triangle(), kRed,
                                      {.width = width, .position = position})),
                       0);
             EXPECT_EQ(inkOf(keylined(zigzag(), kRed,
                                      {.width = width, .position = position})),
                       0);
           }
       }},
      {"EitherSideOfAClosedContour",
       [&] {
         for (const Join join : {Join::Round, Join::Miter, Join::Bevel})
           for (const auto position : {material::StrokePosition::Inside,
                                       material::StrokePosition::Outside}) {
             SCOPED_TRACE(std::string(nameOf(join)) +
                          (position == material::StrokePosition::Inside
                               ? " inside"
                               : " outside"));
             const StrokeOptions options{
                 .width = 9, .position = position, .join = join};
             const Agreement agreement =
                 against(keylined(triangle(), kRed, options),
                         skiaStroke(triangle(), options));
             EXPECT_GT(agreement.inked, 1000);
             EXPECT_EQ(agreement.differing, 0);
             // A closed contour has no ends, so its cap changes nothing.
             StrokeOptions squared = options;
             squared.cap = Cap::Square;
             EXPECT_TRUE(identical(keylined(triangle(), kRed, options),
                                   keylined(triangle(), kRed, squared)));
           }
       }},
      {"EndsAndCornersPastTheBoxAreKept",
       [&] {
         // A node baked into a texture of its own holds what its marks reach
         // past its box. An open contour's corner on the box's top edge mitres
         // sixteen pixels above it, and a square cap at the box's left edge
         // reaches its half width past it diagonally.
         const SkPath outline = SkPathBuilder()
                                    .moveTo(0, 120)
                                    .lineTo(40, 0)
                                    .lineTo(80, 120)
                                    .detach();
         for (const auto position : {material::StrokePosition::Inside,
                                     material::StrokePosition::Outside}) {
           SCOPED_TRACE(position == material::StrokePosition::Inside
                            ? "inside"
                            : "outside");
           const StrokeOptions options{.width = 12,
                                       .position = position,
                                       .cap = Cap::Square,
                                       .join = Join::Miter};
           Host host(kSide, kSide);
           host.composer.render(box().children(
               {box()
                    .absolute()
                    .rect(40, 60, 80, 120)
                    .cache(Cache::Texture)
                    .shape(heldPath(geometry::path::fromSk(outline)))
                    .stroke(kRed, options)}));
           host.frame();
           host.frame();
           // The miter's tip, above the box.
           EXPECT_GT(SkColorGetR(host.pixel(80, 48)), 200u);
           // The square cap's outer corner, left of the box.
           EXPECT_GT(SkColorGetR(host.pixel(35, 182)), 200u);
           SkBitmap expected;
           expected.allocN32Pixels(kSide, kSide);
           {
             SkCanvas canvas(expected);
             canvas.clear(SK_ColorBLACK);
             canvas.translate(40, 60);
             SkPaint paint;
             paint.setAntiAlias(true);
             paint.setColor(SK_ColorRED);
             paint.setStyle(SkPaint::kStroke_Style);
             paint.setStrokeWidth(12);
             paint.setStrokeCap(SkPaint::kSquare_Cap);
             paint.setStrokeJoin(SkPaint::kMiter_Join);
             canvas.drawPath(outline, paint);
           }
           EXPECT_EQ(against(host.pixels(), expected).differing, 0);
         }
       }},
  });
}

namespace {

/** A level band: a horizontal open line, twenty pixels wide, ending round
 *  at both ends. */
SkPath level() {
  return SkPathBuilder().moveTo(50, 100).lineTo(150, 100).detach();
}

material::Material metal(float depth) {
  return material::from(material::Color{.85f, .55f, .35f, 1})
      .surface({.metallic = 1.0f, .roughness = .3f})
      .relief({.shoulder = 6, .depth = depth});
}

/** A normal's encoded slope up the picture, up positive. */
int upward(SkColor normal) { return static_cast<int>(SkColorGetG(normal)); }
/** A normal's encoded slope across the picture, right positive. */
int across(SkColor normal) { return static_cast<int>(SkColorGetR(normal)); }

}  // namespace

TEST(ComposeMaterialKeyline,
     ALitKeylinePaintsItsMapsOverTheBandAndItsColourFlatOnAPage) {
  // A rough metal keyline on a page painted as surface maps: its roughness
  // and its metal stand over exactly the band Skia strokes, caps and
  // corners included.
  const StrokeOptions options{.width = 14,
                              .position = material::StrokePosition::Center,
                              .cap = Cap::Square,
                              .join = Join::Miter};
  const material::Material rough =
      material::from(material::Color{.8f, .8f, .82f, 1})
          .surface({.metallic = 1.0f, .roughness = .3f});
  const SkBitmap roughness =
      keylined(zigzag(), rough, options, material::texture::Role::Roughness);
  const SkBitmap metallic =
      keylined(zigzag(), rough, options, material::texture::Role::Metallic);
  const SkBitmap expected = skiaStroke(zigzag(), options);
  int solid = 0, astray = 0;
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x) {
      const unsigned covered = SkColorGetR(expected.getColor(x, y));
      if (covered == 255) {
        ++solid;
        EXPECT_NEAR(SkColor4f::FromColor(roughness.getColor(x, y)).fG, .3f,
                    .012f);
        EXPECT_EQ(SkColorGetG(metallic.getColor(x, y)), 255u);
      } else if (covered == 0) {
        astray += roughness.getColor(x, y) != SK_ColorBLACK;
      }
    }
  EXPECT_GT(solid, 2000);
  EXPECT_EQ(astray, 0);
  // Flat, the same keyline paints its colour over the same band.
  const Agreement flat = against(
      keylined(
          zigzag(),
          material::from(kRed).surface({.metallic = 1.0f, .roughness = .3f}),
          options),
      expected);
  EXPECT_EQ(flat.differing, 0);
}
