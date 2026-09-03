/** @file
 * The pen: p5's semantics where p5 states them, and this library's own
 * where it departs.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
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

}  // namespace
