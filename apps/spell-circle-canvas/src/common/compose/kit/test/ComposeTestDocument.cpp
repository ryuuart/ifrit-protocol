#include <sigilcompose/core/Measure.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>

#include "support/ShapeTestSupport.h"

namespace doc = sigil::compose::document;

namespace {
Element onPage(Element content) {
  return box()
      .font({.face = sigil::test::instrument::sans(), .size = 16})
      .ink(SkColors::kWhite)
      .children({std::move(content)});
}
}  // namespace

TEST(KitDocument, StandaloneHeadingsHaveAHierarchyAndKeepExplicitOverrides) {
  Host host(700, 300);
  host.composer.render(onPage(
      box()
          .column()
          .alignItems(Align::Start)
          .children({doc::h1("AAAA").key("h1"), doc::h2("AAAA").key("h2"),
                     doc::paragraph("AAAA").key("body"),
                     doc::h1("AAAA").font({.size = 16}).key("explicit")})));
  host.frame();
  const float body = require(host.composer.bounds("body")).width();
  EXPECT_NEAR(require(host.composer.bounds("h1")).width(), body * 2, 1);
  EXPECT_NEAR(require(host.composer.bounds("h2")).width(), body * 1.5f, 1);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("explicit")).width(), body);
  EXPECT_THROW(doc::heading(0, "Invalid"), std::out_of_range);
  EXPECT_THROW(doc::heading(7, "Invalid"), std::out_of_range);
}

TEST(KitDocument, APreviouslyBuiltArticleAdoptsMeasureAndZeroGap) {
  const Element article = doc::article({doc::paragraph("First").key("first"),
                                        doc::paragraph("Second").key("second")})
                              .key("article");
  Host host(900, 300);
  host.composer.render(onPage(article));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("article")).width(), 608);
  const auto first = require(host.composer.bounds("first"));
  EXPECT_FLOAT_EQ(
      require(host.composer.bounds("second")).top() - first.bottom(), 16);
  host.composer.render(onPage(article)
                           .var(doc::measure, Dimension(320))
                           .var(doc::gap, Dimension(0)));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("article")).width(), 320);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("second")).top(),
                  require(host.composer.bounds("first")).bottom());
}

TEST(KitDocument, RoleRulesRethemeExistingContentAndReflowParagraphs) {
  const Element article =
      doc::article(
          {doc::h2("A heading").key("heading"),
           doc::paragraph(
               "A paragraph carries enough words to occupy several lines "
               "when the document changes its body size.")
               .key("body")})
          .var(doc::measure, Dimension(250));
  Host host(400, 800);
  host.composer.render(onPage(article));
  host.frame();
  const float before = require(host.composer.bounds("body")).height();
  host.composer.render(onPage(article).applyStyleSheet(
      StyleSheet{rule("h2").font({.size = 36}),
                 rule("paragraph").font({.size = 24})}));
  host.frame();
  EXPECT_GT(require(host.composer.bounds("body")).height(), before * 1.5f);
  EXPECT_GE(require(host.composer.bounds("body")).top(),
            require(host.composer.bounds("heading")).bottom());
}

TEST(KitDocument, RichParagraphsRemainOnePassageWithTheirInlineStyles) {
  const auto words = weave::rich()
                         .add(u8"Plain ")
                         .add(u8"accent", weave::Type{.color = SkColors::kRed})
                         .add(u8" plain");
  Host semantic(350, 100), raw(350, 100);
  semantic.composer.render(onPage(doc::paragraph(words).width(330)));
  raw.composer.render(onPage(text(words).width(330)));
  semantic.frame();
  raw.frame();
  EXPECT_TRUE(identicalPixels(semantic, raw, 350, 100));
}

TEST(KitDocument, WrappedListItemsKeepMarkersBesideTheirFirstLine) {
  Host host(300, 400);
  host.composer.render(onPage(
      doc::list(
          {doc::item(doc::paragraph("A long item wraps onto several lines in a "
                                    "narrow reading column.")
                         .key("body"),
                     "1.")
               .key("first"),
           doc::item("Second item", "2.").key("second")})
          .width(160)));
  host.frame();
  const auto first = require(host.composer.bounds("first"));
  const auto body = require(host.composer.bounds("body"));
  EXPECT_GT(body.left(), first.left());
  EXPECT_LE(body.right(), first.right() + 0.1f);
  EXPECT_GT(body.height(), 32);
  EXPECT_GT(require(host.composer.bounds("second")).top(), first.bottom());
}

TEST(KitDocument, ADocumentMeasuresWithoutAWindow) {
  auto article =
      doc::article({doc::h1("A document"),
                    doc::paragraph("One paragraph below its heading.")})
          .font({.face = sigil::test::instrument::sans(), .size = 16})
          .width(300);
  const SkSize size = intrinsicSize(article, fonts());
  EXPECT_FLOAT_EQ(size.width(), 300);
  EXPECT_GT(size.height(), 48);
}

TEST(KitDocument, ALongMarkerReservesItsOwnWidth) {
  Host host(300, 150);
  host.composer.render(onPage(
      doc::list(
          {doc::item(doc::paragraph("A numbered item").key("body"), "100.")
               .key("item")})
          .width(240)));
  host.frame();
  const auto body = require(host.composer.bounds("body"));
  const auto item = require(host.composer.bounds("item"));
  EXPECT_GT(body.left() - item.left(), 16 * 1.5f);
  EXPECT_LE(body.right(), item.right() + 0.1f);
  EXPECT_LE(body.height(), item.height());
}
