// The cascade: what a node leaves unset it takes from the nearest ancestor
// that set it — the font, its colour the ink, and the custom properties —
// wherever the code that built it ran; what it names it keeps. Each test
// states one promise, with its instances as named rows.

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Length.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

using sigil::compose::test::checkRows;
using sigil::compose::test::forEachRow;

using namespace sigil::weave::literals;

namespace {

/** The width a keyed leaf laid out to, which a face and a size decide. */
float widthOf(Host& host, std::string_view key) {
  return require(host.composer.bounds(key)).width();
}

/** A leaf of one word under a root that sets the font and the ink white,
 *  the size @p size, keyed "t". */
Element pageWith(Element leaf, float size) {
  return box()
      .padding(10)
      .font({.face = sigil::test::instrument::sans(), .size = size})
      .ink({1, 1, 1, 1})
      .children({std::move(leaf).key("t")});
}

/** A red-to-blue ramp across the unit square, left to right. */
material::Material redToBlue() {
  return material::linearGradient({0, 0}, {1, 0},
                                  {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
}

/** Two 40x40 boxes filled with the ink in force, side by side inside a
 *  120-wide box that states @p ink stretched over @p over. */
Element twoSwatches(Fill ink, PaintBox over) {
  const auto swatch = [](std::string key) {
    return box()
        .key(std::move(key))
        .width(40)
        .height(40)
        .fill(Fill::currentInk());
  };
  return box()
      .row()
      .width(120)
      .height(40)
      .ink(std::move(ink), over)
      .children({swatch("left"), swatch("right")});
}

}  // namespace

// A leaf that names no style takes the font and the ink in force where it
// lands, and a partial overrides only the fields it names.
TEST(ComposeCascade, ALeafIsSetInTheFontAndInkInForceWhereItLands) {
  checkRows({
      {"ALeafThatNamesNoStyleIsSetInTheNearestAncestorsFontAndInk",
       [] {
         // Through a box that says nothing: the font and the ink cross it. The
         // leaf itself is keyed, since the box around it is stretched to the
         // page and says nothing about the type inside it.
         const auto page = [](float size) {
           return box()
               .padding(10)
               .font({.face = sigil::test::instrument::sans(), .size = size})
               .ink({1, 1, 1, 1})
               .children({box().children({text(u8"AAAA").key("leaf")})});
         };
         Host big, small;
         big.composer.render(page(30));
         small.composer.render(page(15));
         big.frame();
         small.frame();
         EXPECT_GT(widthOf(big, "leaf"), widthOf(small, "leaf") * 1.5f);
         EXPECT_TRUE(anyWhiteIn(big, SkIRect::MakeXYWH(10, 10, 180, 60)));
       }},
      {"APartialFontOverlaysTheInheritedOneFieldByField",
       [] {
         // A size named on the leaf; the face and the ink still come down.
         Host host;
         host.composer.render(
             box()
                 .padding(10)
                 .font({.face = sigil::test::instrument::sans(), .size = 12})
                 .ink({1, 1, 1, 1})
                 .children({text(u8"AAAA").key("base"),
                            text(u8"AAAA").font({.size = 36}).key("big")}));
         host.frame();
         EXPECT_GT(widthOf(host, "big"), widthOf(host, "base") * 2.0f);
         EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 100)));
       }},
  });
}

// Resolution follows the retained tree, not the code that built the node.
TEST(ComposeCascade, ANodeInheritsFromWhereItLandsNotWhereItWasBuilt) {
  checkRows({
      {"AChildBuiltBeforeItWasAdoptedIsSetInTheAdoptersInk",
       [] {
         // Built first, with no scope around it, then handed to the box that
         // sets the ink: the leaf is white because of where it LANDED.
         Element content = box().children({text(u8"AAAA")});
         Host host;
         host.composer.render(pageWith(std::move(content), 24));
         host.frame();
         EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 60)));
       }},
      {"ASlotInheritsFromWhereItStands",
       [] {
         // The slot's content is described elsewhere and later; it is set in
         // the font and ink of the slot's ancestors all the same.
         Host host;
         host.composer.render(
             box()
                 .padding(10)
                 .font({.face = sigil::test::instrument::sans(), .size = 28})
                 .ink({1, 1, 1, 1})
                 .children({slot("hud")}));
         host.composer.renderSlot("hud", text(u8"AAAA").key("t"));
         host.frame();
         EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 60)));
         EXPECT_GT(widthOf(host, "t"), 40.0f);
       }},
      {"AClassResolvesWhereTheTextIsWrittenAgainstTheSheetInScope",
       [] {
         // The sheet is bound around the code that builds the leaf and nowhere
         // near the tree the leaf ends up in: the class still took, and its
         // fields inherit the face and the ink from the tree.
         sigil::compose::StyleSheet sheet{
             sigil::compose::rule(".big").font(sigil::weave::Type{.size = 40})};
         Element classed =
             text(u8"AAAA").applyStyleSheet(sheet).styleClass("big");
         Host with, without;
         with.composer.render(pageWith(std::move(classed), 10));
         without.composer.render(pageWith(text(u8"AAAA"), 10));
         with.frame();
         without.frame();
         EXPECT_GT(widthOf(with, "t"), widthOf(without, "t") * 2.5f);
         EXPECT_TRUE(anyWhiteIn(with, SkIRect::MakeXYWH(10, 10, 180, 80)));
       }},
  });
}

// A leaf set in a whole style is set in it alone, and an ink stated on the
// leaf paints over the style's colour whichever its kind.
TEST(ComposeCascade, AWholeStyleInheritsNothingButTheInkStatedOnItsLeaf) {
  checkRows({
      {"ATotalStyleInheritsNothing",
       [] {
         // A leaf given a TextStyle is set in it alone, whatever the ancestors
         // set.
         Host under, alone;
         under.composer.render(
             box()
                 .padding(10)
                 .font({.size = 48})
                 .children({text(u8"AAAA", whiteStyle(12)).key("t")}));
         alone.composer.render(box().padding(10).children(
             {text(u8"AAAA", whiteStyle(12)).key("t")}));
         under.frame();
         alone.frame();
         EXPECT_FLOAT_EQ(widthOf(under, "t"), widthOf(alone, "t"));
       }},
      {"ATotalStyleTakesTheInkStatedOnItselfWhicheverItsKind",
       [] {
         // A clear copy of a leaf set in a whole style is written the same way
         // whatever its ink is: a clear colour draws nothing, as a clear paint
         // does. A colour stated above it does not reach it; a paint in force
         // paints any passage.
         const auto drawsWhite = [](Element page) {
           Host host;
           host.composer.render(std::move(page));
           host.frame();
           return anyWhiteIn(host, SkIRect::MakeXYWH(0, 0, 200, 80));
         };
         const auto leaf = [] {
           return text(u8"AAAA", whiteStyle(36)).key("t");
         };
         EXPECT_TRUE(drawsWhite(box().padding(10).children({leaf()})));
         EXPECT_FALSE(drawsWhite(
             box().padding(10).children({leaf().ink(SkColor4f{0, 0, 0, 0})})));
         EXPECT_FALSE(drawsWhite(
             box().padding(10).children({leaf().ink(material::skia::base(
                 material::Paint::solid(material::Color{0, 0, 0, 0})))})));
         EXPECT_TRUE(drawsWhite(
             box().padding(10).ink(SkColor4f{0, 0, 0, 0}).children({leaf()})));
         EXPECT_FALSE(
             drawsWhite(box()
                            .padding(10)
                            .ink(material::skia::base(material::Paint::solid(
                                material::Color{0, 0, 0, 0})))
                            .children({leaf()})));
         // The stated colour moves the paragraph it is set in.
         Host host;
         const auto page = [&](SkColor4f ink) {
           return box().padding(10).children({leaf().ink(ink)});
         };
         host.composer.render(page({0, 0, 0, 0}));
         host.frame();
         EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeXYWH(0, 0, 200, 80)));
         host.composer.render(page({1, 1, 1, 1}));
         host.frame();
         EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(0, 0, 200, 80)));
       }},
      {"APaintHoldingOneColourAndAPlainColourBothInkAWholeStyle",
       [] {
         // A plain colour is the lane that eases and a paint that happens to be
         // flat is a paint, but stated on a leaf set in a whole style, each
         // paints its glyphs over the style's own colour.
         Host host;
         host.composer.render(box().padding(10).children(
             {text(u8"HH", whiteStyle(48))
                  .ink(material::skia::base(
                      material::Paint::solid({1, 0, 0, 1})))}));
         host.frame();
         EXPECT_GT(redInk(host), 40);
         Host plain;
         plain.composer.render(box().padding(10).children(
             {text(u8"HH", whiteStyle(48)).ink(material::Color{1, 0, 0, 1})}));
         plain.frame();
         EXPECT_GT(redInk(plain), 40);
       }},
  });
}

TEST(ComposeCascade,
     AFaceStatedAsTheDefaultFamilyReturnsToItUnderAFacedAncestor) {
  // The instrument sans sets wider than the context's default family at
  // one size; a leaf that states the default family under it is set as a
  // leaf under no face is, and a leaf that says nothing keeps the sans.
  Host faced, reset, bare;
  faced.composer.render(
      box()
          .padding(10)
          .font({.face = sigil::test::instrument::sans(), .size = 24})
          .children({text(u8"AAAA").key("t")}));
  reset.composer.render(
      box()
          .padding(10)
          .font({.face = sigil::test::instrument::sans(), .size = 24})
          .children({text(u8"AAAA")
                         .font({.face = sigil::weave::defaultFace()})
                         .key("t")}));
  bare.composer.render(
      box().padding(10).font({.size = 24}).children({text(u8"AAAA").key("t")}));
  faced.frame();
  reset.frame();
  bare.frame();
  EXPECT_FLOAT_EQ(widthOf(reset, "t"), widthOf(bare, "t"));
  EXPECT_NE(widthOf(faced, "t"), widthOf(bare, "t"));
}

TEST(ComposeCascade, AFillNamingTheCurrentInkFollowsARecolouredAncestor) {
  // The leaf is described identically both times and prunes; only the
  // ancestor's ink moved, and the fill written as the ink follows it.
  Host host;
  const auto page = [](SkColor4f ink) {
    return box().key("p").ink(ink).children(
        {box().key("c").width(60).height(60).fill(Fill::currentInk())});
  };
  host.composer.render(page({1, 0, 0, 1}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 255, 0, 0));
  host.composer.render(page({0, 0, 1, 1}));
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);  // the ancestor alone
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 0, 0, 255));
}

TEST(ComposeCascade, TheRootInheritsWhatTheComposerWasTold) {
  // setInherited() is what a leaf under nothing is set in: a guest or a
  // texture scene seeds it from where it stands.
  Host host;
  host.composer.setInherited(
      sigil::weave::Type{.face = sigil::test::instrument::sans(), .size = 30},
      {1, 1, 1, 1});
  host.composer.render(box().padding(10).children({text(u8"AAAA").key("t")}));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 60)));
  EXPECT_GT(widthOf(host, "t"), 40.0f);
}

// A bake resolves against the initial values, and the cascade runs inside it
// as it does in a frame.
TEST(ComposeCascade, ABakeIsARootAndRunsTheCascadeOverItsOwnTree) {
  checkRows({
      {"ABakeIsARoot",
       [] {
         // A tree baked through snapshot() has no ancestors: a fill written as
         // the ink resolves against the initial values, black, whatever scope
         // the snapshot was taken in.
         const sk_sp<SkPicture> picture = snapshot(
             box().width(20).height(20).fill(Fill::currentInk()), fonts());
         ASSERT_TRUE(picture);
         sk_sp<SkSurface> surface =
             SkSurfaces::Raster(SkImageInfo::MakeN32Premul(20, 20));
         surface->getCanvas()->clear(SK_ColorWHITE);
         surface->getCanvas()->drawPicture(picture);
         SkBitmap bm;
         bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
         surface->readPixels(bm.pixmap(), 10, 10);
         EXPECT_EQ(bm.getColor(0, 0), SK_ColorBLACK);
       }},
      {"ABakeSetsAnInheritingLeafInTheFontOfItsRoot",
       [] {
         // The cascade runs inside a bake as it does in a frame: a leaf that
         // names no style is measured and drawn in the font its ancestors
         // WITHIN the bake set, and comes out exactly as the leaf given that
         // font whole.
         const sk_sp<SkTypeface> face = sigil::test::instrument::sans();
         const SkSize inheriting = intrinsicSize(
             box().font({.face = face, .size = 40}).children({text(u8"AAAA")}),
             fonts());
         const SkSize whole = intrinsicSize(
             box().children({text(u8"AAAA", sigil::weave::textStyle(
                                                {.face = face, .size = 40}))}),
             fonts());
         const SkSize initial =
             intrinsicSize(box().children({text(u8"AAAA")}), fonts());
         EXPECT_FLOAT_EQ(inheriting.width(), whole.width());
         EXPECT_FLOAT_EQ(inheriting.height(), whole.height());
         EXPECT_GT(inheriting.width(), initial.width() * 1.5f)
             << "not the initial 16 px the root would give a leaf under "
                "nothing";
         const sk_sp<SkPicture> picture = snapshot(
             box().font({.face = face, .size = 40}).children({text(u8"AAAA")}),
             fonts());
         ASSERT_TRUE(picture);
         EXPECT_FLOAT_EQ(picture->cullRect().width(), whole.width());
       }},
  });
}

// A span's or a run's partial keeps what the range inherits in every field it
// does not name.
TEST(ComposeCascade, ARangesPartialIsLaidOverTheStyleTheRangeIsSetIn) {
  checkRows({
      {"APartialSpanStyleIsLaidOverTheStyleTheRangeIsSetIn",
       [] {
         // The words a partial covers keep the face and the ink the leaf
         // inherits and take the one field the partial names; a partial naming
         // only a colour repaints them without moving a glyph.
         const auto page = [](Element leaf) {
           return box()
               .padding(10)
               .font({.face = sigil::test::instrument::sans(), .size = 12})
               .ink({1, 1, 1, 1})
               .children({std::move(leaf).key("t")});
         };
         const auto second = [] {
           return sigil::weave::selectors::text(u8"BBBB");
         };
         Host plain, sized, tinted;
         plain.composer.render(page(text(u8"AAAA BBBB")));
         sized.composer.render(page(
             text(u8"AAAA BBBB").span(second(), SpanStyle().fontSize(36))));
         tinted.composer.render(
             page(text(u8"AAAA BBBB")
                      .span(second(), SpanStyle().ink(SkColor4f{1, 0, 0, 1}))));
         plain.frame();
         sized.frame();
         tinted.frame();
         EXPECT_GT(widthOf(sized, "t"), widthOf(plain, "t") * 1.5f);
         EXPECT_FLOAT_EQ(widthOf(tinted, "t"), widthOf(plain, "t"))
             << "a colour alone is a repaint: nothing re-shaped";
         EXPECT_TRUE(anyWhiteIn(sized, SkIRect::MakeXYWH(10, 10, 40, 60)))
             << "the first word is still set in the inherited ink";
         bool red = false;
         for (int y = 10; y < 40 && !red; ++y)
           for (int x = 10; x < 190 && !red; ++x)
             red = tinted.pixel(x, y) == SkColorSetARGB(255, 255, 0, 0);
         EXPECT_TRUE(red) << "the second word took the partial's colour";
       }},
      {"AnInheritingRichTextsPartialRunKeepsTheInheritedFaceAndSize",
       [] {
         // A run written with a partial that names only a colour is set in the
         // inherited face and size: the passage is exactly as wide as the same
         // words in one run, and the run's own colour shows.
         Host mixed, plain;
         mixed.composer.render(pageWith(
             text(sigil::weave::rich().add(u8"AA").add(
                 u8"BB", sigil::weave::Type{.ink = SkColor4f{1, 0, 0, 1}})),
             24));
         plain.composer.render(pageWith(text(u8"AABB"), 24));
         mixed.frame();
         plain.frame();
         EXPECT_FLOAT_EQ(widthOf(mixed, "t"), widthOf(plain, "t"));
         bool red = false;
         for (int y = 10; y < 60 && !red; ++y)
           for (int x = 10; x < 190 && !red; ++x) {
             const SkColor c = mixed.pixel(x, y);
             red = SkColorGetR(c) > 200 && SkColorGetG(c) < 60 &&
                   SkColorGetB(c) < 60;
           }
         EXPECT_TRUE(red);
       }},
  });
}

// A custom property, set by a verb or by a rule, is read by anything under
// the node, the nearest ancestor that set it winning.
TEST(ComposeCascade, ACustomPropertyIsReadUnderItTheNearestSetterWinning) {
  checkRows({
      {"ACustomPropertyIsReadByAnyDescendantThatNamesIt",
       [] {
         // A colour and a length set on the root, read two levels down: the
         // padding measures the gutter, the fill takes the accent.
         Host host;
         host.composer.render(
             box()
                 .var("accent", SkColor4f{0, 0, 1, 1})
                 .var("gutter", Dimension(40.0f))
                 .children(
                     {box()
                          .padding(var("gutter"))
                          .children({box().key("in").width(20).height(20).fill(
                              var("accent"))})}));
         host.frame();
         const SkRect in = require(host.composer.bounds("in"));
         EXPECT_FLOAT_EQ(in.left(), 40.0f);
         EXPECT_FLOAT_EQ(in.top(), 40.0f);
         EXPECT_EQ(host.pixel(50, 50), SkColorSetARGB(255, 0, 0, 255));
       }},
      {"ANearerAncestorsPropertyWinsOverAFartherOnes",
       [] {
         Host host;
         host.composer.render(
             box()
                 .var("accent", SkColor4f{1, 0, 0, 1})
                 .children({box()
                                .var("accent", SkColor4f{0, 1, 0, 1})
                                .children({box().width(40).height(40).fill(
                                    var("accent"))})}));
         host.frame();
         EXPECT_EQ(host.pixel(20, 20), SkColorSetARGB(255, 0, 255, 0));
       }},
      {"ARuleSetsACustomPropertyEveryMatchedSubtreeReads",
       [] {
         // A rule sets the property on every `.themed` node, and a second rule
         // reads it as the ink of the swatches under one: the swatch outside
         // any `.themed` node keeps the white ink of the root.
         const auto swatch = [] {
           return box()
               .width(20)
               .height(20)
               .fill(Fill::currentInk())
               .styleClass("swatch");
         };
         Host host;
         host.composer.render(
             box()
                 .ink(SkColor4f{1, 1, 1, 1})
                 .applyStyleSheet(sigil::compose::StyleSheet{
                     sigil::compose::rule(".themed").var("accent",
                                                         SkColor4f{1, 0, 0, 1}),
                     sigil::compose::rule(".themed .swatch")
                         .ink(sigil::compose::var("accent"))})
                 .children({box().styleClass("themed").children({swatch()}),
                            box().children({swatch()})}));
         host.frame();
         EXPECT_EQ(host.pixel(10, 10), SkColorSetARGB(255, 255, 0, 0));
         EXPECT_EQ(host.pixel(10, 30), SkColorSetARGB(255, 255, 255, 255));
       }},
  });
}

// A font change above a node moves every length measured in it, with nothing
// below re-described.
TEST(ComposeCascade, AnAncestorsFontChangeRewritesTheLengthsMeasuredInIt) {
  checkRows({
      {"AChangedAncestorFontRelaysOutTheLengthsMeasuredInIt",
       [] {
         // The same tree described again with a larger font: the em padding
         // under it is rewritten, and the layout moves without any leaf
         // changing.
         Host host;
         const auto page = [](float size) {
           return box()
               .key("root")
               .font({.size = size})
               .children({box().padding(1_em).children(
                   {box().key("in").width(10).height(10).fill(green())})});
         };
         host.composer.render(page(10));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("in")).left(), 10.0f);
         host.composer.render(page(30));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("in")).left(), 30.0f);
       }},
      {"AnOriginInEmsFollowsTheFontItIsMeasuredIn",
       [] {
         // The pivot is a length of the node like any other, read at paint: two
         // ems is 20 px under a 10 px font and 60 under 30. The box collapses
         // to a sliver across its pivot, so the sliver's column is the pivot's,
         // and a font that changes above the node moves it with nothing else
         // said.
         Host host;
         const auto page = [](float size) {
           return box()
               .font({.size = size})
               .children({box()
                              .absolute()
                              .rect(0, 0, 100, 20)
                              .fill(red())
                              .transformOrigin(2_em, percent(0))
                              .scaleX(0.04f)});
         };
         host.composer.render(page(10));
         host.frame();
         EXPECT_EQ(host.pixel(20, 10), SK_ColorRED);
         EXPECT_NE(host.pixel(60, 10), SK_ColorRED);
         host.composer.render(page(30));
         host.frame();
         EXPECT_NE(host.pixel(20, 10), SK_ColorRED);
         EXPECT_EQ(host.pixel(60, 10), SK_ColorRED);
       }},
  });
}

// An ink change eases under the transition in force however it arrives, and
// every lane of one describe runs on one clock.
TEST(ComposeCascade, AnInkChangeEasesUnderTheTransitionInForce) {
  checkRows({
      {"AnInkTransitionEasesEverythingUnderTheNodeAndSettles",
       [] {
         // The node that declares the ink eases it; the fill under it, written
         // as the ink, is mixed while the ramp runs and lands when it settles.
         // The ramp starts at the DESCRIBE, where the cascade pass resolves the
         // new target, so the first tick after it already moves the colour.
         Host host;
         const auto page = [](SkColor4f ink) {
           return box().key("p").ink(ink).transition(200ms).children(
               {box().width(60).height(60).fill(Fill::currentInk())});
         };
         host.composer.render(page({1, 0, 0, 1}));
         host.frame();
         host.composer.render(page({0, 0, 1, 1}));
         host.frame(0.1);
         const SkColor mid = host.pixel(30, 30);
         EXPECT_GT(SkColorGetR(mid), 0u);
         EXPECT_LT(SkColorGetR(mid), 255u);
         EXPECT_GT(SkColorGetB(mid), 0u);
         host.frame(0.3);
         EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 0, 0, 255));
         host.frame(0.1);
         EXPECT_FALSE(host.composer.isRunning());
       }},
      {"AnInkChangedThroughAClassEasesAsTheVerbsChangeDoes",
       [] {
         // THE POINT OF WATCHING THE COMPUTED VALUE. The node writes no ink of
         // its own in either description: the colour arrives through a class,
         // so nothing the node DECLARES moves, and the lane still eases — the
         // two spellings of one change cannot disagree.
         const sigil::compose::StyleSheet sheet{
             sigil::compose::rule(".hot").font(
                 sigil::weave::Type{.ink = SkColor4f{1, 0, 0, 1}}),
             sigil::compose::rule(".cold").font(
                 sigil::weave::Type{.ink = SkColor4f{0, 0, 1, 1}})};
         Host host;
         const auto page = [&](std::string_view name) {
           return box()
               .applyStyleSheet(sheet)
               .styleClass(name)
               .transition(200ms)
               .children({box().width(60).height(60).fill(Fill::currentInk())});
         };
         host.composer.render(page("hot"));
         host.frame();
         EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 255, 0, 0));
         host.composer.render(page("cold"));
         host.frame(0.1);
         const SkColor mid = host.pixel(30, 30);
         EXPECT_GT(SkColorGetR(mid), 0u)
             << "the class change eased, it did not snap";
         EXPECT_LT(SkColorGetR(mid), 255u);
         EXPECT_GT(SkColorGetB(mid), 0u);
         host.frame(0.3);
         EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 0, 0, 255));
       }},
      {"AnInheritedInkEasesUnderTheNodesOwnTransition",
       [] {
         // ONE RULE FOR EVERY INHERITED PROPERTY: a node stating a transition
         // of its own eases what it inherits with a lane of its own, toward the
         // target the ancestor is headed for; a node with none follows the
         // ancestor's ramp as it runs. The ancestor here eases over 100 ms. The
         // box on the left has no transition and lands with it; the one on the
         // right states 400 ms and is still on its way when the ancestor has
         // landed.
         Host host;
         const auto page = [](SkColor4f ink) {
           return box().ink(ink).transition(100ms).children(
               {box().row().children({
                   box().width(60).height(60).fill(Fill::currentInk()),
                   box().transition(400ms).children(
                       {box().width(60).height(60).fill(Fill::currentInk())}),
               })});
         };
         host.composer.render(page({1, 0, 0, 1}));
         host.frame();
         ASSERT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 255, 0, 0));
         ASSERT_EQ(host.pixel(90, 30), SkColorSetARGB(255, 255, 0, 0));
         host.composer.render(page({0, 0, 1, 1}));
         host.frame();
         EXPECT_EQ(host.pixel(90, 30), SkColorSetARGB(255, 255, 0, 0))
             << "the lane has begun and stands at the colour it begins at";
         host.frame(0.05);
         const SkColor following = host.pixel(30, 30);
         const SkColor owning = host.pixel(90, 30);
         EXPECT_GT(SkColorGetR(following), 0u)
             << "no transition of its own: it stands where the ancestor's ramp "
                "stands, part of the way";
         EXPECT_LT(SkColorGetR(following), 255u);
         EXPECT_GT(SkColorGetB(following), 0u);
         EXPECT_LT(SkColorGetB(owning), SkColorGetB(following))
             << "its own, longer lane is behind the ancestor's ramp";
         host.frame(0.1);
         EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 0, 0, 255))
             << "no transition of its own: it landed with the ancestor";
         const SkColor mid = host.pixel(90, 30);
         EXPECT_GT(SkColorGetR(mid), 0u) << "its own lane is still easing";
         EXPECT_LT(SkColorGetR(mid), 255u);
         EXPECT_GT(SkColorGetB(mid), 0u);
         host.frame(0.35);
         EXPECT_EQ(host.pixel(90, 30), SkColorSetARGB(255, 0, 0, 255))
             << "settled on the target the ancestor is headed for";
         host.frame(0.1);
         EXPECT_FALSE(host.composer.isRunning());
       }},
      {"AnInkReadFromACustomPropertyEasesWhenThePropertyMoves",
       [] {
         // The custom property reaches the resolved colour by its own line of
         // the fold, beside the class and the verb, and it is the spelling
         // furthest from the node: the node reading the property writes the
         // same two verbs in both frames, so nothing of its own moved and only
         // the value above it did.
         Host host;
         const auto page = [](material::Color accent) {
           return box()
               .var("accent", accent)
               .children({box()
                              .ink(var("accent"))
                              .transition(200ms)
                              .children({box().width(60).height(60).fill(
                                  Fill::currentInk())})});
         };
         host.composer.render(page({1, 0, 0, 1}));
         host.frame();
         ASSERT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 255, 0, 0));
         host.composer.render(page({0, 0, 1, 1}));
         host.frame(0.1);
         const SkColor mid = host.pixel(30, 30);
         EXPECT_GT(SkColorGetR(mid), 0u)
             << "the property's new value eased, it did not snap";
         EXPECT_LT(SkColorGetR(mid), 255u);
         EXPECT_GT(SkColorGetB(mid), 0u);
         host.frame(0.15);
         EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 0, 0, 255));
       }},
      {"AnInkAndAFillOfOneDurationOnOneNodeRunOnOneClock",
       [] {
         // The ink lane is started by the cascade pass and the fill lane by the
         // patch. Both belong to the DESCRIBE, so the clock they start on is
         // one clock: this host advances its own between the describe and the
         // draw, which would show a lane started at the later moment standing
         // still while its neighbour was three quarters along. At every frame
         // of the ramp the box painted in the fill and the box painted in the
         // ink are one colour, and they settle together.
         Host host;
         const auto page = [](material::Color colour) {
           return box()
               .width(120)
               .height(60)
               .transition(200ms)
               .ink(colour)
               .fill(Fill::color(colour))
               .children({box().width(30).height(30).fill(Fill::currentInk())});
         };
         host.composer.render(page({1, 0, 0, 1}));
         host.frame();
         ASSERT_EQ(host.pixel(10, 10), SK_ColorRED) << "the ink box";
         ASSERT_EQ(host.pixel(100, 50), SK_ColorRED) << "the fill around it";
         host.composer.render(page({0, 0, 1, 1}));
         host.frame(0.1);
         const SkColor ink = host.pixel(10, 10);
         EXPECT_EQ(ink, host.pixel(100, 50))
             << "one moment, one colour: the two lanes are not a tick apart";
         EXPECT_NE(ink, SK_ColorRED);
         EXPECT_NE(ink, SK_ColorBLUE);
         host.frame(0.15);
         EXPECT_EQ(host.pixel(10, 10), SK_ColorBLUE);
         EXPECT_EQ(host.pixel(100, 50), SK_ColorBLUE) << "settled on one frame";
       }},
  });
}

// Classes fold in the sheet's order under the node's own partial, a nearer
// sheet stands over a farther one by name, and a class no sheet carries sets
// nothing.
TEST(ComposeCascade, AClassResolvesThroughTheSheetsInForce) {
  checkRows({
      {"ClassesFoldInTheSheetsOrderAndAPartialLaysOverThem",
       [] {
         // Between classes the sheet's order decides, so "dim big" and "big
         // dim" are one thing: the later entry of the sheet in force wins. A
         // partial after the names is the node's own and stands over both.
         const sigil::compose::StyleSheet bigLast{
             sigil::compose::rule(".dim").font({.size = 14}),
             sigil::compose::rule(".big").font({.size = 40})};
         const sigil::compose::StyleSheet dimLast{
             sigil::compose::rule(".big").font({.size = 40}),
             sigil::compose::rule(".dim").font({.size = 14})};
         Host a, b, c, d;
         a.composer.render(pageWith(
             text("AAAA").applyStyleSheet(bigLast).styleClass("dim big"), 10));
         b.composer.render(pageWith(
             text("AAAA").applyStyleSheet(bigLast).styleClass("big dim"), 10));
         c.composer.render(pageWith(
             text("AAAA").applyStyleSheet(dimLast).styleClass("dim big"), 10));
         d.composer.render(pageWith(
             text("AAAA").applyStyleSheet(bigLast).styleClass("big").font(
                 {.size = 14}),
             10));
         a.frame();
         b.frame();
         c.frame();
         d.frame();
         EXPECT_FLOAT_EQ(widthOf(a, "t"), widthOf(b, "t"));
         EXPECT_GT(widthOf(a, "t"), widthOf(c, "t") * 2.5f);
         EXPECT_FLOAT_EQ(widthOf(d, "t"), widthOf(c, "t"));
       }},
      {"ANearerSheetStandsOverAFartherOneByName",
       [] {
         // A subtree states a sheet of its own: the names it carries win there,
         // the names it leaves alone still resolve through the outer sheet.
         const sigil::compose::StyleSheet outer{
             sigil::compose::rule(".big").font({.size = 40}),
             sigil::compose::rule(".dim").font({.size = 14})};
         const sigil::compose::StyleSheet inner{
             sigil::compose::rule(".big").font({.size = 14})};
         Host a, b;
         a.composer.render(
             pageWith(box().applyStyleSheet(outer).children(
                          {box().applyStyleSheet(inner).children(
                              {text("AAAA").styleClass("big").key("t")})}),
                      10));
         b.composer.render(
             pageWith(box().applyStyleSheet(outer).children(
                          {box().applyStyleSheet(inner).children(
                              {text("AAAA").styleClass("dim").key("t")})}),
                      10));
         a.frame();
         b.frame();
         EXPECT_FLOAT_EQ(widthOf(a, "t"), widthOf(b, "t"));
       }},
      {"AClassNoSheetCarriesSetsNothing",
       [] {
         Host with, without;
         with.composer.render(pageWith(text(u8"AAAA").styleClass("nope"), 14));
         without.composer.render(pageWith(text(u8"AAAA"), 14));
         with.frame();
         without.frame();
         EXPECT_FLOAT_EQ(widthOf(with, "t"), widthOf(without, "t"));
       }},
  });
}

// The ink holds a paint as a fill does, stretched over the box it names.
TEST(ComposeCascade, AnInkPaintIsStretchedOverTheBoxItNames) {
  checkRows({
      {"AnInkPaintPaintsEveryMarkUnderItThatNamesNoColour",
       [] {
         // The ink takes what a fill takes; a fill written as the ink in force
         // resolves to the paint rather than to a colour.
         Host host;
         host.composer.render(twoSwatches(redToBlue(), PaintBox::Element));
         host.frame();
         // The element's own box: each swatch shows the WHOLE ramp, so both run
         // red to blue.
         EXPECT_GT(SkColorGetR(host.pixel(2, 20)), 200u);
         EXPECT_GT(SkColorGetB(host.pixel(38, 20)), 200u);
         EXPECT_GT(SkColorGetR(host.pixel(42, 20)), 200u);
         EXPECT_GT(SkColorGetB(host.pixel(78, 20)), 200u);
       }},
      {"AnInkPaintOverTheSubtreeGivesEachMarkItsOwnSlice",
       [] {
         Host host;
         host.composer.render(twoSwatches(redToBlue(), PaintBox::Subtree));
         host.frame();
         // One ramp across the box that stated it: each swatch holds its own
         // stretch of it, so the ramp runs on THROUGH the two rather than
         // restarting, and the right swatch is bluer than the left at every
         // point.
         EXPECT_GT(SkColorGetR(host.pixel(2, 20)), 200u);
         EXPECT_GT(SkColorGetB(host.pixel(42, 20)),
                   SkColorGetB(host.pixel(2, 20)));
         EXPECT_GT(SkColorGetB(host.pixel(78, 20)),
                   SkColorGetB(host.pixel(38, 20)));
         // …which is exactly what the element's own box does not do: there the
         // second swatch starts the ramp again, so the same offset into either
         // swatch is the same colour, and redder than the one ramp is by then.
         Host own;
         own.composer.render(twoSwatches(redToBlue(), PaintBox::Element));
         own.frame();
         EXPECT_EQ(own.pixel(42, 20), own.pixel(2, 20));
         EXPECT_GT(SkColorGetR(own.pixel(42, 20)),
                   SkColorGetR(host.pixel(42, 20)));
       }},
      {"AnInkPaintOverTheCanvasIsOneFieldTheWholeTreeStandsIn",
       [] {
         // The same two swatches, moved: an ink over the canvas is a field the
         // canvas owns, so what a mark shows is decided by where it stands.
         Host host;
         const auto page = [](float left) {
           return box().children({twoSwatches(redToBlue(), PaintBox::Canvas)
                                      .absolute()
                                      .left(left)
                                      .top(0.0f)});
         };
         host.composer.render(page(0.0f));
         host.frame();
         const SkColor atOrigin = host.pixel(20, 20);
         host.composer.render(page(100.0f));
         host.frame();
         const SkColor moved = host.pixel(120, 20);
         // The 200px canvas carries the whole ramp, so a swatch 100px further
         // right shows a bluer slice of it than the same swatch at the origin.
         EXPECT_GT(SkColorGetB(moved), SkColorGetB(atOrigin) + 60u);
       }},
      {"AnEmptyInkPaintClearsAnAncestorsAndLeavesTheColour",
       [] {
         Host host;
         host.composer.render(
             box()
                 .ink({0, 1, 0, 1})
                 .ink(redToBlue())
                 .children(
                     {box()
                          .ink(Fill::none())
                          .children({box().key("c").width(40).height(40).fill(
                              Fill::currentInk())})}));
         host.frame();
         EXPECT_EQ(host.pixel(20, 20), SkColorSetARGB(255, 0, 255, 0));
       }},
  });
}

// Text is one value however it is spelled.
TEST(ComposeCascade, TextIsWrittenInAnySpellingOfUtf8) {
  checkRows({
      {"ATextLeafFromAPlainStringIsTheSameLeaf",
       [] {
         // UTF-8 held as char and as char8_t describe one leaf, alone and as a
         // rich run.
         Host plain, typed, rich;
         plain.composer.render(pageWith(text("AAAA"), 14));
         typed.composer.render(pageWith(text(u8"AAAA"), 14));
         rich.composer.render(
             pageWith(text(sigil::weave::rich().add("AAAA")), 14));
         plain.frame();
         typed.frame();
         rich.frame();
         EXPECT_FLOAT_EQ(widthOf(plain, "t"), widthOf(typed, "t"));
         EXPECT_FLOAT_EQ(widthOf(rich, "t"), widthOf(typed, "t"));
       }},
      {"AValueThatReadsItselfOutAsTextIsText",
       [] {
         // A node of a document, standing in for the one this library may not
         // name: it answers `text()` and nothing else, and it is text wherever
         // text is taken. A value that does not answer it — every string
         // spelling — keeps its own constructor, which is what keeps a literal
         // from becoming ambiguous.
         struct Node {
           std::string_view text() const { return "THE RULE AND THE STRANDS"; }
         };
         const Utf8 read = Node{};
         EXPECT_EQ(
             std::string(reinterpret_cast<const char*>(read.bytes().data()),
                         read.bytes().size()),
             "THE RULE AND THE STRANDS");
         EXPECT_TRUE(Utf8("THE RULE AND THE STRANDS") == read);
         EXPECT_TRUE(Utf8().empty());
       }},
  });
}

// A children block holds elements and the runs `each` makes, in the order
// they are written.
TEST(ComposeElement, ChildrenTakeElementsAndTheRunsEachMakesInOrder) {
  checkRows({
      {"AChildrenBlockHoldsElementsAndListsInOrder",
       [] {
         // A block of one element, a list each() made, and another element, is
         // the same tree as the children added one by one.
         const std::array<int, 2> sizes{20, 30};
         const auto leaf = [](int size, std::string key) {
           return text("A").font({.size = (float)size}).key(std::move(key));
         };
         Element block = box().column().children({
             leaf(10, "a"),
             each(sizes,
                  [&](int size, size_t i) {
                    return leaf(size, "e" + std::to_string(i));
                  }),
             leaf(40, "b"),
         });
         Element oneByOne = box().column().children(
             {leaf(10, "a"), leaf(20, "e0"), leaf(30, "e1"), leaf(40, "b")});
         Host a, b;
         a.composer.render(pageWith(std::move(block), 10));
         b.composer.render(pageWith(std::move(oneByOne), 10));
         a.frame();
         b.frame();
         for (const char* key : {"a", "e0", "e1", "b"})
           EXPECT_FLOAT_EQ(widthOf(a, key), widthOf(b, key)) << key;
         EXPECT_GT(widthOf(a, "e1"), widthOf(a, "e0"));
         EXPECT_GT(widthOf(a, "b"), widthOf(a, "e1"));
       }},
      {"ARunOverACountIsOnePerIndex",
       [] {
         // The run whose items are their own place in it says the count and
         // nothing else; the make takes the index, or nothing at all.
         std::vector<size_t> seen;
         const std::vector<Element> indexed = each(4, [&seen](size_t i) {
           seen.push_back(i);
           return text(std::to_string(i));
         });
         EXPECT_EQ(indexed.size(), 4u);
         EXPECT_EQ(seen, (std::vector<size_t>{0, 1, 2, 3}));
         EXPECT_EQ(each(3, [] { return box(); }).size(), 3u);
         EXPECT_TRUE(each(0, [] { return box(); }).empty());
       }},
      {"ARunInterleavesTheSeparatorItIsGiven",
       [] {
         // One separator before every item but the first, so a strip never ends
         // on a dot: three names make five children.
         const std::vector<std::string> names{"one", "two", "three"};
         const std::vector<Element> ruled = each(
             names, [](const std::string& n) { return text(n); },
             box().key("rule").width(1));
         EXPECT_EQ(ruled.size(), 5u);
         // A separator that is a function of what FOLLOWS it takes the
         // parameters it names, as every other run does.
         std::vector<std::string> before;
         const std::vector<Element> counted = each(
             names, [](const std::string& n) { return text(n); },
             [&before](const std::string& next, size_t at) {
               before.push_back(next + std::to_string(at));
               return box();
             });
         EXPECT_EQ(counted.size(), 5u);
         EXPECT_EQ(before, (std::vector<std::string>{"two1", "three2"}));
         // A run of one has nothing between, and an empty one nothing at all.
         EXPECT_EQ(each(
                       std::vector<std::string>{"only"},
                       [](const std::string& n) { return text(n); }, box())
                       .size(),
                   1u);
         EXPECT_TRUE(each(
                         std::vector<std::string>{},
                         [](const std::string& n) { return text(n); }, box())
                         .empty());
       }},
  });
}
