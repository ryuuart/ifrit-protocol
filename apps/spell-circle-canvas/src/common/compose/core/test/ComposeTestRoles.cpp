// Semantic roles supply useful defaults without overriding a document's
// rules, authored classes or direct declarations. Resolution follows the
// retained tree, including content built before its document exists.

#include <sigilcompose/core/Cascade.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/style/Length.h>

#include <array>
#include <utility>

#include "support/CoreTestSupport.h"

using namespace sigil::weave::literals;

namespace {

using sigil::weave::Leading;
using sigil::weave::Rule;
using sigil::weave::StyleSheet;

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
      .role(Rule("heading")
                .font({.size = 36})
                .block({.leading = Leading::multiple(1.0f)}));
}

}  // namespace

TEST(ComposeRoles, AStandaloneRoleSuppliesTypeAndBlockDefaults) {
  Host role, direct;
  role.composer.render(page(heading()));
  direct.composer.render(
      page(text("AAAA\nAAAA")
               .font({.size = 36})
               .block({.leading = Leading::multiple(1.0f)})));
  EXPECT_EQ(contentRect(role), contentRect(direct));
  EXPECT_TRUE(anyWhiteIn(role, SkIRect::MakeXYWH(10, 10, 170, 90)));
}

TEST(ComposeRoles, ClassesAndDirectDeclarationsBeatRolesInEitherSheetOrder) {
  const Rule selected = Rule("heading")
                            .font({.size = 24})
                            .block({.leading = Leading::multiple(1.5f)});
  const Rule authored = Rule("authored")
                            .font({.size = 18})
                            .block({.leading = Leading::multiple(2.0f)});
  Host expectedRole, expectedClass, expectedDirect;
  expectedRole.composer.render(
      page(text("AAAA\nAAAA").font(selected.type()).block(selected.block())));
  expectedClass.composer.render(
      page(text("AAAA\nAAAA").font(authored.type()).block(authored.block())));
  expectedDirect.composer.render(
      page(text("AAAA\nAAAA")
               .font({.size = 14})
               .block({.leading = Leading::multiple(1.25f)})));
  const SkRect roleRect = contentRect(expectedRole);
  const SkRect classRect = contentRect(expectedClass);
  const SkRect directRect = contentRect(expectedDirect);
  for (const StyleSheet& sheet : std::array{StyleSheet{selected, authored},
                                            StyleSheet{authored, selected}}) {
    Host role, classed, direct;
    role.composer.render(page(heading()).styleSheet(sheet));
    classed.composer.render(
        page(heading().styleClass("authored")).styleSheet(sheet));
    direct.composer.render(
        page(text("AAAA\nAAAA")
                 .font({.size = 14})
                 .block({.leading = Leading::multiple(1.25f)})
                 .styleClass("authored")
                 .role(Rule("heading").font({.size = 36})))
            .styleSheet(sheet));
    EXPECT_EQ(contentRect(role), roleRect);
    EXPECT_EQ(contentRect(classed), classRect);
    EXPECT_EQ(contentRect(direct), directRect);
  }
}

TEST(ComposeRoles, ANearerRoleRuleChangesOnlyItsDeclaredFields) {
  const StyleSheet outer{Rule("heading")
                             .font({.size = 24})
                             .block({.leading = Leading::multiple(2.0f)})};
  const StyleSheet inner{{"heading", {.size = 18}}};
  Element adopted = heading().styleSheet(inner);
  Host role, direct;
  role.composer.render(page(std::move(adopted)).styleSheet(outer));
  direct.composer.render(
      page(text("AAAA\nAAAA")
               .font({.size = 18})
               .block({.leading = Leading::multiple(2.0f)})));
  EXPECT_EQ(contentRect(role), contentRect(direct));
}

TEST(ComposeRoles, ADocumentRuleChangeUpdatesAPrunedRoleDescendant) {
  const Element content = text("AAAA").role("heading");
  Host host;
  host.composer.render(page(content).styleSheet({{"heading", {.size = 16}}}));
  const SkRect small = contentRect(host);
  host.composer.render(page(content).styleSheet({{"heading", {.size = 32}}}));
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
        box().key("left").width(90).styleSheet({{"heading", {.size = 16}}});
    Element right =
        box().key("right").width(90).styleSheet({{"heading", {.size = 32}}});
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
  const StyleSheet sheet{{"authored", {.size = 1.25_em}},
                         {"heading", {.size = 1.5_em}}};
  for (int level = 0; level < 4; ++level) {
    Element content = box()
                          .role(Rule("heading").font({.size = 2_em}))
                          .width(2_em)
                          .height(1_em)
                          .key("content");
    if (level >= 2) content.styleClass("authored");
    if (level >= 3) content.font({.size = 0.75_em});
    Element document = box().font({.size = 20}).children({content});
    if (level >= 1) document.styleSheet(sheet);
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
        .role(Rule("panel").font({.size = 1.5_em}))
        .varDefaults(defaults)
        .padding(var("gutter"))
        .children({box()
                       .width(10)
                       .height(10)
                       .fill(Fill::var("accent"))
                       .key("content")});
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
        .role(Rule("panel").font({.size = size}))
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
