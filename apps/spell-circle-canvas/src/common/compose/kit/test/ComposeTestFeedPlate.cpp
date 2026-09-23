// kit/Plate.h — the bordered strip a feed is set in: the border and the
// rows it holds, how many feeds fit one plate, and what a row factory
// declares that the plain kernel column does not.

#include <sigilcompose/core/Feed.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilmotion/values/Keyframes.h>

#include <algorithm>

#include "support/StudioTestSupport.h"

TEST(ComposeFeed, PlateIsTheBorderedStripAFeedIsSetIn) {
  // A bordered plate holding N feeds with dividers between them. The plate
  // does NOT place itself: it takes the rect, following the same rule every
  // layout scheme does.
  Host host(240, 120);
  feed::TextRing a, b;
  a.append({u8"alpha"});
  b.append({u8"beta"});

  feed::TextOptions style;
  style.styles.base(
      weave::textStyle({.size = 9, .color = SkColor4f{1, 1, 1, 1}}));
  style.window.gap = 1.0f;

  auto strip = [&] {
    return box().children(
        {kit::plate({.columns = {feed::feed(a, style), feed::feed(b, style)},
                     .paddingX = 10,
                     .paddingY = 6,
                     .gap = 8,
                     .fill = Fill::color({0, 0, 0.5f, 1}),
                     .border = green(),
                     .divider = red()})
             .key("plate")
             .rect(SkRect::MakeXYWH(20, 20, 200, 80))});
  };
  host.composer.render(strip());
  host.frame();

  ASSERT_TRUE(host.composer.bounds("plate").has_value());
  EXPECT_EQ(require(host.composer.bounds("plate")),
            SkRect::MakeXYWH(20, 20, 200, 80));
  // The ground, inside the keyline.
  EXPECT_EQ(SkColorGetB(host.pixel(120, 25)), 128);
  // The inner keyline sits ON the edge, not outside it.
  EXPECT_GT(SkColorGetG(host.pixel(120, 20)), 180);
  EXPECT_EQ(host.pixel(120, 19), SK_ColorBLACK);
  // The divider: one red column at the horizontal midpoint of the interior.
  int redColumns = 0;
  for (int x = 21; x < 219; ++x)
    if (SkColorGetR(host.pixel(x, 60)) > 180 &&
        SkColorGetG(host.pixel(x, 60)) < 80)
      ++redColumns;
  EXPECT_EQ(redColumns, 1) << "a row plate of two feeds has exactly one "
                              "divider between them";

  // Re-describing an unchanged plate prunes: it is composition over the
  // kernel and adds no volatility of its own.
  host.composer.render(strip());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);

  // A column plate puts the divider on the other axis, and the same call
  // site says so with one field.
  Host col(240, 120);
  col.composer.render(box().children(
      {kit::plate({.columns = {feed::feed(a, style), feed::feed(b, style)},
                   .column = true,
                   .paddingX = 10,
                   .paddingY = 6,
                   .gap = 8,
                   .fill = Fill::color({0, 0, 0.5f, 1}),
                   .divider = red()})
           .rect(SkRect::MakeXYWH(20, 20, 200, 80))}));
  col.frame();
  int redRows = 0;
  for (int y = 21; y < 99; ++y)
    if (SkColorGetR(col.pixel(120, y)) > 180 &&
        SkColorGetG(col.pixel(120, y)) < 80)
      ++redRows;
  EXPECT_EQ(redRows, 1);

  // tinted() builds one style per named colour from a single face and size.
  // The names carry no meaning to it, deliberately: what a study calls its
  // passing ink is the study's convention, not the library's.
  const weave::TypeSheet mono = kit::tinted(nullptr, 10.5f, {1, 1, 1, 1},
                                            {{"dim", {0.5f, 0.5f, 0.5f, 1}},
                                             {"pass", {0, 1, 0, 1}},
                                             {"fail", {1, 0, 0, 1}}});
  EXPECT_FLOAT_EQ(mono.base().shaping.fontSize, 10.5f);
  ASSERT_EQ(mono.size(), 3u);
  EXPECT_FLOAT_EQ(mono["pass"].shaping.fontSize, 10.5f);
  EXPECT_FLOAT_EQ(mono["fail"].paint.foreground.getColor4f().fR, 1.0f);
  EXPECT_FLOAT_EQ(mono["fail"].paint.foreground.getColor4f().fG, 0.0f);
  // And a name nobody registered still sets: it takes the base ink.
  EXPECT_TRUE(mono["passs"] == mono.base());
}

TEST(ComposeFeed, VisibleRowsHaveAHeightAndThreeFeedsFitOnePlate) {
  // Fitting several feeds with dividers into ONE fixed-height plate
  // otherwise means hand-tuning that height against font size times row
  // count. feed::height() is that number, and the plate below is built
  // from it with no slack at all.
  feed::TextOptions st;
  st.styles.base(
      weave::textStyle({.size = 9.2f, .color = SkColor4f{1, 1, 1, 1}}));
  st.window.gap = 1.0f;
  st.window.visible = 12;

  const float rows = feed::height(st, fonts());
  EXPECT_GT(rows, 12.0f * 9.2f) << "twelve 9.2 px rows plus eleven gaps";
  EXPECT_LT(rows, 12.0f * 9.2f * 3.0f);

  // The window CLAMPS: the feed shows the newest visible rows, so asking
  // for more rows than the options show is the same height.
  EXPECT_FLOAT_EQ(feed::height(st, 400, fonts()), rows);
  EXPECT_LT(feed::height(st, 6, fonts()), rows);

  // The snapshot()/intrinsicSize() rule — those size by the root's CHILDREN,
  // not the root's own dims — is DEMONSTRATED, not assumed: feed() returns a
  // column that sets neither width nor height, so the shell box the
  // implementation wraps it in cannot change the answer.
  feed::TextRing full;
  for (int i = 0; i < 20; ++i) full.append({u8"the ring outruns its window"});
  EXPECT_FLOAT_EQ(intrinsicSize(feed::feed(full, st), fonts()).height(), rows)
      << "the un-shelled spelling disagrees — feed() grew its own dims";

  // And it is the height the feed ACTUALLY takes when laid out: three feeds
  // and two dividers in a column plate sized from the answer, with no room
  // to spare, must not shrink. A wrong answer here is SILENT — flex shrink
  // absorbs the deficit and every feed quietly loses rows.
  feed::TextRing a, b, c;
  for (int i = 0; i < 20; ++i) {
    a.append({u8"alpha"});
    b.append({u8"beta"});
    c.append({u8"gamma"});
  }
  const float padY = 9.0f, gap = 6.0f, div = 1.0f;
  const float panelH = 2 * padY + 3 * rows + 4 * gap + 2 * div;

  Host host(320, (int)std::ceil(panelH) + 40);
  auto divider = [&] { return box().height(div).fill(green()); };
  host.composer.render(box().children(
      {box()
           .key("panel")
           .padding(padY, 12.0f)
           .column()
           .gap(gap)
           .children({feed::feed(a, st).key("feedA")})
           .children({divider()})
           .children({feed::feed(b, st).key("feedB")})
           .children({divider()})
           .children({feed::feed(c, st).key("feedC")})
           .rect(SkRect::MakeXYWH(10, 10, 300, panelH))}));
  host.frame();

  ASSERT_TRUE(host.composer.bounds("panel").has_value());
  EXPECT_FLOAT_EQ(require(host.composer.bounds("panel")).height(), panelH);
  for (const char* k : {"feedA", "feedB", "feedC"}) {
    ASSERT_TRUE(host.composer.bounds(k).has_value()) << k;
    EXPECT_FLOAT_EQ(require(host.composer.bounds(k)).height(), rows)
        << k << " shrank: feed::height() is not the laid-out height";
  }
  // The three feeds tile the interior exactly — the last one ends on the
  // padding, with nothing clipped and nothing left over.
  EXPECT_FLOAT_EQ(require(host.composer.bounds("feedC")).bottom(),
                  10.0f + panelH - padY);
}

TEST(ComposeFeed, TheRowFactoryDeclaresTheEntranceAndTheColumnIsPlainKernel) {
  // What a row IS belongs to the caller. feed() windows the ring and keys
  // each row by its sequence id; everything else — the style, an entrance,
  // an fx track — is whatever the factory returns. The hand-built column
  // below IS what feed() builds, so it reconciles onto it with nothing
  // patched, and an author who needs something the options do not carry can
  // write that column themselves without losing the identity discipline.
  feed::TextOptions st;
  st.styles.base(weave::textStyle({.size = 20, .color = SkColor4f{1, 1, 1, 1}}))
      .set("alert", weave::Type{.color = material::skia::toSkColor(
                                    SkColor4f{1, 0, 0, 1})});
  st.window.gap = 4.0f;
  feed::TextRing ring;
  ring.append({u8"AAAA"});
  ring.append({u8"BBBB", "alert"});

  Host host(220, 90);
  host.composer.render(box().children({feed::feed(ring, st)}));
  host.frame();
  auto byHand = [&](bool staggered) {
    auto column = box().column().gap(st.window.gap).overflow(Overflow::Clip);
    if (staggered) column.staggerChildren(400ms);
    for (const feed::Row<feed::TextRow>& r : ring.rows()) {
      Element row = feed::textRow(r.value, st.styles);
      row.key(feed::rowKey(r.sequence));
      if (staggered)
        row.opacity(animate(motion::from(0.0f).to(1.0f),
                            {200ms, &choreograph::easeNone}));
      column.children({std::move(row)});
    }
    return box().children({std::move(column)});
  };
  host.composer.render(byHand(false));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "feed()'s column is not the spelling an author has to reproduce";

  // The row key is the sequence id, and the row's named style is the one it
  // is set in.
  ASSERT_TRUE(host.composer.bounds(feed::rowKey(1)).has_value());
  ASSERT_TRUE(host.composer.bounds(feed::rowKey(2)).has_value());
  const SkRect band = require(host.composer.bounds(feed::rowKey(2)));
  int redInk = 0;
  for (int y = (int)band.top(); y < (int)band.bottom(); ++y)
    for (int x = (int)band.left(); x < (int)band.right(); ++x) {
      const SkColor c = host.pixel(x, y);
      redInk += SkColorGetR(c) > 150 && SkColorGetG(c) < 80;
    }
  EXPECT_GT(redInk, 10) << "the row did not take the style it named";

  // And now the feed types out: each row's OWN mount animation is what the
  // container cascade has to delay, so row 2 is still dark while row 1 has
  // finished.
  auto brightest = [](Host& h, SkRect r) {
    int best = 0;
    for (int y = (int)r.top(); y < (int)r.bottom(); ++y)
      for (int x = (int)r.left(); x < (int)r.right(); ++x)
        best = std::max(best, (int)SkColorGetR(h.pixel(x, y)));
    return best;
  };
  Host typed(220, 90);
  typed.composer.render(byHand(true));
  typed.frame(0.25);  // row 1's 200 ms is done; row 2 waits out its 400 ms
  const SkRect r1 = require(typed.composer.bounds(feed::rowKey(1)));
  const SkRect r2 = require(typed.composer.bounds(feed::rowKey(2)));
  EXPECT_GT(brightest(typed, r1), 150);
  EXPECT_LT(brightest(typed, r2), 40) << "the stagger did not delay row 2";
  typed.frame(0.5);  // t = 0.75 — row 2 is 350 ms into its own 200 ms
  EXPECT_GT(brightest(typed, r2), 150);
}
