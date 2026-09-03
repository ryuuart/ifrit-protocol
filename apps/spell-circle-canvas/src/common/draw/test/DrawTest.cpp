/** @file
 * The pen: p5's semantics where p5 states them, and this library's own
 * where it departs.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <sigilcore/compute/Noise.h>
#include <sigildraw/Draw.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cmath>
#include <memory>
#include <vector>

namespace {

using namespace sigil::draw;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** A pen over a raster surface, with the pixels readable back. */
struct Paper {
  explicit Paper(int w = 100, int h = 100)
      : surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h))),
        width(w),
        height(h) {
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  }

  void begin(int frame = 1, double seconds = 0.0) {
    Frame f;
    f.width = (float)width;
    f.height = (float)height;
    f.seconds = seconds;
    f.deltaSeconds = 1.0 / 60.0;
    f.frameCount = frame;
    f.fonts = &fonts();
    pen.begin(*surface->getCanvas(), f);
  }
  void end() { pen.end(); }

  SkColor pixel(int x, int y) {
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap.getColor(x, y);
  }

  /** The columns and rows that hold any ink at all. */
  SkIRect inked() {
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    SkIRect box = SkIRect::MakeEmpty();
    bool any = false;
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        if (SkColorGetA(bitmap.getColor(x, y)) > 0) {
          if (!any) {
            box = SkIRect::MakeXYWH(x, y, 1, 1);
            any = true;
          } else {
            box.join(SkIRect::MakeXYWH(x, y, 1, 1));
          }
        }
    return box;
  }

  sk_sp<SkSurface> surface;
  Pen pen;
  int width;
  int height;
};

// ---- p5's semantics ---------------------------------------------------------

TEST(Pen, RectModeCenterLandsWhereP5Says) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rectMode(CENTER);
  paper.pen.rect(50, 50, 20, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorRED);
  EXPECT_EQ(paper.pixel(41, 46), SK_ColorRED);
  EXPECT_EQ(paper.pixel(58, 53), SK_ColorRED);
  EXPECT_EQ(paper.pixel(38, 50), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pixel(50, 44), SK_ColorTRANSPARENT);
}

TEST(Pen, RectModeCornerIsTheDefault) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(10, 10, 20, 20);
  paper.end();
  EXPECT_EQ(paper.pixel(12, 12), SK_ColorRED);
  EXPECT_EQ(paper.pixel(28, 28), SK_ColorRED);
  EXPECT_EQ(paper.pixel(8, 8), SK_ColorTRANSPARENT);
}

TEST(Pen, EllipseModeCenterIsTheDefaultAndRadiusDoubles) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(0, 255, 0);
  paper.pen.circle(50, 50, 20);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(50, 42), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(50, 38), SK_ColorTRANSPARENT);

  Paper radius;
  radius.begin();
  radius.pen.noStroke();
  radius.pen.fill(0, 255, 0);
  radius.pen.ellipseMode(RADIUS);
  radius.pen.ellipse(50, 50, 20, 20);
  radius.end();
  EXPECT_EQ(radius.pixel(50, 32), SK_ColorGREEN);
  EXPECT_EQ(radius.pixel(50, 28), SK_ColorTRANSPARENT);
}

TEST(Pen, ArcFillsThePieUnlessChord) {
  // A quarter from 12 o'clock to 3 o'clock, drawn OPEN: the pie reaches
  // the centre. Drawn CHORD, the centre is outside the segment.
  Paper open;
  open.begin();
  open.pen.noStroke();
  open.pen.fill(0, 0, 255);
  open.pen.arc(50, 50, 80, 80, -HALF_PI, 0);
  open.end();
  EXPECT_EQ(open.pixel(55, 45), SK_ColorBLUE);
  EXPECT_EQ(open.pixel(45, 55), SK_ColorTRANSPARENT);

  Paper chord;
  chord.begin();
  chord.pen.noStroke();
  chord.pen.fill(0, 0, 255);
  chord.pen.arc(50, 50, 80, 80, -HALF_PI, 0, CHORD);
  chord.end();
  EXPECT_EQ(chord.pixel(55, 45), SK_ColorTRANSPARENT);
  EXPECT_EQ(chord.pixel(80, 30), SK_ColorBLUE);
}

TEST(Pen, AngleModeDegreesReadsRotateInDegrees) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.angleMode(DEGREES);
  paper.pen.translate(50, 50);
  paper.pen.rotate(90);
  paper.pen.rect(0, 0, 30, 4);  // a bar along +x turns to point down +y
  paper.end();
  EXPECT_EQ(paper.pixel(48, 65), SK_ColorRED);
  EXPECT_EQ(paper.pixel(65, 52), SK_ColorTRANSPARENT);
}

TEST(Pen, PushPopRestoresFillAndTransform) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.push();
  paper.pen.fill(0, 0, 255);
  paper.pen.translate(50, 50);
  paper.pen.rect(0, 0, 10, 10);
  paper.pen.pop();
  paper.pen.rect(0, 0, 10, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorBLUE);
  EXPECT_EQ(paper.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(paper.pixel(15, 15), SK_ColorTRANSPARENT);
}

TEST(Pen, StyleSurvivesFramesAndTheTransformDoesNot) {
  Paper paper;
  paper.begin(1);
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.translate(50, 50);
  paper.end();
  paper.begin(2);
  paper.pen.rect(0, 0, 10, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pen.strokePaint(), nullptr);
}

TEST(Pen, BackgroundCoversTheCanvasWhateverTheTransform) {
  Paper paper;
  paper.begin();
  paper.pen.translate(50, 50);
  paper.pen.scale(0.1f);
  paper.pen.background(255);
  paper.end();
  EXPECT_EQ(paper.pixel(2, 2), SK_ColorWHITE);
  EXPECT_EQ(paper.pixel(97, 97), SK_ColorWHITE);
}

TEST(Pen, ColourArgumentsReadAsP5Reads) {
  Pen pen;
  EXPECT_EQ(pen.color(255, 0, 0).toSkColor(), SK_ColorRED);
  EXPECT_EQ(pen.color("#00ff00").toSkColor(), SK_ColorGREEN);
  EXPECT_EQ(pen.color("#00f").toSkColor(), SK_ColorBLUE);
  EXPECT_EQ(pen.color("blue").toSkColor(), SK_ColorBLUE);
  EXPECT_EQ(pen.color(128).toSkColor(), SkColorSetARGB(255, 128, 128, 128));
  EXPECT_EQ(pen.color(0, 51).toSkColor(), SkColorSetARGB(51, 0, 0, 0));
  pen.colorMode(HSB);
  EXPECT_EQ(pen.color(0, 100, 100).toSkColor(), SK_ColorRED);
  EXPECT_EQ(pen.color(120, 100, 100).toSkColor(), SK_ColorGREEN);
  EXPECT_EQ(pen.color(240, 100, 100).toSkColor(), SK_ColorBLUE);
  pen.colorMode(RGB, 1);
  EXPECT_EQ(pen.color(1, 1, 1).toSkColor(), SK_ColorWHITE);
}

TEST(Pen, SeededRandomRepeats) {
  Pen a;
  Pen b;
  for (int i = 0; i < 16; ++i) EXPECT_EQ(a.random(), b.random());
  a.randomSeed(42);
  std::vector<float> first;
  for (int i = 0; i < 16; ++i) first.push_back(a.random(-3, 3));
  a.randomSeed(42);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(first[(size_t)i], a.random(-3, 3));
  for (float v : first) {
    EXPECT_GE(v, -3.0f);
    EXPECT_LT(v, 3.0f);
  }
  for (int i = 0; i < 256; ++i) {
    const float u = a.random(10);
    EXPECT_GE(u, 0.0f);
    EXPECT_LT(u, 10.0f);
  }
}

TEST(Pen, NoiseIsCoresLatticeAtTheCorners) {
  Pen pen;
  pen.noiseSeed(7);
  pen.noiseDetail(1, 0.5f);
  // One octave at an integer position IS the corner value, at the half
  // weight p5's first octave carries.
  const float corner =
      (float)(sigil::core::noise::lattice(7, 3, 4, 5) & 0x00FFFFFFu) /
      16777216.0f;
  EXPECT_FLOAT_EQ(pen.noise(3, 4, 5), 0.5f * corner);
  EXPECT_FLOAT_EQ(NoiseField::corner(7, 3, 4, 5), corner);

  pen.noiseDetail(4, 0.5f);
  float previous = pen.noise(0.0f, 0.37f);
  for (int i = 1; i < 2000; ++i) {
    const float v = pen.noise((float)i * 0.01f, 0.37f);
    EXPECT_GE(v, 0.0f);
    EXPECT_LT(v, 1.0f);
    EXPECT_LT(std::fabs(v - previous), 0.05f);  // continuous
    previous = v;
  }
  const float here = pen.noise(1.5f, 2.5f);
  pen.noiseSeed(8);
  EXPECT_NE(here, pen.noise(1.5f, 2.5f));
  pen.noiseSeed(7);
  EXPECT_EQ(here, pen.noise(1.5f, 2.5f));
}

TEST(Pen, BeginShapeCloseFillsThePolygon) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.beginShape();
  paper.pen.vertex(10, 10);
  paper.pen.vertex(90, 10);
  paper.pen.vertex(50, 90);
  paper.pen.endShape(CLOSE);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 30), SK_ColorRED);
  EXPECT_EQ(paper.pixel(10, 80), SK_ColorTRANSPARENT);
}

TEST(Pen, BlendModeAddPutsLightTogetherAndClamps) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(200, 0, 40);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(ADD);
  paper.pen.fill(100, 0, 80);
  paper.pen.rect(10, 10, 40, 40);
  paper.end();
  const SkColor lit = paper.pixel(30, 30);
  EXPECT_EQ(SkColorGetR(lit), 255u);  // 200 + 100, clamped
  EXPECT_EQ(SkColorGetB(lit), 120u);  // 40 + 80
}

TEST(Pen, BlendModeReplaceOverwritesAlphaAndAll) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(REPLACE);
  paper.pen.fill(0, 0, 255, 128);
  paper.pen.rect(10, 10, 40, 40);
  paper.end();
  const SkColor over = paper.pixel(30, 30);
  // Laid over, this would be an opaque purple; replaced, the source's
  // own half alpha is what stands.
  EXPECT_EQ(SkColorGetA(over), 128u);
  EXPECT_EQ(SkColorGetR(over), 0u);
}

TEST(Pen, BlendModeRemoveTakesThePixelsAway) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(REMOVE);
  paper.pen.fill(0, 255, 0);
  paper.pen.rect(10, 10, 20, 20);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(20, 20)), 0u);
  EXPECT_EQ(paper.pixel(40, 40), SK_ColorRED);
}

TEST(Pen, BlendModeSubtractTakesLightAwayAndKeepsTheAlpha) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(200, 200, 200);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(SUBTRACT);
  paper.pen.fill(50, 0, 0);
  paper.pen.rect(10, 10, 40, 40);
  paper.end();
  const SkColor dark = paper.pixel(30, 30);
  EXPECT_NEAR(SkColorGetR(dark), 150u, 1u);
  EXPECT_NEAR(SkColorGetG(dark), 200u, 1u);
  EXPECT_EQ(SkColorGetA(dark), 255u);
}

TEST(Pen, BlendModeReachesTheMeshTheImageAndTheGround) {
  // The per-corner mesh: one fill on each end of a triangle sends the
  // shape down the vertices route, which has a paint of its own.
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(120, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.pen.blendMode(ADD);
  paper.pen.beginShape(TRIANGLES);
  paper.pen.fill(60, 0, 0);
  paper.pen.vertex(10, 10);
  paper.pen.vertex(90, 10);
  paper.pen.fill(60, 0, 0);
  paper.pen.vertex(50, 90);
  paper.pen.endShape();
  paper.end();
  EXPECT_EQ(SkColorGetR(paper.pixel(50, 40)), 180u);

  // The image: the same source put down twice adds to itself.
  Paper sheet;
  sheet.begin();
  sheet.pen.noStroke();
  sheet.pen.fill(70, 0, 0);
  sheet.pen.rect(0, 0, 100, 100);
  sheet.end();
  const sk_sp<SkImage> stamp = sheet.surface->makeImageSnapshot();

  Paper page;
  page.begin();
  page.pen.image(stamp, 0, 0, 100, 100);
  page.pen.blendMode(ADD);
  page.pen.image(stamp, 0, 0, 100, 100);
  page.end();
  EXPECT_EQ(SkColorGetR(page.pixel(50, 50)), 140u);

  // The ground a background lays.
  Paper ground;
  ground.begin();
  ground.pen.background(30, 0, 0);
  ground.pen.blendMode(ADD);
  ground.pen.background(30, 0, 0);
  ground.end();
  EXPECT_EQ(SkColorGetR(ground.pixel(50, 50)), 60u);
}

TEST(Pen, BlendModeIsStyleSoPushAndPopCarryIt) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(100, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.pen.push();
  paper.pen.blendMode(ADD);
  paper.pen.fill(50, 0, 0);
  paper.pen.rect(0, 0, 50, 100);
  paper.pen.pop();
  // Back under BLEND, an opaque fill covers rather than adds.
  paper.pen.fill(50, 0, 0);
  paper.pen.rect(50, 0, 50, 100);
  paper.end();
  EXPECT_EQ(SkColorGetR(paper.pixel(25, 50)), 150u);
  EXPECT_EQ(SkColorGetR(paper.pixel(75, 50)), 50u);
}

TEST(Pen, ClipKeepsOnlyWhatTheShapeCovered) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] { paper.pen.rect(20, 20, 40, 40); });
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_EQ(paper.pixel(30, 30), SK_ColorRED);
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorRED);
  EXPECT_EQ(SkColorGetA(paper.pixel(10, 10)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(30, 80)), 0u);
}

TEST(Pen, NothingTheClipShapeDrawsLandsOnTheCanvas) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] {
    paper.pen.fill(0, 255, 0);
    paper.pen.rect(20, 20, 40, 40);
    paper.pen.line(0, 0, 100, 100);
    paper.pen.text("mask", 4, 90);
  });
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(30, 30)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(5, 5)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(10, 86)), 0u);
}

TEST(Pen, ClipInvertedCutsTheShapeOut) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] { paper.pen.rect(20, 20, 40, 40); }, {.invert = true});
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(40, 40)), 0u);
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorRED);
}

TEST(Pen, ClipLastsUntilTheMatchingPop) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.push();
  paper.pen.clip([&] { paper.pen.rect(0, 0, 50, 100); });
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.pen.pop();
  paper.pen.fill(0, 0, 255);
  paper.pen.rect(60, 0, 40, 100);
  paper.end();
  EXPECT_EQ(paper.pixel(25, 50), SK_ColorRED);   // inside the mask
  EXPECT_EQ(SkColorGetA(paper.pixel(55, 50)), 0u);  // outside it
  EXPECT_EQ(paper.pixel(70, 50), SK_ColorBLUE);  // after the pop
}

TEST(Pen, TheMaskCarriesTheTransformItWasDrawnUnder) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] {
    paper.pen.push();
    paper.pen.translate(50, 50);
    paper.pen.rect(0, 0, 20, 20);
    paper.pen.pop();
  });
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorRED);
  EXPECT_EQ(SkColorGetA(paper.pixel(10, 10)), 0u);
}

/** A pen set up to draw crisp two-pixel strokes, so a dash reads as
 *  whole inked and blank pixels. */
void dashPen(Pen& pen) {
  pen.noSmooth();
  pen.noFill();
  pen.stroke(0);
  pen.strokeWeight(2);
  pen.strokeCap(SQUARE);
}

TEST(Pen, StrokeDashBreaksTheStrokeIntoItsRun) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4, 4});
  paper.pen.line(10, 50, 90, 50);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(11, 50)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(15, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(19, 50)), 0u);
}

TEST(Pen, AnOddDashRunRepeatsItself) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4});
  paper.pen.line(10, 50, 90, 50);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(11, 50)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(15, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(19, 50)), 0u);
}

TEST(Pen, TheDashPhaseStartsTheRunPartwayIn) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4, 4}, 4);
  paper.pen.line(10, 50, 90, 50);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(11, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(15, 50)), 0u);
}

TEST(Pen, ABeginShapeOutlineWearsTheDash) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4, 4});
  paper.pen.beginShape();
  paper.pen.vertex(20, 20);
  paper.pen.vertex(80, 20);
  paper.pen.vertex(80, 80);
  paper.pen.vertex(20, 80);
  paper.pen.endShape(CLOSE);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(21, 20)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(25, 20)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(29, 20)), 0u);
}

TEST(Pen, APointIsADiscAndNeverDashed) {
  Paper paper;
  paper.begin();
  paper.pen.noSmooth();
  paper.pen.stroke(0);
  paper.pen.strokeWeight(10);
  paper.pen.strokeDash({2, 2});
  paper.pen.point(50, 50);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(50, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(52, 52)), 0u);
}

TEST(Pen, NoDashPutsTheSolidStrokeBackAndPushPopCarriesIt) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.push();
  paper.pen.strokeDash({4, 4});
  paper.pen.line(10, 30, 90, 30);
  paper.pen.pop();
  // Outside the push the stroke was never dashed.
  paper.pen.line(10, 50, 90, 50);
  paper.pen.strokeDash({4, 4});
  paper.pen.noDash();
  paper.pen.line(10, 70, 90, 70);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(15, 30)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(15, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(15, 70)), 0u);
}

TEST(Pen, MathIsP5s) {
  EXPECT_FLOAT_EQ(map(5, 0, 10, 0, 100), 50.0f);
  EXPECT_FLOAT_EQ(map(15, 0, 10, 0, 100, true), 100.0f);
  EXPECT_FLOAT_EQ(lerp(0, 10, 0.25f), 2.5f);
  EXPECT_FLOAT_EQ(constrain(11, 0, 10), 10.0f);
  EXPECT_FLOAT_EQ(dist(0, 0, 3, 4), 5.0f);
  EXPECT_FLOAT_EQ(norm(5, 0, 10), 0.5f);
  EXPECT_FLOAT_EQ(radians(180), PI);
  EXPECT_FLOAT_EQ(degrees(PI), 180.0f);
}

// ---- this library's own --------------------------------------------------

TEST(Pen, TextIsShapedAndCentredByTheAlignment) {
  Paper left;
  left.begin();
  left.pen.textSize(20);
  left.pen.text("Hello", 10, 60);
  left.end();
  const SkIRect ink = left.inked();
  ASSERT_FALSE(ink.isEmpty());
  EXPECT_GE(ink.left(), 9);
  EXPECT_LT(ink.top(), 60);  // the ascent stands above the baseline
  EXPECT_GE(ink.bottom(), 44);

  Paper centred;
  centred.begin();
  centred.pen.textSize(20);
  centred.pen.textAlign(CENTER, CENTER);
  centred.pen.text("Hello", 50, 50);
  centred.end();
  const SkIRect box = centred.inked();
  ASSERT_FALSE(box.isEmpty());
  EXPECT_NEAR((box.left() + box.right()) / 2.0, 50.0, 4.0);
  EXPECT_NEAR((box.top() + box.bottom()) / 2.0, 50.0, 6.0);
}

TEST(Pen, TheBoxIsTheExtentTheVerticalAlignmentDistributesOver) {
  // One passage in a box deeper than it needs. The room left over is
  // what the alignment places, and only the box says how much room that
  // is — a distribution over an extent of nobody said has none to place
  // and seats the middle and the foot where the top would be.
  constexpr float kX = 4, kY = 4, kW = 92;
  const auto ink = [](Constant vertical, float height) {
    Paper paper(100, 140);
    paper.begin();
    paper.pen.textSize(12);
    paper.pen.textAlign(LEFT, vertical);
    paper.pen.text("one two three", kX, kY, kW, height);
    paper.end();
    const SkIRect box = paper.inked();
    EXPECT_FALSE(box.isEmpty());
    return box;
  };
  const SkIRect top = ink(TOP, 92);
  const SkIRect middle = ink(CENTER, 92);
  const SkIRect foot = ink(BOTTOM, 92);

  // The passage is the same passage: only its seat moves.
  EXPECT_NEAR(middle.height(), top.height(), 1);
  EXPECT_NEAR(foot.height(), top.height(), 1);
  EXPECT_EQ(middle.left(), top.left());

  // HALF OF THE ROOM, AND ALL OF IT.
  const int all = foot.top() - top.top();
  const int half = middle.top() - top.top();
  EXPECT_GT(all, 8) << "the box is deeper than the passage, so there is room";
  EXPECT_NEAR(half * 2, all, 2);

  // AND THE ROOM IS THE BOX'S. A box twenty pixels deeper leaves twenty
  // more for the foot to take and none for the top, which stacks from
  // the near edge whatever stands past the last line.
  const SkIRect deeperFoot = ink(BOTTOM, 112);
  const SkIRect deeperTop = ink(TOP, 112);
  EXPECT_EQ(deeperFoot.top() - foot.top(), 20);
  EXPECT_EQ(deeperTop.top(), top.top());
}

TEST(Pen, TextIsBlackUntilAFillIsSet) {
  Paper paper;
  paper.begin();
  paper.pen.textSize(30);
  paper.pen.text("I", 40, 70);
  paper.end();
  const SkIRect ink = paper.inked();
  ASSERT_FALSE(ink.isEmpty());
  const SkColor c = paper.pixel((ink.left() + ink.right()) / 2,
                                (ink.top() + ink.bottom()) / 2);
  EXPECT_EQ(SkColorGetR(c), 0u);
  EXPECT_EQ(SkColorGetG(c), 0u);
  EXPECT_EQ(SkColorGetB(c), 0u);
}

TEST(Pen, AMaterialIsAFill) {
  using sigil::material::skia::Paint;
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(Paint::linear({0, 0}, {100, 0},
                               {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}));
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_GT(SkColorGetR(paper.pixel(2, 50)), 200u);
  EXPECT_GT(SkColorGetB(paper.pixel(97, 50)), 200u);
}

TEST(Pen, AMaterialFitsTheCanvasUnlessTheFillSaysTheShape) {
  using sigil::material::skia::Paint;
  using sigil::material::skia::Stop;
  const std::vector<Stop> ramp{{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  // ONE unit-square ramp, TWO boxes far apart. The only difference between
  // the two papers is the word on the fill.
  const auto draw = [&](Paper& paper, bool fitted) {
    paper.begin();
    paper.pen.noStroke();
    if (fitted)
      paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp), SHAPE);
    else
      paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp));
    paper.pen.rect(0, 0, 40, 40);
    paper.pen.rect(60, 0, 40, 40);
    paper.end();
  };

  Paper canvasFit;
  draw(canvasFit, false);
  // One ramp across the frame: the left box sits at its red end, the right
  // box at its blue one, and neither box has a ramp of its own.
  EXPECT_GT(SkColorGetR(canvasFit.pixel(2, 20)), 200u);
  EXPECT_LT(SkColorGetB(canvasFit.pixel(37, 20)), 150u);
  EXPECT_GT(SkColorGetB(canvasFit.pixel(97, 20)), 200u);

  Paper shapeFit;
  draw(shapeFit, true);
  // A ramp per box: each runs the whole way from red to blue inside its
  // own bounds, wherever those bounds are.
  EXPECT_GT(SkColorGetR(shapeFit.pixel(2, 20)), 200u);
  EXPECT_GT(SkColorGetB(shapeFit.pixel(37, 20)), 200u);
  EXPECT_GT(SkColorGetR(shapeFit.pixel(62, 20)), 200u);
  EXPECT_GT(SkColorGetB(shapeFit.pixel(97, 20)), 200u);
}

TEST(Pen, AFitIsSaidOnTheFillThatSetsIt) {
  using sigil::material::skia::Paint;
  using sigil::material::skia::Stop;
  const std::vector<Stop> ramp{{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp), SHAPE);
  // A fill set without the word goes back to the canvas rather than
  // inheriting the fit of the fill before it.
  paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp));
  paper.pen.rect(0, 0, 40, 40);
  paper.end();
  EXPECT_LT(SkColorGetB(paper.pixel(37, 20)), 150u);
}

TEST(Pen, AMeshIsDrawnWithThePensFillWhereItGoverns) {
  const SkPoint corners[3] = {{10, 10}, {90, 10}, {10, 90}};
  const SkColor reds[3] = {SK_ColorRED, SK_ColorRED, SK_ColorRED};

  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(0, 255, 0);
  paper.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, reds));
  paper.end();
  // The mesh carries its own corner colours and the fill is a plain one,
  // so the corners paint — the rule the per-corner form of `vertex`
  // follows. It lands in the pen's space, and only inside the triangle.
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorRED);
  EXPECT_EQ(paper.pixel(80, 80), SK_ColorTRANSPARENT);

  // The pen's transform carries it, and `noFill()` means there is nothing
  // to draw it with.
  Paper moved;
  moved.begin();
  moved.pen.noStroke();
  moved.pen.noFill();
  moved.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, reds));
  EXPECT_EQ(moved.inked(), SkIRect::MakeEmpty());
  moved.pen.fill(0, 0, 255);
  moved.pen.translate(0, 5);
  moved.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, nullptr));
  moved.end();
  // No corner colours, so the fill's own colour paints, five rows down.
  EXPECT_EQ(moved.pixel(20, 25), SK_ColorBLUE);
  EXPECT_EQ(moved.pixel(20, 12), SK_ColorTRANSPARENT);
}

TEST(Pen, AMeshTakesAFittedMaterialOverItsOwnBounds) {
  using sigil::material::skia::Paint;
  using sigil::material::skia::Stop;
  const std::vector<Stop> ramp{{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  // A triangle occupying the left half of the paper. Fitted, its ramp runs
  // red to blue across THAT, not across the frame.
  const SkPoint corners[3] = {{0, 0}, {50, 0}, {0, 100}};
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp), SHAPE);
  paper.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, nullptr));
  paper.end();
  EXPECT_GT(SkColorGetR(paper.pixel(1, 1)), 200u);
  EXPECT_GT(SkColorGetB(paper.pixel(46, 1)), 180u);
}

TEST(Pen, TheFillPaintIsNullUnderNoFillAndSoIsTheStroke) {
  Paper paper;
  paper.begin();
  paper.pen.fill(255, 0, 0);
  paper.pen.stroke(0, 0, 255);
  paper.pen.blendMode(ADD);
  const SkPaint* fill = paper.pen.fillPaint();
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->asBlendMode(), SkBlendMode::kPlus);

  // `noFill()` means there is NO FILL to hand over, not a colourless one:
  // the answer is null, and the pen's blend has to be read off the stroke
  // instead. Every verb in the class checks before it dereferences and a
  // caller through the canvas door has to as well.
  paper.pen.noFill();
  EXPECT_EQ(paper.pen.fillPaint(), nullptr);
  const SkPaint* stroke = paper.pen.strokePaint();
  ASSERT_NE(stroke, nullptr);
  EXPECT_EQ(stroke->asBlendMode(), SkBlendMode::kPlus);

  paper.pen.noStroke();
  EXPECT_EQ(paper.pen.strokePaint(), nullptr);
  // A zero weight is the other way a stroke stops existing.
  paper.pen.stroke(0, 0, 255);
  paper.pen.strokeWeight(0);
  EXPECT_EQ(paper.pen.strokePaint(), nullptr);
  paper.end();
}

struct Ring {
  float inset = 0;
  SkPath path(SkSize size) const {
    return SkPath::Oval(
        SkRect::MakeWH(size.width(), size.height()).makeInset(inset, inset));
  }
};

TEST(Pen, ASilhouetteIsAShapeInTheRectModesBox) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rectMode(CENTER);
  paper.pen.shape(Ring{}, 50, 50, 40, 40);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorRED);
  EXPECT_EQ(paper.pixel(50, 33), SK_ColorRED);
  EXPECT_EQ(paper.pixel(50, 28), SK_ColorTRANSPARENT);
}

}  // namespace

// A guest of the pen's, in a namespace of its own, as another library
// would declare one.
namespace probe {

struct Card {
  int id = 0;
};

struct Seen {
  int paints = 0;
  SkRect box = SkRect::MakeEmpty();
};

void paintRetained(sigil::draw::Pen& pen, const Card& card, const SkRect& box,
                   sigil::draw::Slot slot) {
  Seen& seen =
      pen.retained().get<Seen>(slot, [] { return std::make_shared<Seen>(); });
  ++seen.paints;
  seen.box = box;
  (void)card;
}

}  // namespace probe

namespace {

TEST(Pen, AGuestIsRetainedPerCallSite) {
  Paper paper;
  const probe::Card card{1};
  for (int frame = 1; frame <= 3; ++frame) {
    paper.begin(frame);
    paper.pen.element(card, SkRect::MakeXYWH(10, 10, 30, 20));
    for (int i = 0; i < 2; ++i)
      paper.pen.element(card, SkRect::MakeXYWH(0, 0, 5, 5), i);
    paper.end();
  }
  // One slot for the single call, one per index for the loop.
  EXPECT_EQ(paper.pen.retained().size(), 3u);
}

TEST(Pen, TheRetainedStoreKeepsAValueByItsSlot) {
  Retained store;
  const sigil::draw::Slot slot =
      sigil::draw::Slot::at(std::source_location::current(), 0);
  int& first = store.get<int>(slot, [] { return std::make_shared<int>(5); });
  first = 9;
  int& again = store.get<int>(slot, [] { return std::make_shared<int>(1); });
  EXPECT_EQ(&first, &again);
  EXPECT_EQ(again, 9);
  const sigil::draw::Slot other =
      sigil::draw::Slot::at(std::source_location::current(), 1);
  EXPECT_NE(&store.get<int>(other, [] { return std::make_shared<int>(2); }),
            &first);
  EXPECT_EQ(store.size(), 2u);
}

// ---- smoothing reaches the image sampler ------------------------------------

/** A two-by-two image, one colour per texel, for a blit to magnify. */
sk_sp<SkImage> quadrants() {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(2, 2));
  bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 1, 1), SK_ColorRED);
  bitmap.eraseArea(SkIRect::MakeXYWH(1, 0, 1, 1), SK_ColorGREEN);
  bitmap.eraseArea(SkIRect::MakeXYWH(0, 1, 1, 1), SK_ColorBLUE);
  bitmap.eraseArea(SkIRect::MakeXYWH(1, 1, 1, 1), SK_ColorWHITE);
  bitmap.setImmutable();
  return bitmap.asImage();
}

TEST(Pen, NoSmoothMakesImageDrawingNearestNeighbour) {
  Paper paper;
  paper.begin();
  paper.pen.noSmooth();
  paper.pen.image(quadrants(), 0, 0, 40, 40);
  paper.end();
  // The boundary between two texels is a step: the last column of the
  // first block is the whole first colour and the first column of the
  // second is the whole second one, with nothing between them.
  EXPECT_EQ(paper.pixel(19, 5), SK_ColorRED);
  EXPECT_EQ(paper.pixel(20, 5), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(5, 35), SK_ColorBLUE);
}

TEST(Pen, SmoothingOnBlendsAcrossTheImageBoundary) {
  Paper paper;
  paper.begin();
  paper.pen.image(quadrants(), 0, 0, 40, 40);
  paper.end();
  // The default is p5's smoothing: the same boundary is a ramp, so
  // neither side of it is either texel's own colour.
  const SkColor left = paper.pixel(19, 20);
  EXPECT_NE(left, SK_ColorRED);
  EXPECT_NE(left, SK_ColorGREEN);
}

// ---- a fill between two vertices colours the corners ------------------------

TEST(Pen, FillBetweenVerticesColoursEachCorner) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.beginShape(QUADS);
  paper.pen.fill(255, 0, 0);
  paper.pen.vertex(0, 0);
  paper.pen.vertex(0, 99);
  paper.pen.fill(0, 0, 255);
  paper.pen.vertex(99, 99);
  paper.pen.vertex(99, 0);
  paper.pen.endShape();
  paper.end();
  const SkColor left = paper.pixel(2, 50);
  const SkColor right = paper.pixel(97, 50);
  const SkColor middle = paper.pixel(50, 50);
  EXPECT_GT(SkColorGetR(left), 200u);
  EXPECT_LT(SkColorGetB(left), 60u);
  EXPECT_GT(SkColorGetB(right), 200u);
  EXPECT_LT(SkColorGetR(right), 60u);
  // Interpolated across the quad, so the middle is neither corner.
  EXPECT_GT(SkColorGetR(middle), 60u);
  EXPECT_GT(SkColorGetB(middle), 60u);
}

TEST(Pen, AlphaRampsAcrossAQuadTheSameWay) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.beginShape(QUADS);
  paper.pen.fill(255, 255, 255, 255);
  paper.pen.vertex(0, 0);
  paper.pen.vertex(0, 99);
  paper.pen.fill(255, 255, 255, 0);
  paper.pen.vertex(99, 99);
  paper.pen.vertex(99, 0);
  paper.pen.endShape();
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(2, 50)), 200u);
  EXPECT_LT(SkColorGetA(paper.pixel(97, 50)), 60u);
}

TEST(Pen, OneFillAcrossTheShapeStaysAPathAndStrokes) {
  Paper paper;
  paper.begin();
  paper.pen.fill(0, 255, 0);
  paper.pen.stroke(255, 0, 0);
  paper.pen.strokeWeight(6);
  paper.pen.beginShape(QUADS);
  paper.pen.vertex(20, 20);
  paper.pen.vertex(20, 80);
  paper.pen.vertex(80, 80);
  paper.pen.vertex(80, 20);
  paper.pen.endShape();
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(50, 20), SK_ColorRED);
}

TEST(Pen, AStrokedMeshStillWearsItsOutline) {
  Paper paper;
  paper.begin();
  paper.pen.stroke(255, 255, 255);
  paper.pen.strokeWeight(6);
  paper.pen.beginShape(QUADS);
  paper.pen.fill(255, 0, 0);
  paper.pen.vertex(20, 20);
  paper.pen.vertex(20, 80);
  paper.pen.fill(0, 0, 255);
  paper.pen.vertex(80, 80);
  paper.pen.vertex(80, 20);
  paper.pen.endShape();
  paper.end();
  EXPECT_EQ(paper.pixel(50, 20), SK_ColorWHITE);
  EXPECT_GT(SkColorGetR(paper.pixel(24, 50)), 200u);
}

// ---- the canvas itself, for another library's drawing -----------------------

TEST(Pen, TheCanvasCarriesThePensTransform) {
  Paper paper;
  paper.begin();
  paper.pen.translate(30, 40);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  paint.setAntiAlias(false);
  ASSERT_NE(paper.pen.canvas(), nullptr);
  paper.pen.canvas()->drawRect(SkRect::MakeWH(10, 10), paint);
  paper.end();
  // Drawn in the pen's space: the rect landed where pen.rect would have
  // put it, not at the canvas origin.
  EXPECT_EQ(paper.inked(), SkIRect::MakeXYWH(30, 40, 10, 10));
}

TEST(Pen, TheCanvasAndThePensOwnVerbsShareOneOrder) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(0, 255, 0);
  paper.pen.rect(0, 0, 50, 50);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  paper.pen.canvas()->drawRect(SkRect::MakeWH(50, 50), paint);
  paper.end();
  EXPECT_EQ(paper.pixel(25, 25), SK_ColorRED);
}

// ---- p5's createGraphics ----------------------------------------------------

TEST(Graphics, DrawnOnceAndPutDownWhereTheFrameSaysInCanvasUnits) {
  Paper paper;
  Graphics buffer{20, 20};
  paper.begin();
  Pen& g = buffer.begin(paper.pen);
  g.noStroke();
  g.fill(0, 255, 0);
  g.rect(0, 0, 20, 20);
  buffer.end();
  paper.pen.image(buffer, 10, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(15, 15), SK_ColorGREEN);
  EXPECT_EQ(SkColorGetA(paper.pixel(5, 5)), 0u);
  EXPECT_EQ(paper.inked(), SkIRect::MakeXYWH(10, 10, 20, 20));
}

TEST(Graphics, KeepsItsPixelsAndItsStyleBetweenFrames) {
  Paper paper;
  Graphics buffer{20, 20};
  paper.begin(1);
  Pen& first = buffer.begin(paper.pen);
  first.noStroke();
  first.fill(255, 0, 0);
  first.rect(0, 0, 10, 20);
  buffer.end();
  paper.end();

  paper.begin(2);
  // No fill set this time: the buffer's own style held from the last
  // frame, as a pen's does, and the first frame's block is still there.
  Pen& second = buffer.begin(paper.pen);
  second.rect(10, 0, 10, 20);
  buffer.end();
  paper.pen.image(buffer, 0, 0);
  paper.end();
  EXPECT_EQ(paper.pixel(5, 10), SK_ColorRED);
  EXPECT_EQ(paper.pixel(15, 10), SK_ColorRED);
}

TEST(Graphics, IsFormedAtTheHostsDensityAndDrawnInCanvasUnits) {
  Paper paper;
  paper.surface->getCanvas()->scale(2, 2);
  Graphics buffer{20, 20};
  paper.begin();
  Pen& g = buffer.begin(paper.pen);
  g.noStroke();
  g.fill(0, 0, 255);
  g.circle(10, 10, 20);
  buffer.end();
  paper.pen.image(buffer, 0, 0);
  paper.end();
  EXPECT_EQ(buffer.extent(), SkISize::Make(40, 40));
  // Placed by its canvas size: twenty units on a doubled canvas is
  // forty pixels across, and the circle's centre lands at (20, 20).
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorBLUE);
  EXPECT_EQ(SkColorGetA(paper.pixel(45, 45)), 0u);
}

TEST(Pen, TheContentScaleIsWhatTheFrameBeganOn) {
  Paper paper;
  paper.surface->getCanvas()->scale(2, 2);
  paper.begin();
  EXPECT_FLOAT_EQ(paper.pen.contentScale(), 2.0f);
  paper.end();
}

}  // namespace
