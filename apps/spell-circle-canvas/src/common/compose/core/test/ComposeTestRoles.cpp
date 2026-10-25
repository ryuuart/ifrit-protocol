// Semantic roles supply useful defaults without overriding a document's
// rules, authored classes or direct declarations. Resolution follows the
// retained tree, including content built before its document exists.

#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilweave/style/Length.h>

#include <array>
#include <utility>

#include "support/CoreTestSupport.h"

using namespace sigil::weave::literals;

namespace {

using sigil::weave::Leading;

Element page(Element content) {
  return box()
      .padding(10)
      .font({.face = sigil::test::instrument::sans(), .size = 12})
      .ink({1, 1, 1, 1})
      .children({std::move(content).key("content")});
}

SkRect contentRect(Host& host) {
  host.frame();
  return require(host.composer.bounds("content"));
}

Element heading() {
  return text("AAAA\nAAAA")
      .role("heading", {.size = 36}, {.leading = Leading::multiple(1.0f)});
}

}  // namespace

TEST(ComposeRoles, AStandaloneRoleSuppliesTypeAndBlockDefaults) {
  Host role, direct;
  role.composer.render(page(heading()));
  direct.composer.render(
      page(text("AAAA\nAAAA")
               .font({.size = 36})
               .paragraph({.leading = Leading::multiple(1.0f)})));
  EXPECT_EQ(contentRect(role), contentRect(direct));
  EXPECT_TRUE(anyWhiteIn(role, SkIRect::MakeXYWH(10, 10, 170, 90)));
}

TEST(ComposeRoles, ClassesAndDirectDeclarationsBeatRolesInEitherSheetOrder) {
  const sigil::compose::Rule selected =
      sigil::compose::rule("heading")
          .font({.size = 24})
          .paragraph({.leading = Leading::multiple(1.5f)});
  const sigil::compose::Rule authored =
      sigil::compose::rule(".authored")
          .font({.size = 18})
          .paragraph({.leading = Leading::multiple(2.0f)});
  Host expectedRole, expectedClass, expectedDirect;
  expectedRole.composer.render(page(text("AAAA\nAAAA")
                                        .font(selected.type())
                                        .paragraph(selected.paragraph())));
  expectedClass.composer.render(page(text("AAAA\nAAAA")
                                         .font(authored.type())
                                         .paragraph(authored.paragraph())));
  expectedDirect.composer.render(
      page(text("AAAA\nAAAA")
               .font({.size = 14})
               .paragraph({.leading = Leading::multiple(1.25f)})));
  const SkRect roleRect = contentRect(expectedRole);
  const SkRect classRect = contentRect(expectedClass);
  const SkRect directRect = contentRect(expectedDirect);
  for (const sigil::compose::StyleSheet& sheet :
       std::array{sigil::compose::StyleSheet{selected, authored},
                  sigil::compose::StyleSheet{authored, selected}}) {
    Host role, classed, direct;
    role.composer.render(page(heading()).applyStyleSheet(sheet));
    classed.composer.render(
        page(heading().styleClass("authored")).applyStyleSheet(sheet));
    direct.composer.render(
        page(text("AAAA\nAAAA")
                 .font({.size = 14})
                 .paragraph({.leading = Leading::multiple(1.25f)})
                 .styleClass("authored")
                 .role("heading", {.size = 36}))
            .applyStyleSheet(sheet));
    EXPECT_EQ(contentRect(role), roleRect);
    EXPECT_EQ(contentRect(classed), classRect);
    EXPECT_EQ(contentRect(direct), directRect);
  }
}

TEST(ComposeRoles, ANearerRoleRuleChangesOnlyItsDeclaredFields) {
  const sigil::compose::StyleSheet outer{
      sigil::compose::rule("heading")
          .font({.size = 24})
          .paragraph({.leading = Leading::multiple(2.0f)})};
  const sigil::compose::StyleSheet inner{
      sigil::compose::rule("heading").font({.size = 18})};
  Element adopted = heading().applyStyleSheet(inner);
  Host role, direct;
  role.composer.render(page(std::move(adopted)).applyStyleSheet(outer));
  direct.composer.render(
      page(text("AAAA\nAAAA")
               .font({.size = 18})
               .paragraph({.leading = Leading::multiple(2.0f)})));
  EXPECT_EQ(contentRect(role), contentRect(direct));
}

TEST(ComposeRoles, ADocumentRuleChangeUpdatesAPrunedRoleDescendant) {
  const Element content = text("AAAA").role("heading");
  Host host;
  host.composer.render(page(content).applyStyleSheet(sigil::compose::StyleSheet{
      sigil::compose::rule("heading").font({.size = 16})}));
  const SkRect small = contentRect(host);
  host.composer.render(page(content).applyStyleSheet(sigil::compose::StyleSheet{
      sigil::compose::rule("heading").font({.size = 32})}));
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  const SkRect big = contentRect(host);
  EXPECT_GT(big.width(), small.width() * 1.8f);
  host.composer.render(page(content));
  const SkRect inherited = contentRect(host);
  EXPECT_LT(inherited.width(), small.width());
}

TEST(ComposeRoles, AReparentedRoleResolvesAgainstItsNewDocument) {
  const Element content = text("AA").role("heading").key("content");
  const auto documents = [&](bool moveRight) {
    Element left =
        box().key("left").width(90).applyStyleSheet(sigil::compose::StyleSheet{
            sigil::compose::rule("heading").font({.size = 16})});
    Element right =
        box().key("right").width(90).applyStyleSheet(sigil::compose::StyleSheet{
            sigil::compose::rule("heading").font({.size = 32})});
    (moveRight ? right : left).children({content});
    return box()
        .row()
        .font({.face = sigil::test::instrument::sans()})
        .children({std::move(left), std::move(right)});
  };
  Host host;
  host.composer.render(documents(false));
  const SkRect left = contentRect(host);
  host.composer.render(documents(true));
  const SkRect right = contentRect(host);
  EXPECT_GT(right.left(), left.left());
  EXPECT_GT(right.width(), left.width() * 1.8f);
}

TEST(ComposeRoles, RelativeSizesResolveOnceAfterRoleClassAndDirectOverrides) {
  const sigil::compose::StyleSheet sheet{
      sigil::compose::rule(".authored").font({.size = 1.25_em}),
      sigil::compose::rule("heading").font({.size = 1.5_em})};
  for (int level = 0; level < 4; ++level) {
    Element content = box()
                          .role("heading", {.size = 2_em})
                          .width(2_em)
                          .height(1_em)
                          .key("content");
    if (level >= 2) content.styleClass("authored");
    if (level >= 3) content.font({.size = 0.75_em});
    Element document = box().font({.size = 20}).children({content});
    if (level >= 1) document.applyStyleSheet(sheet);
    Host host;
    host.composer.render(document);
    const SkRect rect = contentRect(host);
    constexpr std::array<float, 4> sizes{40, 30, 25, 15};
    EXPECT_FLOAT_EQ(rect.width(), sizes[level] * 2);
    EXPECT_FLOAT_EQ(rect.height(), sizes[level]);
  }
}

TEST(ComposeRoles, FallbackVariablesYieldToInheritedAndDirectZeroValues) {
  VarTable defaults;
  defaults.set(var("gutter"), Dimension(1_em));
  defaults.set(var("accent"), SkColor4f{1, 0, 0, 1});
  const auto component = [&] {
    return box()
        .role("panel", {.size = 1.5_em})
        .varDefaults(defaults)
        .padding(var("gutter"))
        .children(
            {box().width(10).height(10).fill(var("accent")).key("content")});
  };
  Host host;
  host.composer.render(box().font({.size = 20}).children({component()}));
  EXPECT_FLOAT_EQ(contentRect(host).left(), 30);
  EXPECT_EQ(host.pixel(35, 35), SkColorSetARGB(255, 255, 0, 0));

  host.composer.render(box()
                           .font({.size = 20})
                           .var("gutter", Dimension(0))
                           .var("accent", SkColor4f{0, 0, 1, 1})
                           .children({component()}));
  EXPECT_FLOAT_EQ(contentRect(host).left(), 0);
  EXPECT_EQ(host.pixel(5, 5), SkColorSetARGB(255, 0, 0, 255));

  host.composer.render(
      box()
          .font({.size = 20})
          .var("gutter", Dimension(18))
          .children({component().var("gutter", Dimension(0))}));
  EXPECT_FLOAT_EQ(contentRect(host).left(), 0);
}

TEST(ComposeRoles, ChangingRoleAndVariableDefaultsInvalidatesTheNode) {
  const auto content = [](float size, float gutter) {
    VarTable defaults;
    defaults.set(var("gutter"), Dimension(gutter));
    return box()
        .role("panel", {.size = size})
        .varDefaults(defaults)
        .padding(var("gutter"))
        .children({box().width(1_em).height(1_em).key("content")});
  };
  Host host;
  host.composer.render(box().children({content(12, 5)}));
  const SkRect first = contentRect(host);
  host.composer.render(box().children({content(24, 10)}));
  const SkRect second = contentRect(host);
  EXPECT_FLOAT_EQ(first.left(), 5);
  EXPECT_FLOAT_EQ(first.width(), 12);
  EXPECT_FLOAT_EQ(second.left(), 10);
  EXPECT_FLOAT_EQ(second.width(), 24);
}

namespace {

/** A swatch whose own fill is the ink in force, in a role whose default
 *  ink is the property `accent`. */
Element inkedSwatch() {
  return box()
      .role("swatch", {}, var("accent"))
      .width(20)
      .height(20)
      .fill(Fill::currentInk());
}

}  // namespace

/** A ROLE'S DEFAULT INK MAY BE A PROPERTY, and it stands where a colour in
 *  the role's defaults would: the property inks the role where no rule
 *  names a colour, a rule naming one wins, and the node's own ink wins
 *  over both. */
TEST(ComposeRoles, ARoleDefaultInkIsAPropertyUnderEveryRule) {
  const SkColor4f amber{1, 0, 0, 1};
  Host host;
  host.composer.render(box().var("accent", amber).children({inkedSwatch()}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SkColorSetARGB(255, 255, 0, 0));

  host.composer.render(box()
                           .var("accent", amber)
                           .applyStyleSheet(StyleSheet{rule("swatch").font(
                               {.color = SkColor4f{0, 0, 1, 1}})})
                           .children({inkedSwatch()}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SkColorSetARGB(255, 0, 0, 255))
      << "a rule naming a colour stands over the role's property";

  host.composer.render(
      box()
          .var("accent", amber)
          .children({inkedSwatch().ink(SkColor4f{0, 1, 0, 1})}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SkColorSetARGB(255, 0, 255, 0))
      << "the node's own ink stands over both";
}

/** THE COMPOSER'S SHEET STANDS AT THE ROOT of whatever is rendered: its
 *  `:root` is the tree's root, so its properties reach every node; a
 *  sheet the tree applies on its root stands after it and wins a tie; and
 *  an empty sheet clears it. */
TEST(ComposeRoles, TheComposersSheetStandsAtTheRoot) {
  Host host;
  host.composer.setStyleSheet(
      StyleSheet{rule(":root")
                     .var("accent", SkColor4f{1, 0, 0, 1})
                     .var("gutter", Dimension(30))});
  const auto tree = [] {
    return box().children({box()
                               .key("content")
                               .margin(var("gutter"), 0, 0, 0)
                               .width(20)
                               .height(20)
                               .fill(var("accent"))});
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_FLOAT_EQ(contentRect(host).top(), 30);
  EXPECT_EQ(host.pixel(10, 40), SkColorSetARGB(255, 255, 0, 0));

  host.composer.render(tree().applyStyleSheet(
      StyleSheet{rule(":root").var("accent", SkColor4f{0, 0, 1, 1})}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 40), SkColorSetARGB(255, 0, 0, 255))
      << "the tree's own sheet stands after the composer's";
  EXPECT_FLOAT_EQ(contentRect(host).top(), 30)
      << "and what it does not name stays the composer's";

  host.composer.setStyleSheet({});
  host.composer.render(tree());
  host.frame();
  EXPECT_FLOAT_EQ(contentRect(host).top(), 0) << "an empty sheet clears it";
}
