// The block in force: what a text leaf's blocks are set in when the leaf
// says nothing about them — leading, alignment, writing mode and the rest
// a passage inherits — flowing down the tree beside the font and the ink.
// Each case asserts one thing the headers promise.

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/layout/StyleSheet.h>

#include <array>
#include <string_view>

#include "support/CoreTestSupport.h"

namespace {

using sigil::weave::Block;
using sigil::weave::Leading;
using sigil::weave::TextAlignment;

/** A short passage that breaks into three lines at the measure below. */
constexpr const char8_t* kLines = u8"AAAA AAAA AAAA AAAA AAAA AAAA AAAA";

/** The keyed leaf's box, laid out and painted. */
SkRect boxOf(Host& host, std::string_view key) {
  host.frame();
  return require(host.composer.bounds(key));
}

/** The leftmost column with white ink inside @p rect, or -1. */
int inkStartIn(Host& host, SkRect rect) {
  for (int x = (int)rect.left(); x < (int)rect.right(); ++x)
    for (int y = (int)rect.top(); y < (int)rect.bottom(); ++y)
      if (host.pixel(x, y) == SkColorSetARGB(255, 255, 255, 255)) return x;
  return -1;
}

Element leaf() {
  return text(kLines)
      .font({.face = sigil::test::instrument::sans(), .size = 12})
      .ink({1, 1, 1, 1})
      .width(Dimension(120.0f))
      .key("t");
}

}  // namespace

TEST(ComposeBlockLane, ALeafIsSetInTheLeadingInForce) {
  // Double leading on an ancestor: the same three lines take about twice
  // the height, through a box that says nothing.
  Host plain, open;
  plain.composer.render(box().padding(10).children({box().children({leaf()})}));
  Block wide;
  wide.leading = Leading::multiple(2.0f);
  open.composer.render(
      box().padding(10).block(wide).children({box().children({leaf()})}));
  const float single = boxOf(plain, "t").height();
  const float doubled = boxOf(open, "t").height();
  EXPECT_GT(doubled, single * 1.6f);
  EXPECT_LT(doubled, single * 2.4f);
}

TEST(ComposeBlockLane, APartialOverlaysTheInheritedBlockFieldByField) {
  // The parent sets the leading, the child names the alignment: the
  // child's lines are centred AND double-leaded.
  Block wide;
  wide.leading = Leading::multiple(2.0f);
  Block centred;
  centred.alignment = TextAlignment::kCenter;
  Host start, both;
  start.composer.render(box().padding(10).block(wide).children({leaf()}));
  both.composer.render(box().padding(10).block(wide).children(
      {box().block(centred).children({leaf()})}));
  const SkRect a = boxOf(start, "t");
  const SkRect b = boxOf(both, "t");
  EXPECT_NEAR(a.height(), b.height(), 0.5f) << "the leading still inherits";
  EXPECT_GT(inkStartIn(both, b), inkStartIn(start, a) + 4)
      << "the last, short line is centred";
}

TEST(ComposeBlockLane, AWholeParagraphStyleInheritsNothing) {
  Block wide;
  wide.leading = Leading::multiple(2.0f);
  Host plain, whole;
  plain.composer.render(box().padding(10).children({leaf()}));
  whole.composer.render(box().padding(10).block(wide).children(
      {leaf().paragraphs({sigil::weave::ParagraphStyle{}})}));
  EXPECT_NEAR(boxOf(plain, "t").height(), boxOf(whole, "t").height(), 0.5f);
}

TEST(ComposeBlockLane, AClassCarriesBothHalves) {
  // One name in both sheets: the text half sets the size, the block half
  // the leading, and styleClass folds both.
  Block wide;
  wide.leading = Leading::multiple(2.0f);
  // One class, both halves: the type half and the block half under one
  // name.
  const sigil::weave::StyleSheet look{{"body", {.size = 24.0f}},
                                      {"body", wide}};
  Host plain, classed;
  plain.composer.render(box().padding(10).children({leaf()}));
  // The leaf states no size of its own: a node's own font stands over its
  // classes, so the size here is the class's to give.
  classed.composer.render(box().padding(10).styleSheet(look).children(
      {text(kLines)
           .font({.face = sigil::test::instrument::sans()})
           .ink({1, 1, 1, 1})
           .width(Dimension(240.0f))
           .key("t")
           .styleClass("body")}));
  const SkRect a = boxOf(plain, "t");
  const SkRect b = boxOf(classed, "t");
  EXPECT_GT(b.height(), a.height() * 2.5f)
      << "twice the size and twice the leading over the same three lines";
}

TEST(ComposeBlockLane, ANamedBlockIsLaidOverTheBlockInForce) {
  // paragraphs({"lead"}) under double leading: the named block takes its
  // alignment from the name and its leading from the lane.
  Block centred;
  centred.alignment = TextAlignment::kCenter;
  const sigil::weave::StyleSheet blocks{{"lead", centred}};
  Block wide;
  wide.leading = Leading::multiple(2.0f);
  Host start, named;
  start.composer.render(box().padding(10).block(wide).children({leaf()}));
  const std::array<std::string_view, 1> names{"lead"};
  named.composer.render(
      box().padding(10).block(wide).styleSheet(blocks).children(
          {leaf().paragraphs(names)}));
  const SkRect a = boxOf(start, "t");
  const SkRect b = boxOf(named, "t");
  EXPECT_NEAR(a.height(), b.height(), 0.5f);
  EXPECT_GT(inkStartIn(named, b), inkStartIn(start, a) + 4);
}

TEST(ComposeBlockLane, TheWritingModeInForceSetsALeafVertical) {
  Block vertical;
  vertical.writingMode = sigil::weave::WritingMode::kVerticalRL;
  Host across, down;
  across.composer.render(box().padding(10).children(
      {text(u8"AAAA").font({.size = 12}).ink({1, 1, 1, 1}).key("t")}));
  down.composer.render(box().padding(10).block(vertical).children(
      {text(u8"AAAA").font({.size = 12}).ink({1, 1, 1, 1}).key("t")}));
  const SkRect a = boxOf(across, "t");
  const SkRect b = boxOf(down, "t");
  EXPECT_GT(a.width(), a.height());
  EXPECT_GT(b.height(), b.width());
}

TEST(ComposeBlockLane, TheTextVerbsAreTheLanesSpellings) {
  // textAlign on a box that is not text: every leaf under it is centred,
  // through a box that says nothing — the verb is block({.alignment}).
  Host start, centred, vertical;
  start.composer.render(box().padding(10).children({box().children({leaf()})}));
  centred.composer.render(box()
                              .padding(10)
                              .block({.alignment = TextAlignment::kCenter})
                              .children({box().children({leaf()})}));
  const SkRect a = boxOf(start, "t");
  const SkRect b = boxOf(centred, "t");
  EXPECT_GT(inkStartIn(centred, b), inkStartIn(start, a) + 4);
  vertical.composer.render(
      box()
          .padding(10)
          .block({.writingMode = sigil::weave::WritingMode::kVerticalRL})
          .children(
              {text(u8"AAAA").font({.size = 12}).ink({1, 1, 1, 1}).key("t")}));
  const SkRect c = boxOf(vertical, "t");
  EXPECT_GT(c.height(), c.width());
}

TEST(ComposeBlockLane, ImageSamplingSetOnAnAncestorReachesTheImageUnderIt) {
  // A 2x2 source, half red and half green, magnified: linear invents a
  // blend band across the seam and nearest does not — whether the
  // sampling stands on the leaf or on a box above it.
  sk_sp<SkSurface> source =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  source->getCanvas()->clear(SK_ColorRED);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  source->getCanvas()->drawRect(SkRect::MakeXYWH(1, 0, 1, 2), green);
  const auto asset = std::make_shared<const sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(source->makeImageSnapshot()));
  const auto mixed = [&](Element tree) {
    Host host(200, 200);
    host.composer.render(std::move(tree));
    host.frame();
    int count = 0;
    for (int x = 60; x < 100; ++x) {
      const SkColor c = host.pixel(x, 80);
      const bool pureRed = SkColorGetR(c) > 200 && SkColorGetG(c) < 40;
      const bool pureGreen = SkColorGetG(c) > 200 && SkColorGetR(c) < 40;
      count += !pureRed && !pureGreen;
    }
    return count;
  };
  const auto picture = [&] { return image(asset).width(160).height(160); };
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  EXPECT_GT(mixed(box().children({picture()})), 2)
      << "linear when nothing states it";
  EXPECT_LE(mixed(box().children({picture().sampling(nearest)})), 1)
      << "on the leaf";
  EXPECT_LE(
      mixed(box().sampling(nearest).children({box().children({picture()})})), 1)
      << "on an ancestor, through a box that says nothing";
}

TEST(ComposeBlockLane, AChangedAncestorBlockRelaysOutTheLeavesUnderIt) {
  Host host;
  const auto page = [](float factor) {
    Block lead;
    lead.leading = Leading::multiple(factor);
    return box().padding(10).block(lead).children({box().children({leaf()})});
  };
  host.composer.render(page(1.0f));
  const float single = boxOf(host, "t").height();
  host.composer.render(page(2.0f));
  const float doubled = boxOf(host, "t").height();
  EXPECT_GT(doubled, single * 1.6f);
  host.composer.render(page(1.0f));
  EXPECT_NEAR(boxOf(host, "t").height(), single, 0.5f);
}
