// What a rem measures: the root element's computed font size, as CSS's
// rem is, so a sheet states one root size and writes its type scale in
// rems against it.

#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/SpanStyle.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Length.h>

#include <string_view>
#include <utility>

#include "support/CoreTestSupport.h"

using namespace sigil::weave::literals;

namespace {

/** A box one em wide under a box whose font is @p size, so the width the
 *  keyed box lays out to is the font size that box resolved. */
Element sizedAt(sigil::weave::Length size) {
  return box().fontSize(size).children(
      {box().key("measured").width(1_em).height(10).flexShrink(0)});
}

/** The width the keyed box laid out to. */
float widthOf(Host& host) {
  return require(host.composer.bounds("measured")).width();
}

}  // namespace

TEST(ComposeRem, ARemIsTheRootElementsFontSize) {
  // The root states ten pixels, so one and a half rems are fifteen: the
  // root, not what the composer inherits, is what a rem measures.
  Host host(400, 200);
  host.composer.render(box()
                           .font({.face = sigil::test::instrument::sans()})
                           .fontSize(10)
                           .children({sizedAt(1.5_rem)}));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 15.0f);
}

TEST(ComposeRem, ASizeARootRuleStatesIsTheRootsSize) {
  // The one root size a token sheet states on `:root`.
  Host host(400, 200);
  host.composer.render(
      box()
          .font({.face = sigil::test::instrument::sans()})
          .applyStyleSheet(StyleSheet{rule(":root").fontSize(10)})
          .children({sizedAt(1.5_rem)}));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 15.0f);
}

TEST(ComposeRem, WhatTheComposerInheritsDecidesWhereTheRootStatesNoSize) {
  Host host(400, 200);
  host.composer.setInherited(
      sigil::weave::Type{.face = sigil::test::instrument::sans(), .size = 20},
      {1, 1, 1, 1});
  host.composer.render(box().children({sizedAt(1.5_rem)}));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 30.0f);
}

TEST(ComposeRem, TheRootsOwnSizeInRemsMeasuresWhatItInherits) {
  // A root cannot be measured against itself, so a rem in its own font
  // size is the inherited size, and every rem below it is the result.
  Host host(400, 200);
  host.composer.setInherited(
      sigil::weave::Type{.face = sigil::test::instrument::sans(), .size = 10},
      {1, 1, 1, 1});
  host.composer.render(box().fontSize(2_rem).children({sizedAt(1_rem)}));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 20.0f);
}

TEST(ComposeRem, ALengthInRemsFollowsTheRootWhenNothingElseAboutItMoved) {
  // The measured box states its font in pixels, so only the root moving
  // can move a width it writes in rems — the case no change of its own
  // font would announce.
  Host host(400, 200);
  const auto page = [](float rootSize) {
    return box()
        .font({.face = sigil::test::instrument::sans()})
        .fontSize(rootSize)
        .children({box().fontSize(40).children(
            {box().key("measured").width(2_rem).height(10).flexShrink(0)})});
  };
  host.composer.render(page(10));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 20.0f);
  host.composer.render(page(12));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 24.0f);
}

namespace {

/** The box a one-letter leaf shrinks to, so its extent is its type's. */
SkRect letterBox(Host& host, const char* key) {
  return require(host.composer.bounds(key));
}

}  // namespace

TEST(ComposeRem, ARunASheetNamesMeasuresItsRemsAgainstTheRoot) {
  // A named run is a virtual child of the leaf, and a span restyles part
  // of it: a rem either states is the root element's size, as it would be
  // on a child.
  Host host(400, 200);
  host.composer.render(
      box()
          .font({.face = sigil::test::instrument::sans()})
          .fontSize(10)
          .applyStyleSheet(StyleSheet{rule(".big").fontSize(2_rem)})
          .alignItems(Align::Start)
          .children(
              {text(sigil::weave::rich().add(std::u8string_view(u8"H"), "big"))
                   .key("run"),
               text(u8"H")
                   .span(sigil::weave::selectors::range({0, 1}),
                         SpanStyle().fontSize(2_rem))
                   .key("span"),
               text(u8"H").fontSize(20).key("plain")}));
  host.frame();
  const SkRect plain = letterBox(host, "plain");
  EXPECT_FLOAT_EQ(letterBox(host, "run").width(), plain.width());
  EXPECT_FLOAT_EQ(letterBox(host, "run").height(), plain.height());
  EXPECT_FLOAT_EQ(letterBox(host, "span").width(), plain.width());
  EXPECT_FLOAT_EQ(letterBox(host, "span").height(), plain.height());
}
