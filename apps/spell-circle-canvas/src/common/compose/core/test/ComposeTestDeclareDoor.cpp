// THE DECLARE DOOR — a property's field is written only by the writer named
// for it, which marks the property stated.
//
// A field written without its bit is a node that states the default and a
// node that says nothing at once: a rule overrides it, an inherited value
// covers it, and the prune compares it as unstated. For the fields the
// computed style carries, the compile-time half below proves they cannot
// be reached any other way. The properties kept on the node itself — the
// depth lanes, the shape, the grid area, the decoration outline and the
// five the cascade pass resolves — have writers that mark their bits, but
// their storage is the node's and stays reachable. The walks prove every
// writer, of either kind, marks its own property and no other, writes the
// field its property is kept in, and moves a value the prune sees.
//
// THIS FILE REACHES THE LIBRARY'S OWN SOURCE DIRECTORY, for the same reason
// the field walk beside it does: an unmarked write is invisible from
// outside, where the two descriptions simply compare equal.

#include <type_traits>

#include "../ComputedStyle.h"
#include "support/CoreTestSupport.h"

namespace cd = sigil::compose::detail;

namespace {

// What a verb body can reach, asked from outside the class as a verb asks:
// the readers write nothing, the storage cannot be named, the mask and the
// keyword table are read-only, and the key to the one door that writes
// without a bit cannot be made. Every other public member is a writer,
// and the walks below hold each one to its own property and its own field.
template <class Fields>
concept ReaderWrites =
    requires(Fields& fields, Dimension length) {
      fields.layout().gap = length;
    } || requires(Fields& fields) { fields.paint().zIndex = 1; } ||
    requires(Fields& fields) { fields.corners().topLeft = 1.0f; } ||
    requires(Fields& fields) { fields.clipContent() = true; };
template <class Fields>
concept StorageReachable = requires(Fields& fields) { fields.m_storage; };
template <class Fields>
concept MaskWritable =
    requires(Fields& fields) { fields.declared().set(Property::Gap); };
template <class Fields>
concept KeywordsWritable =
    requires(Fields& fields) { fields.keywords().ensure(); };
template <class Fields>
concept DefaultsOpen = requires(Fields& fields) { fields.defaults(); } ||
                       requires(Fields& fields) { fields.defaults({}); };
template <class Key>
concept KeyMakeable = requires { Key{}; };
template <class Node>
concept NodeHoldsLayout = requires(Node& node) { node.layout; };
template <class Node>
concept NodeHoldsMask = requires(Node& node) { node.declared; };

static_assert(!ReaderWrites<cd::DeclaredFields>,
              "a reader hands back a field a verb can write through");
static_assert(!StorageReachable<cd::DeclaredFields>,
              "the declared fields are reachable without a writer");
static_assert(!MaskWritable<cd::DeclaredFields> &&
                  !KeywordsWritable<cd::DeclaredFields>,
              "the mask or the keyword table is writable from outside");
static_assert(!DefaultsOpen<cd::DeclaredFields> &&
                  !KeyMakeable<cd::DefaultsKey> &&
                  !std::is_default_constructible_v<cd::DefaultsKey>,
              "the door that writes without a bit opens without its key");
static_assert(!NodeHoldsLayout<cd::ElementNode> &&
                  !NodeHoldsMask<cd::ElementNode>,
              "the node holds a declared field of its own again");

/** Writes @p property on @p node through the property's own writer: its
 *  starting value, or with @p moved a value it does not start with. */
void writeThroughWriter(cd::ElementNode& node, Property property, bool moved) {
  cd::DeclaredFields& fields = node.fields;
  const Dimension length = moved ? Dimension(4.0f) : Dimension(0.0f);
  const float turn = moved ? 1.0f : 0.0f;
  switch (property) {
    case Property::Display:
      fields.display() = moved ? Display::None : Display::Flex;
      return;
    case Property::BoxSizing:
      fields.boxSizing() = moved ? BoxSizing::ContentBox : BoxSizing::BorderBox;
      return;
    case Property::Gap:
      fields.gap() = length;
      return;
    case Property::PaddingTop:
      fields.paddingTop() = length;
      return;
    case Property::PaddingRight:
      fields.paddingRight() = length;
      return;
    case Property::PaddingBottom:
      fields.paddingBottom() = length;
      return;
    case Property::PaddingLeft:
      fields.paddingLeft() = length;
      return;
    case Property::MarginTop:
      fields.marginTop() = length;
      return;
    case Property::MarginRight:
      fields.marginRight() = length;
      return;
    case Property::MarginBottom:
      fields.marginBottom() = length;
      return;
    case Property::MarginLeft:
      fields.marginLeft() = length;
      return;
    case Property::Width:
      if (moved)
        fields.width() = length;
      else
        fields.width();
      return;
    case Property::Height:
      if (moved)
        fields.height() = length;
      else
        fields.height();
      return;
    case Property::MinWidth:
      if (moved)
        fields.minWidth() = length;
      else
        fields.minWidth();
      return;
    case Property::MaxWidth:
      if (moved)
        fields.maxWidth() = length;
      else
        fields.maxWidth();
      return;
    case Property::MinHeight:
      if (moved)
        fields.minHeight() = length;
      else
        fields.minHeight();
      return;
    case Property::MaxHeight:
      if (moved)
        fields.maxHeight() = length;
      else
        fields.maxHeight();
      return;
    case Property::AspectRatio:
      fields.aspectRatio() = moved ? 2.0f : 0.0f;
      return;
    case Property::FlexDirection:
      fields.flexDirection() =
          moved ? FlexDirection::Row : FlexDirection::Column;
      return;
    case Property::FlexWrap:
      fields.flexWrap() = moved ? FlexWrap::Wrap : FlexWrap::NoWrap;
      return;
    case Property::FlexBasis:
      if (moved)
        fields.flexBasis() = length;
      else
        fields.flexBasis();
      return;
    case Property::FlexGrow:
      fields.flexGrow() = turn;
      return;
    case Property::FlexShrink:
      fields.flexShrink() = moved ? 0.0f : 1.0f;
      return;
    case Property::AlignItems:
      fields.alignItems() = moved ? Align::Center : Align::Stretch;
      return;
    case Property::AlignSelf:
      fields.alignSelf() = moved ? Align::Center : Align::Auto;
      return;
    case Property::JustifyContent:
      fields.justifyContent() = moved ? Justify::Center : Justify::Start;
      return;
    case Property::Absolute:
      fields.absolute().absolute = moved;
      return;
    case Property::Left:
      if (moved)
        fields.left() = length;
      else
        fields.left();
      return;
    case Property::Top:
      if (moved)
        fields.top() = length;
      else
        fields.top();
      return;
    case Property::Right:
      if (moved)
        fields.right() = length;
      else
        fields.right();
      return;
    case Property::Bottom:
      if (moved)
        fields.bottom() = length;
      else
        fields.bottom();
      return;
    case Property::CenterAt:
      if (moved)
        fields.centerAt() = SkPoint::Make(1.0f, 2.0f);
      else
        fields.centerAt();
      return;
    case Property::GridCells:
      fields.gridCells().column = moved ? 1 : 0;
      return;
    case Property::GridCellAlign:
      fields.gridCellAlign().across = moved ? Align::Center : Align::Auto;
      return;
    case Property::GridArea:
      node.gridArea() = moved ? "moved" : "";
      return;
    case Property::BorderRadius:
      fields.borderRadius() = Corners(turn);
      return;
    case Property::Shape:
      if (moved)
        node.shape() = Shape([](SkSize) { return SkPath(); });
      else
        node.shape();
      return;
    case Property::Overflow:
      fields.overflow() = moved;
      return;
    case Property::Fill:
      if (moved)
        fields.fill() = motion::Animatable<Fill>(Fill::color({1, 0, 0, 1}));
      else
        fields.fill();
      return;
    case Property::Opacity:
      fields.opacity() = moved ? 0.5f : 1.0f;
      return;
    case Property::BlendMode:
      fields.blendMode() =
          moved ? SkBlendMode::kMultiply : SkBlendMode::kSrcOver;
      return;
    case Property::BackgroundOrigin:
      fields.backgroundOrigin() =
          moved ? BackgroundOrigin::ContentBox : BackgroundOrigin::BorderBox;
      return;
    case Property::ZIndex:
      fields.zIndex() = moved ? 1 : 0;
      return;
    case Property::TranslateX:
      fields.translateX() = turn;
      return;
    case Property::TranslateY:
      fields.translateY() = turn;
      return;
    case Property::Rotate:
      fields.rotate() = turn;
      return;
    case Property::Scale:
      fields.scale() = 1.0f + turn;
      return;
    case Property::ScaleX:
      fields.scaleX() = 1.0f + turn;
      return;
    case Property::ScaleY:
      fields.scaleY() = 1.0f + turn;
      return;
    case Property::SkewX:
      fields.skewX() = turn;
      return;
    case Property::SkewY:
      fields.skewY() = turn;
      return;
    case Property::TransformOrigin:
      fields.transformOrigin().x = moved ? Dimension(4.0f) : pct(50.0f);
      return;
    case Property::RotateX:
      node.rotateX() = turn;
      return;
    case Property::RotateY:
      node.rotateY() = turn;
      return;
    case Property::TranslateZ:
      node.translateZ() = turn;
      return;
    case Property::ScaleZ:
      node.scaleZ() = 1.0f + turn;
      return;
    case Property::Perspective:
      node.perspective() = turn;
      return;
    case Property::PerspectiveOrigin:
      node.perspectiveOrigin().x = moved ? Dimension(4.0f) : pct(50.0f);
      return;
    case Property::TransformOriginZ:
      if (moved)
        node.transformOriginZ().ensure().originZ = length;
      else
        node.transformOriginZ();
      return;
    case Property::Preserve3d:
      node.preserve3d() = moved;
      return;
    case Property::Backface:
      node.backface() =
          moved ? material::Backface::Hidden : material::Backface::Visible;
      return;
    case Property::DecorationOutline:
      node.decorationOutline().source =
          moved ? Boundary::Glyphs : Boundary::Auto;
      return;
    case Property::Font:
      if (moved)
        node.font().font.emplace().weight = 700.0f;
      else
        node.font();
      return;
    case Property::Paragraph:
      if (moved)
        node.paragraph().block.emplace();
      else
        node.paragraph();
      return;
    case Property::Ink:
      node.ink().statesInk = moved;
      return;
    case Property::CustomProperties:
      if (moved)
        node.customProperties().vars.set(var("moved"), length);
      else
        node.customProperties();
      return;
    case Property::ImageRendering:
      if (moved)
        node.imageRendering().sampling =
            SkSamplingOptions(SkFilterMode::kLinear);
      else
        node.imageRendering();
      return;
    case Property::kCount:
      break;
  }
  ADD_FAILURE() << "no writer is walked for property #" << (int)property;
}

}  // namespace

TEST(ComposeDeclarations, EveryWriterMarksExactlyItsOwnProperty) {
  for (size_t i = 0; i < (size_t)Property::kCount; ++i) {
    const auto property = (Property)i;
    for (const bool moved : {false, true}) {
      cd::ElementNode node;
      writeThroughWriter(node, property, moved);
      PropertyMask expected;
      expected.set(property);
      EXPECT_TRUE(node.fields.declared() == expected)
          << propertyName(property) << "'s writer marked a different set "
          << "of properties than its own";
    }
  }
}

TEST(ComposeDeclarations, EveryWrittenValueReachesThePrune) {
  // Both nodes state the property, so the masks agree and only the value
  // can tell them apart.
  for (size_t i = 0; i < (size_t)Property::kCount; ++i) {
    const auto property = (Property)i;
    cd::ElementNode standing, moved;
    writeThroughWriter(standing, property, false);
    writeThroughWriter(moved, property, true);
    EXPECT_FALSE(cd::propertiesEqual(standing, moved))
        << propertyName(property) << " was written to a new value and the "
        << "node compares equal to one that kept the old";
  }
}

TEST(ComposeDeclarations, AWriterEndsAKeywordWrittenBeforeIt) {
  for (size_t i = 0; i < (size_t)Property::kCount; ++i) {
    const auto property = (Property)i;
    if (!answersKeyword(property)) continue;
    cd::ElementNode node;
    node.fields.keyword(property, sigil::weave::Keyword::Inherit);
    ASSERT_TRUE(node.fields.keywords() &&
                node.fields.keywords()->find(property));
    writeThroughWriter(node, property, true);
    EXPECT_FALSE(node.fields.keywords()->find(property))
        << propertyName(property) << " was written as a value after a "
        << "keyword and the keyword still stands";
    EXPECT_TRUE(node.fields.declared().has(property));
  }
}

TEST(ComposeDeclarations, EveryWriterWritesTheFieldItsPropertyIsKeptIn) {
  // A writer handing back its neighbour's field marks the right bit and
  // moves a value the prune sees, so the two walks above pass it. Where the
  // value lands is what tells them apart: the property copied alone out of
  // the node's style must carry everything the writer moved, and a
  // property the style does not carry must move nothing the style holds.
  const cd::ComputedStyle initial;
  for (size_t i = 0; i < (size_t)Property::kCount; ++i) {
    const auto property = (Property)i;
    cd::ElementNode node;
    writeThroughWriter(node, property, true);
    cd::ComputedStyle written;
    cd::resolveStyle(nullptr, node, written);
    cd::ComputedStyle copied;
    cd::copyProperty(property, written, copied);
    EXPECT_TRUE(cd::computedStyleEqual(copied, written))
        << propertyName(property) << "'s writer moved a field its property "
        << "is not kept in";
    const bool carried =
        answersKeyword(property) && !inheritsByDefault(property);
    EXPECT_EQ(!cd::computedStyleEqual(written, initial), carried)
        << propertyName(property)
        << (carried ? "'s writer moved nothing the "
                      "computed style carries"
                    : " is kept on the node, and its "
                      "writer moved the computed style");
  }
}

TEST(ComposeDeclarations, TheOperatorsZIndexIsAStartingValueNotAStatement) {
  // What an operator gives an element it adds carries no bit, so the fold
  // lets a matching rule's value through; an element that states its own,
  // even the default, keeps it, and the operator's never lands.
  cd::RuleLayer rules;
  rules.declared.set(Property::ZIndex);
  rules.values.paint.zIndex = 3;

  cd::ElementNode added;
  cd::startWithOperatorZIndex(added.fields, -1);
  EXPECT_FALSE(added.fields.declared().has(Property::ZIndex));
  cd::ComputedStyle alone, underRule;
  cd::resolveStyle(nullptr, added, alone);
  cd::resolveStyle(nullptr, added, underRule, &rules);
  EXPECT_EQ(alone.paint.zIndex, -1);
  EXPECT_EQ(underRule.paint.zIndex, 3);

  cd::ElementNode stating;
  stating.fields.zIndex() = 0;
  cd::startWithOperatorZIndex(stating.fields, -1);
  cd::ComputedStyle own;
  cd::resolveStyle(nullptr, stating, own, &rules);
  EXPECT_EQ(own.paint.zIndex, 0);
}
