// What a node STATED, as against what its fields happen to hold, and the
// three keywords a property may be written as instead of a value. Each
// case asserts one thing the declared mask, the keyword table or the fold
// that reads them promises.

#include <sigilcompose/core/Property.h>

#include "support/CoreTestSupport.h"

namespace {

/** The width the keyed node laid out to. */
float widthOf(Host& host, std::string_view key) {
  return require(host.composer.bounds(key)).width();
}

}  // namespace

TEST(ComposeDeclarations, StatingTheDefaultIsNotTheSameAsSayingNothing) {
  // The mask IS the difference, and it is compared by value: a
  // description that states a property and one that leaves it to whatever
  // a rule or an inherited value gives it hold the same number in the
  // same field, so nothing but the mask can tell them apart. A comparator
  // that read only the numbers would prune the node for good.
  PropertyMask stated;
  EXPECT_TRUE(stated.empty());
  stated.set(Property::PaddingLeft);
  EXPECT_TRUE(stated.has(Property::PaddingLeft));
  EXPECT_FALSE(stated.has(Property::PaddingRight));
  EXPECT_FALSE(stated == PropertyMask{});
  stated.clear(Property::PaddingLeft);
  EXPECT_TRUE(stated == PropertyMask{});

  // Every property has a bit of its own, including the last one declared:
  // a mask one word too narrow drops the high bits silently, and a
  // property whose bit is always clear reads as one no node ever stated.
  PropertyMask last;
  last.set(static_cast<Property>((int)Property::kCount - 1));
  EXPECT_FALSE(last.empty());
  EXPECT_TRUE(last.has(static_cast<Property>((int)Property::kCount - 1)));
  EXPECT_FALSE(last.has(Property::Display));
}

TEST(ComposeDeclarations, UnsetIsInheritWhereThePropertyInheritsAndInitialElse) {
  // The whole of the inherited set, asked of the one table that holds it:
  // the type, the block, the ink, the custom properties and the image
  // sampling. Nothing about a box inherits — a padding taken from the
  // parent would be applied again at every depth.
  EXPECT_TRUE(inheritsByDefault(Property::Font));
  EXPECT_TRUE(inheritsByDefault(Property::Block));
  EXPECT_TRUE(inheritsByDefault(Property::Ink));
  EXPECT_TRUE(inheritsByDefault(Property::CustomProperties));
  EXPECT_TRUE(inheritsByDefault(Property::ImageRendering));
  EXPECT_FALSE(inheritsByDefault(Property::PaddingLeft));
  EXPECT_FALSE(inheritsByDefault(Property::Width));
  EXPECT_FALSE(inheritsByDefault(Property::Fill));
  EXPECT_FALSE(inheritsByDefault(Property::Opacity));
  EXPECT_FALSE(inheritsByDefault(Property::BorderRadius));
  EXPECT_FALSE(inheritsByDefault(Property::RotateX));

  using sigil::weave::Keyword;
  EXPECT_EQ(resolveKeyword(Keyword::Unset, Property::Ink), Keyword::Inherit);
  EXPECT_EQ(resolveKeyword(Keyword::Unset, Property::Width), Keyword::Initial);
  EXPECT_EQ(resolveKeyword(Keyword::Inherit, Property::Width),
            Keyword::Inherit);
  EXPECT_EQ(resolveKeyword(Keyword::Initial, Property::Ink), Keyword::Initial);
}

TEST(ComposeDeclarations, EveryPropertyAnswersToItsAuthoredName) {
  // The only reader is a diagnostic, and a diagnostic naming a number
  // tells nobody anything. A property with no row here answers with an
  // empty name, which is the failure this asks about.
  for (int i = 0; i < (int)Property::kCount; ++i)
    EXPECT_FALSE(propertyName(static_cast<Property>(i)).empty())
        << "property " << i << " has no name";
  EXPECT_EQ(propertyName(Property::PaddingLeft), "paddingLeft");
  EXPECT_EQ(propertyName(Property::JustifyContent), "justifyContent");
}

TEST(ComposeDeclarations, InheritTakesTheParentsComputedValue) {
  // A box property does not inherit on its own, so the keyword is the
  // only way to say it — and what it takes is the parent's COMPUTED
  // value, the one the parent ended up with.
  Host host(400, 200);
  host.composer.render(box().width(200).children(
      {box().key("child").inherit(Property::Width).flexShrink(0).height(10)}));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host, "child"), 200.0f);
}

TEST(ComposeDeclarations, AnInheritedValueFollowsAParentThatMovedBehindAPrune) {
  // THE CASE THE PATCH CANNOT SEE. The child's description does not
  // change between the two frames, so the reconciler prunes it and the
  // patch never runs; the only thing that moved is its PARENT's width.
  // An answer filled in the patch alone would stand at 200 for good.
  Host host(400, 200);
  const auto tree = [](Dimension parentWidth) {
    return box().width(parentWidth).children({box()
                                                  .key("child")
                                                  .inherit(Property::Width)
                                                  .flexShrink(0)
                                                  .height(10)});
  };
  host.composer.render(tree(200.0f));
  host.frame();
  ASSERT_FLOAT_EQ(widthOf(host, "child"), 200.0f);
  host.composer.render(tree(300.0f));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host, "child"), 300.0f);
}

TEST(ComposeDeclarations, InitialOnANonInheritingPropertyIsTheValueItStartsAt) {
  // Written after a value, the keyword is what stands: it is the same
  // declaration layer, and the later statement wins. The claim is that
  // the node lays out exactly as one that never stated a width does —
  // whatever that comes to where it stands — so the sibling beside it is
  // what the answer is read against.
  Host host(400, 200);
  host.composer.render(box().alignItems(Align::Start).children({
      box().key("silent").height(10),
      box().key("reset").width(120).initial(Property::Width).height(10),
      box().key("stated").width(120).height(10),
  }));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host, "reset"), widthOf(host, "silent"));
  EXPECT_FLOAT_EQ(widthOf(host, "stated"), 120.0f);
}

TEST(ComposeDeclarations, AKeywordIsADeclarationAndMakesTwoNodesUnequal) {
  // The values in the fields are IDENTICAL between the two frames — ten
  // pixels of padding on all four sides. Only the keyword differs, so
  // only the keyword table standing in the comparator keeps the node from
  // pruning and holding its first answer.
  Host host(400, 200);
  const auto tree = [](bool unsetLeft) {
    Element child = box().key("child").padding(10);
    if (unsetLeft) child.unset(Property::PaddingLeft);
    return box().children(
        {std::move(child).children({box().key("inner").width(20).height(10)})});
  };
  host.composer.render(tree(false));
  host.frame();
  ASSERT_FLOAT_EQ(require(host.composer.bounds("inner")).fLeft, 10.0f);
  host.composer.render(tree(true));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("inner")).fLeft, 0.0f)
      << "the left padding was unset, and unset for a box property is "
         "initial";
}

TEST(ComposeDeclarations, InitialStopsAnInheritedInk) {
  // The ink is one of the five that inherit, so `initial` is the keyword
  // that says something about it: stop taking the ancestor's, stand in
  // the colour the property has under no ancestor at all, which is
  // opaque black.
  Host host(200, 200);
  host.composer.render(box().ink({1, 0, 0, 1}).children({
      box().key("takes").width(40).height(40).fill(Fill::currentInk()),
      box().key("stops").width(40).height(40).initial(Property::Ink).fill(
          Fill::currentInk()),
  }));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(20, 60), SK_ColorBLACK);
}

TEST(ComposeDeclarations, UnsetOnAnInheritingPropertyKeepsInheriting) {
  // `unset` asks the table, and the table says the ink inherits — so it
  // is the ancestor's colour, exactly as if nothing had been written.
  Host host(200, 200);
  host.composer.render(box().ink({1, 0, 0, 1}).children(
      {box().key("takes").width(40).height(40).unset(Property::Ink).fill(
          Fill::currentInk())}));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);
}

TEST(ComposeDeclarations, ASizeAfterCoverPutsTheNodeBackInTheFlowUndeclared) {
  // `covering` is a placement STATE, not a property, and a size cancels
  // it. What the mask must do with that is forget the placement cover()
  // declared: a node put back in the flow states nothing about where it
  // sits, and one that still said so would be unequal to a node that
  // never covered and could never prune against it.
  Host host(200, 200);
  host.composer.render(box().padding(20).children(
      {box().key("flowed").cover().width(100).height(30)}));
  host.frame();
  const SkRect flowed = require(host.composer.bounds("flowed"));
  EXPECT_FLOAT_EQ(flowed.width(), 100.0f);
  EXPECT_FLOAT_EQ(flowed.fLeft, 20.0f) << "inside the parent's padding";

  // A pin stated after it is a PLACEMENT, and it stands.
  host.composer.render(box().padding(20).children(
      {box().key("pinned").cover().left(8).height(30)}));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("pinned")).fLeft, 8.0f);
}
