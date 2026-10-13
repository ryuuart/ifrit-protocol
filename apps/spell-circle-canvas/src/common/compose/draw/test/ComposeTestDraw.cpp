/** @file
 * The door between the tree and the pen, both ways: a pen program as a
 * node's content, and an element retained inside a pen's loop.
 */

#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Tool.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmaterial/skia/Paint.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <source_location>

#include "support/Host.h"
#include "support/Paper.h"

namespace {

using sigil::draw::Frame;
using sigil::draw::Pen;

using Paper = sigil::draw::testing::Paper;

bool anyWhiteIn(Paper& paper, SkIRect region) {
  const SkBitmap pixels = paper.pixels();
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (pixels.getColor(x, y) == SK_ColorWHITE) return true;
  return false;
}

TEST(DrawNode, DrawWithHandsAProgramTheScopeAndPaintsOverIt) {
  // Two keyed boxes; the program draws a bar between their centres, read
  // off the scope, and the bar lands between them on the canvas.
  Host host;
  int runs = 0;
  auto tree = [&] {
    return box()
        .width(200)
        .height(200)
        .children({box().key("a").left(20).top(90).width(20).height(20),
                   box().key("b").left(160).top(90).width(20).height(20)})
        .operators({drawWith("bar", [&](Pen& pen, const Scope& scope) {
          ++runs;
          const Scope::Node* a = scope.find("a");
          const Scope::Node* b = scope.find("b");
          ASSERT_TRUE(a && b);
          EXPECT_EQ(scope.nodes().size(), 2u);
          pen.noStroke();
          pen.fill(255, 0, 0);
          pen.rect(a->bounds.centre().x, a->bounds.centre().y - 3,
                   b->bounds.centre().x - a->bounds.centre().x, 6);
        })});
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(runs, 1);
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 60), SK_ColorBLACK);
  // Keyed, the pen it attaches prunes: a second identical describe
  // patches nothing, and the program runs once more only because a pen
  // at Cache::None runs every frame.
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame(1.0 / 60.0);
  EXPECT_EQ(runs, 2);
}

TEST(DrawNode, RunsThePenOverTheNodesBoxEveryFrame) {
  Host host;
  int runs = 0;
  float width = 0;
  host.composer.render(stack().children({pen([&](Pen& pen) {
                                           ++runs;
                                           width = pen.width;
                                           pen.noStroke();
                                           pen.fill(255, 0, 0);
                                           pen.rect(0, 0, 10, 10);
                                         })
                                             .width(50)
                                             .height(40)}));
  host.frame();
  EXPECT_EQ(runs, 1);
  EXPECT_FLOAT_EQ(width, 50.0f);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 15), SK_ColorBLACK);
  host.frame(1.0 / 60.0);
  EXPECT_EQ(runs, 2);  // Cache::None: the program runs each frame
}

TEST(DrawNode, ACanvasFillsTheBoxItStandsIn) {
  // `cover()` is `absolute().inset(0)` said once, and a pen comes back
  // wearing it — a p5 canvas fills its box by nature.
  Host host;
  float width = 0;
  float height = 0;
  host.composer.render(
      box().children({box().width(80).height(60).children({pen([&](Pen& pen) {
        width = pen.width;
        height = pen.height;
      })})}));
  host.frame();
  EXPECT_FLOAT_EQ(width, 80.0f);
  EXPECT_FLOAT_EQ(height, 60.0f);
  // The verb says what the two said between them: the node is taken out
  // of the flow and stretched to the box it stands in.
  host.composer.render(box().children({box().width(80).height(60).children(
      {box().height(12), box().cover().key("over")})}));
  host.frame();
  const std::optional<geometry::path::Rect> over = host.composer.bounds("over");
  ASSERT_TRUE(over.has_value());
  EXPECT_EQ(geometry::path::toSk(*over), SkRect::MakeWH(80, 60));
}

TEST(DrawNode, AProgramNamesTheParametersItReadsAndTheVerbTakesTheCache) {
  Host host;
  int runs = 0;
  glm::vec2 box{0, 0};
  // The pen and the paint context are both offered; this program names
  // both, and the one below names neither.
  host.composer.render(
      stack().children({pen(
                            "once",
                            [&](Pen& pen, const PaintContext& ctx) {
                              ++runs;
                              box = ctx.size;
                              pen.noStroke();
                              pen.fill(255, 0, 0);
                              pen.rect(0, 0, 10, 10);
                            },
                            Cache::Texture)
                            .width(50)
                            .height(40)}));
  host.frame();
  EXPECT_EQ(runs, 1);
  EXPECT_FLOAT_EQ(box.x, 50.0f);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  // A drawing the verb was told is drawn ONCE is not run again: the bake
  // is put down instead.
  host.frame(1.0 / 60.0);
  EXPECT_EQ(runs, 1);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);

  int bare = 0;
  host.composer.render(stack().children(
      {pen("none", [&bare] { ++bare; }).width(10).height(10)}));
  host.frame();
  EXPECT_EQ(bare, 1);
}

TEST(DrawNode, ThePenHoldsItsStyleFromFrameToFrame) {
  Host host;
  host.composer.render(stack().children({pen([](Pen& pen) {
                                           if (pen.frameCount == 1) {
                                             pen.noStroke();
                                             pen.fill(0, 0, 255);
                                           }
                                           pen.rect(0, 0, 20, 20);
                                         })
                                             .width(50)
                                             .height(50)}));
  host.frame();
  host.frame(1.0 / 60.0);
  EXPECT_EQ(host.pixel(10, 10), SK_ColorBLUE);
}

class DrawProgramRecovery : public testing::TestWithParam<bool> {};

TEST_P(DrawProgramRecovery, ClosesAThrowingFrameAndCanDrawAgain) {
  Host host;
  Pen* borrowed = nullptr;
  int runs = 0;
  const PenProgram program = [&](Pen& pen) {
    borrowed = &pen;
    if (++runs == 1) {
      pen.noStroke();
      pen.fill(255, 0, 0);
      pen.push();
      pen.fill(0, 255, 0);
      pen.translate(40, 40);
      throw std::runtime_error("Drawing program failed.");
    }
    pen.rect(0, 0, 10, 10);
  };
  auto drawing = GetParam() ? graphics(program) : pen(program);
  host.composer.render(
      stack().children({drawing.width(50).height(50).left(20).top(30)}));
  {
    // The caller owns the canvas around a failed composer draw; the
    // node owns closing its pen and unwinding the program's saved style.
    const SkAutoCanvasRestore restore(host.surface->getCanvas(), true);
    EXPECT_THROW(host.frame(), std::runtime_error);
  }
  ASSERT_NE(borrowed, nullptr);
  ASSERT_EQ(borrowed->canvas(), nullptr);

  // A resumed draw may use another surface. A pen retaining the previous
  // canvas would try to end its old frame against a destroyed surface.
  host.surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
  host.frame(1.0 / 60.0);
  EXPECT_EQ(runs, 2);
  EXPECT_EQ(host.pixel(25, 35), SK_ColorRED);
  EXPECT_EQ(host.pixel(65, 75), SK_ColorBLACK);
}

INSTANTIATE_TEST_SUITE_P(PenAndGraphics, DrawProgramRecovery, testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Graphics" : "Pen";
                         });

TEST(DrawNode, TheTransformStartsAtTheBox) {
  Host host;
  host.composer.render(stack().children({pen([](Pen& pen) {
                                           pen.noStroke();
                                           pen.fill(0, 255, 0);
                                           pen.rect(0, 0, 10, 10);
                                         })
                                             .width(30)
                                             .height(30)
                                             .left(60)
                                             .top(70)}));
  host.frame();
  EXPECT_EQ(host.pixel(65, 75), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);
}

// A world-space material spans the composer's root inside a pen leaf as it
// does in a node's own fill: two pen leaves of unequal width, one under a
// pen transform, sample the same field at the same root positions as a fill
// across the canvas, through a resize of the root and a move of the leaves.
TEST(DrawNode, AWorldSpaceMaterialSpansTheRootInsideEachPen) {
  material::Paint field = material::Paint::linearGradient(
      {0, 0}, {1, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
  field.worldSpace();
  const auto penLeaf = [&](float left, float width, bool translated) {
    return pen([field, translated](Pen& pen, const PaintContext&) {
             pen.noStroke();
             pen.fill(field);
             if (translated) pen.translate(10, 0);
             pen.rect(translated ? -10 : 0, 0, pen.width, pen.height);
           })
        .absolute()
        .left(left)
        .top(60)
        .width(width)
        .height(40);
  };
  const auto describe = [&](float shift) {
    return box().children(
        {box().absolute().left(0).top(10).width(pct(100)).height(40).fill(
             material::skia::base(field)),
         penLeaf(20 + shift, 110, false), penLeaf(150 + shift, 40, true)});
  };
  const auto matches = [](Host& host, int x) {
    const SkColor fill = host.pixel(x, 30);
    const SkColor drawn = host.pixel(x, 80);
    return std::abs((int)SkColorGetR(fill) - (int)SkColorGetR(drawn)) <= 2 &&
           std::abs((int)SkColorGetB(fill) - (int)SkColorGetB(drawn)) <= 2;
  };
  Host host(200, 120);
  host.composer.render(describe(0));
  host.frame();
  for (int x : {25, 60, 125, 155, 185})
    EXPECT_TRUE(matches(host, x)) << "at x" << x;
  // Halfway across the root reads the field's middle, whichever leaf.
  EXPECT_NEAR(SkColorGetR(host.pixel(100, 80)), 128, 3);
  host.composer.setSize({160, 120});
  host.composer.render(describe(-15));
  host.frame();
  for (int x : {10, 60, 100, 140, 150})
    EXPECT_TRUE(matches(host, x)) << "at x" << x << " after a resize";
  EXPECT_NEAR(SkColorGetR(host.pixel(80, 80)), 128, 3);
}

// A nib lays each dab as a disc of its own colour, inside a pen leaf and
// inside a kept canvas alike: over black, the stroke's discs stand in the
// tool's colour across their width, and nothing is lighter than it.
TEST(DrawNode, ANibLaysDiscsOfItsOwnColour) {
  const material::Color cinnabar{0.89f, 0.26f, 0.2f, 1};
  const auto program = [cinnabar](Pen& pen, const PaintContext&) {
    sigil::draw::brush::Tool tool = sigil::draw::brush::marker(cinnabar, 14);
    tool.opacity = 1;
    tool.pressureOpacity = 0;
    tool.opacityJitter = 0;
    tool.sizeJitter = 0;
    tool.scatter = 0;
    tool.noise = 0;
    sigil::draw::brush::line(pen, tool, {30, 50}, {170, 50});
  };
  for (const bool kept : {false, true}) {
    Host host(200, 100);
    host.composer.render(
        box().children({(kept ? graphics(program) : pen(program))
                            .absolute()
                            .left(0)
                            .top(0)
                            .width(200)
                            .height(100)}));
    host.frame();
    for (int y = 47; y <= 53; ++y)
      for (int x = 60; x < 140; x += 7) {
        const SkColor c = host.pixel(x, y);
        EXPECT_NEAR(SkColorGetR(c), 227, 4)
            << (kept ? "kept " : "pen ") << x << "," << y;
        EXPECT_NEAR(SkColorGetG(c), 66, 4)
            << (kept ? "kept " : "pen ") << x << "," << y;
      }
    int lighter = 0;
    for (int y = 30; y < 71; ++y)
      for (int x = 10; x < 190; ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetA(c) == 255 &&
            (SkColorGetG(c) > 66 + 6 || SkColorGetB(c) > 51 + 6))
          ++lighter;
      }
    EXPECT_EQ(lighter, 0) << (kept ? "kept" : "pen");
  }
}

// A pen leaf whose drawing is held keeps its world-space field on the root
// when an ancestor moves: the recording holding it is remade rather than
// replayed at the new place with the old anchoring, so a sheet turned
// to an angle frame by frame paints what the same sheet standing at that
// angle from its first frame paints.
TEST(DrawNode, APenInAHeldRecordingFollowsTheRootWhenAnAncestorMoves) {
  material::Paint field = material::Paint::linearGradient(
      {0, 0}, {1, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
  field.worldSpace();
  const auto describe = [&](const motion::Animatable<float>& turn) {
    const auto leaf = pen([field](Pen& pen, const PaintContext&) {
                        pen.noStroke();
                        pen.fill(field);
                        pen.rect(0, 0, pen.width, pen.height);
                      })
                          .cache(Cache::Picture)
                          .absolute()
                          .left(0)
                          .top(0)
                          .width(120)
                          .height(40);
    // The corner child makes the parent record rather than paint live.
    const auto parent =
        box().absolute().left(0).top(0).width(120).height(40).children(
            {leaf,
             box().absolute().left(0).top(0).width(2).height(2).fill(green())});
    return box().children(
        {box()
             .absolute()
             .left(40)
             .top(80)
             .width(120)
             .height(40)
             .transformOrigin(pct(50), pct(50))
             .rotate(motion::bind(turn, {.to = {0.0f, 360.0f}}))
             .children({parent})});
  };
  motion::Animatable<float> turn = motion::animatable(0.0f);
  Host turning(200, 200);
  turning.composer.render(describe(turn));
  for (int frame = 0; frame <= 25; ++frame) {
    turn = (float)frame / 100.0f;
    turning.frame();
  }
  motion::Animatable<float> held = motion::animatable(0.25f);
  Host still(200, 200);
  still.composer.render(describe(held));
  still.frame();
  int far = 0;
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) {
      const SkColor a = turning.pixel(x, y), b = still.pixel(x, y);
      if (std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)) > 8 ||
          std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b)) > 8)
        ++far;
    }
  EXPECT_LT(far, 20);
}

TEST(RetainedElement, PaintsAtTheBoxAndKeepsItsComposer) {
  Paper paper;
  for (int frame = 1; frame <= 2; ++frame) {
    paper.begin(frame, frame / 60.0);
    paper.pen.element(box().fill(red()), 10, 10, 30, 30);
    paper.pen.end();
  }
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorRED);
  EXPECT_EQ(paper.pixel(39, 39), SK_ColorRED);
  EXPECT_EQ(paper.pixel(5, 5), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pixel(45, 45), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pen.retained().size(), 1u);
}

TEST(RetainedElement, UsesTheWholeStatedFrameDelta) {
  Paper paper;
  double seconds = -1.0;
  const Element guest = custom("clock", [&](Pen&, const PaintContext& context) {
                          seconds = context.elapsedSeconds;
                        }).cache(Cache::None);
  Frame frame;
  frame.width = 100;
  frame.height = 100;
  frame.seconds = 0.5;
  frame.deltaSeconds = 0.5;
  frame.frameCount = 1;
  frame.fonts = &fonts();
  paper.pen.begin(*paper.surface->getCanvas(), frame);
  paper.pen.element(guest, 0, 0, 100, 100);
  paper.pen.end();
  EXPECT_DOUBLE_EQ(seconds, 0.5);
}

TEST(RetainedElement, ShapesTextWithThePensFonts) {
  Paper paper;
  paper.begin(1);
  paper.pen.element(text(u8"Hi", whiteStyle(24)), 10, 10, 80, 40);
  paper.pen.end();
  EXPECT_TRUE(anyWhiteIn(paper, SkIRect::MakeXYWH(10, 10, 80, 40)));
  EXPECT_FALSE(anyWhiteIn(paper, SkIRect::MakeXYWH(0, 60, 100, 40)));
}

TEST(RetainedElement, FollowsThePensTransform) {
  Paper paper;
  paper.begin(1);
  paper.pen.translate(50, 50);
  paper.pen.element(box().fill(blue()), 0, 0, 20, 20);
  paper.pen.end();
  EXPECT_EQ(paper.pixel(60, 60), SK_ColorBLUE);
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorTRANSPARENT);
}

TEST(RetainedElement, AnotherFontContextGetsAComposerBuiltOnIt) {
  // The guest is kept across frames and the composer inside it holds the
  // font context it was built from BY REFERENCE. A pen that draws with a
  // different one must not be answered with that composer: it would shape
  // against a context this pen does not hold, and one that has gone is a
  // dangling reference. The tree is the same, so what the case can see is
  // that the guest was remade and the drawing still lands.
  weave::FontContext second(sigil::weave::ports::systemFontManager());
  Paper paper;
  // ONE call site, so one slot and one kept guest — which is what makes
  // the second frame reach the composer the first frame built.
  for (int frame = 1; frame <= 2; ++frame) {
    paper.beginWith(frame, frame == 1 ? &fonts() : &second, frame / 60.0);
    paper.pen.element(text(u8"Hi", whiteStyle(24)), 0,
                      frame == 1 ? 0.0f : 50.0f, 90, 40);
    paper.pen.end();
  }
  EXPECT_EQ(paper.pen.retained().size(), 1u) << "one slot, one guest";
  EXPECT_TRUE(anyWhiteIn(paper, SkIRect::MakeXYWH(0, 0, 90, 40)));
  EXPECT_TRUE(anyWhiteIn(paper, SkIRect::MakeXYWH(0, 50, 90, 40)))
      << "the second context's text is shaped and drawn";
}

TEST(RetainedElement, PaintsNothingWithoutFonts) {
  Paper paper;
  paper.beginWith(1, nullptr);
  paper.pen.element(box().fill(red()), 10, 10, 30, 30);
  paper.pen.end();
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pen.retained().size(), 0u);
}

TEST(RetainedElement, RetirementReleasesTheComposerAndLeavesItsPeerRunning) {
  Paper paper;
  const auto where = std::source_location::current();
  const auto drawGuest = [&](const Element& element, int index, float x) {
    paper.pen.element(element, x, 0, 40, 40, index, where);
  };
  double firstTime = -1, peerTime = -1;
  auto resource = std::make_shared<int>(42);
  const std::weak_ptr<int> weak = resource;
  Element first =
      custom("first", [resource, &firstTime](Pen&, const PaintContext& ctx) {
        if (*resource == 42) firstTime = ctx.elapsedSeconds;
      }).cache(Cache::None);
  const Element peer = custom("peer", [&](Pen&, const PaintContext& ctx) {
                         peerTime = ctx.elapsedSeconds;
                       }).cache(Cache::None);

  paper.begin(1);
  drawGuest(first, 1, 0);
  drawGuest(peer, 2, 50);
  paper.end();
  resource.reset();
  first = box();
  ASSERT_FALSE(weak.expired());
  EXPECT_NEAR(firstTime, 1.0 / 60.0, 1e-12);

  paper.begin(2);
  drawGuest(peer, 2, 50);
  paper.end();
  EXPECT_EQ(paper.pen.retained().size(), 2u);
  EXPECT_NEAR(peerTime, 2.0 / 60.0, 1e-12);
  ASSERT_TRUE(paper.pen.retained().erase(sigil::draw::Slot::at(where, 1)));
  EXPECT_TRUE(weak.expired());
  EXPECT_EQ(paper.pen.retained().size(), 1u);

  const Element replacement =
      custom("replacement", [&](Pen&, const PaintContext& ctx) {
        firstTime = ctx.elapsedSeconds;
      }).cache(Cache::None);
  paper.begin(3);
  drawGuest(replacement, 1, 0);
  drawGuest(peer, 2, 50);
  paper.end();
  EXPECT_NEAR(firstTime, 1.0 / 60.0, 1e-12);
  EXPECT_NEAR(peerTime, 3.0 / 60.0, 1e-12);
  EXPECT_EQ(paper.pen.retained().size(), 2u);
}

}  // namespace
