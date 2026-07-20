// Feasibility spike for the SigilCompose design (see ../DESIGN.md):
// proves that Yoga's measure and baseline callbacks can be driven by
// SigilWeave, which is the load-bearing assumption of the whole proposal.
//
// No poster API exists yet — this talks to Yoga and SigilWeave directly,
// the way the future layout pass would.

#include <sigilweave/Flow.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/Paragraph.h>
#include <sigilweave/ParagraphLayout.h>
#include <sigilweave/Style.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <yoga/Yoga.h>

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string_view>

namespace {

using namespace sigil::weave;

FontContext &sharedContext() {
  static auto *context = new FontContext(ports::systemFontManager());
  return *context;
}

TextStyle styleAt(float fontSize) {
  TextStyle style;
  style.shaping.fontSize = fontSize;
  return style;
}

/** The future Text leaf: a paragraph plus the layout produced by the
 *  last measurement, which painting would reuse. */
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
    for (const LineMetrics &line : lines)
      rect.join(line.rect());
    return rect;
  }
};

/** Yoga measure callback → SigilWeave layout at the constraint width. */
YGSize measureText(YGNodeConstRef node, float width, YGMeasureMode widthMode,
                   float /*height*/, YGMeasureMode /*heightMode*/) {
  auto *leaf = static_cast<TextLeaf *>(YGNodeGetContext(node));
  const float constraint =
      widthMode == YGMeasureModeUndefined ? 100000.0f : width;
  leaf->layoutAt(constraint);
  SkRect bounds = leaf->bounds();
  return {std::ceil(bounds.width()), std::ceil(bounds.height())};
}

/** Yoga baseline callback → the first line's baseline from SigilWeave. */
float baselineOfText(YGNodeConstRef node, float /*width*/, float /*height*/) {
  auto *leaf = static_cast<TextLeaf *>(YGNodeGetContext(node));
  if (leaf->lines.empty())
    return 0.0f;
  const LineMetrics &first = leaf->lines.front();
  return first.baseline - first.rect().top();
}

YGNodeRef makeTextNode(TextLeaf &leaf) {
  YGNodeRef node = YGNodeNew();
  YGNodeSetContext(node, &leaf);
  YGNodeSetMeasureFunc(node, measureText);
  YGNodeSetBaselineFunc(node, baselineOfText);
  YGNodeSetNodeType(node, YGNodeTypeText);
  return node;
}

} // namespace

// A leaf sized by its own measured text: the poster's "box grows to fit
// the type" behavior.
TEST(PosterSpike, TextLeafSizesToMeasurement) {
  TextLeaf leaf(u8"IFRIT PROTOCOL", 48.0f);

  YGNodeRef root = YGNodeNew();
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetWidth(root, 1080);
  YGNodeStyleSetHeight(root, 400);
  YGNodeStyleSetPadding(root, YGEdgeAll, 40);
  // Flexbox's align-items default is stretch, which would override the
  // measured height on the cross axis — the poster API should default
  // text leaves away from that (design note).
  YGNodeStyleSetAlignItems(root, YGAlignFlexStart);

  YGNodeRef text = makeTextNode(leaf);
  YGNodeInsertChild(root, text, 0);

  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

  EXPECT_EQ(YGNodeLayoutGetLeft(text), 40.0f);
  EXPECT_EQ(YGNodeLayoutGetTop(text), 40.0f);
  // One unwrapped line at 48px: a plausible headline extent, and the
  // node adopted the measured size rather than stretching to the row.
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
  YGNodeCalculateLayout(narrowRoot, YGUndefined, YGUndefined,
                        YGDirectionLTR);

  EXPECT_EQ(wide.lines.size(), 1u);
  EXPECT_GT(narrow.lines.size(), 2u);
  EXPECT_GT(YGNodeLayoutGetHeight(narrowText),
            YGNodeLayoutGetHeight(wideText));

  YGNodeFreeRecursive(wideRoot);
  YGNodeFreeRecursive(narrowRoot);
}

// Baseline alignment across mixed type sizes — the typographic feature
// that justifies Yoga's baseline callback in the design.
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
