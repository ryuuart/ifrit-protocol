// The cascade: what a node leaves unset it takes from the nearest ancestor
// that set it — the font, its colour the ink, and the custom properties —
// wherever the code that built it ran; what it names it keeps. Each case
// asserts one thing the headers promise.

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/StyleSheet.h>

#include "support/CoreTestSupport.h"

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
      .child(std::move(leaf).key("t"));
}

}  // namespace

TEST(ComposeCascade, ALeafThatNamesNoStyleIsSetInTheNearestAncestorsFontAndInk) {
  // Through a box that says nothing: the font and the ink cross it. The
  // leaf itself is keyed, since the box around it is stretched to the
  // page and says nothing about the type inside it.
  const auto page = [](float size) {
    return box()
        .padding(10)
        .font({.face = sigil::test::instrument::sans(), .size = size})
        .ink({1, 1, 1, 1})
        .child(box().child(text(u8"AAAA").key("leaf")));
  };
  Host big, small;
  big.composer.render(page(30));
  small.composer.render(page(15));
  big.frame();
  small.frame();
  EXPECT_GT(widthOf(big, "leaf"), widthOf(small, "leaf") * 1.5f);
  EXPECT_TRUE(anyWhiteIn(big, SkIRect::MakeXYWH(10, 10, 180, 60)));
}

TEST(ComposeCascade, APartialFontOverlaysTheInheritedOneFieldByField) {
  // A size named on the leaf; the face and the ink still come down.
  Host host;
  host.composer.render(
      box()
          .padding(10)
          .font({.face = sigil::test::instrument::sans(), .size = 12})
          .ink({1, 1, 1, 1})
          .child(text(u8"AAAA").key("base"))
          .child(text(u8"AAAA").font({.size = 36}).key("big")));
  host.frame();
  EXPECT_GT(widthOf(host, "big"), widthOf(host, "base") * 2.0f);
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 100)));
}

TEST(ComposeCascade, AChildBuiltBeforeItWasAdoptedIsSetInTheAdoptersInk) {
  // Built first, with no scope around it, then handed to the box that
  // sets the ink: the leaf is white because of where it LANDED.
  Element content = box().child(text(u8"AAAA"));
  Host host;
  host.composer.render(pageWith(std::move(content), 24));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 60)));
}

TEST(ComposeCascade, ATotalStyleInheritsNothing) {
  // A leaf given a TextStyle is set in it alone, whatever the ancestors set.
  Host under, alone;
  under.composer.render(box()
                            .padding(10)
                            .font({.size = 48})
                            .child(text(u8"AAAA", whiteStyle(12)).key("t")));
  alone.composer.render(
      box().padding(10).child(text(u8"AAAA", whiteStyle(12)).key("t")));
  under.frame();
  alone.frame();
  EXPECT_FLOAT_EQ(widthOf(under, "t"), widthOf(alone, "t"));
}

TEST(ComposeCascade, AFillNamingTheCurrentInkFollowsARecolouredAncestor) {
  // The leaf is described identically both times and prunes; only the
  // ancestor's ink moved, and the fill written as the ink follows it.
  Host host;
  const auto page = [](SkColor4f ink) {
    return box().key("p").ink(ink).child(
        box().key("c").width(60).height(60).fill(Fill::currentInk()));
  };
  host.composer.render(page({1, 0, 0, 1}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 255, 0, 0));
  host.composer.render(page({0, 0, 1, 1}));
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);  // the ancestor alone
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SkColorSetARGB(255, 0, 0, 255));
}

TEST(ComposeCascade, ACustomPropertyIsReadByAnyDescendantThatNamesIt) {
  // A colour and a length set on the root, read two levels down: the
  // padding measures the gutter, the fill takes the accent.
  Host host;
  host.composer.render(
      box()
          .var("accent", SkColor4f{0, 0, 1, 1})
          .var("gutter", Dimension(40.0f))
          .child(box().padding(var("gutter")).child(
              box().key("in").width(20).height(20).fill(Fill::var("accent")))));
  host.frame();
  const SkRect in = require(host.composer.bounds("in"));
  EXPECT_FLOAT_EQ(in.left(), 40.0f);
  EXPECT_FLOAT_EQ(in.top(), 40.0f);
  EXPECT_EQ(host.pixel(50, 50), SkColorSetARGB(255, 0, 0, 255));
}

TEST(ComposeCascade, ANearerAncestorsPropertyWinsOverAFartherOnes) {
  Host host;
  host.composer.render(
      box()
          .var("accent", SkColor4f{1, 0, 0, 1})
          .child(box()
                     .var("accent", SkColor4f{0, 1, 0, 1})
                     .child(box().width(40).height(40).fill(
                         Fill::var("accent")))));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SkColorSetARGB(255, 0, 255, 0));
}

TEST(ComposeCascade, AnEmLengthResolvesAgainstTheNodesOwnFontSize) {
  // 1em of padding under a 20 px font is 20 px; under 40 px it is 40.
  Host twenty, forty;
  const auto page = [](float size) {
    return box().font({.size = size}).child(
        box().padding(1_em).child(
            box().key("in").width(10).height(10).fill(green())));
  };
  twenty.composer.render(page(20));
  forty.composer.render(page(40));
  twenty.frame();
  forty.frame();
  EXPECT_FLOAT_EQ(require(twenty.composer.bounds("in")).left(), 20.0f);
  EXPECT_FLOAT_EQ(require(forty.composer.bounds("in")).left(), 40.0f);
}

TEST(ComposeCascade, AnEmFontSizeResolvesAgainstTheParentsSize) {
  // A leaf at 2em under a 12 px font is set at 24 px: as wide as a leaf
  // given 24 px outright.
  Host relative, absolute;
  relative.composer.render(pageWith(text(u8"AAAA").font({.size = 2_em}), 12));
  absolute.composer.render(pageWith(text(u8"AAAA").font({.size = 24}), 12));
  relative.frame();
  absolute.frame();
  EXPECT_FLOAT_EQ(widthOf(relative, "t"), widthOf(absolute, "t"));
}

TEST(ComposeCascade, AChangedAncestorFontRelaysOutTheLengthsMeasuredInIt) {
  // The same tree described again with a larger font: the em padding
  // under it is rewritten, and the layout moves without any leaf changing.
  Host host;
  const auto page = [](float size) {
    return box().key("root").font({.size = size}).child(
        box().padding(1_em).child(
            box().key("in").width(10).height(10).fill(green())));
  };
  host.composer.render(page(10));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("in")).left(), 10.0f);
  host.composer.render(page(30));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("in")).left(), 30.0f);
}

TEST(ComposeCascade, ASlotInheritsFromWhereItStands) {
  // The slot's content is described elsewhere and later; it is set in the
  // font and ink of the slot's ancestors all the same.
  Host host;
  host.composer.render(box()
                           .padding(10)
                           .font({.face = sigil::test::instrument::sans(),
                                  .size = 28})
                           .ink({1, 1, 1, 1})
                           .child(slot("hud")));
  host.composer.renderSlot("hud", text(u8"AAAA").key("t"));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 60)));
  EXPECT_GT(widthOf(host, "t"), 40.0f);
}

TEST(ComposeCascade, ABakeIsARoot) {
  // A tree baked through snapshot() has no ancestors: a fill written as
  // the ink resolves against the initial values, black, whatever scope the
  // snapshot was taken in.
  const sk_sp<SkPicture> picture =
      snapshot(box().width(20).height(20).fill(Fill::currentInk()), fonts());
  ASSERT_TRUE(picture);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(20, 20));
  surface->getCanvas()->clear(SK_ColorWHITE);
  surface->getCanvas()->drawPicture(picture);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surface->readPixels(bm.pixmap(), 10, 10);
  EXPECT_EQ(bm.getColor(0, 0), SK_ColorBLACK);
}

TEST(ComposeCascade, AnInkTransitionEasesEverythingUnderTheNodeAndSettles) {
  // The node that declares the ink eases it; the fill under it, written as
  // the ink, is mixed while the ramp runs and lands when it settles.
  Host host;
  const auto page = [](SkColor4f ink) {
    return box()
        .key("p")
        .ink(ink)
        .transition({.duration = 200ms})
        .child(box().width(60).height(60).fill(Fill::currentInk()));
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
  EXPECT_FALSE(host.composer.active());
}

TEST(ComposeCascade, AClassResolvesWhereTheTextIsWrittenAgainstTheSheetInScope) {
  // The sheet is bound around the code that builds the leaf and nowhere
  // near the tree the leaf ends up in: the class still took, and its
  // fields inherit the face and the ink from the tree.
  sigil::weave::StyleSheet sheet;
  sheet.set("big", sigil::weave::Type{.size = 40});
  Element classed;
  {
    const core::environment::Provide<sigil::weave::StyleSheet> scope(sheet);
    classed = text(u8"AAAA").styleClass("big");
  }
  Host with, without;
  with.composer.render(pageWith(std::move(classed), 10));
  without.composer.render(pageWith(text(u8"AAAA"), 10));
  with.frame();
  without.frame();
  EXPECT_GT(widthOf(with, "t"), widthOf(without, "t") * 2.5f);
  EXPECT_TRUE(anyWhiteIn(with, SkIRect::MakeXYWH(10, 10, 180, 80)));
}

TEST(ComposeCascade, AClassNoSheetCarriesSetsNothing) {
  Host with, without;
  with.composer.render(pageWith(text(u8"AAAA").styleClass("nope"), 14));
  without.composer.render(pageWith(text(u8"AAAA"), 14));
  with.frame();
  without.frame();
  EXPECT_FLOAT_EQ(widthOf(with, "t"), widthOf(without, "t"));
}

TEST(ComposeCascade, AnInheritingRichTextsPartialRunKeepsTheInheritedFaceAndSize) {
  // A run written with a partial that names only a colour is set in the
  // inherited face and size: the passage is exactly as wide as the same
  // words in one run, and the run's own colour shows.
  Host mixed, plain;
  mixed.composer.render(pageWith(
      text(sigil::weave::rich().add(u8"AA").add(
          u8"BB", sigil::weave::Type{.color = SkColor4f{1, 0, 0, 1}})),
      24));
  plain.composer.render(pageWith(text(u8"AABB"), 24));
  mixed.frame();
  plain.frame();
  EXPECT_FLOAT_EQ(widthOf(mixed, "t"), widthOf(plain, "t"));
  bool red = false;
  for (int y = 10; y < 60 && !red; ++y)
    for (int x = 10; x < 190 && !red; ++x) {
      const SkColor c = mixed.pixel(x, y);
      red = SkColorGetR(c) > 200 && SkColorGetG(c) < 60 && SkColorGetB(c) < 60;
    }
  EXPECT_TRUE(red);
}

TEST(ComposeCascade, TheRootInheritsWhatTheComposerWasTold) {
  // setInherited() is what a leaf under nothing is set in: a guest or a
  // texture scene seeds it from where it stands.
  Host host;
  host.composer.setInherited(
      sigil::weave::Type{.face = sigil::test::instrument::sans(), .size = 30},
      {1, 1, 1, 1});
  host.composer.render(box().padding(10).child(text(u8"AAAA").key("t")));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 60)));
  EXPECT_GT(widthOf(host, "t"), 40.0f);
}
