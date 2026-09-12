// A SLOT IN THE FLOW: the rect a reserved run opens, the child that paints
// inside it, what happens to a line it is taller than, and the namespace the
// slot key lives in.
//
// The text binary's share of the content suites, one file per subject.

#include "DressedTypeProbes.h"

namespace {

/** A caption with one reserved slot, and a pill child keyed for it. */
Element pillCaption(const std::string& childKey, float width,
                    SkSize size = {34, 16}) {
  // The width lives on an inner box: the render root is always resized to
  // the composer's own size, so a width written there is overwritten.
  return box().child(box().padding(8).width(width).child(
      text(sigil::weave::rich(coloredStyle(18, SK_ColorWHITE))
               .add(u8"press the archive key ")
               .slot("pill", size, 4)
               .add(u8" to continue the long descent"))
          .key("caption")
          .child(box().key(std::move(childKey)).fill(red()))));
}

}  // namespace

TEST(TextSlot, ASlotTallerThanTheTypeOpensTheLinesItSitsIn) {
  // The reserved box is one unbreakable word of the flow, so a box that
  // reaches further above the baseline than the face does is a fact about
  // the strut: the band is deep enough BEFORE a break is decided, and the
  // pill has room rather than being drawn over the line above it.
  const auto baselinesWithSlot = [](SkSize size) {
    Host host(300, 260);
    host.composer.render(pillCaption("pill", 280, size));
    host.frame();
    std::vector<float> found;
    for (const TextUnit& line : host.composer.units(
             "caption", sigil::weave::selectors::each(sigil::weave::Unit::Line),
             sigil::weave::Unit::Line))
      found.push_back(line.axis);
    return found;
  };
  const std::vector<float> small = baselinesWithSlot({34, 16});
  ASSERT_GE(small.size(), 2u);
  const std::vector<float> tall = baselinesWithSlot({34, 60});
  ASSERT_EQ(tall.size(), small.size());
  const float smallPitch = small[1] - small[0];
  const float tallPitch = tall[1] - tall[0];
  EXPECT_GT(tallPitch, smallPitch + 20.0f)
      << "a 60 px box in an 18 px face left the pitch where it was";
  // The room goes where the box needs it: the box's bottom sits 4 px below
  // the baseline, so all of the rest is above it and the first baseline
  // moves down by what the box asked for.
  EXPECT_GT(tall.front(), small.front() + 20.0f);
}

TEST(TextSlot, AChildPaintsInsideTheReservedRect) {
  Host host(300, 200);
  host.composer.render(pillCaption("pill", 280));
  host.frame();

  const std::optional<SkRect> rect = host.composer.bounds("pill");
  ASSERT_TRUE(rect);
  // The box IS the size the content reserved — nothing about the child's
  // own description decides it.
  EXPECT_FLOAT_EQ(rect->width(), 34.0f);
  EXPECT_FLOAT_EQ(rect->height(), 16.0f);
  // …and the fill lands inside it.
  EXPECT_EQ(host.pixel((int)rect->centerX(), (int)rect->centerY()),
            SK_ColorRED);
  // The reserved run is blank: the caption's own words sit either side of
  // it, never through it.
  EXPECT_GT(countColor(host, SkIRect::MakeXYWH(0, 0, 300, 200), SK_ColorWHITE),
            20);
}

TEST(TextSlot, TheReservedRunIsUnbreakableAndMovesOnRelayout) {
  // The placeholder re-resolves with the paragraph: narrow the box and the
  // pill lands on a different line, at a different place on it.
  Host wide(300, 200), narrow(300, 200);
  wide.composer.render(pillCaption("pill", 280));
  wide.frame();
  narrow.composer.render(pillCaption("pill", 120));
  narrow.frame();

  const std::optional<SkRect> a = wide.composer.bounds("pill");
  const std::optional<SkRect> b = narrow.composer.bounds("pill");
  ASSERT_TRUE(a && b);
  EXPECT_NE(a->top(), b->top());
  // Same reserved size wherever it lands — the box travels, it does not
  // stretch, and no line breaks inside it.
  EXPECT_FLOAT_EQ(a->width(), b->width());
  EXPECT_FLOAT_EQ(a->height(), b->height());
  EXPECT_EQ(narrow.pixel((int)b->centerX(), (int)b->centerY()), SK_ColorRED);
}

TEST(TextSlot, ATallSlotOpensItsLine) {
  // The breakers treat the reserved box as a word with a height, so a slot
  // taller than the type pushes the whole paragraph down.
  Host shortPill(300, 240), tallPill(300, 240);
  shortPill.composer.render(pillCaption("pill", 280, {34, 16}));
  shortPill.frame();
  tallPill.composer.render(pillCaption("pill", 280, {34, 60}));
  tallPill.frame();
  const std::optional<SkRect> a = shortPill.composer.bounds("caption");
  const std::optional<SkRect> b = tallPill.composer.bounds("caption");
  ASSERT_TRUE(a && b);
  EXPECT_GT(b->height(), a->height());
}

TEST(TextSlot, AnUnknownKeyDrawsNothing) {
  // The loud-once member of the silent-no-op family: a child keyed for a
  // slot the content never reserved lays out at zero and paints nothing.
  Host host(300, 200);
  host.composer.render(pillCaption("typo", 280));
  host.frame();
  const std::optional<SkRect> rect = host.composer.bounds("typo");
  ASSERT_TRUE(rect);
  EXPECT_TRUE(rect->isEmpty());
  EXPECT_EQ(countColor(host, SkIRect::MakeXYWH(0, 0, 300, 200), SK_ColorRED),
            0);
}

TEST(TextSlot, TheHitTestReachesThePillChild) {
  Host host(300, 200);
  host.composer.render(pillCaption("pill", 280));
  host.frame();
  const std::optional<SkRect> rect = host.composer.bounds("pill");
  ASSERT_TRUE(rect);
  const std::optional<std::string> hit =
      host.composer.hitTest({rect->centerX(), rect->centerY()});
  ASSERT_TRUE(hit);
  EXPECT_EQ(*hit, "pill");
}

TEST(TextSlot, TheSlotNamespaceIsTheValuesOwnNotTheMountRegistry) {
  // Two captions may both reserve "icon" without colliding, because a text
  // slot is matched against THIS text node's children and nowhere else.
  Host host(320, 240);
  auto caption = [](SkColor ink, const char8_t* words) {
    return text(sigil::weave::rich(coloredStyle(16, SK_ColorWHITE))
                    .slot("icon", {20, 12}, 2)
                    .add(words))
        .child(box().key("icon").fill(Fill::color(SkColor4f::FromColor(ink))));
  };
  host.composer.render(
      box()
          .column()
          .padding(6)
          .width(300)
          .child(caption(SK_ColorRED, u8" first line of the pair").key("a"))
          .child(
              caption(SK_ColorGREEN, u8" second line of the pair").key("b")));
  host.frame();
  const SkIRect all = SkIRect::MakeXYWH(0, 0, 320, 240);
  EXPECT_GT(countColor(host, all, SK_ColorRED), 20);
  EXPECT_GT(countColor(host, all, SK_ColorGREEN), 20);
}
