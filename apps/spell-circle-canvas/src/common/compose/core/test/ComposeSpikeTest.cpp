// Pins the Yoga/SigilWeave seam that SigilCompose's text leaves are built
// on: a Yoga node whose size comes from a measure callback running real
// paragraph layout, and whose baseline comes from that layout's first line.
//
// Deliberately talks to Yoga and SigilWeave directly, with no Composer in
// the picture. If a text leaf mis-sizes, these tests say whether the seam
// itself broke or only compose's use of it did.

#include <gtest/gtest.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Style.h>
#include <yoga/Yoga.h>

#include <cmath>
#include <memory>
#include <string_view>

namespace {

using namespace sigil::weave;

FontContext& sharedContext() {
  static auto* context = new FontContext(ports::systemFontManager());
  return *context;
}

TextStyle styleAt(float fontSize) {
  TextStyle style;
  style.shaping.fontSize = fontSize;
  return style;
}

/** Stands in for a text leaf: a paragraph plus the layout produced by the
 *  most recent measurement, which a painter would reuse rather than
 *  laying out again. */
struct TextLeaf {
  explicit TextLeaf(std::u8string_view utf8, float fontSize) {
    paragraph.appendText(utf8, styleAt(fontSize));
  }

  Paragraph paragraph;
  ParagraphLayout layout;
  std::vector<LineMetrics> lines;
  float measuredWidth = -1.0f;

  void layoutAt(float width) {
    BlockFlow flow(SkRect::MakeWH(width, 100000.0f));
    layout = layoutParagraph(sharedContext(), paragraph, flow, {});
    lines = layout.lineMetrics(paragraph);
    measuredWidth = width;
  }

  SkRect bounds() const {
    SkRect rect = SkRect::MakeEmpty();
    for (const LineMetrics& line : lines) rect.join(line.rect());
    return rect;
  }
};

/** Yoga measure callback → SigilWeave layout at the constraint width. */
YGSize measureText(YGNodeConstRef node, float width, YGMeasureMode widthMode,
                   float /*height*/, YGMeasureMode /*heightMode*/) {
  auto* leaf = static_cast<TextLeaf*>(YGNodeGetContext(node));
  const float constraint =
      widthMode == YGMeasureModeUndefined ? 100000.0f : width;
  leaf->layoutAt(constraint);
  SkRect bounds = leaf->bounds();
  return {std::ceil(bounds.width()), std::ceil(bounds.height())};
}

/** Yoga baseline callback → the first line's baseline from SigilWeave. */
float baselineOfText(YGNodeConstRef node, float /*width*/, float /*height*/) {
  auto* leaf = static_cast<TextLeaf*>(YGNodeGetContext(node));
  if (leaf->lines.empty()) return 0.0f;
  const LineMetrics& first = leaf->lines.front();
  return first.baseline - first.rect().top();
}

YGNodeRef makeTextNode(TextLeaf& leaf) {
  YGNodeRef node = YGNodeNew();
  YGNodeSetContext(node, &leaf);
  YGNodeSetMeasureFunc(node, measureText);
  YGNodeSetBaselineFunc(node, baselineOfText);
  YGNodeSetNodeType(node, YGNodeTypeText);
  return node;
}

}  // namespace

// A leaf sized by its own measured text — the "box grows to fit the type"
// behaviour, which only happens if the measure callback's return value
// survives flexbox sizing.
TEST(PosterSpike, TextLeafSizesToMeasurement) {
  TextLeaf leaf(u8"IFRIT PROTOCOL", 48.0f);

  YGNodeRef root = YGNodeNew();
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetWidth(root, 1080);
  YGNodeStyleSetHeight(root, 400);
  YGNodeStyleSetPadding(root, YGEdgeAll, 40);
  // Flexbox's align-items default is stretch, which would override the
  // measured height on the cross axis and make the height assertions below
  // meaningless. Flex-start is what leaves the measurement visible.
  YGNodeStyleSetAlignItems(root, YGAlignFlexStart);

  YGNodeRef text = makeTextNode(leaf);
  YGNodeInsertChild(root, text, 0);

  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

  EXPECT_EQ(YGNodeLayoutGetLeft(text), 40.0f);
  EXPECT_EQ(YGNodeLayoutGetTop(text), 40.0f);
  // Loose bounds on purpose: the exact extent depends on which system font
  // resolves, so these only assert that one unwrapped line at 48px landed
  // in a plausible range rather than stretching to the 1080-wide row.
  EXPECT_GT(YGNodeLayoutGetWidth(text), 200.0f);
  EXPECT_LT(YGNodeLayoutGetWidth(text), 1000.0f);
  EXPECT_GT(YGNodeLayoutGetHeight(text), 30.0f);
  EXPECT_LT(YGNodeLayoutGetHeight(text), 120.0f);
  EXPECT_EQ(leaf.lines.size(), 1u);

  YGNodeFreeRecursive(root);
}

// Constrained width wraps through SigilWeave: Yoga's measure constraint
// really reaches the line breaker.
TEST(PosterSpike, ConstrainedWidthWrapsText) {
  TextLeaf wide(u8"the quick brown fox jumps over the lazy dog", 24.0f);
  TextLeaf narrow(u8"the quick brown fox jumps over the lazy dog", 24.0f);

  YGNodeRef wideRoot = YGNodeNew();
  YGNodeStyleSetWidth(wideRoot, 2000);
  YGNodeRef wideText = makeTextNode(wide);
  YGNodeInsertChild(wideRoot, wideText, 0);
  YGNodeCalculateLayout(wideRoot, YGUndefined, YGUndefined, YGDirectionLTR);

  YGNodeRef narrowRoot = YGNodeNew();
  YGNodeStyleSetWidth(narrowRoot, 160);
  YGNodeRef narrowText = makeTextNode(narrow);
  YGNodeInsertChild(narrowRoot, narrowText, 0);
  YGNodeCalculateLayout(narrowRoot, YGUndefined, YGUndefined, YGDirectionLTR);

  EXPECT_EQ(wide.lines.size(), 1u);
  EXPECT_GT(narrow.lines.size(), 2u);
  EXPECT_GT(YGNodeLayoutGetHeight(narrowText), YGNodeLayoutGetHeight(wideText));

  YGNodeFreeRecursive(wideRoot);
  YGNodeFreeRecursive(narrowRoot);
}

// Baseline alignment across mixed type sizes. This is the one thing the
// baseline callback exists for: without it Yoga would align the two nodes
// on their boxes, and type at different sizes would sit off the same line.
TEST(PosterSpike, MixedSizesAlignOnBaseline) {
  TextLeaf big(u8"Poster", 64.0f);
  TextLeaf small(u8"vol. 4", 16.0f);

  YGNodeRef row = YGNodeNew();
  YGNodeStyleSetFlexDirection(row, YGFlexDirectionRow);
  YGNodeStyleSetAlignItems(row, YGAlignBaseline);
  YGNodeStyleSetWidth(row, 1080);

  YGNodeRef bigNode = makeTextNode(big);
  YGNodeRef smallNode = makeTextNode(small);
  YGNodeInsertChild(row, bigNode, 0);
  YGNodeInsertChild(row, smallNode, 1);

  YGNodeCalculateLayout(row, YGUndefined, YGUndefined, YGDirectionLTR);

  // Absolute baseline y = node top + baseline offset within the node.
  const float bigBaseline =
      YGNodeLayoutGetTop(bigNode) + baselineOfText(bigNode, 0, 0);
  const float smallBaseline =
      YGNodeLayoutGetTop(smallNode) + baselineOfText(smallNode, 0, 0);
  EXPECT_NEAR(bigBaseline, smallBaseline, 1.0f);
  // And the small node's top is pushed down to make that happen.
  EXPECT_GT(YGNodeLayoutGetTop(smallNode), YGNodeLayoutGetTop(bigNode));

  YGNodeFreeRecursive(row);
}
