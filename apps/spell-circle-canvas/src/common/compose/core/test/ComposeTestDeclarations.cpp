// What a node STATED, as against what its fields happen to hold, and the
// three keywords a property may be written as instead of a value. Each
// test states one promise of the declared mask, the keyword table or the
// fold that reads them, with its instances as named rows.

#include <sigilcompose/core/Property.h>
#include <sigilcompose/core/StyleSheet.h>

#include <optional>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

using sigil::compose::test::checkRows;
using sigil::compose::test::forEachRow;

namespace {

/** The width the keyed node laid out to. */
float widthOf(Host& host, std::string_view key) {
  return require(host.composer.bounds(key)).width();
}

}  // namespace

// What a node states is recorded beside its values, so stating a default, or
// a keyword, is a different description from saying nothing.
TEST(ComposeDeclarations, TheMaskRecordsWhatANodeStated) {
  checkRows({
      {"StatingTheDefaultIsNotTheSameAsSayingNothing",
       [] {
         // The mask IS the difference, and it is compared by value: a
         // description that states a property and one that leaves it to
         // whatever a rule or an inherited value gives it hold the same number
         // in the same field, so nothing but the mask can tell them apart. A
         // comparator that read only the numbers would prune the node for good.
         PropertyMask stated;
         EXPECT_TRUE(stated.empty());
         stated.set(Property::PaddingLeft);
         EXPECT_TRUE(stated.has(Property::PaddingLeft));
         EXPECT_FALSE(stated.has(Property::PaddingRight));
         EXPECT_FALSE(stated == PropertyMask{});
         stated.clear(Property::PaddingLeft);
         EXPECT_TRUE(stated == PropertyMask{});

         // Every property has a bit of its own, including the last one
         // declared: a mask one word too narrow drops the high bits silently,
         // and a property whose bit is always clear reads as one no node ever
         // stated.
         PropertyMask last;
         last.set(static_cast<Property>((int)Property::kCount - 1));
         EXPECT_FALSE(last.empty());
         EXPECT_TRUE(
             last.has(static_cast<Property>((int)Property::kCount - 1)));
         EXPECT_FALSE(last.has(Property::Display));
       }},
      {"AKeywordIsADeclarationAndMakesTwoNodesUnequal",
       [] {
         // The values in the fields are IDENTICAL between the two frames — ten
         // pixels of padding on all four sides. Only the keyword differs, so
         // only the keyword table standing in the comparator keeps the node
         // from pruning and holding its first answer.
         Host host(400, 200);
         const auto tree = [](bool unsetLeft) {
           Element child = box().key("child").padding(10);
           if (unsetLeft) child.unset(Property::PaddingLeft);
           return box().children({std::move(child).children(
               {box().key("inner").width(20).height(10)})});
         };
         host.composer.render(tree(false));
         host.frame();
         ASSERT_FLOAT_EQ(require(host.composer.bounds("inner")).fLeft, 10.0f);
         host.composer.render(tree(true));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("inner")).fLeft, 0.0f)
             << "the left padding was unset, and unset for a box property is "
                "initial";
       }},
  });
}

// The inherited set is one table, and `unset` asks it: inherit where the
// property inherits, initial where it does not. The other two keywords are
// what they say about any property.
TEST(ComposeDeclarations, TheInheritedSetDecidesWhatUnsetResolvesTo) {
  using sigil::weave::Keyword;
  struct InheritedRow {
    const char* name;
    Property property;
    bool inherits;
  };
  // The whole of the inherited set (the type, the block, the ink, the
  // custom properties and the image sampling) beside properties of the box,
  // the paint and the plane, none of which inherits: a padding taken from
  // the parent would be applied again at every depth.
  const InheritedRow rows[] = {
      {"Font", Property::Font, true},
      {"Paragraph", Property::Paragraph, true},
      {"Ink", Property::Ink, true},
      {"CustomProperties", Property::CustomProperties, true},
      {"ImageRendering", Property::ImageRendering, true},
      {"PaddingLeft", Property::PaddingLeft, false},
      {"Width", Property::Width, false},
      {"Fill", Property::Fill, false},
      {"Opacity", Property::Opacity, false},
      {"BorderRadius", Property::BorderRadius, false},
      {"RotateX", Property::RotateX, false},
  };
  forEachRow(rows, [](const InheritedRow& row) {
    EXPECT_EQ(inheritsByDefault(row.property), row.inherits);
    EXPECT_EQ(resolveKeyword(Keyword::Unset, row.property),
              row.inherits ? Keyword::Inherit : Keyword::Initial);
    EXPECT_EQ(resolveKeyword(Keyword::Inherit, row.property), Keyword::Inherit);
    EXPECT_EQ(resolveKeyword(Keyword::Initial, row.property), Keyword::Initial);
  });
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

// A keyword is a declaration in the node's own layer: `inherit` is the
// parent's computed value, `initial` the value where nothing states it, and
// `unset` whichever the property calls for. It stands over a rule, drops
// what was folded over the value arriving from the parent, and the later of
// a keyword and a value about one property is the one that stands.
TEST(ComposeDeclarations, AKeywordResolvesInTheNodesOwnLayer) {
  // A box property, which does not inherit on its own: the subject under a
  // parent two hundred wide, beside a sibling that states no width, read as
  // a width or as that sibling's.
  struct WidthRow {
    const char* name;
    void (*declare)(Element&);
    std::optional<float> width;  // nothing: as wide as the silent sibling
  };
  const WidthRow widths[] = {
      {"InheritTakesTheParentsComputedValue",
       [](Element& node) { node.inherit(Property::Width).flexShrink(0); },
       200.0f},
      {"InitialOnANonInheritingPropertyIsTheValueItStartsAt",
       [](Element& node) { node.width(120).initial(Property::Width); },
       std::nullopt},
      {"AValueWrittenAfterAKeywordIsTheLaterStatement",
       [](Element& node) { node.initial(Property::Width).width(120); }, 120.0f},
      {"ARuleKeywordStandsWhereTheElementSaysNothing",
       [](Element& node) { node.styleClass("inheriting").flexShrink(0); },
       200.0f},
  };
  forEachRow(widths, [](const WidthRow& row) {
    Element subject = box().key("subject").height(10);
    row.declare(subject);
    Host host(400, 200);
    host.composer.render(
        box()
            .width(200)
            .alignItems(Align::Start)
            .applyStyleSheet(
                StyleSheet{rule(".inheriting").inherit(Property::Width)})
            .children({box().key("silent").height(10), std::move(subject)}));
    host.frame();
    EXPECT_FLOAT_EQ(widthOf(host, "subject"),
                    row.width ? *row.width : widthOf(host, "silent"));
  });

  // The ink, which inherits: the subject under a red parent and a sheet
  // whose `.loud` states green through the font, whose `.restated` writes
  // `initial` and then blue, and whose `.reset` writes `initial` alone. The
  // value that ARRIVES is the weakest layer, so `inherit` on a property that
  // inherits anyway drops the rule and the node's own verb folded over it.
  struct InkRow {
    const char* name;
    void (*declare)(Element&);
    SkColor ink;
  };
  const InkRow inks[] = {
      {"WithNothingStatedTheParentsInkArrives", [](Element&) {}, SK_ColorRED},
      {"TheNodesOwnInkStandsOverTheParents",
       [](Element& node) { node.ink({0, 0, 1, 1}); }, SK_ColorBLUE},
      {"InheritOnAnInheritingPropertyDropsWhatTheNodeSaid",
       [](Element& node) { node.ink({0, 0, 1, 1}).inherit(Property::Ink); },
       SK_ColorRED},
      {"InitialStopsAnInheritedInk",
       [](Element& node) { node.initial(Property::Ink); }, SK_ColorBLACK},
      {"UnsetOnAnInheritingPropertyKeepsInheriting",
       [](Element& node) { node.unset(Property::Ink); }, SK_ColorRED},
      {"AClassColourStandsOverTheParents",
       [](Element& node) { node.styleClass("loud"); }, SK_ColorGREEN},
      {"AKeywordOnAnInheritingPropertyStandsOverARule",
       [](Element& node) { node.styleClass("loud").unset(Property::Ink); },
       SK_ColorRED},
      {"InitialOnTheInkOverridesARulesFontColour",
       [](Element& node) { node.styleClass("loud").initial(Property::Ink); },
       SK_ColorBLACK},
      {"WithinOneRuleAFontColourAfterAKeywordStands",
       [](Element& node) { node.styleClass("restated"); }, SK_ColorBLUE},
      {"ARuleKeywordResetsAnInheritedLane",
       [](Element& node) { node.styleClass("reset"); }, SK_ColorBLACK},
  };
  forEachRow(inks, [](const InkRow& row) {
    Element subject = box().width(40).height(40);
    row.declare(subject);
    Host host(200, 200);
    host.composer.render(
        box()
            .ink({1, 0, 0, 1})
            .applyStyleSheet(
                StyleSheet{rule(".loud").font({.ink = SkColors::kGreen}),
                           rule(".restated")
                               .initial(Property::Ink)
                               .font({.ink = SkColors::kBlue}),
                           rule(".reset").initial(Property::Ink)})
            .children({std::move(subject).fill(Fill::currentInk())}));
    host.frame();
    EXPECT_EQ(host.pixel(20, 20), row.ink);
  });
}

// What a node takes from above follows the parent when the node itself
// prunes, and eases under the node's own transition.
TEST(ComposeDeclarations, AnInheritedValueFollowsAParentThatMovedBehindAPrune) {
  checkRows({
      {"AnInheritedValueFollowsAParentThatMovedBehindAPrune",
       [] {
         // THE CASE THE PATCH CANNOT SEE. The child's description does not
         // change between the two frames, so the reconciler prunes it and the
         // patch never runs; the only thing that moved is its PARENT's width.
         // An answer filled in the patch alone would stand at 200 for good.
         Host host(400, 200);
         const auto tree = [](Dimension parentWidth) {
           return box()
               .width(parentWidth)
               .children({box()
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
       }},
      {"AnInheritedPropertyThatMovedBehindAPruneEases",
       [] {
         // ONE RULE FOR EVERY INHERITED PROPERTY: a node stating a transition
         // of its own eases what it inherits with a lane of its own, toward the
         // target the ancestor is headed for. The child's description is the
         // SAME in both frames — one keyword and one transition — so it never
         // reaches the patch; its fill still moves, because the answer it takes
         // from above moved, and it eases wherever the movement was found.
         Host host(200, 200);
         const auto tree = [](material::Color fill) {
           return box()
               .fill(Fill::color(fill))
               .children({box()
                              .key("child")
                              .width(40)
                              .height(40)
                              .inherit(Property::Fill)
                              .transition({
                                  .duration = 200ms,
                              })});
         };
         host.composer.render(tree({1, 0, 0, 1}));
         host.frame();
         ASSERT_EQ(host.pixel(20, 20), SK_ColorRED);
         host.composer.render(tree({0, 0, 1, 1}));
         host.frame();
         EXPECT_EQ(host.pixel(20, 20), SK_ColorRED)
             << "the lane has begun and stands at the colour it begins at";
         EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE)
             << "the node that STATED the fill has no transition and snapped";
         host.frame(0.1);
         const SkColor mid = host.pixel(20, 20);
         EXPECT_NE(mid, SK_ColorRED);
         EXPECT_NE(mid, SK_ColorBLUE);
         host.frame(0.3);
         EXPECT_EQ(host.pixel(20, 20), SK_ColorBLUE);
       }},
  });
}

// Some properties are kept on the description, which no fold reads: a
// keyword about one of them would set the declared bit, make the node
// unequal to its old description, and then stand for nothing. The table says
// which, and the verb refuses those rather than taking them.
TEST(ComposeDeclarations, AKeywordNoFoldAnswersIsRefusedRatherThanDropped) {
  struct AnswerRow {
    const char* name;
    Property property;
    bool answers;
  };
  const AnswerRow rows[] = {
      {"Width", Property::Width, true},
      {"Ink", Property::Ink, true},
      {"Opacity", Property::Opacity, true},
      {"Shape", Property::Shape, false},
      {"GridArea", Property::GridArea, false},
      {"RotateX", Property::RotateX, false},
      {"DecorationOutline", Property::DecorationOutline, false},
  };
  forEachRow(rows, [](const AnswerRow& row) {
    EXPECT_EQ(answersKeyword(row.property), row.answers);
  });

  // Refused means the description is the one it would have been: the node
  // lays out and paints exactly as the sibling that never said it.
  Host host(400, 200);
  host.composer.render(
      box()
          .alignItems(Align::Start)
          .children({
              box().key("said").width(40).height(10).initial(Property::Shape),
              box().key("silent").width(40).height(10),
          }));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host, "said"), widthOf(host, "silent"));
}

// One statement, two spellings: a longhand writes the one field of the
// partial its shorthand writes, into the same lane, so the two describe the
// same node and the later of two statements wins field by field; and a
// colour written through the font is the ink.
TEST(ComposeDeclarations, ALonghandAndAFontColourWriteTheFieldsOfTheirLane) {
  struct LonghandRow {
    const char* name;
    Element longhand;
    Element shorthand;
  };
  const LonghandRow longhands[] = {
      {"FontSize", box().fontSize(18), box().font({.size = 18})},
      {"FontWeight", box().fontWeight(700), box().font({.weight = 700})},
      {"FontStyle", box().fontStyle(12), box().font({.slant = -12})},
      {"LetterSpacing", box().letterSpacing(2),
       box().font({.letterSpacing = 2})},
      {"TextAlign", box().textAlign(sigil::weave::TextAlignment::kCenter),
       box().paragraph({.alignment = sigil::weave::TextAlignment::kCenter})},
      {"TextIndent", box().textIndent(24),
       box().paragraph({.firstLineIndent = 24})},
      {"LineHeight", box().lineHeight(sigil::weave::LineHeight::multiple(1.4f)),
       box().paragraph(
           {.lineHeight = sigil::weave::LineHeight::multiple(1.4f)})},
      {"TextWrapBalance", box().textWrap(TextWrap::Balance),
       box().paragraph(
           {.balanceRaggedLines = true,
            .lineBreak = sigil::weave::LineBreakStrategy::kKnuthPlass})},
      {"TextWrapStable", box().textWrap(TextWrap::Stable),
       box().paragraph(
           {.balanceRaggedLines = false,
            .lineBreak = sigil::weave::LineBreakStrategy::kGreedy})},
      {"TextJustify", box().textJustify(TextJustify::InterCharacter),
       box().paragraph(
           {.justificationMethod =
                sigil::weave::JustificationMethod::kInterCharacter})},
      {"ALaterLonghandReplacesOneFieldOfTheShorthand",
       box().font({.size = 12, .weight = 700}).fontSize(18),
       box().font({.size = 18, .weight = 700})},
  };
  forEachRow(longhands, [](const LonghandRow& row) {
    EXPECT_TRUE(sameDescription(row.longhand, row.shorthand));
  });
  EXPECT_FALSE(sameDescription(box().fontSize(18), box().fontSize(19)))
      << "a longhand's value is part of the description";
  checkRows({
      {"AColourWrittenThroughTheFontIsTheInk",
       [] {
         // One lane, one bit: a colour inside `font()` states the ink as
         // `ink()` does, so the two spellings are one property and the later
         // statement wins between them, and a keyword about the ink ends where
         // either is written after it.
         Host host(200, 200);
         const material::Color blue{0, 0, 1, 1}, red{1, 0, 0, 1};
         host.composer.render(box().children({
             box()
                 .key("font")
                 .width(40)
                 .height(40)
                 .ink(red)
                 .font({.ink = blue})
                 .fill(Fill::currentInk()),
             box()
                 .key("ink")
                 .width(40)
                 .height(40)
                 .font({.ink = blue})
                 .ink(red)
                 .fill(Fill::currentInk()),
             box()
                 .key("after")
                 .width(40)
                 .height(40)
                 .initial(Property::Ink)
                 .font({.ink = blue})
                 .fill(Fill::currentInk()),
             box()
                 .key("before")
                 .width(40)
                 .height(40)
                 .font({.ink = blue})
                 .initial(Property::Ink)
                 .fill(Fill::currentInk()),
         }));
         host.frame();
         EXPECT_EQ(host.pixel(20, 20), SK_ColorBLUE)
             << "the font's colour came last";
         EXPECT_EQ(host.pixel(20, 60), SK_ColorRED) << "ink() came last";
         EXPECT_EQ(host.pixel(20, 100), SK_ColorBLUE)
             << "a colour through the font ends a keyword stated before it";
         EXPECT_EQ(host.pixel(20, 140), SK_ColorBLACK);
       }},
      {"AFontColourAndAnInkAreOneLaneToEveryReader",
       [] {
         // The same colour stated either way is the same ink in force: the
         // node's own currentInk and a child's inherited one read it alike, and
         // a node that switches from one spelling to the other keeps drawing
         // it.
         const material::Color red{1, 0, 0, 1};
         const auto scene = [&](bool throughTheFont) {
           Element parent = box().width(80).height(40).row();
           if (throughTheFont)
             parent.font({.ink = red});
           else
             parent.ink(red);
           return box().children({parent.children({
               box().width(40).height(40).fill(Fill::currentInk()),
               box().width(40).height(40).children(
                   {box().width(40).height(40).fill(Fill::currentInk())}),
           })});
         };
         Host font(80, 40), ink(80, 40), switched(80, 40);
         font.composer.render(scene(true));
         ink.composer.render(scene(false));
         switched.composer.render(scene(true));
         font.frame();
         ink.frame();
         switched.frame();
         switched.composer.render(scene(false));
         switched.frame();
         EXPECT_EQ(font.pixel(20, 20), SK_ColorRED);
         EXPECT_EQ(font.pixel(60, 20), SK_ColorRED) << "a child inherits it";
         EXPECT_TRUE(identicalPixels(font, ink, 80, 40));
         EXPECT_TRUE(identicalPixels(switched, ink, 80, 40));
       }},
  });
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
