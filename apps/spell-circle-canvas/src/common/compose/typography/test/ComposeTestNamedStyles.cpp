// What a selector sheet says about the NAMES a text leaf's rich runs and
// paragraphs were written with: each name matched as a virtual child of
// the leaf whose one class is the name, so an ancestor compound narrows
// which leaves a rule reaches, and a sheet that changes what a name means
// re-shapes the leaf. Beside them, the defaults a component states for a
// role, which every matching rule stands over.

#include <sigilcompose/core/StyleSheet.h>

#include <string_view>
#include <vector>

#include "DressedTypeProbes.h"

namespace {

using sigil::compose::rule;
using sigil::compose::StyleSheet;

sigil::weave::TextStyle white() {
  return sigil::weave::textStyle(
      {.face = sigil::test::instrument::sans(), .size = 20.0f,
       .color = SkColor4f{1, 1, 1, 1}});
}

/** The colour the first cluster of the run named @p name was set in. */
SkColor runColour(Host& host, std::string_view key, std::string_view name) {
  const std::vector<TextUnit> units = host.composer.units(
      key, selectors::style(name), sigil::weave::Unit::Cluster);
  return units.empty() ? SK_ColorTRANSPARENT
                       : units[0].style.paint.foreground.getColor();
}

/** A leaf whose second run is named `ts`, under a box of @p classes. */
Element logLine(std::string_view key, std::string_view classes) {
  return box().styleClass(classes).children(
      {text(sigil::weave::rich(white()).add(u8"x ").add(u8"12:00", "ts"))
           .key(key)});
}

}  // namespace

TEST(ComposeNamedStyles, AnAncestorCompoundNarrowsWhichRunsARuleReaches) {
  Host host(300, 200);
  host.composer.render(
      box()
          .applyStyleSheet(StyleSheet{
              rule(".log .ts").ink(SkColor4f{1, 0, 0, 1})})
          .children({logLine("inLog", "log"), logLine("outside", "other")}));
  host.frame();
  EXPECT_EQ(runColour(host, "inLog", "ts"), SK_ColorRED);
  EXPECT_EQ(runColour(host, "outside", "ts"), SK_ColorWHITE)
      << "a run under no .log is not spoken about";
}

TEST(ComposeNamedStyles, ASheetThatChangesWhatANameMeansReshapesTheLeaf) {
  Host host(300, 200);
  const auto page = [](SkColor4f ink) {
    return box()
        .applyStyleSheet(StyleSheet{rule(".ts").ink(ink)})
        .children({logLine("line", "log")});
  };
  host.composer.render(page({1, 0, 0, 1}));
  host.frame();
  EXPECT_EQ(runColour(host, "line", "ts"), SK_ColorRED);
  host.composer.render(page({0, 1, 0, 1}));
  host.frame();
  EXPECT_EQ(runColour(host, "line", "ts"), SK_ColorGREEN);
}

TEST(ComposeNamedStyles, ARunIsCountedAmongNoSiblings) {
  // Structural pseudo-classes are the element tree's; a run is not a
  // sibling of anything, so none of them reaches it.
  Host host(300, 200);
  host.composer.render(
      box()
          .applyStyleSheet(StyleSheet{
              rule(".ts:first-child").ink(SkColor4f{1, 0, 0, 1}),
              rule(".ts:only-child").ink(SkColor4f{1, 0, 0, 1})})
          .children({logLine("line", "log")}));
  host.frame();
  EXPECT_EQ(runColour(host, "line", "ts"), SK_ColorWHITE);
}

TEST(ComposeNamedStyles, ARoleDefaultStandsUnderEveryMatchingRule) {
  Host host(300, 200);
  const auto page = [](bool withRule) {
    Element root = box().key("root");
    if (withRule)
      root.applyStyleSheet(StyleSheet{rule("heading").font({.size = 32})});
    return root.children({text("AAAA")
                              .role("heading", {.size = 16})
                              .key("t")});
  };
  host.composer.render(page(false));
  host.frame();
  const float byDefault = require(host.composer.bounds("t")).height();
  host.composer.render(page(true));
  host.frame();
  EXPECT_GT(require(host.composer.bounds("t")).height(), byDefault * 1.5f)
      << "the rule for the role stands over the component's default";
}
