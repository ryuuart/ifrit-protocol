#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/Feed.h>

#include <numeric>

#include "ComposeTestSupport.h"

// ---- feed(): the streaming collection --------------------------------------

namespace {

/** Plain rows, white on the Host's black ground so ink reads as brightness. */
feed::TextOptions feedOptions(size_t visible, float size = 12.0f) {
  feed::TextOptions options;
  options.styles.base(whiteStyle(size));
  options.window.visible = visible;
  options.window.gap = 2.0f;
  return options;
}

/** The brightest ink inside a rect — how lit a row is, whatever it says. */
int brightestIn(Host& host, SkRect r) {
  int best = 0;
  for (int y = (int)r.top(); y < (int)r.bottom(); ++y)
    for (int x = (int)r.left(); x < (int)r.right(); ++x)
      best = std::max(best, (int)SkColorGetR(host.pixel(x, y)));
  return best;
}

}  // namespace

TEST(ComposeFeed, AnAppendCostsOneMountAndNeverRerecordsTheRowsAboveIt) {
  // Rows are keyed by their sequence id, not their position in the visible
  // window, so an append shifts nothing: surviving rows prune and keep their
  // pictures, and only the new tail mounts while the scrolled-out head
  // unmounts. Position keys would give every visible row a new key on every
  // append and re-patch the whole window.
  feed::TextRing ring;
  for (int i = 0; i < 30; ++i)
    ring.append({util::toU8("boot sequence line " + std::to_string(i))});
  const feed::TextOptions options = feedOptions(10);
  Host host(200, 400);
  auto describe = [&] {
    return box().padding(6).child(feed::feed(ring, options));
  };
  host.composer.render(describe());
  host.frame();  // records the visible window

  ring.append({util::toU8("intrusion detected")});
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);  // the new tail only
  host.frame();
  // Ancestor chain re-records + the tail's own picture; the nine surviving
  // rows replay their cached pictures untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 4u);

  // The price is CONSTANT, which is the whole claim: a second append costs
  // exactly what the first did, and the retained tree does not grow.
  const size_t liveAfterFirst = host.composer.stats().instances;
  ring.append({util::toU8("second intrusion")});
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_LE(host.composer.stats().picturesRecorded, 4u);
  EXPECT_EQ(host.composer.stats().instances, liveAfterFirst)
      << "the window is bounded: one mount in, one unmount out";
}

TEST(ComposeFeed, ASurvivingRowKeepsItsInstanceRatherThanReentering) {
  // The sharper form of the same property. A row whose factory declares a
  // mount entrance re-runs that entrance whenever it MOUNTS, so a row that
  // was silently remounted by an append would flash back to nothing. Every
  // row here is fully lit before the append, and must still be after it.
  feed::TextRing ring;
  for (int i = 0; i < 4; ++i) ring.append({util::toU8("row")});
  const feed::TextOptions options = feedOptions(6, 16.0f);
  auto lit = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .opacity(animate(from(0.0f).to(1.0f), {200ms, &choreograph::easeNone}));
  };
  Host host(160, 200);
  auto describe = [&] {
    return box().padding(4).child(feed::feed(ring, options.window, lit));
  };

  host.composer.render(describe());
  host.frame(0.4);  // every mounted row has finished its entrance
  for (uint64_t seq = 1; seq <= 4; ++seq) {
    const std::optional<SkRect> band = host.composer.bounds(feed::rowKey(seq));
    ASSERT_TRUE(band.has_value()) << seq;
    EXPECT_GT(brightestIn(host, *band), 150) << "row " << seq;
  }

  ring.append({util::toU8("tail")});
  host.composer.render(describe());
  host.frame(0.016);
  for (uint64_t seq = 1; seq <= 4; ++seq) {
    const std::optional<SkRect> band = host.composer.bounds(feed::rowKey(seq));
    ASSERT_TRUE(band.has_value()) << seq;
    EXPECT_GT(brightestIn(host, *band), 150)
        << "row " << seq << " re-entered: the append remounted it";
  }
  // …and the new row really is new — it is mid-entrance, not already lit.
  const std::optional<SkRect> tail = host.composer.bounds(feed::rowKey(5));
  ASSERT_TRUE(tail.has_value());
  EXPECT_LT(brightestIn(host, *tail), 120);
}

TEST(ComposeFeed, TheWindowNeverMountsTheRowsOutsideIt) {
  // Virtualization is the window, not a separate mechanism: rows older than
  // Options::visible are never built, so they have no instance, no bounds
  // and no layout cost, and a ring that keeps growing does not.
  feed::TextRing ring{600};
  for (int i = 0; i < 300; ++i)
    ring.append({util::toU8("line " + std::to_string(i))});
  const feed::TextOptions options = feedOptions(8);
  Host host(200, 200);
  host.composer.render(box().child(feed::feed(ring, options)));
  host.frame();

  EXPECT_FALSE(host.composer.bounds(feed::rowKey(1)).has_value())
      << "a row outside the window was mounted";
  EXPECT_FALSE(host.composer.bounds(feed::rowKey(292)).has_value());
  EXPECT_TRUE(host.composer.bounds(feed::rowKey(293)).has_value())
      << "the window is the NEWEST visible rows";
  EXPECT_TRUE(host.composer.bounds(feed::rowKey(300)).has_value());

  const size_t live = host.composer.stats().instances;
  for (int i = 0; i < 200; ++i)
    ring.append({util::toU8("more " + std::to_string(i))});
  host.composer.render(box().child(feed::feed(ring, options)));
  host.frame();
  EXPECT_EQ(host.composer.stats().instances, live)
      << "the retained tree grew with the ring instead of with the window";
}

TEST(ComposeFeed, TheEntranceStaggerDelaysOnlyTheRowsThatMount) {
  // Options::entrance is a Stagger — the same value the glyph engine and
  // staggerChildren speak — with a ROW as the beat. The initial describe
  // cascades the window; an append is the only new mount in its patch, so
  // it enters AT ONCE instead of inheriting a full window's worth of steps,
  // and no row already on screen re-enters.
  feed::TextRing ring;
  for (int i = 0; i < 3; ++i) ring.append({util::toU8("row")});
  feed::TextOptions options = feedOptions(6, 16.0f);
  options.window.entrance = {.eachMs = 400};
  auto lit = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .opacity(animate(from(0.0f).to(1.0f), {200ms, &choreograph::easeNone}));
  };
  Host host(160, 200);
  auto describe = [&] {
    return box().padding(4).child(feed::feed(ring, options.window, lit));
  };

  host.composer.render(describe());
  host.frame(0.25);  // row 1's 200 ms is done; row 2 waits out its 400 ms
  const std::optional<SkRect> r1 = host.composer.bounds(feed::rowKey(1));
  const std::optional<SkRect> r2 = host.composer.bounds(feed::rowKey(2));
  ASSERT_TRUE(r1.has_value());
  ASSERT_TRUE(r2.has_value());
  EXPECT_GT(brightestIn(host, *r1), 150);
  EXPECT_LT(brightestIn(host, *r2), 40) << "the cascade did not delay row 2";
  host.frame(0.5);  // t = 0.75 — row 2 is past its 400 ms delay
  EXPECT_GT(brightestIn(host, *r2), 150);

  // The append: one new mount, so no extra delay at all. Waiting only its
  // own 200 ms entrance is what proves the cascade counts MOUNTS and not
  // positions — an ordinal-based delay would hold this row for 1.2 s.
  ring.append({util::toU8("tail")});
  host.composer.render(describe());
  host.frame(0.25);
  const std::optional<SkRect> r4 = host.composer.bounds(feed::rowKey(4));
  ASSERT_TRUE(r4.has_value());
  EXPECT_GT(brightestIn(host, *r4), 150)
      << "the appended row inherited the window's cascade";
}

TEST(ComposeFeed, ATypedOnRowPaintsLiveThenCachesWhenItsTrackSettles) {
  // A glyph entrance on a feed row is affordable because it ENDS: while the
  // track's progress moves the row paints live, and once it settles the row
  // is a static leaf again, cached like every row above it. A track that
  // never settled would pin the whole window volatile.
  feed::TextRing ring;
  ring.append({util::toU8("daemon bound port 6042")});
  const feed::TextOptions options = feedOptions(8, 16.0f);
  auto typed = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .fx({.effect = fx::typeOn(),
             .stagger = {.eachMs = 12, .durationMs = 40},
             .progress = animate(from(0.0f).to(1.0f),
                                 {300ms, &choreograph::easeNone})});
  };
  Host host(240, 120);
  host.composer.render(
      box().padding(4).child(feed::feed(ring, options.window, typed)));
  host.frame(0.05);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "a running typeOn track must paint live";

  for (int i = 0; i < 40; ++i) host.frame(0.033);  // the entrance finishes
  unsigned paints = 0, records = 0;
  for (int i = 0; i < 5; ++i) {
    host.frame(0.016);
    paints += host.composer.stats().nodesPainted;
    records += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(paints, 0u) << "a settled row kept painting live";
  EXPECT_EQ(records, 0u) << "a settled row kept re-recording";
}

namespace {

/** The shape a designed console row takes: several fields, not one line. */
struct StructuredRow {
  std::string ts, tag, body;
  bool operator==(const StructuredRow&) const = default;
};

}  // namespace

TEST(ComposeFeed, AStructuredRowAppendsAtItsOwnConstantCost) {
  // A richer row — a severity stripe beside ONE rich() leaf whose runs
  // speak named styles, entered by a track that settles — is a small
  // subtree rather than a single text node. That changes the CONSTANT in
  // the append price, never its shape: an append patches exactly the new
  // row's own nodes, however many rows the window holds, and the price
  // repeats append after append.
  sigil::weave::StyleSet styles(whiteStyle(12));
  styles.set("ts", whiteStyle(10));
  styles.set("tag", whiteStyle(11));
  feed::Ring<StructuredRow> ring;
  for (int i = 0; i < 30; ++i)
    ring.append({"0412.50", "AUTH", "row " + std::to_string(i)});
  auto rowEl = [&](const StructuredRow& r) {
    auto line = rich(styles.base())
                    .styles(styles)
                    .add(util::toU8(r.ts + "  "), "ts")
                    .add(util::toU8(r.tag + "  "), "tag")
                    .add(util::toU8(r.body));
    return box()
        .row()
        .gap(6)
        .child(box().width(3).height(10).fill(Fill::color(SkColors::kRed)))
        .child(text(std::move(line))
                   .fx({.effect = fx::typeOn(),
                        .stagger = {.eachMs = 5, .durationMs = 30},
                        .progress = animate(from(0.0f).to(1.0f),
                                            {200ms, &choreograph::easeNone})}));
  };
  constexpr size_t kRowNodes = 3;  // the row box, the stripe, the text leaf

  const auto perAppendCost = [&](size_t visible) {
    feed::Options options;
    options.visible = visible;
    options.gap = 2.0f;
    Host host(260, 500);
    auto describe = [&] {
      return box().padding(6).child(feed::feed(ring, options, rowEl));
    };
    host.composer.render(describe());
    host.frame(0.4);  // every mounted entrance has settled

    ring.append({"0413.00", "WARD", "breach"});
    host.composer.render(describe());
    const size_t patched = host.composer.stats().patchedNodes;
    host.frame(0.016);
    const size_t live = host.composer.stats().instances;

    // The same price again, and the retained tree does not grow: one
    // subtree in at the tail, one out at the head.
    ring.append({"0413.50", "LATT", "sweep"});
    host.composer.render(describe());
    EXPECT_EQ(host.composer.stats().patchedNodes, patched);
    host.frame(0.016);
    EXPECT_EQ(host.composer.stats().instances, live)
        << "the window is bounded at " << visible;
    return patched;
  };

  EXPECT_EQ(perAppendCost(8), kRowNodes);
  // Twice the window, the same append price: the cost is the row's own
  // shape, not the window's size.
  EXPECT_EQ(perAppendCost(16), kRowNodes);
}

#ifdef SIGILCOMPOSE_ENABLE_OCIO
#include <sigilcompose/Ocio.h>

TEST(ComposeColor, OcioViewTransformsOutputAndClears) {
  // The OCIO output stage end-to-end: an exponent transform baked to a LUT
  // darkens mid-gray (0.5^2.2 ≈ 0.218); clearing the view restores
  // pass-through. Exercises bake → SkImage LUT → SkSL trilinear → saveLayer.
  ASSERT_TRUE(sigil::compose::ocio::available());
  Host host;
  host.composer.setView(sigil::compose::ocio::exponent(2.2f));
  host.composer.render(box().child(
      box().width(60).height(60).fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))));
  host.frame();
  const uint32_t dark = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(dark, 30u);  // ≈ 56 (LUT-quantized)
  EXPECT_LT(dark, 80u);
  host.composer.setView({});  // pass-through again
  host.frame();
  const uint32_t plain = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(plain, 118u);  // ≈ 128
  EXPECT_LT(plain, 138u);
}
#endif  // SIGILCOMPOSE_ENABLE_OCIO

TEST(ComposeMaterial, UnknownUniformNamesWarnAndIgnore) {
  // A typo'd uniform name must never abort (SkDEBUGFAIL kills the sketch
  // host in debug): unknown names are warned and dropped, at sksl() and at
  // uniform(), constant and bound alike.
  Material m = Material::sksl(ukEffect(), {{"uTypo", 1.0f}});
  choreograph::Output<float> o{1.0f};
  m.uniform("uAlsoMissing", &o);  // dropped → still not live
  EXPECT_FALSE(m.isAnimated());
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();  // paints with uK at its SkSL default (0) — and does not crash
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 40u);
}

TEST(ComposeDerive, FlowAroundWrapsTextAroundFrame) {
  const std::u8string body =
      u8"the quick brown fox jumps over the lazy dog and keeps running "
      u8"through the tall summer grass until the river bend appears and "
      u8"the evening light settles over the water in long amber bands";

  auto tree = [&](bool flow) {
    auto t = text(body, whiteStyle(18)).key("body");
    if (flow) t.flowAround("frame", 6);
    return stack()
        .child(box()
                   .key("frame")
                   .width(150)
                   .height(140)
                   .inset(200, 10, 10, 210)
                   .absolute()
                   .fill(Fill::color({0, 0.4f, 0, 1})))
        .child(box().inset(0).child(std::move(t)).zIndex(1));
  };

  Host plain(360, 420), flowed(360, 420);
  plain.composer.render(tree(false));
  plain.frame();
  flowed.composer.render(tree(true));
  flowed.frame();

  // Without the exclusion, text runs under the frame region; with it,
  // the region stays text-free (frame color only).
  const SkIRect inner = SkIRect::MakeLTRB(215, 25, 345, 135);
  EXPECT_TRUE(anyWhiteIn(plain, inner));
  EXPECT_FALSE(anyWhiteIn(flowed, inner));

  // Displaced words push the flowed paragraph taller.
  auto plainBounds = plain.composer.bounds("body");
  auto flowedBounds = flowed.composer.bounds("body");
  ASSERT_TRUE(plainBounds && flowedBounds);
  EXPECT_GT(flowedBounds->height(), plainBounds->height());
}

namespace {
/** How tall the paragraph came out. It is the one number that reads
 *  "how much room the geometry offered": the same words in the same box
 *  need fewer lines when the exclusion gives room back. */
float flowedHeight(Host& host, const char* key) {
  std::optional<SkRect> bounds = host.composer.bounds(key);
  return bounds ? bounds->height() : 0.0f;
}

/** Words placed on the line band the caller names — the direct reading of
 *  "this line's intervals were longer". */
int inkInBand(Host& host, SkIRect band) {
  int lit = 0;
  for (int y = band.top(); y < band.bottom(); ++y)
    for (int x = band.left(); x < band.right(); ++x)
      if (host.pixel(x, y) == SK_ColorWHITE) ++lit;
  return lit;
}

const std::u8string& flowBody() {
  // Long enough to run past the obstacle on every geometry, so a height
  // comparison reads room-per-line and not "the text stopped early".
  static const std::u8string body = [] {
    std::u8string one =
        u8"the quick brown fox jumps over the lazy dog and keeps running "
        u8"through the tall summer grass until the river bend appears and "
        u8"the evening light settles over the water in long amber bands "
        u8"while the swallows turn above the reeds and the mill wheel "
        u8"grinds on into the blue hour without hurry or complaint ";
    std::u8string all;
    for (int i = 0; i < 4; ++i) all += one;
    return all;
  }();
  return body;
}

/** One paragraph flowing around one keyed target of the caller's making. */
Element flowScene(Element target, float margin) {
  return stack()
      .child(std::move(target))
      .child(box()
                 .inset(0)
                 .child(text(flowBody(), whiteStyle(15))
                            .key("body")
                            .flowAround("obstacle", margin))
                 .zIndex(1));
}

Element obstacleBox(Shape silhouette) {
  Element el = box()
                   .key("obstacle")
                   .width(160)
                   .height(160)
                   .left(100)
                   .top(40)
                   .fill(Fill::color({0, 0.4f, 0, 1}));
  if (silhouette) el.shape(std::move(silhouette));
  return el;
}
}  // namespace

TEST(ComposeDerive, FlowAroundShapelessTargetKeepsItsBox) {
  // The pin: a target with no silhouette of its own is subtracted by its
  // BOX, exactly as it always was. Every line the box crosses is cut to
  // the box's full width, whatever the type does.
  Host host(360, 460);
  host.composer.render(flowScene(obstacleBox({}), 6));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(106, 46, 254, 194)));
}

TEST(ComposeDerive, FlowAroundFollowsACircleSilhouette) {
  // A round target gives back the four corners its bounding box was
  // eating, so the same paragraph in the same box gets measurably more
  // room — and the corners themselves take type.
  Host boxed(360, 460), disc(360, 460);
  boxed.composer.render(flowScene(obstacleBox({}), 6));
  boxed.frame();
  disc.composer.render(flowScene(obstacleBox(shapes::circle()), 6));
  disc.frame();

  EXPECT_LT(flowedHeight(disc, "body"), flowedHeight(boxed, "body"));
  // The corner: inside the box, outside the circle. Type reaches it only
  // under the silhouette.
  const SkIRect corner = SkIRect::MakeLTRB(103, 43, 127, 67);
  EXPECT_FALSE(anyWhiteIn(boxed, corner));
  EXPECT_TRUE(anyWhiteIn(disc, corner));
  // The middle of the disc stays clear either way.
  EXPECT_FALSE(anyWhiteIn(disc, SkIRect::MakeLTRB(150, 100, 210, 140)));
}

TEST(ComposeDerive, FlowAroundFollowsAStarSilhouette) {
  // A concave silhouette is the case a bounding box cannot approximate:
  // text runs INTO the notches between the points.
  Host boxed(360, 460), star(360, 460);
  boxed.composer.render(flowScene(obstacleBox({}), 6));
  boxed.frame();
  star.composer.render(flowScene(obstacleBox(shapes::star(5)), 6));
  star.frame();

  EXPECT_LT(flowedHeight(star, "body"), flowedHeight(boxed, "body"));
  // The concave half: a star leaves far more of its bounding box open
  // than a disc does, so the notches and corners take more type than the
  // round silhouette can.
  const SkIRect band = SkIRect::MakeLTRB(103, 43, 257, 197);
  EXPECT_GT(inkInBand(star, band), inkInBand(boxed, band));
  // The star's own body still refuses type at its centre.
  EXPECT_FALSE(anyWhiteIn(star, SkIRect::MakeLTRB(165, 105, 195, 135)));
}

TEST(ComposeDerive, FlowAroundMarginHoldsOffTheSilhouette) {
  // The margin means one thing on a silhouette and on a box alike: a
  // standoff from whatever edge is being subtracted. A wider one buys the
  // paragraph less room, never more.
  Host tight(360, 460), wide(360, 460);
  tight.composer.render(flowScene(obstacleBox(shapes::circle()), 2));
  tight.frame();
  wide.composer.render(flowScene(obstacleBox(shapes::circle()), 26));
  wide.frame();
  EXPECT_LT(flowedHeight(tight, "body"), flowedHeight(wide, "body"));
}

TEST(ComposeDerive, FlowAroundSilhouetteTracksAMovingTarget) {
  // Moving targets already re-derive; a silhouette target must too.
  auto scene = [](float left) {
    return stack()
        .child(box()
                   .key("obstacle")
                   .width(160)
                   .height(160)
                   .left(left)
                   .top(40)
                   .shape(shapes::circle())
                   .fill(Fill::color({0, 0.4f, 0, 1})))
        .child(box()
                   .inset(0)
                   .child(text(flowBody(), whiteStyle(15))
                              .key("body")
                              .flowAround("obstacle", 6))
                   .zIndex(1));
  };
  Host host(360, 460);
  host.composer.render(scene(100));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(165, 105, 195, 135)));
  host.composer.render(scene(20));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(85, 105, 115, 135)));
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeLTRB(165, 105, 195, 135)));
}

TEST(ComposeDerive, FlowAroundCycleIsIgnored) {
  Host host;
  host.composer.render(box().child(
      text(u8"self reference", whiteStyle(16)).key("self").flowAround("self")));
  host.frame();  // must not hang or exclude itself into nothing
  EXPECT_NE(host.composer.paragraphLayout("self"), nullptr);
}

TEST(ComposeDerive, ConnectorTracksMovedEndpoints) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});

  auto tree = [&](float bLeft) {
    return stack()
        .child(box()
                   .key("a")
                   .width(20)
                   .height(20)
                   .inset(10, 10, 170, 170)
                   .absolute()
                   .fill(red()))
        .child(box()
                   .key("b")
                   .width(20)
                   .height(20)
                   .inset(bLeft, 160, 180 - bLeft, 20)
                   .absolute()
                   .fill(green()))
        .child(connector("a", "b").inset(0).foreground(wire).zIndex(-1));
  };

  host.composer.render(tree(10.0f));
  host.frame();
  // Vertical wire at x=20 between the stacked boxes.
  EXPECT_EQ(host.pixel(20, 100), SK_ColorYELLOW);

  host.composer.render(tree(160.0f));  // move b to the right
  host.frame();
  EXPECT_EQ(host.pixel(20, 100), SK_ColorBLACK);  // old route gone
  EXPECT_NE(host.pixel(95, 95), SK_ColorBLACK);   // new diagonal route
}

TEST(Shape, CustomOutlineShapesFillAndClip) {
  Host host;
  // A diamond outline over a 100x100 box: the box's corner pixels sit
  // outside the shape, so fill and clipped children must not reach them.
  auto diamond = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() / 2, 0);
    b.lineTo(s.width(), s.height() / 2);
    b.lineTo(s.width() / 2, s.height());
    b.lineTo(0, s.height() / 2);
    b.close();
    return b.detach();
  };
  host.composer.render(
      box().width(100).height(100).clip().shape(diamond).fill(red()).child(
          box().inset(0).absolute().fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorGREEN);  // clipped child inside
  EXPECT_EQ(host.pixel(3, 3), SK_ColorBLACK);    // box corner outside shape
}

TEST(Shape, RoundedOutlineCutsSharpCorners) {
  Host host;
  auto diamond = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() / 2, 0);
    b.lineTo(s.width(), s.height() / 2);
    b.lineTo(s.width() / 2, s.height());
    b.lineTo(0, s.height() / 2);
    b.close();
    return b.detach();
  };
  // Nested (the root always fills the viewport); radius 20 pulls the
  // 100x100 diamond's top vertex from y=0 down to y≈7.
  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(100)
                                       .shape(shapes::rounded(diamond, 20))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);   // body intact
  EXPECT_EQ(host.pixel(50, 3), SK_ColorBLACK);  // sharp tip rounded away
  EXPECT_EQ(host.pixel(50, 12), SK_ColorRED);   // rounded apex below y≈7

  host.composer.render(box().child(
      box().width(100).height(100).shape(shapes::star(5)).fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);    // star body
  EXPECT_NE(host.pixel(50, 3), SK_ColorBLACK);   // sharp top point present
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // gap between arms
}

TEST(TextLayout, FullyConstrainedAbsoluteTextPaints) {
  // Yoga skips the measure callback when absolute insets determine both
  // dimensions; the kernel must lay the paragraph out at paint time.
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      stack().child(text(u8"WWWW", style).absolute().inset(10, 10, 10, 120)));
  host.frame();
  int lit = 0;
  for (int x = 10; x < 190; x += 4)
    for (int y = 10; y < 70; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 20);  // glyph coverage, not empty
}

TEST(TextLayout, AlignItemsCentersTextLeaf) {
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(box()
                           .width(200)
                           .height(60)
                           .alignItems(Align::Center)
                           .child(text(u8"W", style)));
  host.frame();

  int litLeft = 0, litMiddle = 0;
  for (int x = 0; x < 50; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litLeft++;
  for (int x = 75; x < 125; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litMiddle++;
  EXPECT_EQ(litLeft, 0);    // nothing hugging the start edge
  EXPECT_GT(litMiddle, 5);  // the glyph sits in the middle
}

TEST(ComposeCaching, TextureBakeScaleQuantized) {
  // A continuously changing canvas scale (live window resize, pinch
  // zoom) must not re-bake Cache::Texture nodes every frame: the bake
  // scale quantizes up to a coarse step.
  Host host;
  host.composer.render(box()
                           .width(60)
                           .height(60)
                           .cache(Cache::Texture)
                           .fill(red())
                           .child(box().width(20).height(20).fill(green())));
  auto drawAt = [&](float s) {
    SkCanvas& canvas = *host.surface->getCanvas();
    canvas.save();
    canvas.scale(s, s);
    host.composer.draw(canvas);
    canvas.restore();
  };
  drawAt(1.6f);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);  // first bake
  drawAt(1.7f);
  drawAt(1.9f);
  drawAt(2.0f);  // still within the 2.0 step: the bake is reused
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  drawAt(2.2f);  // crossed into the 3.0 step: one re-bake
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);
}

TEST(ComposeCaching, TextureBakeReusedUnderAMovingAncestor) {
  // The same guarantee from the side the test above cannot see. A bake
  // taken in DEVICE space is exact but pinned to one device rect, so it
  // may only be taken while the node is holding still — and "still" has
  // two independent measures that are easy to mistake for one:
  //
  //   * the node's own transform is not declared as animating, and
  //   * the device rect it LANDS on has not moved.
  //
  // This node declares nothing. It is dragged across the canvas by an
  // ancestor, through a Cache::None parent so no recording intervenes —
  // the one arrangement where a moving rect reaches a node that looks
  // static from every declaration available to it. A device-pinned bake
  // would re-rasterize every frame here, which is precisely the cost the
  // quantized local bake exists to avoid.
  //
  // Note this cannot be a pixel assertion: every arrangement below draws
  // the correct picture. Only the bake COUNT tells them apart.
  Host host(300, 300);
  choreograph::Output<float> slide{0.0f};
  host.composer.render(
      box()
          .cache(Cache::None)
          .child(box()
                     .cache(Cache::None)
                     .absolute()
                     .translateX(&slide)
                     .child(box()
                                .width(60)
                                .height(60)
                                .cache(Cache::Texture)
                                .fill(red())
                                .child(box().width(20).height(20).fill(
                                    green())))));
  host.frame();
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);  // the first bake
  // The still -> moving transition costs exactly one re-bake, because the
  // held image is in the wrong space for the path now being taken. That is
  // inherent to having two bake spaces and is not what this test guards.
  slide = 7.0f;
  host.frame();
  // From here the guarantee is absolute: a moving node reuses ONE local
  // bake and blits it through its transform, however far it travels.
  for (int i = 2; i <= 5; ++i) {
    slide = (float)i * 7.0f;  // whole-pixel slides: the rect really moves
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
        << "frame " << i
        << ": the bake was re-rasterized while the node slid, instead of "
           "being reused and blitted through the transform";
  }
}

// ---------------------------------------------------------------------------
// Layout and leaf surface: wrap, per-edge spacing, per-corner radii,
// Dim literals, atlas regions, the Paragraph overload, contentScale.

TEST(ComposeLayout, WrapLinesFlowsToSecondRow) {
  Host host;
  host.composer.render(
      box().child(box()
                      .row()
                      .wrapLines()
                      .width(200)
                      .child(box().width(80).height(40).fill(red()))
                      .child(box().width(80).height(40).fill(green()))
                      .child(box().width(80).height(40).fill(blue()))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(120, 20), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(40, 60), SK_ColorBLUE);  // wrapped to the next line
}

TEST(ComposeLayout, PerEdgePaddingAndMargin) {
  Host host;
  host.composer.render(box().child(
      box()
          .padding(10, 20, 30, 40)
          .key("outer")
          .child(box().margin(5, 6, 7, 8).width(50).height(50).key("inner"))));
  host.frame();
  auto inner = host.composer.bounds("inner");
  ASSERT_TRUE(inner.has_value());
  EXPECT_FLOAT_EQ(inner->left(), 10 + 5);  // padding.left + margin.left
  EXPECT_FLOAT_EQ(inner->top(), 20 + 6);   // padding.top + margin.top
}

TEST(Shape, PerCornerRadiiIndependent) {
  Host host;
  // Sharp top-left, heavily rounded top-right.
  host.composer.render(box().child(
      box().width(100).height(100).corners({0, 40, 0, 0}).fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(2, 2), SK_ColorRED);     // sharp TL corner filled
  EXPECT_EQ(host.pixel(97, 2), SK_ColorBLACK);  // rounded TR corner empty
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
}

TEST(ComposeLayout, DimLiteralsResolvePercent) {
  Host host;
  host.composer.render(
      box().child(box().width(50_pct).height(25_pct).fill(red()).key("half")));
  host.frame();
  auto rect = host.composer.bounds("half");
  ASSERT_TRUE(rect.has_value());
  EXPECT_FLOAT_EQ(rect->width(), 100.0f);  // 50% of the 200px host
  EXPECT_FLOAT_EQ(rect->height(), 50.0f);  // 25% of 200px
}

TEST(ComposeContent, ImageRegionDrawsAtlasCell) {
  Host host;
  auto atlas = twoCellAtlas();
  host.composer.render(box()
                           .row()
                           .child(image(atlas)
                                      .region(SkRect::MakeXYWH(16, 0, 16, 16))
                                      .width(50)
                                      .height(50))
                           .child(image(atlas).width(50).height(50)));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorGREEN);  // region: right cell only
  EXPECT_EQ(host.pixel(60, 25), SK_ColorRED);    // whole atlas: left half
}

TEST(TextLayout, ParagraphOverloadPaintsMixedSpans) {
  Host host(400, 200);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  sigil::weave::TextStyle big = styleAt(40);
  big.paint.foreground.setColor(SK_ColorWHITE);
  sigil::weave::TextStyle small = styleAt(16);
  small.paint.foreground.setColor(SK_ColorWHITE);
  para->appendText(std::u8string(u8"BIG"), big);
  para->appendText(std::u8string(u8" and small"), small);

  host.composer.render(box().padding(10).child(text(para).key("spans")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("spans");
  ASSERT_NE(layout, nullptr);
  int lit = 0;
  for (int x = 10; x < 390; x += 3)
    for (int y = 10; y < 70; y += 3)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 15);  // both spans shaped and painted
}

TEST(ComposePaint, ContentScaleReportsHostScale) {
  Host host;
  float seen = 0.0f;
  host.composer.render(
      box().child(custom([&seen](SkCanvas&, const PaintContext& ctx) {
                    seen = ctx.contentScale;
                  })
                      .width(50)
                      .height(50)
                      .cache(Cache::None)));
  SkCanvas& canvas = *host.surface->getCanvas();
  canvas.save();
  canvas.scale(2.0f, 2.0f);
  host.composer.draw(canvas);
  canvas.restore();
  EXPECT_FLOAT_EQ(seen, 2.0f);
}

TEST(ComposePaint, AnimatingReportsTheTickersState) {
  // `PaintContext::animating` looks dead from inside the library: Paint.cpp
  // assigns it from `ticker.active()` (and the Brushes.h wrappers copy that
  // forward rather than a constant), but nothing in the library ever reads
  // it back. Its only consumer is a paint program written by a caller, so
  // this test is the only thing keeping the field wired up.
  Host host;
  bool seen = false;
  host.composer.render(
      box()
          .child(box().width(40).height(40).fill(red()).opacity(
              animate(from(0.0f).to(1.0f), {400ms})))
          .child(custom([&seen](SkCanvas&, const PaintContext& ctx) {
                   seen = ctx.animating;
                 })
                     .width(10)
                     .height(10)
                     .cache(Cache::None)));
  host.frame(0.016);
  EXPECT_TRUE(seen) << "an entrance is running: the ticker is active";
  for (int i = 0; i < 40; ++i)
    host.frame(0.016);  // 640 ms — well past the 400 ms entrance
  EXPECT_FALSE(seen) << "and false again once nothing is moving";
}

// ---------------------------------------------------------------------------
// Shape kit (Shapes.h): organic generators, per-edge extraction.

TEST(Shape, PolygonAndSquircleSilhouettes) {
  Host host;
  host.composer.render(
      box()
          .row()
          .child(
              box().width(90).height(90).shape(shapes::polygon(6)).fill(red()))
          .child(box()
                     .width(90)
                     .height(90)
                     .shape(shapes::squircle(4))
                     .fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(45, 45), SK_ColorRED);     // hexagon body
  EXPECT_EQ(host.pixel(2, 2), SK_ColorBLACK);     // hexagon corner cut
  EXPECT_EQ(host.pixel(135, 45), SK_ColorGREEN);  // squircle body
  EXPECT_EQ(host.pixel(92, 2), SK_ColorBLACK);    // squircle corner soft
  EXPECT_EQ(host.pixel(135, 3), SK_ColorGREEN);   // but edge midpoints full
}

TEST(Shape, BlobIsDeterministicOrganicAndBounded) {
  auto probe = [](uint32_t seed) {
    Host host;
    host.composer.render(box().child(box()
                                         .width(120)
                                         .height(120)
                                         .shape(shapes::blob(seed, 0.3f, 9))
                                         .fill(red())));
    host.frame();
    std::vector<SkColor> px;
    for (int y = 0; y < 130; y += 4)
      for (int x = 0; x < 130; x += 4) px.push_back(host.pixel(x, y));
    return px;
  };
  std::vector<SkColor> a1 = probe(7), a2 = probe(7), b = probe(8);
  EXPECT_EQ(a1, a2);  // same seed → identical pixels (cacheable chaos)
  EXPECT_NE(a1, b);   // different seed → different blob

  Host host;
  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .shape(shapes::blob(7, 0.3f, 9))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorRED);  // center always covered
  int outside = 0;
  for (int x = 121; x < 200; x += 4)
    for (int y = 0; y < 200; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK) outside++;
  EXPECT_EQ(outside, 0);  // never escapes its layout box
}

TEST(ComposeDecorations, EdgeSliceStrokesSelectedEdgesOnly) {
  Host host;
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).foreground(
          shapes::onEdges(shapes::Edge::Top | shapes::Edge::Left,
                          util::stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top edge stroked
  EXPECT_EQ(host.pixel(1, 50), SK_ColorWHITE);  // left edge stroked
  EXPECT_EQ(host.pixel(98, 50), SK_ColorBLUE);  // right edge bare
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom edge bare
}

TEST(ComposeDecorations, EdgesSplitRoundedCornersDiagonally) {
  // A rounded rect's corner arcs divide between their adjacent edges at
  // the diagonal — the top run must include the upper half of the
  // top-left arc but none of the left flank.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue()).foreground(
          shapes::onEdges(shapes::Edge::Top,
                          util::stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top run center
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);   // left flank untouched
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom untouched
}

// ---------------------------------------------------------------------------
// Shape VALUES: an outline generator is a comparable scheme, so a shaped
// node can prune. This matters more than it looks — a node whose outline
// cannot compare re-patches on every describe, which throws away its
// recording and its bake, so a single such node in an otherwise static tree
// puts the whole tree back on the live-paint path.

TEST(ComposeShapeValues, AStockGeneratorShapePrunes) {
  // The core case: a shaped node re-described identically must patch nothing
  // and re-record nothing. If structural equality refuses shaped nodes
  // outright, this tree re-records every frame for its entire life and
  // nothing reports it.
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .shape(shapes::star(5, 0.5f, 0.12f))
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);  // the star is really there

  host.composer.render(tree());  // brand-new Elements, identical values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeShapeValues, AChangedParameterPatchesAndMovesPixels) {
  // The other half of the prune contract: equality must be HONEST. A
  // different parameter is a different value, patches, and redraws.
  Host host;
  auto tree = [](int sides) {
    return box().child(
        box().width(100).height(100).shape(shapes::polygon(sides)).fill(red()));
  };
  host.composer.render(tree(4));  // diamond: box corners empty
  host.frame();
  EXPECT_EQ(host.pixel(6, 6), SK_ColorBLACK);
  host.composer.render(tree(40));  // ~circle: still empty corners, more ink
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  // A 40-gon covers (25, 12); a diamond does not.
  EXPECT_EQ(host.pixel(25, 12), SK_ColorRED);
}

TEST(ComposeShapeValues, ARawCallableIsTheEscapeHatchAndStaysConservative) {
  // A hand-rolled OutlineFn cannot compare, so its node re-patches on every
  // describe. That is the conservative answer and it is deliberate: claiming
  // equality for two callables would prune a node whose outline had in fact
  // changed. An author who needs the prune wraps the node in memo().
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .shape([](SkSize s) {
                             SkPathBuilder b;
                             b.addOval(SkRect::MakeWH(s.width(), s.height()));
                             return b.detach();
                           })
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "an incomparable callable pruned — equality is lying";
}

TEST(ComposeShapeValues, CopiesOfOneShapeCompareEqualEvenWhenRaw) {
  // Two copies of one Shape share state, and shared state IS identity — so
  // a caller who builds a raw callable once and holds it gets a real prune,
  // where one who re-mints an equivalent lambda each describe does not.
  const Shape raw = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  const Shape copy = raw;
  EXPECT_TRUE(raw == copy);
  // But two separate constructions from equivalent lambdas cannot know
  // they agree, and must not claim to.
  const Shape other = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  EXPECT_FALSE(raw == other);
}

TEST(ComposeShapeValues, WrappersAreComparableWhenTheirInnerIs) {
  // rounded() composes: value in, value out. Wrapping the escape hatch
  // stays the escape hatch.
  EXPECT_TRUE(Shape(shapes::rounded(shapes::star(5), 8)) ==
              Shape(shapes::rounded(shapes::star(5), 8)));
  EXPECT_FALSE(Shape(shapes::rounded(shapes::star(5), 8)) ==
               Shape(shapes::rounded(shapes::star(5), 9)));
  EXPECT_FALSE(Shape(shapes::rounded(shapes::star(5), 8)) ==
               Shape(shapes::rounded(shapes::star(6), 8)));
  auto lambda = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  EXPECT_FALSE(Shape(shapes::rounded(lambda, 8)) ==
               Shape(shapes::rounded(lambda, 8)));
}

TEST(ComposeShapeValues, SvgShapesAreValuesNow) {
  // svg() parses its d-string once into an SkPath, and SkPath has structural
  // equality — so an svg() silhouette compares by geometry and prunes,
  // unlike the raw-callable hatch above.
  EXPECT_TRUE(Shape(shapes::svg("M0 0L10 0L10 10Z")) ==
              Shape(shapes::svg("M0 0L10 0L10 10Z")));
  EXPECT_FALSE(Shape(shapes::svg("M0 0L10 0L10 10Z")) ==
               Shape(shapes::svg("M0 0L10 0L5 10Z")));
}

TEST(ComposeShapeValues, KeyedParametricIsAValueUnkeyedIsNot) {
  auto fig8 = [](float t) { return SkPoint{std::sin(2 * t), std::sin(t)}; };
  // Unkeyed: the callable is the identity and cannot compare.
  EXPECT_FALSE(Shape(shapes::parametric(fig8, 0, 6.2832f, 720)) ==
               Shape(shapes::parametric(fig8, 0, 6.2832f, 720)));
  // Keyed: (key, window, samples) is the identity — the author's contract
  // that one key names one curve.
  EXPECT_TRUE(Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
              Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)));
  EXPECT_FALSE(Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
               Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 360)));
  EXPECT_FALSE(Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
               Shape(shapes::parametric("orbit", fig8, 0, 6.2832f, 720)));
  // The named families carry their identity in their parameters.
  EXPECT_TRUE(Shape(shapes::lissajous(3, 2)) == Shape(shapes::lissajous(3, 2)));
  EXPECT_FALSE(Shape(shapes::lissajous(3, 2)) ==
               Shape(shapes::lissajous(5, 4)));
  EXPECT_TRUE(Shape(shapes::rose(3)) == Shape(shapes::rose(3)));
  EXPECT_FALSE(Shape(shapes::spiral(3.0f)) == Shape(shapes::spiral(4.0f)));
}

TEST(ComposeShapeValues, TextOnAComparableBaselinePrunes) {
  // A TextPath's baseline is a Shape, so TextPath compares and a curved run
  // prunes. Without this every radial label in a figure re-records on every
  // render(), and a ring of labels is exactly where a figure has the most of
  // them.
  Host host(240, 240);
  auto ring = [](float at) {
    return text(u8"HHHHHHHHHH", whiteStyle(22))
        .width(240)
        .height(240)
        .absolute()
        .left(0)
        .top(0)
        .onPath({.path = shapes::arc(180.0f, 359.9f),
                 .at = at,
                 .align = TextPath::Align::Center});
  };
  host.composer.render(box().child(ring(0.25f)));
  host.frame();
  host.composer.render(box().child(ring(0.25f)));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical curved run re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // …and the equality is honest: moving `at` IS a change. Omit `at` from
  // textEqual and a run that slides along its baseline compares equal to
  // where it was, prunes, and keeps the OLD placement forever with no
  // diagnostic — so this half of the case is the load-bearing one.
  host.composer.render(box().child(ring(0.75f)));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeShapeValues, ABandWithAComparableSpinePrunes) {
  // A band's authored spine is a Shape too, so deriveEqual must compare it
  // rather than refusing any authored spine outright.
  Host host;
  auto tree = [] {
    return box().child(band(shapes::circle(), across(8.0f))
                           .width(100)
                           .height(100)
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical authored band spine re-patched";
}

TEST(ComposeShapeValues, TheChevreulScenarioKeepsItsBake) {
  // The steady state the prune exists for: a texture-cached node whose shape
  // is a generator, re-described every frame. If the shape does not compare,
  // the node patches, the patch drops the bake, and the node re-rasterizes
  // every frame — a single such node dominates a frame that is otherwise
  // entirely cached.
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(120)
                           .height(120)
                           .shape(shapes::circle())
                           .fill(red())
                           .cache(Cache::Texture));
  };
  host.composer.render(tree());
  host.frame();  // records + bakes once
  for (int i = 0; i < 3; ++i) {
    host.composer.render(tree());
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
        << "the bake was thrown away by an identical re-describe";
  }
}

// ---------------------------------------------------------------------------
// Element stamps + snapshot().

TEST(ComposeStamps, SnapshotBakesIntrinsicSize) {
  sk_sp<SkPicture> pic =
      snapshot(box()
                   .row()
                   .gap(4)
                   .child(box().width(20).height(12).fill(red()))
                   .child(box().width(20).height(12).fill(green())),
               fonts());
  ASSERT_NE(pic, nullptr);
  EXPECT_FLOAT_EQ(pic->cullRect().width(), 44.0f);   // 20 + 4 + 20
  EXPECT_FLOAT_EQ(pic->cullRect().height(), 12.0f);  // content height
}

// ---------------------------------------------------------------------------
// tiles::window() / tiles::sliceable() — slicing one long bake into a run
// of tile rasters. These pin the ORIENTATION CONVENTION IN PIXELS, which is
// the whole reason the door exists. Slice arithmetic derived by hand goes
// wrong at the mirror, and it goes wrong invisibly: a strip sliced with the
// mirror on the wrong axis still produces a plausible-looking run of tiles.

namespace {

constexpr int kTileW = 40;
constexpr int kTileH = 20;
constexpr int kTileCount = 3;
// The mark sits near the tile's top-left and is small enough that its own
// mirror image never overlaps it — on EITHER axis, which is what lets a
// single sample point tell a flip from no flip.
constexpr float kMark = 3.0f;
constexpr float kMarkSize = 6.0f;
constexpr int kProbe = 5;  ///< inside the mark
constexpr int kFarX = kTileW - kProbe;
constexpr int kFarY = kTileH - kProbe;

SkColor stripMark(int index) {
  static const SkColor marks[3] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  return marks[index % 3];
}

/** A strip whose every tile carries ONE mark, near the tile's top-LEFT and
 *  in a per-tile colour — so a rendered tile reports its index by colour and
 *  its handedness by which side the mark landed on. `flow` picks whether the
 *  strip runs down (a column of tiles) or across (a row). */
sk_sp<SkPicture> markedStrip(tiles::Flow flow) {
  const bool down = flow == tiles::Flow::Down;
  const float w = (float)(down ? kTileW : kTileW * kTileCount);
  const float h = (float)(down ? kTileH * kTileCount : kTileH);
  auto strip = box().width(w).height(h);
  for (int j = 0; j < kTileCount; ++j)
    strip.child(box()
                    .absolute()
                    .left(kMark + (down ? 0.0f : (float)(j * kTileW)))
                    .top(kMark + (down ? (float)(j * kTileH) : 0.0f))
                    .width(kMarkSize)
                    .height(kMarkSize)
                    .fill(Fill::color(SkColor4f::FromColor(stripMark(j)))));
  // Shell box: snapshot() sizes by the ROOT's children, not its own dims.
  return snapshot(box().child(std::move(strip)), fonts());
}

sk_sp<SkSurface> renderTile(const sk_sp<SkPicture>& pic, int index,
                            tiles::Flow flow, tiles::Facing facing) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kTileW, kTileH));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  canvas->concat(tiles::window({kTileW, kTileH}, index, flow, facing));
  canvas->drawPicture(pic);
  return surface;
}

SkColor tilePixel(SkSurface& surface, int x, int y) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surface.readPixels(bm.pixmap(), x, y);
  return bm.getColor(0, 0);
}

}  // namespace

TEST(ComposeStripTiles, ForwardWindowSlicesInOrderAndDoesNotMirror) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> tile =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Forward);
    // The mark keeps its own side and its own offset within the tile: the
    // colour names the tile, so an off-by-one or a reversed step shows up
    // as the WRONG colour here.
    EXPECT_EQ(tilePixel(*tile, kProbe, kProbe), stripMark(k)) << "tile " << k;
    EXPECT_EQ(tilePixel(*tile, kFarX, kProbe), SK_ColorBLACK)
        << "tile " << k << " picked up a mirror it was not asked for";
  }
}

TEST(ComposeStripTiles, MirroredWindowFlipsAcrossTheStripNotAlongIt) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> tile =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    // ACROSS: x is reflected (4..12 becomes 28..36) …
    EXPECT_EQ(tilePixel(*tile, kFarX, kProbe), stripMark(k)) << "tile " << k;
    EXPECT_EQ(tilePixel(*tile, kProbe, kProbe), SK_ColorBLACK) << "tile " << k;
    // … and NOT along: y is untouched, so the mark stays near the top. A
    // flip on the flow axis would put it at kTileH - 8 and reverse the run.
    EXPECT_EQ(tilePixel(*tile, kFarX, kFarY), SK_ColorBLACK)
        << "tile " << k << " was mirrored along the flow, not across it";
  }
}

TEST(ComposeStripTiles, MirroredTileReadsForwardUnderMirroredSampling) {
  // What Facing::Mirrored actually promises: bake mirrored, sample mirrored,
  // and the strip reads exactly as the forward bake does. A caller drawing
  // the back face of a ribbon relies on this being an exact reflection
  // rather than an approximately-similar one.
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> forward =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Forward);
    sk_sp<SkSurface> mirrored =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    for (int y = 1; y < kTileH; y += 3)
      for (int x = 1; x < kTileW; x += 3)
        ASSERT_EQ(tilePixel(*forward, x, y),
                  tilePixel(*mirrored, kTileW - 1 - x, y))
            << "tile " << k << " at " << x << "," << y;
  }
}

TEST(ComposeStripTiles, AcrossFlowStepsRightwardAndMirrorsInY) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Across);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> forward =
        renderTile(strip, k, tiles::Flow::Across, tiles::Facing::Forward);
    EXPECT_EQ(tilePixel(*forward, kProbe, kProbe), stripMark(k))
        << "tile " << k;
    sk_sp<SkSurface> mirrored =
        renderTile(strip, k, tiles::Flow::Across, tiles::Facing::Mirrored);
    // The across-flow mirror is in Y — perpendicular to the flow again, so
    // x keeps its place and the mark drops to the bottom.
    EXPECT_EQ(tilePixel(*mirrored, kProbe, kFarY), stripMark(k))
        << "tile " << k;
    EXPECT_EQ(tilePixel(*mirrored, kProbe, kProbe), SK_ColorBLACK)
        << "tile " << k;
  }
}

TEST(ComposeStripTiles, SliceableFlattensTheOpsAndChangesNoPixel) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  const sk_sp<SkPicture> sliced = tiles::sliceable(strip);
  ASSERT_NE(sliced, nullptr);
  // The trap this verb exists for: drawPicture() into the recorder would
  // store ONE nested op the hierarchy cannot index into. Counting
  // non-nested ops is what tells the two apart.
  EXPECT_EQ(sliced->approximateOpCount(false), strip->approximateOpCount(false))
      << "sliceable() nested the picture instead of flattening it";
  EXPECT_GT(sliced->approximateOpCount(false), 3);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> plain =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    sk_sp<SkSurface> fast =
        renderTile(sliced, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    for (int y = 0; y < kTileH; ++y)
      for (int x = 0; x < kTileW; ++x)
        ASSERT_EQ(tilePixel(*plain, x, y), tilePixel(*fast, x, y))
            << "tile " << k << " at " << x << "," << y;
  }
}

TEST(ComposeStamps, StampRecordsOnceReplaysPerSample) {
  static int stampDescribes;
  stampDescribes = 0;
  Host host;

  ContourWalk vine;
  vine.spacing = 25.0f;
  vine.stamp =
      custom([](SkCanvas& c, const PaintContext& ctx) {
        ++stampDescribes;
        SkPaint p;
        p.setColor(SK_ColorYELLOW);
        c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
      })
          .width(12)
          .height(12);

  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(100)
                                       .inset(50, 50, 50, 50)
                                       .absolute()
                                       .fill(blue())
                                       .foreground(vine)));
  host.frame();
  host.frame();
  EXPECT_EQ(stampDescribes, 1);  // baked once, replayed at every sample

  // Stamps are centered on the outline: the top-left corner sample
  // lands half outside the box.
  EXPECT_EQ(host.pixel(50, 50), SK_ColorYELLOW);   // corner sample center
  EXPECT_EQ(host.pixel(100, 46), SK_ColorYELLOW);  // top edge, above box
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE);   // interior untouched
}

TEST(ComposeStamps, RecursiveStampWalksItsOwnContour) {
  // Two levels of recursion: the stamp is itself decorated by a ContourWalk
  // dotting its own outline. This terminates because a stamp may only
  // decorate art that is already baked, never itself — so the nesting is
  // finite by construction rather than by a depth limit.
  Host host;
  ContourWalk dots;
  dots.spacing = 6.0f;
  dots.draw = [](SkCanvas& c, const PathSample&, const PaintContext&) {
    SkPaint p;
    p.setColor(SK_ColorCYAN);
    c.drawRect(SkRect::MakeXYWH(-1, -1, 2, 2), p);
  };

  ContourWalk outer;
  outer.spacing = 40.0f;
  outer.stamp = box().width(16).height(16).fill(red()).foreground(dots);

  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .inset(40, 40, 40, 40)
                                       .absolute()
                                       .foreground(outer)));
  host.frame();
  int redPx = 0, cyanPx = 0;
  for (int x = 0; x < 200; x += 2)
    for (int y = 0; y < 200; y += 2) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorRED)
        redPx++;
      else if (c == SK_ColorCYAN)
        cyanPx++;
    }
  EXPECT_GT(redPx, 20);   // stamps landed
  EXPECT_GT(cyanPx, 10);  // and their own walked borders too
}

TEST(ComposeStamps, CustomLeafDrawsNestedComposer) {
  // A custom() leaf hosting an entire nested Composer: the recursion closes
  // at the paint phase, with the inner composer owning its own ticker and
  // size and drawing straight onto the outer canvas.
  Host host;
  auto nestedTicker = std::make_shared<sigil::motion::Ticker>();
  auto nested = std::make_shared<Composer>(*nestedTicker, fonts());
  nested->setSize({60, 60});
  nested->render(
      box().padding(10).fill(green()).child(box().grow(1).fill(red())));

  host.composer.render(box().child(
      custom([nested, nestedTicker](SkCanvas& c, const PaintContext&) {
        nested->draw(c);
      })
          .width(60)
          .height(60)
          .cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);  // nested padding ring
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);  // nested content
}

// ---------------------------------------------------------------------------
// hitTest: paint order, transforms, shapes.

TEST(ComposeQueries, HitTestRespectsPaintOrderAndKeys) {
  Host host;
  host.composer.render(
      stack()
          .child(box().key("under").inset(0).fill(red()))
          .child(box()
                     .key("over")
                     .width(60)
                     .height(60)
                     .inset(20, 20, 120, 120)
                     .absolute()
                     .fill(green()))
          .child(box()
                     .width(30)
                     .height(30)
                     .inset(150, 150, 20, 20)
                     .absolute()
                     .fill(blue())));  // keyless → falls to root
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "over");
  EXPECT_EQ(host.composer.hitTest({120, 120}).value_or(""), "under");
  // Keyless box resolves to its nearest keyed ancestor (none here above
  // the stack root, which is keyless) — the "under" sibling is NOT an
  // ancestor, so the keyless box hits nothing of its own and the point
  // falls through to "under".
  EXPECT_EQ(host.composer.hitTest({160, 160}).value_or(""), "under");
  EXPECT_FALSE(host.composer.hitTest({500, 500}).has_value());
}

TEST(ComposeQueries, HitTestHonorsShapeAndRotation) {
  Host host;
  host.composer.render(box()
                           .child(box()
                                      .key("star")
                                      .width(100)
                                      .height(100)
                                      .shape(shapes::star(5))
                                      .fill(red()))
                           .child(box()
                                      .key("spun")
                                      .width(80)
                                      .height(20)
                                      .inset(60, 140, 60, 40)
                                      .absolute()
                                      .rotate(90.0f)
                                      .fill(green())));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "star");
  // Between the star's arms: inside the box, outside the silhouette.
  EXPECT_FALSE(host.composer.hitTest({20, 20}).has_value());

  // The 80x20 bar at (60,140) rotated 90° about its center paints as a
  // 20x80 bar centered at (100,150): x∈[90,110], y∈[110,190].
  EXPECT_EQ(host.composer.hitTest({100, 115}).value_or(""), "spun");
  EXPECT_FALSE(host.composer.hitTest({70, 150}).has_value());  // unrotated
                                                               // footprint
}

#include <sigilcompose/Routers.h>

// ---------------------------------------------------------------------------
// Routers (connector route library).

TEST(ComposeDerive, OrthogonalRouterRunsManhattan) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});
  host.composer.render(stack()
                           .child(box()
                                      .key("a")
                                      .width(20)
                                      .height(20)
                                      .inset(10, 10, 170, 170)
                                      .absolute()
                                      .fill(red()))
                           .child(box()
                                      .key("b")
                                      .width(20)
                                      .height(20)
                                      .inset(160, 160, 20, 20)
                                      .absolute()
                                      .fill(green()))
                           .child(connector("a", "b", routers::orthogonal())
                                      .inset(0)
                                      .foreground(wire)
                                      .zIndex(-1)));
  host.frame();
  // Centers (20,20) and (170,170); midX = 95: H leg at y=20, V leg at
  // x=95, H leg at y=170.
  EXPECT_EQ(host.pixel(60, 20), SK_ColorYELLOW);    // first horizontal leg
  EXPECT_EQ(host.pixel(95, 100), SK_ColorYELLOW);   // vertical run
  EXPECT_EQ(host.pixel(130, 170), SK_ColorYELLOW);  // final horizontal leg
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK);    // nowhere near diagonal
}

TEST(ComposeDerive, ArcRouterBowsOffTheChord) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});
  host.composer.render(stack()
                           .child(box()
                                      .key("a")
                                      .width(10)
                                      .height(10)
                                      .inset(20, 95, 170, 95)
                                      .absolute()
                                      .fill(red()))
                           .child(box()
                                      .key("b")
                                      .width(10)
                                      .height(10)
                                      .inset(170, 95, 20, 95)
                                      .absolute()
                                      .fill(green()))
                           .child(connector("a", "b", routers::arc(0.3f))
                                      .inset(0)
                                      .foreground(wire)
                                      .zIndex(-1)));
  host.frame();
  // Horizontal chord from (25,100) to (175,100), bulge 0.3×150 = 45 px
  // toward +normal (downward-left convention: normal of (+x,0) is
  // (0,+y) → the bow lands at y ≈ 145).
  EXPECT_EQ(host.pixel(100, 145), SK_ColorYELLOW);  // bowed midpoint
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);   // chord midpoint empty
}

TEST(ComposeDerive, ConnectorGapPullsTheWireOffTheEndpoints) {
  // A route runs to the node BOX's centre, and a box is often much larger
  // than the shape drawn inside it — an sdf:: panel, for instance, reserves
  // room for its glow. Without a terminal gap the wire is drawn straight
  // through the visible terminal to a centre nobody can see. The gap is the
  // same pull-back Anchor takes, spelled on connector().
  const auto scene = [](float gap) {
    PathFormat wire;
    wire.width = 4;
    wire.strokeFill = Fill::color({1, 1, 0, 1});
    return stack()
        .child(box()
                   .key("a")
                   .width(20)
                   .height(20)
                   .inset(10, 90, 170, 90)
                   .absolute()
                   .fill(red()))
        .child(box()
                   .key("b")
                   .width(20)
                   .height(20)
                   .inset(170, 90, 10, 90)
                   .absolute()
                   .fill(green()))
        .child(
            connector("a", "b", {}, gap).inset(0).foreground(wire).zIndex(1));
  };
  // Control: with gap 0 the wire runs centre to centre, (20,100) → (180,100),
  // and paints OVER both terminal boxes. Without this arm, "the gapped wire
  // does not reach the terminal" would also pass on a wire that was never
  // drawn.
  Host flush;
  flush.composer.render(scene(0.0f));
  flush.frame();
  EXPECT_EQ(flush.pixel(25, 100), SK_ColorYELLOW)
      << "the gapless wire must still reach into its near terminal";
  EXPECT_EQ(flush.pixel(175, 100), SK_ColorYELLOW)
      << "…and pierce the far one (that IS the complaint)";
  EXPECT_EQ(flush.pixel(100, 100), SK_ColorYELLOW);
  // The gap: 30 px pulled back at EACH end — the wire now runs x ∈
  // [50, 150], both terminals show their own fill, the middle survives.
  Host gapped;
  gapped.composer.render(scene(30.0f));
  gapped.frame();
  EXPECT_EQ(gapped.pixel(25, 100), SK_ColorRED)
      << "the gap did not pull the wire off its near terminal";
  EXPECT_EQ(gapped.pixel(175, 100), SK_ColorGREEN)
      << "the gap did not pull the wire off its far terminal";
  EXPECT_EQ(gapped.pixel(145, 100), SK_ColorYELLOW)
      << "the wire ends before its gap says to";
  EXPECT_EQ(gapped.pixel(160, 100), SK_ColorBLACK)
      << "the wire overran its gap";
  EXPECT_EQ(gapped.pixel(100, 100), SK_ColorYELLOW);  // the run survives
}

#include <sigilcompose/Layouts.h>

// ---------------------------------------------------------------------------
// Organic layout schemes (Layouts.h).

TEST(ComposeLayouts, RadialPlacesChildrenOnTheRing) {
  Host host;
  std::vector<Element> dots;
  for (int i = 0; i < 4; ++i)
    dots.push_back(
        box().width(10).height(10).fill(red()).key("d" + std::to_string(i)));
  host.composer.render(
      box().child(layout(layouts::Radial{.radiusFraction = 0.8f})
                      .width(200)
                      .height(200)
                      .children(dots)));
  host.frame();
  // Radius 80 from center (100,100), starting up, clockwise quarters.
  auto center = [&](const char* k) {
    auto r = host.composer.bounds(k);
    return SkPoint{r->centerX(), r->centerY()};
  };
  EXPECT_NEAR(center("d0").x(), 100, 1);
  EXPECT_NEAR(center("d0").y(), 20, 1);   // top
  EXPECT_NEAR(center("d1").x(), 180, 1);  // right
  EXPECT_NEAR(center("d1").y(), 100, 1);
  EXPECT_NEAR(center("d2").y(), 180, 1);  // bottom
  EXPECT_NEAR(center("d3").x(), 20, 1);   // left
}

TEST(ComposeLayouts, AlongPathFollowsAStarContour) {
  Host host;
  std::vector<Element> beads;
  for (int i = 0; i < 10; ++i)
    beads.push_back(
        box().width(6).height(6).fill(green()).key("b" + std::to_string(i)));
  host.composer.render(
      box().child(layout(layouts::AlongPath{.path = shapes::star(5)})
                      .width(180)
                      .height(180)
                      .children(beads)));
  host.frame();
  // First bead sits on the star's top point (contour start).
  auto b0 = host.composer.bounds("b0");
  ASSERT_TRUE(b0.has_value());
  EXPECT_NEAR(b0->centerX(), 90, 1.5);
  EXPECT_NEAR(b0->centerY(), 0, 1.5);
  // All beads land ON the star outline: distance from center between
  // inner and outer radius.
  for (int i = 0; i < 10; ++i) {
    auto r = host.composer.bounds(("b" + std::to_string(i)).c_str());
    ASSERT_TRUE(r.has_value());
    const float dx = r->centerX() - 90, dy = r->centerY() - 90;
    const float dist = std::sqrt(dx * dx + dy * dy);
    EXPECT_GE(dist, 0.4f * 90 - 2);
    EXPECT_LE(dist, 90 + 2);
  }
}

TEST(ComposeTransform, SkewLeansPaintAndHits) {
  // skewX(−12°) leans the card's top to the right about its centre. The
  // point of the case is the second half: hit-testing must walk the shear
  // backwards, so a point that is inside the leaning card but outside its
  // unsheared box still hits it.
  Host host;
  host.composer.render(box().child(box()
                                       .key("card")
                                       .width(40)
                                       .height(40)
                                       .inset(60, 60, 100, 100)
                                       .absolute()
                                       .fill(red())
                                       .skewX(-12.0f)));
  host.frame();
  EXPECT_EQ(host.pixel(101, 64), SK_ColorRED);   // top leaned right
  EXPECT_EQ(host.pixel(61, 64), SK_ColorBLACK);  // vacated top-left
  EXPECT_EQ(host.pixel(58, 97), SK_ColorRED);    // bottom leaned left
  EXPECT_EQ(host.pixel(98, 97), SK_ColorBLACK);  // vacated bottom-right
  auto hit = host.composer.hitTest({101, 64});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, "card");  // transform-aware hit through the shear
  EXPECT_FALSE(host.composer.hitTest({61, 64}).has_value());
}

TEST(ComposeTransform, SkewXPositiveLeansTheTopTowardNegativeX) {
  // THE SIGN PIN. skewX shears about the box centre in screen space, y
  // down, by tan(skewX degrees): a POSITIVE angle displaces the top edge
  // toward NEGATIVE x relative to the bottom edge — the top leans left.
  // The sign is easy to state backwards, so the runtime's answer is
  // pinned here in pixels.
  Host host;
  host.composer.render(box().child(box()
                                       .key("card")
                                       .width(40)
                                       .height(40)
                                       .inset(60, 60, 100, 100)
                                       .absolute()
                                       .fill(red())
                                       .skewX(30.0f)));
  host.frame();
  // The unsheared box is x in [60, 100], y in [60, 100], centre (80, 80).
  // At y = 64 (16 above centre) the shift is tan(30) * -16 ~ -9.2, so the
  // top row spans about [50.8, 90.8]; at y = 97 (17 below) the shift is
  // +9.8, spanning about [69.8, 109.8].
  EXPECT_EQ(host.pixel(54, 64), SK_ColorRED);    // top edge left of the box
  EXPECT_EQ(host.pixel(97, 64), SK_ColorBLACK);  // vacated top-right
  EXPECT_EQ(host.pixel(106, 97), SK_ColorRED);   // bottom edge leaned right
  EXPECT_EQ(host.pixel(63, 97), SK_ColorBLACK);  // vacated bottom-left
  // And hit-testing walks the same shear: the leaned top-left corner is
  // inside the card, the vacated top-right is not.
  auto hit = host.composer.hitTest({54, 64});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, "card");
  EXPECT_FALSE(host.composer.hitTest({97, 64}).has_value());
}

// ---- text fx
// ------------------------------------------------------

#include <sigilcompose/TextFx.h>

TEST(ComposeKinetic, StaggeredRiseRevealsInOrder) {
  // The stagger law: at mid-progress the early glyphs are fully revealed
  // while the late ones haven't started — the canonical staggered reveal,
  // rendered through batched RSXform draws.
  Host host;
  auto tree = [](Animatable<float> progress) {
    return box().padding(10).child(
        text(u8"IIIIIIIIIIII", whiteStyle(32))
            .key("k")
            .fx({.effect = fx::rise(24),
                 .stagger = {.eachMs = 40, .durationMs = 200},
                 .progress = std::move(progress)}));
  };
  host.composer.render(tree(0.0f));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect leftEdge = SkIRect::MakeLTRB(
      (int)b->left(), (int)b->top(), (int)b->left() + 24, (int)b->bottom());
  const SkIRect rightEdge = SkIRect::MakeLTRB(
      (int)b->right() - 24, (int)b->top(), (int)b->right(), (int)b->bottom());
  EXPECT_FALSE(anyWhiteIn(host, leftEdge));  // progress 0: nothing revealed
  host.composer.render(tree(0.45f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, leftEdge));    // head fully in
  EXPECT_FALSE(anyWhiteIn(host, rightEdge));  // tail not started
  host.composer.render(tree(1.0f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, rightEdge));  // everything landed
}

TEST(ComposeKinetic, ATrackKeepsABlurredUnderlayBeneathTheStroke) {
  // A dressed style through the track's batched draw: a dark blurred
  // stroke underlay must stay BENEATH the light stroked foreground — the
  // halo hugs the letterform, the stroke keeps its colour — including
  // mid-cascade, when each glyph's own fade splits the style across
  // several paint buckets and a later letter's halo reaches a landed
  // letter's stroke. The reference is the same cascade with the style cut
  // into two tracked nodes, halo under stroke by stacking order: the one
  // dressed node must composite exactly as that split does.
  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(4.0f);
  stroke.setColor(SK_ColorWHITE);
  SkPaint halo;
  halo.setAntiAlias(true);
  halo.setStyle(SkPaint::kStroke_Style);
  halo.setStrokeWidth(8.0f);
  halo.setColor(0xFF000000);
  const sigil::weave::PaintLayer blurredHalo =
      sigil::weave::PaintLayer::blurred(halo, 4);

  sigil::weave::TextStyle dressed;
  dressed.shaping.fontSize = 64.0f;
  dressed.paint.foreground = stroke;
  dressed.paint.underlays.push_back(blurredHalo);
  sigil::weave::TextStyle haloOnly = dressed;
  haloOnly.paint.foreground = blurredHalo.paint;
  haloOnly.paint.underlays.clear();
  sigil::weave::TextStyle strokeOnly = dressed;
  strokeOnly.paint.underlays.clear();

  constexpr float kMidCascade = 0.45f;
  const auto tracked = [&](const sigil::weave::TextStyle& style,
                           const char* key) {
    return text(u8"OOOOO", style)
        .key(key)
        .absolute()
        .inset(20, 20, 20, 20)
        .fx({.effect = fx::pop(),
             .stagger = {.eachMs = 30, .durationMs = 480},
             .progress = kMidCascade});
  };

  Host actual(420, 140);
  actual.composer.render(box().child(tracked(dressed, "word")));
  actual.frame();
  Host expected(420, 140);
  expected.composer.render(box()
                               .child(tracked(haloOnly, "halos"))
                               .child(tracked(strokeOnly, "strokes")));
  expected.frame();

  int worst = 0, worstX = -1, worstY = -1;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 420; ++x) {
      const SkColor a = actual.pixel(x, y);
      const SkColor b = expected.pixel(x, y);
      const int diff =
          std::max({std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                    std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                    std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b))});
      if (diff > worst) {
        worst = diff;
        worstX = x;
        worstY = y;
      }
    }
  EXPECT_LE(worst, 4) << "the dressed node's underlay left its place under "
                         "the strokes at ("
                      << worstX << ", " << worstY << ")";
}

TEST(ComposeKinetic, TransitionedProgressPaintsLive) {
  // The master progress takes the full Animatable treatment: a with()
  // transition animates the reveal and the node paints live while moving.
  Host host;
  auto tree = [](Animatable<float> progress) {
    return box().padding(10).child(
        text(u8"POP", whiteStyle(40))
            .key("k")
            .fx({.effect = fx::pop(),
                 .stagger = {.eachMs = 20, .durationMs = 150},
                 .progress = std::move(progress)}));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  host.composer.render(
      tree(animate(to(1.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2);                                    // mid-ramp
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // live while animating
  host.frame(0.3);                                    // settle
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())));
}

TEST(ComposeKinetic, ABoundProgressRevealsWithoutARedescribe) {
  // Glyph progress must be classified as CONTENT volatility, not paint-only:
  // it rebuilds glyph geometry, so the node's own recording is invalid the
  // moment it ticks. Classified paint-only, computeVolatile leaves
  // `ownContent` and `subtreeVolatile` false, the picture is never reset,
  // and the reveal FREEZES at whatever progress the last describe recorded.
  //
  // Driving the reveal from a BOUND Output is the only way to see that. Any
  // case that moves the reveal by RE-DESCRIBING marks the node paint-dirty
  // and hides the question entirely, and a case that only checks for ink
  // after settling is satisfied by a frozen half-revealed recording. Hence
  // both halves here: the tail must be dark before, and lit after, with no
  // describe in between.
  Host host;
  choreograph::Output<float> progress{0.0f};
  host.composer.render(box().padding(10).child(
      text(u8"IIIIIIIIIIII", whiteStyle(32))
          .key("k")
          .fx({.effect = fx::rise(24),
               .stagger = {.eachMs = 40, .durationMs = 200},
               .progress = &progress})));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect tail = SkIRect::MakeLTRB((int)b->right() - 24, (int)b->top(),
                                         (int)b->right(), (int)b->bottom());
  ASSERT_FALSE(anyWhiteIn(host, tail))
      << "the tail was already revealed at progress 0, so the check below "
         "cannot tell a live reveal from a frozen one";

  progress = 1.0f;  // ONE describe, and the value moves underneath it
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, tail))
      << "the tail never appeared: the node replayed the recording it made at "
         "progress 0, so glyph progress is not invalidating its own picture";
}

// ---- text fx: tracks, selectors, cascades and the combinators -------------

namespace {

/** What one glyph's effect saw on one call. */
struct FxSample {
  GlyphInfo info;
  float t = 0;
  float random = 0;
};

/** An effect that draws nothing and RECORDS what it was asked. Every fx
 *  question below — which glyphs a selector addressed, which beat a
 *  cascade put them in, what local time they saw — is answerable from the
 *  samples one frame collects, without reading a single pixel. */
TextEffect probe(std::string key, std::vector<FxSample>* into) {
  return fx::effect(std::move(key),
                    [into](const GlyphInfo& g, float t, Rng& rng) {
                      into->push_back({g, t, rng.unit()});
                      return GlyphMod{};
                    });
}

/** The walk-order indices the samples cover. */
std::vector<size_t> addressed(const std::vector<FxSample>& samples) {
  std::vector<size_t> out;
  out.reserve(samples.size());
  for (const FxSample& s : samples) out.push_back(s.info.index);
  return out;
}

/** A track whose effect records, over the whole text unless told otherwise. */
Track probeTrack(std::vector<FxSample>* into, Selector where = {},
                 Stagger cascade = {.eachMs = 0, .durationMs = 100},
                 float progress = 1.0f) {
  return Track{.where = std::move(where),
               .effect = probe("probe", into),
               .stagger = std::move(cascade),
               .progress = progress};
}

}  // namespace

TEST(ComposeTextFx, AWordCascadeBeatsOncePerWordAndInOrder) {
  // The stagger runs over the track's UNITS, so a word-unit cascade gives
  // every glyph of a word one shared local time and delays the next word by
  // a whole beat. Per-glyph spacing inside a word would be a different
  // effect entirely, and is what `unit::Glyph` is for.
  Host host(300, 120);
  std::vector<FxSample> samples;
  host.composer.render(box().padding(10).child(
      text(u8"AAA BBB CCC", whiteStyle(20))
          .key("k")
          .fx(probeTrack(
              &samples, {},
              stagger(unit::Word, {.eachMs = 100, .durationMs = 100}), 0.5f))));
  host.frame();
  ASSERT_EQ(samples.size(), 9u) << "the probe did not see every glyph";

  // Three words, three beats: 0..2 / 3..5 / 6..8.
  for (size_t i = 0; i < 9; ++i)
    EXPECT_EQ(samples[i].info.unitIndex, (uint32_t)(i / 3))
        << "glyph " << i << " landed in the wrong beat";
  EXPECT_EQ(samples[0].info.unitCount, 3u);
  // Within a word every glyph shares its word's time…
  EXPECT_FLOAT_EQ(samples[0].t, samples[2].t);
  EXPECT_FLOAT_EQ(samples[3].t, samples[5].t);
  // …and the cascade runs head-first.
  EXPECT_GT(samples[0].t, samples[3].t);
  EXPECT_GT(samples[3].t, samples[6].t);
}

TEST(ComposeTextFx, ASentenceCascadeBeatsOncePerSentence) {
  Host host(400, 160);
  std::vector<FxSample> samples;
  host.composer.render(box().padding(10).child(
      text(u8"One two. Three four. Five.", whiteStyle(16))
          .key("k")
          .width(pct(100))
          .fx(probeTrack(
              &samples, {},
              stagger(unit::Sentence, {.eachMs = 100, .durationMs = 100}),
              0.5f))));
  host.frame();
  ASSERT_FALSE(samples.empty());
  EXPECT_EQ(samples.front().info.unitCount, 3u)
      << "ICU sentence segmentation did not produce three beats";
  // Every glyph of a sentence shares one time, and the sentences cascade.
  float previous = 2.0f;
  uint32_t previousUnit = ~0u;
  for (const FxSample& s : samples) {
    if (s.info.unitIndex != previousUnit) {
      EXPECT_LT(s.t, previous) << "sentence " << s.info.unitIndex
                               << " did not start after its predecessor";
      previous = s.t;
      previousUnit = s.info.unitIndex;
    } else {
      EXPECT_FLOAT_EQ(s.t, previous) << "one sentence split into two beats";
    }
  }
}

TEST(ComposeTextFx, AClusterIsOneBeatSoAMarkNeverLeavesItsLetter) {
  // The default unit is the CLUSTER, and that is the whole reason it is
  // the default: a base letter and its combining mark are one thing on the
  // page. Staggered per glyph the accent would fly on its own.
  Host host(300, 120);
  std::vector<FxSample> samples;
  host.composer.render(box().padding(10).child(
      text(u8"x́ýz", whiteStyle(28))
          .key("k")
          .fx(probeTrack(&samples, {}, {.eachMs = 100, .durationMs = 100},
                         0.5f))));
  host.frame();
  ASSERT_FALSE(samples.empty());
  const uint32_t beats = samples.front().info.unitCount;
  if (beats == samples.size())
    GTEST_SKIP() << "this font composed the marks into single glyphs, so "
                    "there is no multi-glyph cluster to keep together";
  // Glyphs at one text offset are one cluster and must share one time.
  for (size_t i = 1; i < samples.size(); ++i)
    if (samples[i].info.textIndex == samples[i - 1].info.textIndex) {
      EXPECT_EQ(samples[i].info.unitIndex, samples[i - 1].info.unitIndex);
      EXPECT_FLOAT_EQ(samples[i].t, samples[i - 1].t)
          << "a combining mark was staggered away from its base letter";
    }

  // THE CONTROL: unit::Glyph is the raw shaping unit and DOES separate
  // them. Without this the check above is satisfied by a cascade that
  // never beat at all.
  std::vector<FxSample> raw;
  host.composer.render(box().padding(10).child(
      text(u8"x́ýz", whiteStyle(28))
          .key("k")
          .fx(probeTrack(
              &raw, {},
              stagger(unit::Glyph, {.eachMs = 100, .durationMs = 400}),
              0.5f))));
  host.frame();
  ASSERT_EQ(raw.size(), samples.size());
  EXPECT_EQ(raw.front().info.unitCount, (uint32_t)raw.size());
  bool anySplit = false;
  for (size_t i = 1; i < raw.size(); ++i)
    if (raw[i].info.textIndex == raw[i - 1].info.textIndex &&
        raw[i].t != raw[i - 1].t)
      anySplit = true;
  EXPECT_TRUE(anySplit)
      << "unit::Glyph did not split the cluster, so the cluster case above "
         "proves nothing";
}

TEST(ComposeTextFx, TextFillAndTextStrokeTravelWithAMovingGlyph) {
  // A letter in flight is painted with the same glyph paint a resting one
  // is: textFill's material is its foreground and textStroke's outline is
  // an underlay, both carried through the batched draw. Shadowed instead,
  // a chrome wordmark loses its chrome the moment it starts moving.
  Host host(220, 140);
  const auto tree = [](bool moving) {
    Element t = text(u8"II", whiteStyle(48))
                    .key("k")
                    .textFill(Material::solid({0, 1, 0, 1}));
    if (moving)
      t.fx({.effect = fx::effect("still", [](const GlyphInfo&, float, Rng&) {
              return GlyphMod{};
            })});
    return box().padding(10).child(std::move(t));
  };
  const auto greenPixels = [&] {
    auto b = host.composer.bounds("k");
    EXPECT_TRUE(b.has_value());
    int count = 0;
    for (int y = (int)b->top(); y < (int)b->bottom(); ++y)
      for (int x = (int)b->left(); x < (int)b->right(); ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetG(c) > 200 && SkColorGetR(c) < 80) ++count;
      }
    return count;
  };
  host.composer.render(tree(false));
  host.frame();
  const int resting = greenPixels();
  ASSERT_GT(resting, 20) << "textFill did not paint the resting glyphs green";
  host.composer.render(tree(true));
  host.frame();
  EXPECT_GT(greenPixels(), resting / 2)
      << "the fx path dropped textFill's material and painted the style's "
         "own foreground instead";
}

TEST(ComposeTextFx, SelectorsAddressWhatTheyName) {
  // Absolute forms, the regular expression and the substring, all resolved
  // against the paragraph and all comparable values.
  Host host(300, 120);
  const auto run = [&](Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  EXPECT_EQ(run(sel::word(1)), (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sel::words(1, 3)), (std::vector<size_t>{3, 4, 5, 6, 7, 8}));
  EXPECT_EQ(run(sel::text(u8"BBB")), (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sel::regex(u8"B+")), (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sel::line(0)).size(), 9u);  // one line, everything
  EXPECT_EQ(run(Selector()).size(), 9u);    // default: everything
  EXPECT_TRUE(run(sel::regex(u8"([")).empty())
      << "a pattern that does not compile must select NOTHING rather than "
         "everything — silently addressing the whole paragraph is the "
         "failure this rule exists to prevent";
}

TEST(ComposeTextFx, SelectorAlgebraUnionsIntersectsAndComplements) {
  Host host(300, 120);
  const auto run = [&](Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  EXPECT_EQ(run(sel::word(0) | sel::word(2)),
            (std::vector<size_t>{0, 1, 2, 6, 7, 8}));
  EXPECT_EQ(run(sel::words(0, 2) & sel::word(1)),
            (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(!sel::word(1)), (std::vector<size_t>{0, 1, 2, 6, 7, 8}));
  EXPECT_TRUE(run(sel::word(0) & sel::word(1)).empty());
}

TEST(ComposeTextFx, EachTakeAndDropPartitionEveryUnitExactly) {
  // take(n) and drop(n) answer opposite sides of one cut inside every unit,
  // so together they cover the text exactly once. A gap or an overlap here
  // means a two-track composition double-counts letters it should split.
  Host host(300, 120);
  const auto run = [&](Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  const std::vector<size_t> firsts = run(sel::each(unit::Word).take(1));
  const std::vector<size_t> rest = run(sel::each(unit::Word).drop(1));
  EXPECT_EQ(firsts, (std::vector<size_t>{0, 3, 6}));
  EXPECT_EQ(rest, (std::vector<size_t>{1, 2, 4, 5, 7, 8}));
  std::vector<size_t> both = firsts;
  both.insert(both.end(), rest.begin(), rest.end());
  std::sort(both.begin(), both.end());
  std::vector<size_t> all(9);
  std::iota(all.begin(), all.end(), 0u);
  EXPECT_EQ(both, all) << "take/drop is not a partition of the units";
}

TEST(ComposeTextFx, RandomOriginIsAStableScatterAcrossFrames) {
  // A seeded cascade must give the SAME order every frame: a scatter that
  // reshuffles can never settle, so it can never cache, and it flickers.
  std::vector<FxSample> first, second;
  const auto tree = [&](std::vector<FxSample>* into) {
    return box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(into, {},
                           {.eachMs = 100,
                            .durationMs = 100,
                            .from = Stagger::From::Random},
                           0.5f)));
  };
  // TWO HOSTS, one paint each: re-describing the same tracks into one host
  // prunes and replays the recording, which is the right behaviour and
  // would leave this test measuring nothing.
  Host hostA(300, 120), hostB(300, 120);
  hostA.composer.render(tree(&first));
  hostA.frame();
  hostB.composer.render(tree(&second));
  hostB.frame();
  ASSERT_EQ(first.size(), second.size());
  ASSERT_FALSE(first.empty());
  for (size_t i = 0; i < first.size(); ++i)
    EXPECT_FLOAT_EQ(first[i].t, second[i].t)
        << "the seeded cascade reshuffled between frames";
  // …and it really is a scatter, not the Start ladder wearing a new name.
  bool anyOutOfOrder = false;
  for (size_t i = 1; i < first.size(); ++i)
    if (first[i].t > first[i - 1].t) anyOutOfOrder = true;
  EXPECT_TRUE(anyOutOfOrder) << "Random produced the Start ordering";
  // The Rng an effect draws from is seeded per glyph and is stable too.
  for (size_t i = 0; i < first.size(); ++i)
    EXPECT_FLOAT_EQ(first[i].random, second[i].random);
}

TEST(ComposeTextFx, NestedStaggerDelaysGlyphsInsideTheirWordsBeat) {
  // then() compounds: each word gets its beat, and inside that beat each
  // glyph gets its own start. Two ladders, one master progress.
  Host host(300, 120);
  std::vector<FxSample> samples;
  Stagger cascade = stagger(unit::Word, {.eachMs = 200, .durationMs = 100});
  cascade.then(unit::Glyph, {.eachMs = 50, .durationMs = 100});
  // A beat is 100 + 50·2 = 200 ms and the whole cascade spans 400; 0.3 of
  // that lands inside the first word's beat, where the two ladders are
  // both readable.
  host.composer.render(box().padding(10).child(
      text(u8"AAA BBB", whiteStyle(20))
          .key("k")
          .fx(probeTrack(&samples, {}, cascade, 0.3f))));
  host.frame();
  ASSERT_EQ(samples.size(), 6u);
  // Inside a word the glyphs no longer share a time — the inner ladder ran.
  EXPECT_GT(samples[0].t, samples[1].t);
  EXPECT_GT(samples[1].t, samples[2].t);
  // …and the second word still starts a whole beat behind the first.
  EXPECT_GT(samples[0].t, samples[3].t);
}

TEST(ComposeTextFx, TwoTracksComposeByAddingOffsets) {
  // The algebra: offsets add. Two tracks each pushing 30 px right put the
  // glyph 60 px right of rest, which is the only reading under which
  // stacking tracks is composition rather than replacement.
  Host host(220, 120);
  const auto shove = [](float dx) {
    return fx::effect(
        "shove" + std::to_string((int)dx),
        [dx](const GlyphInfo&, float, Rng&) {
          GlyphMod m;
          m.dx = dx;
          return m;
        },
        /*reach=*/80.0f);
  };
  host.composer.render(box().padding(10).child(text(u8"I", whiteStyle(40))
                                                   .key("k")
                                                   .fx({.effect = shove(30)})
                                                   .fx({.effect = shove(30)})));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const int top = (int)b->top(), bottom = (int)b->bottom();
  EXPECT_FALSE(anyWhiteIn(
      host, SkIRect::MakeLTRB((int)b->left(), top, (int)b->right(), bottom)))
      << "the glyph never left its rest position";
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left() + 55, top,
                                                 (int)b->left() + 75, bottom)))
      << "two 30 px tracks did not add to 60 px";
}

TEST(ComposeTextFx, ATrackReachKeepsAWideThrowInsideTheCull) {
  // ownPaintBounds has no idea what an effect will do, so a track that
  // throws a glyph out of the element's box must SAY how far. Under-report
  // and the cached picture truncates it with no diagnostic — which is
  // exactly the control below.
  Host host(220, 200);
  const auto drop = [](float reach) {
    Track t{.effect = fx::effect(
                "drop",
                [](const GlyphInfo&, float, Rng&) {
                  GlyphMod m;
                  m.dy = 60;
                  return m;
                },
                /*reach=*/0.0f)};
    t.reach = reach;
    return t;
  };
  const auto inkBelow = [&](float reach) {
    host.composer.render(box().padding(10).child(text(u8"I", whiteStyle(40))
                                                     .key("k")
                                                     .cache(Cache::Texture)
                                                     .fx(drop(reach))));
    host.frame();
    auto b = host.composer.bounds("k");
    EXPECT_TRUE(b.has_value());
    return anyWhiteIn(
        host, SkIRect::MakeLTRB((int)b->left(), (int)b->bottom() + 20,
                                (int)b->right(), (int)b->bottom() + 70));
  };
  EXPECT_TRUE(inkBelow(80.0f)) << "a declared reach did not reach the cull";
  EXPECT_FALSE(inkBelow(0.0f))
      << "the control did not truncate, so the check above proves nothing "
         "about the cull";
}

TEST(ComposeTextFx, EqualTrackListsPruneAndAKeyedLambdaComparesByKey) {
  // Tracks are values, so a re-describe that says the same thing must not
  // mark the node dirty. Without it every text node carrying fx re-records
  // on every frame, whatever its progress is doing.
  Host host(220, 120);
  const auto tree = [](TextEffect effect) {
    return box().padding(10).child(text(u8"AAA", whiteStyle(20))
                                       .key("k")
                                       .fx({.effect = std::move(effect)}));
  };
  host.composer.render(tree(fx::rise(20)));
  host.frame();
  host.composer.render(tree(fx::rise(20)));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged track list did not prune";
  host.composer.render(tree(fx::rise(24)));
  host.frame();
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "a changed preset parameter pruned anyway";

  // A keyed lambda compares by its KEY: same key prunes, different key does
  // not. That is the whole contract fx::effect() asks of its caller.
  const auto body = [](const GlyphInfo&, float, Rng&) { return GlyphMod{}; };
  host.composer.render(tree(fx::effect("mine", body)));
  host.frame();
  host.composer.render(tree(fx::effect("mine", body)));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "two effects under one key did not compare equal";
  host.composer.render(tree(fx::effect("other", body)));
  host.frame();
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "a re-keyed effect pruned onto the old one";
}

TEST(ComposeTextFx, SettledMultiTrackTextStopsPaintingLive) {
  // Two bound progresses that have held still for long enough release their
  // volatility, exactly as one does — the settle machinery covers EVERY
  // track's progress or a second track pins the node live forever.
  Host host(220, 120);
  choreograph::Output<float> a{0.0f}, b{0.0f};
  host.composer.render(box().padding(10).child(
      text(u8"AAA BBB", whiteStyle(20))
          .key("k")
          .fx({.effect = fx::rise(12), .progress = &a})
          .fx({.effect = fx::slide(-8), .progress = &b})));
  host.frame();
  a = 1.0f;
  b = 1.0f;
  for (int i = 0; i < 12; ++i) host.frame(0.016);  // the release warms up
  unsigned paints = 0, records = 0;
  for (int i = 0; i < 5; ++i) {
    host.frame(0.016);
    paints += host.composer.stats().nodesPainted;
    records += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(paints, 0u) << "settled multi-track text kept painting live";
  EXPECT_EQ(records, 0u) << "settled multi-track text kept re-recording";

  // …and the frame either progress moves again, the node must re-declare
  // before a stale recording replays.
  b = 0.2f;
  host.frame(0.016);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "the second track moved and nothing re-declared";
}

// ---- the combinators, as pure values --------------------------------------

namespace {
/** An effect that reports a constant dy, so a composition's arithmetic is
 *  readable straight off the returned GlyphMod. */
TextEffect constantDy(float dy) {
  return fx::effect("constantDy" + std::to_string((int)dy),
                    [dy](const GlyphInfo&, float, Rng&) {
                      GlyphMod m;
                      m.dy = dy;
                      return m;
                    });
}
/** An effect that reports the local time it was handed. */
TextEffect reportT() {
  return fx::effect("reportT", [](const GlyphInfo&, float t, Rng&) {
    GlyphMod m;
    m.dy = t;
    return m;
  });
}
GlyphMod evaluate(const TextEffect& effect, float t) {
  GlyphInfo g;
  Rng rng(1);
  return effect(g, t, rng);
}
}  // namespace

TEST(ComposeTextFx, SeqRenormalizesEachPhaseOverItsOwnWindow) {
  // Each phase sees a full 0→1 across its slice of local time — that is
  // what makes a sequence a sequence rather than three effects sharing one
  // clock and each playing a third of its curve.
  const TextEffect sequence = fx::seq(reportT().until(0.5f), reportT());
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.0f).dy, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.25f).dy, 0.5f);  // half of phase one
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.75f).dy, 0.5f);  // half of phase two
  EXPECT_FLOAT_EQ(evaluate(sequence, 1.0f).dy, 1.0f);
  // The last phase runs to the end whatever it was declared with.
  const TextEffect short_ =
      fx::seq(reportT().until(0.5f), reportT().until(0.6f));
  EXPECT_FLOAT_EQ(evaluate(short_, 1.0f).dy, 1.0f);
}

TEST(ComposeTextFx, SeqCutsHardByDefaultAndLerpsAcrossAnXfade) {
  const TextEffect cut = fx::seq(constantDy(10).until(0.5f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(cut, 0.49f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(cut, 0.51f).dy, 0.0f);

  const TextEffect faded =
      fx::seq(constantDy(10).until(0.5f).xfade(0.2f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(faded, 0.29f).dy, 10.0f);    // before the window
  EXPECT_NEAR(evaluate(faded, 0.40f).dy, 5.0f, 1e-4f);  // halfway across
  EXPECT_NEAR(evaluate(faded, 0.50f).dy, 0.0f, 1e-4f);  // at the joint
  EXPECT_FLOAT_EQ(evaluate(faded, 0.60f).dy, 0.0f);     // past it
}

TEST(ComposeTextFx, MixEvaluatesBothAndComposesByTheTrackAlgebra) {
  const TextEffect both = fx::mix(constantDy(10), constantDy(4));
  EXPECT_FLOAT_EQ(evaluate(both, 0.5f).dy, 14.0f);  // offsets add
  const auto half = [](float scale) {
    return fx::effect("half" + std::to_string((int)(scale * 10)),
                      [scale](const GlyphInfo&, float, Rng&) {
                        GlyphMod m;
                        m.scale = scale;
                        m.alpha = scale;
                        return m;
                      });
  };
  const TextEffect scaled = fx::mix(half(0.5f), half(0.5f));
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).scale, 0.25f);  // scale multiplies
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).alpha, 0.25f);  // …and so does alpha
}

TEST(ComposeTextFx, CombinatorsAreComparableWhenTheirOperandsAre) {
  EXPECT_TRUE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
              fx::seq(fx::rise(20).until(0.5f), fx::pop()));
  EXPECT_FALSE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
               fx::seq(fx::rise(20).until(0.6f), fx::pop()));
  EXPECT_FALSE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
               fx::seq(fx::rise(22).until(0.5f), fx::pop()));
  EXPECT_TRUE(fx::mix(fx::rise(20), fx::slide()) ==
              fx::mix(fx::rise(20), fx::slide()));
  EXPECT_FALSE(fx::mix(fx::rise(20), fx::slide()) ==
               fx::mix(fx::slide(), fx::rise(20)));
}

TEST(ComposeTextFx, KeysReproducesEveryEntryAtItsOwnPosition) {
  // A published table is a promise about the moments it names. Whatever the
  // curve between them, the deviation AT an entry is the entry.
  const std::vector<fx::Key> table = {
      {0.00f, {}},
      {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
      {0.65f, {.scaleX = 0.95f, .scaleY = 1.05f}},
      {1.00f, {}}};
  const TextEffect rubber = fx::keys(table, &choreograph::easeInOutCubic);
  for (const fx::Key& key : table) {
    EXPECT_FLOAT_EQ(evaluate(rubber, key.at).scaleX, key.mod.scaleX)
        << "scaleX at " << key.at;
    EXPECT_FLOAT_EQ(evaluate(rubber, key.at).scaleY, key.mod.scaleY)
        << "scaleY at " << key.at;
  }
  // Outside the table's own span it HOLDS at the ends rather than
  // extrapolating numbers nobody published.
  EXPECT_FLOAT_EQ(evaluate(rubber, -0.5f).scaleX, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(rubber, 2.0f).scaleX, 1.0f);
}

TEST(ComposeTextFx, KeysEasesEachSegmentOnItsOwn) {
  // THE WHOLE CURVE, EVERY SEGMENT — which is what a keyframe list means
  // and what one curve stretched across the table would not be. Three
  // entries are the fewest that can tell the two apart.
  const std::vector<fx::Key> ramp = {
      {0.0f, {}}, {0.5f, {.dy = 10.0f}}, {1.0f, {}}};
  const TextEffect linear = fx::keys(ramp);
  EXPECT_FLOAT_EQ(evaluate(linear, 0.125f).dy, 2.5f);

  const TextEffect eased = fx::keys(ramp, &choreograph::easeInOutCubic);
  EXPECT_FLOAT_EQ(evaluate(eased, 0.5f).dy, 10.0f);  // the entry is still exact
  EXPECT_LT(evaluate(eased, 0.125f).dy, 1.5f)  // …the middle is not linear
      << "a quarter of the way into the first segment the reading is the "
         "linear one, so the curve was not applied to the segment";
  // The second segment runs the same curve over its own span: a quarter in
  // and a quarter from the end of the two segments are mirror readings.
  EXPECT_NEAR(evaluate(eased, 0.375f).dy, evaluate(eased, 0.625f).dy, 1e-4f);

  // A per-entry curve governs the segment that OPENS at that entry, and no
  // other.
  std::vector<fx::Key> mixed = ramp;
  mixed[0].ease = &choreograph::easeNone;
  const TextEffect part = fx::keys(mixed, &choreograph::easeInOutCubic);
  EXPECT_FLOAT_EQ(evaluate(part, 0.125f).dy, 2.5f);
  EXPECT_NEAR(evaluate(part, 0.625f).dy, evaluate(eased, 0.625f).dy, 1e-4f);
}

TEST(ComposeTextFx, KeysCutsASubstitutionAndLerpsAMatchingAxis) {
  // The seq crossfade's rules, because it is the same arithmetic: there is
  // no half-way glyph between two outlines, and an axis is the one
  // substitution with a continuum — and only between two entries naming the
  // SAME axis.
  const TextEffect letters =
      fx::keys({{0.0f, {.codepoint = U'A'}}, {1.0f, {.codepoint = U'B'}}});
  EXPECT_EQ(evaluate(letters, 0.40f).codepoint, U'A');
  EXPECT_EQ(evaluate(letters, 0.60f).codepoint, U'B');

  const sigil::weave::FontVariation light("GRAD", 400.0f);
  const sigil::weave::FontVariation heavy("GRAD", 800.0f);
  const TextEffect swept =
      fx::keys({{0.0f, {.axis = light}}, {1.0f, {.axis = heavy}}});
  const GlyphMod midway = evaluate(swept, 0.5f);
  ASSERT_TRUE(midway.axis.has_value());
  EXPECT_FLOAT_EQ(midway.axis.value_or(sigil::weave::FontVariation()).value,
                  600.0f);

  const sigil::weave::FontVariation slant("slnt", -10.0f);
  const TextEffect crossed =
      fx::keys({{0.0f, {.axis = light}}, {1.0f, {.axis = slant}}});
  const auto tagOf = [](const GlyphMod& mod) {
    return mod.axis ? std::string(mod.axis->tag, 4) : std::string("(unset)");
  };
  EXPECT_EQ(tagOf(evaluate(crossed, 0.4f)), "GRAD");
  EXPECT_EQ(tagOf(evaluate(crossed, 0.6f)), "slnt")
      << "two different axes were averaged, which names a coordinate on "
         "neither of them";
}

TEST(ComposeTextFx, AKeyTableIsComparableByItsNumbersAndItsCurves) {
  const auto table = [](float peak) {
    return std::vector<fx::Key>{
        {0.0f, {}}, {0.5f, {.scaleY = peak}}, {1.0f, {}}};
  };
  EXPECT_TRUE(fx::keys(table(1.25f)) == fx::keys(table(1.25f)));
  EXPECT_FALSE(fx::keys(table(1.25f)) == fx::keys(table(1.30f)));
  // The curve is part of the identity. A table re-eased is a different
  // motion, and an effect comparing equal to the one it replaced would go
  // on drawing the old one with no diagnostic.
  EXPECT_FALSE(fx::keys(table(1.25f)) ==
               fx::keys(table(1.25f), &choreograph::easeInOutCubic));
  EXPECT_FALSE(fx::keys(table(1.25f), &choreograph::easeOutQuad) ==
               fx::keys(table(1.25f), &choreograph::easeInOutCubic));
  EXPECT_TRUE(fx::keys(table(1.25f), &choreograph::easeInOutCubic) ==
              fx::keys(table(1.25f), &choreograph::easeInOutCubic));
}

TEST(ComposeTextFx, AKeyedTrackPrunesWhenItsTableIsUnchanged) {
  Host host;
  const auto tree = [] {
    return box().padding(10).child(
        text(u8"KEYS", whiteStyle(28))
            .key("k")
            .fx({.effect =
                     fx::keys({{0.0f, {}}, {0.5f, {.dy = -8.0f}}, {1.0f, {}}},
                              &choreograph::easeInOutCubic)}));
  };
  host.composer.render(tree());
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  host.composer.render(tree());  // fresh Elements, an identical table
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame(0.016);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // The control: a table with one number moved is a different value, and
  // the node it describes has to be patched.
  host.composer.render(box().padding(10).child(
      text(u8"KEYS", whiteStyle(28))
          .key("k")
          .fx({.effect =
                   fx::keys({{0.0f, {}}, {0.5f, {.dy = -9.0f}}, {1.0f, {}}},
                            &choreograph::easeInOutCubic)})));
  EXPECT_GT(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeTextFx, HoldWithholdsTheEffectUntilTheBeatOpens) {
  const TextEffect held = fx::hold(constantDy(10));
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).alpha, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).dy, 0.0f)
      << "the wrapped effect ran on a beat that had not opened";
  // …and the moment it opens, EXACTLY the wrapped effect.
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).alpha, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 1.0f).dy, 10.0f);

  EXPECT_TRUE(fx::hold(fx::rise(20)) == fx::hold(fx::rise(20)));
  EXPECT_FALSE(fx::hold(fx::rise(20)) == fx::hold(fx::rise(22)));
  EXPECT_FALSE(fx::hold(fx::rise(20)) == fx::rise(20));
  EXPECT_FLOAT_EQ(fx::hold(fx::rise(20)).reach(), fx::rise(20).reach());
}

// ---- the second wave of deviations: tint, axis, substitution, matrix ------

namespace {

/** An effect under `key` returning one fixed deviation — the readable way
 *  to drive a single GlyphMod field from a test. */
TextEffect fixed(std::string key, GlyphMod mod) {
  return fx::effect(
      std::move(key), [mod](const GlyphInfo&, float, Rng&) { return mod; },
      /*reach=*/120.0f);
}

/** The bounding box of everything that is not the cleared background. */
SkIRect inkBounds(Host& host, int w, int h) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  SkIRect bounds = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        const SkIRect pixel = SkIRect::MakeXYWH(x, y, 1, 1);
        if (bounds.isEmpty())
          bounds = pixel;
        else
          bounds.join(pixel);
      }
  return bounds;
}

/** The mean y of the inked pixels on columns [left, right). */
float inkCentroidY(Host& host, int h, int left, int right) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(right - left, h));
  host.surface->readPixels(bitmap.pixmap(), left, 0);
  double sum = 0;
  int count = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < right - left; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        sum += y;
        ++count;
      }
  return count ? (float)(sum / count) : 0.0f;
}

/** The mean x of the inked pixels on rows [top, bottom). */
float inkCentroidX(Host& host, int w, int top, int bottom) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, bottom - top));
  host.surface->readPixels(bitmap.pixmap(), 0, top);
  double sum = 0;
  int count = 0;
  for (int y = 0; y < bottom - top; ++y)
    for (int x = 0; x < w; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        sum += x;
        ++count;
      }
  return count ? (float)(sum / count) : 0.0f;
}

}  // namespace

TEST(ComposeTextFx, ColorMulTintsEveryPassOfADressedGlyph) {
  // The multiplier is a statement about the GLYPH, not about its fill: it
  // has to reach the underlays and overlays a span was styled with too.
  // Reaching only the foreground would leave a tinted letter wearing its
  // old drop shadow, which is the bug this test exists to name.
  sigil::weave::TextStyle shadowed = whiteStyle(52);
  shadowed.paint.addUnderlay(sigil::weave::PaintLayer(SK_ColorRED, {16, 0}));

  const auto render = [&](Host& host, std::string key, SkColor4f tint) {
    GlyphMod mod;
    mod.colorMul = tint;
    host.composer.render(box().padding(10).child(
        text(u8"I", shadowed)
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  const auto count = [](Host& host, auto&& predicate) {
    int hits = 0;
    for (int y = 0; y < 140; ++y)
      for (int x = 0; x < 140; ++x)
        if (predicate(host.pixel(x, y))) ++hits;
    return hits;
  };
  const auto reddish = [](SkColor c) {
    return SkColorGetR(c) > 150 && SkColorGetG(c) < 80 && SkColorGetB(c) < 80;
  };
  const auto greenish = [](SkColor c) {
    return SkColorGetG(c) > 150 && SkColorGetR(c) < 80 && SkColorGetB(c) < 80;
  };
  const auto whitish = [](SkColor c) {
    return SkColorGetR(c) > 200 && SkColorGetG(c) > 200 && SkColorGetB(c) > 200;
  };

  Host plain(140, 140);
  render(plain, "plain", {1, 1, 1, 1});
  ASSERT_GT(count(plain, reddish), 10) << "the underlay pass never painted";
  ASSERT_GT(count(plain, whitish), 10) << "the foreground pass never painted";

  Host tinted(140, 140);
  render(tinted, "greenOnly", {0, 1, 0, 1});
  EXPECT_EQ(count(tinted, reddish), 0)
      << "the underlay kept its red under a tint that multiplies red by zero";
  EXPECT_EQ(count(tinted, whitish), 0) << "the foreground kept its white";
  EXPECT_GT(count(tinted, greenish), 10)
      << "the tint painted nothing at all — the glyph vanished instead";
}

TEST(ComposeTextFx, ScrambleChurnsDeterministicallyAndResolvesAtOne) {
  // The churn is seeded from the glyph's identity, so the same local time
  // gives the same character every time it is asked — which is what lets a
  // settled scramble cache instead of boiling forever. And every glyph is
  // the letter the text actually says by the end: a decode that never
  // decodes is not the effect.
  const TextEffect churn = fx::scramble(U"ABC");
  GlyphInfo glyph;
  glyph.index = 3;
  glyph.textIndex = 3;
  const auto at = [&](float t) {
    Rng rng(90210);  // one glyph's stream, replayed
    return churn(glyph, t, rng).codepoint;
  };
  EXPECT_EQ(at(0.1f), at(0.1f)) << "the same moment gave two characters";
  EXPECT_EQ(at(1.0f), (char32_t)0) << "the glyph never resolved";
  bool churned = false, inCharset = true;
  for (int step = 0; step < 40; ++step) {
    const char32_t point = at((float)step / 40.0f);
    if (point == 0) continue;
    churned = true;
    if (point != U'A' && point != U'B' && point != U'C') inCharset = false;
  }
  EXPECT_TRUE(churned) << "nothing was substituted at any moment";
  EXPECT_TRUE(inCharset) << "the churn left the charset it was given";

  // A value like any other preset: comparable by its parameters, and the
  // charset is one of them.
  EXPECT_TRUE(fx::scramble(U"ABC") == fx::scramble(U"ABC"));
  EXPECT_FALSE(fx::scramble(U"ABC") == fx::scramble(U"ABD"));
  EXPECT_FALSE(fx::scramble(U"ABC") == fx::scramble(U"ABC", 7));
}

TEST(ComposeTextFx, OnlyAnEqualAdvanceSubstitutionIsDrawn) {
  // A substitution draws at the ORIGINAL glyph's pen position, so it is
  // sound exactly when the advances agree. Refusing is not cosmetic: a
  // wider replacement would overlap the letter after it while every letter
  // after that stayed where shaping put it.
  sk_sp<SkTypeface> face = fonts().defaultTypeface();
  if (!face) GTEST_SKIP() << "no default face on this system";
  SkFont probe(face, 100.0f);
  probe.setLinearMetrics(true);
  probe.setHinting(SkFontHinting::kNone);
  const SkGlyphID base = probe.unicharToGlyph('X');
  if (!base) GTEST_SKIP() << "the default face cannot draw X";
  const float baseWidth = probe.getWidth(base);
  char32_t twin = 0, proportional = 0;
  for (char32_t point = U'!'; point <= U'~'; ++point) {
    const SkGlyphID glyph = probe.unicharToGlyph((SkUnichar)point);
    if (!glyph || glyph == base) continue;
    const float width = probe.getWidth(glyph);
    if (!twin && std::abs(width - baseWidth) <= 0.05f) twin = point;
    if (!proportional && std::abs(width - baseWidth) > 8.0f)
      proportional = point;
  }

  const auto render = [&](Host& host, std::string key, char32_t point) {
    GlyphMod mod;
    mod.codepoint = point;
    host.composer.render(box().padding(10).child(
        text(u8"XXX", whiteStyle(44))
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  Host rest(200, 120);
  render(rest, "none", 0);

  if (proportional) {
    Host refused(200, 120);
    render(refused, "wide", proportional);
    EXPECT_TRUE(identicalPixels(rest, refused, 200, 120))
        << "a proportional substitution was drawn instead of refused";
  }
  if (twin) {
    Host swapped(200, 120);
    render(swapped, "twin", twin);
    EXPECT_FALSE(identicalPixels(rest, swapped, 200, 120))
        << "an equal-advance substitution was refused, so the gate refuses "
           "everything and the case above proves nothing";
  }
  if (!twin && !proportional)
    GTEST_SKIP() << "the default face offered neither an equal-advance nor a "
                    "proportional partner for X";
}

TEST(ComposeTextFx, SkewAndNonUniformScaleTakeTheMatrixPath) {
  // Neither a shear nor an uneven scale is expressible as an RSXform, so
  // these glyphs route through a per-glyph matrix. The assertions are about
  // the SHAPE that appears, because the route is only worth having if it
  // paints what it promises.
  // Room on every side: a doubled glyph and a leaning one both grow past
  // the box, and a clipped measurement would compare two surface edges.
  const auto render = [&](Host& host, std::string key, GlyphMod mod) {
    host.composer.render(box().padding(60).child(
        text(u8"H", whiteStyle(60))
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  Host upright(200, 200);
  render(upright, "upright", GlyphMod{});
  const SkIRect rest = inkBounds(upright, 200, 200);
  ASSERT_FALSE(rest.isEmpty()) << "the resting glyph never painted";

  GlyphMod leaning;
  leaning.skewXDeg = 30;
  Host sheared(200, 200);
  render(sheared, "sheared", leaning);
  const SkIRect leant = inkBounds(sheared, 200, 200);
  EXPECT_GT(leant.width(), rest.width() + 4)
      << "a 30-degree shear did not widen the glyph's footprint";
  // …and it leans the way Element::skewX does: the top toward −x.
  const int band = std::max(leant.height() / 4, 2);
  EXPECT_LT(inkCentroidX(sheared, 200, leant.top(), leant.top() + band),
            inkCentroidX(sheared, 200, leant.bottom() - band, leant.bottom()))
      << "the shear leant the wrong way, or not at all";

  GlyphMod tall;
  tall.scaleY = 2.0f;
  Host stretched(200, 200);
  render(stretched, "tall", tall);
  const SkIRect grown = inkBounds(stretched, 200, 200);
  EXPECT_GT(grown.height(), rest.height() * 3 / 2)
      << "scaleY did not stretch the glyph";
  EXPECT_LE(grown.width(), rest.width() + 2)
      << "scaleY widened the glyph, so the scale was not non-uniform";
}

TEST(ComposeTextFx, AHeldTrackPaintsNothingBeforeItsBeatBesideAnOpenTrack) {
  // The end-to-end half of the hold: not "the effect returns alpha 0" but
  // "no ink reaches the surface" — and it holds against a SECOND track whose
  // own progress is long settled, because alpha multiplies and a glyph that
  // has not arrived has not arrived.
  choreograph::Output<float> progress{0.0f};
  GlyphMod lift;
  lift.dy = -3;
  const auto render = [&](Host& host, TextEffect decode) {
    host.composer.render(box().padding(20).child(
        text(u8"HOLD", whiteStyle(36))
            .key("k")
            .fx({.effect = std::move(decode),
                 .stagger = {.eachMs = 40, .durationMs = 200},
                 .progress = &progress})
            .fx({.effect = fixed("lift", lift)})));
    host.frame();
  };
  // The control first: unheld, the same tree at the same moment paints —
  // wrong letters, but it paints — so an empty surface below is the hold's
  // doing and not a scene that never drew.
  Host unheld(240, 140);
  render(unheld, fx::scramble(U"XYZ", 6));
  ASSERT_FALSE(inkBounds(unheld, 240, 140).isEmpty())
      << "the unheld decode drew nothing, so this test proves nothing";

  Host host(240, 140);
  render(host, fx::hold(fx::scramble(U"XYZ", 6)));
  EXPECT_TRUE(inkBounds(host, 240, 140).isEmpty())
      << "a decode drew before any of its beats had opened — either the "
         "hold let the effect through, or the second track's open beat "
         "overrode it";

  progress = 1.0f;
  host.frame();
  EXPECT_FALSE(inkBounds(host, 240, 140).isEmpty())
      << "the hold never released";
}

TEST(ComposeTextFx, SkewYShearsTheOtherAxisAndTakesTheMatrixPath) {
  // The Y counterpart, probed on the axis skewX leaves alone: an X shear
  // moves the top sideways, a Y shear pushes the right side DOWN. Reading
  // the same asymmetry on the same axis for both would pass for a `skewY`
  // that was quietly wired to `skewXDeg`.
  const auto render = [&](Host& host, std::string key, GlyphMod mod) {
    host.composer.render(box().padding(60).child(
        text(u8"H", whiteStyle(60))
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  Host upright(200, 200);
  render(upright, "upright", GlyphMod{});
  const SkIRect rest = inkBounds(upright, 200, 200);
  ASSERT_FALSE(rest.isEmpty()) << "the resting glyph never painted";

  GlyphMod leaning;
  leaning.skewYDeg = 30;
  Host sheared(200, 200);
  render(sheared, "shearedY", leaning);
  const SkIRect leant = inkBounds(sheared, 200, 200);
  EXPECT_GT(leant.height(), rest.height() + 4)
      << "a 30-degree Y shear did not deepen the glyph's footprint";
  EXPECT_LE(leant.width(), rest.width() + 2)
      << "the Y shear widened the glyph, which is what an X shear does";
  // …and it leans the way Element::skewY does: the right side toward +y.
  const int band = std::max(leant.width() / 4, 2);
  EXPECT_LT(inkCentroidY(sheared, 200, leant.left(), leant.left() + band),
            inkCentroidY(sheared, 200, leant.right() - band, leant.right()))
      << "the shear leant the wrong way, or not at all";
}

namespace {

/** Runs the fast-path/matrix-neighbour check with one line sheared on the
 *  axis @p lean names — the SAME assertion for each shear axis, so a routing
 *  condition that learns about one and not the other fails here. */
void expectFastPathLineUntouched(GlyphMod lean) {
  // The route is decided PER GLYPH. A glyph whose deviation is an RSXform
  // draws exactly as it would if no glyph in the node needed a matrix —
  // otherwise adding a shear to one line would silently re-rasterize every
  // other line through a different code path.
  sigil::weave::TextStyle style = whiteStyle(30);
  const auto tree = [&](bool shearSecondLine) {
    GlyphMod lift;
    lift.dy = -4;
    Element t = text(u8"AAAA BBBB", style)
                    .key("k")
                    .width(70)
                    .fx({.effect = fixed("lift", lift)});
    if (shearSecondLine)
      t.fx({.where = sel::line(1), .effect = fixed("lean", lean)});
    return box().padding(10).child(std::move(t));
  };
  Host plain(200, 200), mixed(200, 200);
  plain.composer.render(tree(false));
  plain.frame();
  mixed.composer.render(tree(true));
  mixed.frame();

  // The empty rows between the two lines: everything above them belongs to
  // the line no track sheared.
  SkBitmap reference;
  reference.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  plain.surface->readPixels(reference.pixmap(), 0, 0);
  const auto rowInked = [&](int y) {
    for (int x = 0; x < 200; ++x)
      if (reference.getColor(x, y) != SK_ColorBLACK) return true;
    return false;
  };
  int firstInked = -1, split = -1;
  for (int y = 0; y < 200; ++y) {
    if (rowInked(y)) {
      if (firstInked < 0) firstInked = y;
    } else if (firstInked >= 0) {
      split = y;
      break;
    }
  }
  ASSERT_GT(split, 0) << "the text did not wrap into two lines, so there is "
                         "no unsheared line to compare";

  SkBitmap sheared;
  sheared.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  mixed.surface->readPixels(sheared.pixmap(), 0, 0);
  constexpr size_t kRowBytes = 200 * sizeof(uint32_t);
  for (int y = 0; y < split; ++y)
    ASSERT_EQ(std::memcmp(reference.getAddr32(0, y), sheared.getAddr32(0, y),
                          kRowBytes),
              0)
        << "row " << y
        << " of the fast-path line changed when a NEIGHBOURING "
           "line took the matrix route";
  // The control: the sheared line really did change, so the rows above it
  // holding still is a fact about the routing and not about a track that
  // did nothing.
  bool below = false;
  for (int y = split; y < 200 && !below; ++y)
    below = std::memcmp(reference.getAddr32(0, y), sheared.getAddr32(0, y),
                        kRowBytes) != 0;
  EXPECT_TRUE(below) << "the shear track changed nothing anywhere";
}

}  // namespace

TEST(ComposeTextFx, AGlyphOnTheFastPathIsUntouchedByAMatrixNeighbour) {
  GlyphMod leanX;
  leanX.skewXDeg = 25;
  expectFastPathLineUntouched(leanX);
}

TEST(ComposeTextFx, AGlyphWithNoYShearKeepsTheFastPathBesideOneThatHasIt) {
  // The same assertion on the axis the routing condition learned LAST: a
  // glyph whose `skewYDeg` is 0 must stay on the shared transform array
  // however its neighbours lean.
  GlyphMod leanY;
  leanY.skewYDeg = 25;
  expectFastPathLineUntouched(leanY);
}

TEST(ComposeTextFx, ContinuousLiftsTheSnapAndStillSettles) {
  // Rotations are snapped to a 64-step table, so a two-degree lean rounds
  // to no lean at all. `continuous` is the opt-out, and it must actually
  // change what is drawn or it is a field that does nothing.
  const auto render = [](Host& host, bool continuous) {
    GlyphMod lean;
    lean.rotateDeg = 2.0f;
    Track track{.effect = fixed("lean2", lean)};
    track.continuous = continuous;
    host.composer.render(box().padding(20).child(
        text(u8"HH", whiteStyle(64)).key("k").fx(std::move(track))));
    host.frame();
  };
  Host snapped(200, 200), smooth(200, 200);
  render(snapped, false);
  render(smooth, true);
  EXPECT_FALSE(identicalPixels(snapped, smooth, 200, 200))
      << "continuous drew exactly what the snapped ladder did, so the "
         "opt-out is inert";

  // …and it is an opt-out of SNAPPING, not of caching: a continuous track
  // whose progress has stopped moving settles like any other.
  Host settling(200, 200);
  choreograph::Output<float> progress{0.0f};
  Track track{.effect = fx::rise(14), .progress = &progress};
  track.continuous = true;
  settling.composer.render(box().padding(20).child(
      text(u8"SETTLE", whiteStyle(24)).key("k").fx(std::move(track))));
  settling.frame();
  progress = 1.0f;
  for (int i = 0; i < 12; ++i) settling.frame(0.016);
  unsigned paints = 0, records = 0;
  for (int i = 0; i < 5; ++i) {
    settling.frame(0.016);
    paints += settling.composer.stats().nodesPainted;
    records += settling.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(paints, 0u) << "settled continuous text kept painting live";
  EXPECT_EQ(records, 0u) << "settled continuous text kept re-recording";
}

// ---- the schedule as a value: beats::Text, cues(), beatsOf ----------------

namespace {
/** The startMs of the beat covering unit @p unitIndex, or -1 where the
 *  track runs no beat there. */
float startOfUnit(const std::vector<Beat>& beats, uint32_t unitIndex) {
  for (const Beat& beat : beats)
    if (beat.unitIndex == unitIndex) return beat.startMs;
  return -1.0f;
}

/** WHERE THE LAYOUT ACTUALLY PUT EACH WORD, straight off the positioned
 *  runs — the ground truth a beat's rect is checked against, so the check
 *  cannot pass by both sides re-measuring the text the same wrong way. */
std::vector<SkRect> wordExtents(const sigil::weave::ParagraphLayout& layout,
                                size_t words, SkPoint origin) {
  std::vector<SkRect> out(words, SkRect::MakeEmpty());
  for (const sigil::weave::PositionedRun& run : layout.runs) {
    if (!run.shaped || run.wordIndex >= words) continue;
    SkRect extent = SkRect::MakeXYWH(run.origin.x() + origin.x(),
                                     run.origin.y() + origin.y() - 0.5f,
                                     run.shaped->advance, 1.0f);
    if (out[run.wordIndex].isEmpty())
      out[run.wordIndex] = extent;
    else
      out[run.wordIndex].join(extent);
  }
  return out;
}

/** A baseline that leaves the node's box entirely: a ring centred well to
 *  the right of it, as a comparable scheme so the node still prunes. */
struct BeatRing {
  SkPath path(SkSize) const {
    SkPathBuilder builder;
    builder.addCircle(200, 100, 60);
    return builder.detach();
  }
  bool operator==(const BeatRing&) const = default;
};
}  // namespace

TEST(ComposeTextFx, PartitioningTracksShareOneClockOnlyUnderBeatsText) {
  // THE FLAGSHIP DEFECT. Two tracks split one paragraph — a track on the
  // words' initials and a track on their bodies — and the initials track
  // additionally spares the first word. Under the default numbering each
  // track counts only the units ITS OWN selector resolved, so the two lists
  // are three long and four long and every word after the first has its
  // initial on one beat and its body on another. Under beats::Text both
  // count the paragraph's words, so word three is beat three in both.
  const auto beatsUnder = [](Beats numbering) {
    Host host(400, 120);
    const Stagger spec{.eachMs = 100,
                       .durationMs = 100,
                       .over = unit::Word,
                       .beatsOver = numbering};
    host.composer.render(box().padding(6).child(
        text(u8"AA BB CC DD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.where = sel::each(unit::Word).take(1) & sel::words(1, 4),
                 .effect = fx::rise(6),
                 .stagger = spec})
            .fx({.where = sel::each(unit::Word).drop(1),
                 .effect = fx::rise(6),
                 .stagger = spec})));
    host.frame();
    return std::pair(host.composer.beatsOf("p", 0),
                     host.composer.beatsOf("p", 1));
  };

  const auto [initialsText, bodiesText] = beatsUnder(beats::Text);
  ASSERT_EQ(initialsText.size(), 3u) << "three words carry a spared initial";
  ASSERT_EQ(bodiesText.size(), 4u) << "four words carry a body";
  // The paragraph numbers the beats, so the initial of word k and the body
  // of word k open together — for every word the two tracks share.
  for (uint32_t word = 1; word <= 3; ++word)
    EXPECT_FLOAT_EQ(startOfUnit(initialsText, word),
                    startOfUnit(bodiesText, word))
        << "word " << word << " arrives in two pieces under beats::Text";
  EXPECT_FLOAT_EQ(startOfUnit(bodiesText, 3), 300.0f);

  // …and the default really is the other thing, or the setting is inert.
  const auto [initialsSel, bodiesSel] = beatsUnder(beats::Selection);
  ASSERT_EQ(initialsSel.size(), 3u);
  ASSERT_EQ(bodiesSel.size(), 4u);
  // The initials track renumbered its three words from zero: its LAST beat
  // is word three's and opens at 200 ms, while word three's body opens at
  // 300 ms. That hundred milliseconds is the defect, stated.
  EXPECT_FLOAT_EQ(initialsSel.back().startMs, 200.0f);
  EXPECT_FLOAT_EQ(bodiesSel.back().startMs, 300.0f);
  EXPECT_NE(initialsSel.back().startMs, bodiesSel.back().startMs)
      << "the two numberings produced the same schedule, so beats::Text is "
         "not doing anything";
}

TEST(ComposeTextFx, ACueTableStartsUnitKAtItsOwnTime) {
  // Real caption timing is a table cut against a recording, not a spacing.
  // Unit k starts at table[k], exactly, and nothing about `eachMs` survives
  // beside it.
  const std::vector<float> table{0.0f, 340.0f, 720.0f, 1180.0f};
  Host host(400, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC DD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6),
               .stagger =
                   stagger(unit::Word,
                           cues(table, {.eachMs = 999, .durationMs = 180}))})));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), table.size());
  for (size_t k = 0; k < table.size(); ++k) {
    EXPECT_EQ(beats[k].unitIndex, (uint32_t)k);
    EXPECT_FLOAT_EQ(beats[k].startMs, table[k])
        << "unit " << k << " did not start at its own cue";
  }
  // cues() is a Stagger, so it compares like one — and a different table is
  // a different cascade, or a re-described track would prune onto the old
  // schedule and keep singing the previous line's timing.
  EXPECT_TRUE(cues(table) == cues(table));
  EXPECT_FALSE(cues(table) == cues({0.0f, 340.0f, 720.0f}));
  EXPECT_FALSE(cues(table) == Stagger{});
}

TEST(ComposeTextFx, AShortCueTablePilesItsTailAndWarnsOnce) {
  // A table that does not have one time per unit is a table cut against the
  // wrong text. The tail holds on the last cue — visible as a pile, rather
  // than times its author never wrote — and it says so once.
  ::testing::internal::CaptureStderr();
  Host host(400, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC DD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6),
               .stagger = stagger(
                   unit::Word, cues({0.0f, 200.0f}, {.durationMs = 100}))})));
  host.frame();
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("cue table"), std::string::npos) << log;
  EXPECT_NE(log.find("last time"), std::string::npos) << log;

  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), 4u);
  EXPECT_FLOAT_EQ(beats[0].startMs, 0.0f);
  EXPECT_FLOAT_EQ(beats[1].startMs, 200.0f);
  EXPECT_FLOAT_EQ(beats[2].startMs, 200.0f) << "the tail must hold, not run on";
  EXPECT_FLOAT_EQ(beats[3].startMs, 200.0f);

  // Once per shape: the same mismatch again is silent.
  ::testing::internal::CaptureStderr();
  host.frame();
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "the cue-table warning is not once per shape";
}

TEST(ComposeTextFx, BeatsOfReportsWhereTheGlyphsActuallyWentAndWhen) {
  // The read-back has to agree with the placement, not with a second
  // measurement: cross-check every beat's rect against the glyphs the
  // layout placed, over a paragraph that WRAPS and carries two sizes, where
  // a re-measured line would land in the wrong place twice over.
  Host host(240, 240);
  choreograph::Output<float> progress{0.5f};
  RichText copy = rich(whiteStyle(15));
  copy.add(u8"alpha bravo ").add(u8"charlie", whiteStyle(24)).add(u8" delta");
  host.composer.render(
      box().padding(10).child(text(copy).key("p").width(120).fx(
          {.effect = fx::rise(8),
           .stagger = stagger(unit::Word, {.eachMs = 100, .durationMs = 200}),
           .progress = &progress})));
  host.frame();

  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), 4u) << "one beat per word of the paragraph";
  const sigil::weave::ParagraphLayout* layout =
      host.composer.paragraphLayout("p");
  ASSERT_NE(layout, nullptr);
  const std::optional<SkRect> box_ = host.composer.bounds("p");
  ASSERT_TRUE(box_.has_value());

  const std::vector<SkRect> placed =
      wordExtents(*layout, beats.size(), {box_->left(), box_->top()});
  bool wrapped = false;
  for (size_t i = 1; i < beats.size(); ++i)
    if (beats[i].rect.centerY() > beats[i - 1].rect.centerY() + 4)
      wrapped = true;
  EXPECT_TRUE(wrapped) << "the paragraph never wrapped: nothing is proven";
  for (size_t i = 0; i < beats.size(); ++i) {
    ASSERT_FALSE(placed[i].isEmpty()) << "word " << i << " was never placed";
    EXPECT_NEAR(beats[i].rect.left(), placed[i].left(), 1.0f) << "word " << i;
    EXPECT_GE(beats[i].rect.right(), placed[i].right() - 2.0f)
        << "word " << i << "'s beat stops short of its own last letter";
    EXPECT_LE(beats[i].rect.top(), placed[i].centerY()) << "word " << i;
    EXPECT_GE(beats[i].rect.bottom(), placed[i].centerY()) << "word " << i;
  }
  EXPECT_GT(beats[2].rect.height(), beats[0].rect.height())
      << "the 24 px run's beat is no taller than the 15 px runs', so the "
         "rect is not reading the placement";

  // …and the LOCAL TIME is the number the glyphs are being handed. The
  // cascade spans 200 + 100·3 = 500 ms of virtual time, so at master 0.5
  // the front stands at 250 ms: word 0 is done, word 1 is three quarters
  // through its own 200 ms beat, word 2 has just started and word 3 has not
  // opened at all.
  EXPECT_FLOAT_EQ(beats[0].startMs, 0.0f);
  EXPECT_FLOAT_EQ(beats[3].startMs, 300.0f);
  EXPECT_FLOAT_EQ(beats[0].localT, 1.0f);
  EXPECT_FLOAT_EQ(beats[1].localT, 0.75f);
  EXPECT_FLOAT_EQ(beats[2].localT, 0.25f);
  EXPECT_FLOAT_EQ(beats[3].localT, 0.0f);
  EXPECT_FALSE(beats[0].active) << "a finished beat is not running";
  EXPECT_TRUE(beats[1].active);
  EXPECT_TRUE(beats[2].active);
  EXPECT_FALSE(beats[3].active) << "a beat that has not opened is not running";
}

TEST(ComposeTextFx, BeatsOfFollowsAPathBaseline) {
  // A path run's letters are not on the node's straight baseline at all,
  // and a mark placed from anything but the placement would sit in the
  // node's box while the type rides a circle beside it.
  Host host(300, 200);
  host.composer.render(box().child(
      text(u8"CIRCVMFERENTIA", whiteStyle(18))
          .key("ring")
          .width(100)
          .height(100)
          .onPath({.path = BeatRing{}})
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Cluster)})));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("ring", 0);
  ASSERT_GT(beats.size(), 8u);
  // The ring is centred at (200, 100) with radius 60, and the node's box is
  // the 100x100 at the origin: every beat must be OUT there, on the curve.
  SkRect union_ = SkRect::MakeEmpty();
  for (const Beat& beat : beats) {
    const float radius =
        std::hypot(beat.rect.centerX() - 200.0f, beat.rect.centerY() - 100.0f);
    EXPECT_NEAR(radius, 60.0f, 14.0f)
        << "a beat left the baseline it was supposed to be reading";
    union_.join(beat.rect);
  }
  EXPECT_GT(union_.right(), 200.0f)
      << "every beat stayed inside the node's box, so the rects are the "
         "straight baseline's rather than the curve's";
  // …and they go ROUND the ring rather than piling at its entry point: the
  // run sweeps a real arc, which only a rect read off the curve can show.
  const auto angleOf = [](const Beat& beat) {
    return std::atan2(beat.rect.centerY() - 100.0f,
                      beat.rect.centerX() - 200.0f);
  };
  float swept = 0;
  for (size_t i = 1; i < beats.size(); ++i) {
    float step = angleOf(beats[i]) - angleOf(beats[i - 1]);
    while (step > 3.14159265f) step -= 6.2831853f;
    while (step < -3.14159265f) step += 6.2831853f;
    swept += std::abs(step);
  }
  EXPECT_GT(swept, 1.0f) << "the beats never went round the ring";
}

TEST(ComposeTextFx, BeatsOfCompoundsANestedCascade) {
  // The case the karaoke study had to give up: a nested beat lasts exactly
  // as long as its inner ladder needs, so no author can restate the start
  // times. Word w's letter i opens at w·outerEach + i·innerEach, and the
  // read-back is the only place that is true without being retyped.
  Host host(400, 120);
  Stagger cascade = stagger(unit::Word, {.eachMs = 300});
  cascade.then(unit::Cluster, {.eachMs = 40, .durationMs = 100});
  host.composer.render(box().padding(6).child(
      text(u8"AB CD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6), .stagger = cascade})));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), 4u) << "one beat per letter, two letters per word";
  // Two beats per outer unit, each carrying its own compounded start.
  EXPECT_EQ(beats[0].unitIndex, 0u);
  EXPECT_EQ(beats[1].unitIndex, 0u);
  EXPECT_EQ(beats[2].unitIndex, 1u);
  EXPECT_EQ(beats[3].unitIndex, 1u);
  EXPECT_FLOAT_EQ(beats[0].startMs, 0.0f);
  EXPECT_FLOAT_EQ(beats[1].startMs, 40.0f);
  EXPECT_FLOAT_EQ(beats[2].startMs, 300.0f);
  EXPECT_FLOAT_EQ(beats[3].startMs, 340.0f);
}

TEST(ComposeTextFx, BeatsOfResolvesEmptyRatherThanGuessing) {
  Host host(200, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB", whiteStyle(16))
          .key("p")
          .fx({.effect = fx::rise(6), .stagger = stagger(unit::Word)})));
  host.frame();
  EXPECT_FALSE(host.composer.beatsOf("p", 0).empty());
  EXPECT_TRUE(host.composer.beatsOf("typo", 0).empty()) << "unknown key";
  EXPECT_TRUE(host.composer.beatsOf("p", 7).empty()) << "no such track";
}

TEST(ComposeTextFx, CascadeSpanMsIsWhatTheMasterProgressMapsOnto) {
  // The one number beatsOf does not carry: a beat says when it OPENS, and
  // nothing else says when the whole schedule is OVER. The span is the
  // last beat's start plus one beat's own duration — checked against the
  // beats themselves, so the two read-backs cannot drift apart — and the
  // declare-time form answers the same number from the counts alone.
  const Stagger spec = stagger(unit::Word, {.eachMs = 100, .durationMs = 200});
  Host host(400, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC DD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6), .stagger = spec})));
  host.frame();
  const float span = host.composer.cascadeSpanMs("p", 0);
  EXPECT_FLOAT_EQ(span, 500.0f) << "durationMs + eachMs·(N−1) over 4 words";
  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_FALSE(beats.empty());
  float latest = 0;
  for (const Beat& beat : beats) latest = std::max(latest, beat.startMs);
  EXPECT_FLOAT_EQ(span, latest + 200.0f)
      << "the span disagrees with the beats about when the last one closes";
  EXPECT_FLOAT_EQ(spec.spanMs(4), span)
      << "the declare-time form and the mounted one answer differently for "
         "the same cascade and count";

  // Amount-mode is count-independent past one unit — the amount IS the
  // spread — and one unit (or none) is a bare beat.
  const Stagger amount{.amountMs = 700, .durationMs = 420};
  EXPECT_FLOAT_EQ(amount.spanMs(2), 1120.0f);
  EXPECT_FLOAT_EQ(amount.spanMs(9), 1120.0f);
  EXPECT_FLOAT_EQ(amount.spanMs(1), 420.0f);
  EXPECT_FLOAT_EQ(amount.spanMs(0), 420.0f);
}

TEST(ComposeTextFx, CascadeSpanMsCompoundsNestingAndReadsTheTable) {
  // Nested: a beat lasts exactly as long as its inner ladder needs, so the
  // span compounds — the latest outer start, plus the latest inner start,
  // plus one INNER duration (the outer durationMs is ignored, as always
  // under then()).
  Stagger nested = stagger(unit::Word, {.eachMs = 300, .durationMs = 999});
  nested.then(unit::Cluster, {.eachMs = 40, .durationMs = 100});
  {
    Host host(400, 120);
    host.composer.render(box().padding(6).child(
        text(u8"AB CD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.effect = fx::rise(6), .stagger = nested})));
    host.frame();
    const float span = host.composer.cascadeSpanMs("p", 0);
    EXPECT_FLOAT_EQ(span, 440.0f) << "300·1 + 40·1 + 100";
    const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
    ASSERT_EQ(beats.size(), 4u);
    EXPECT_FLOAT_EQ(span, beats.back().startMs + 100.0f);
    EXPECT_FLOAT_EQ(nested.spanMs(2, 2), span)
        << "the declare-time form must take the inner count per beat";
  }

  // A cue table's extent is the table's own: the LATEST time any unit
  // reads plus the duration — a max, not the final entry, because a table
  // is not required to ascend.
  const std::vector<float> table{0.0f, 340.0f, 720.0f, 1180.0f};
  const Stagger cued = stagger(unit::Word, cues(table, {.durationMs = 180}));
  {
    Host host(400, 120);
    host.composer.render(box().padding(6).child(
        text(u8"AA BB CC DD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.effect = fx::rise(6), .stagger = cued})));
    host.frame();
    const float span = host.composer.cascadeSpanMs("p", 0);
    EXPECT_FLOAT_EQ(span, 1360.0f) << "the table's last time plus one beat";
    EXPECT_FLOAT_EQ(cued.spanMs(4), span);
  }
  EXPECT_FLOAT_EQ(
      stagger(unit::Word, cues({0.0f, 900.0f, 300.0f}, {.durationMs = 100}))
          .spanMs(3),
      1000.0f)
      << "an out-of-order table spans to its LATEST time, not its last entry";
}

TEST(ComposeTextFx, CascadeSpanMsResolvesZeroRatherThanGuessing) {
  Host host(200, 120);
  host.composer.render(box().padding(6).key("b").child(
      text(u8"AA BB", whiteStyle(16))
          .key("p")
          .fx({.effect = fx::rise(6), .stagger = stagger(unit::Word)})));
  host.frame();
  EXPECT_GT(host.composer.cascadeSpanMs("p", 0), 0.0f);
  EXPECT_FLOAT_EQ(host.composer.cascadeSpanMs("typo", 0), 0.0f)
      << "unknown key";
  EXPECT_FLOAT_EQ(host.composer.cascadeSpanMs("p", 7), 0.0f) << "no such track";
  EXPECT_FLOAT_EQ(host.composer.cascadeSpanMs("b", 0), 0.0f)
      << "a node that is not text";
}

TEST(ComposeLayouts, BaselineGridRendersInsideStackedAbsoluteColumn) {
  // Text inside a BaselineGrid, nested in an absolute column, inside a
  // stack(). A custom layout scheme writes back into Yoga out of band, and
  // an absolute ancestor changes how its subtree is sized, so this is the
  // combination most likely to leave the text laid out at zero size and
  // therefore invisible.
  Host host;
  host.composer.render(stack().child(
      box()
          .column()
          .absolute()
          .inset(10, 10, 10, 10)
          .child(layout(layouts::BaselineGrid{.rhythm = 24})
                     .width(pct(100))
                     .child(text(u8"probe", whiteStyle(28)).key("p")))));
  host.frame();
  auto b = host.composer.bounds("p");
  ASSERT_TRUE(b.has_value());
  EXPECT_GT(b->width(), 5.0f) << "placed rect " << b->left() << "," << b->top()
                              << " " << b->width() << "x" << b->height();
  EXPECT_GT(b->height(), 5.0f);
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())))
      << "placed rect " << b->left() << "," << b->top() << " " << b->width()
      << "x" << b->height();
}

TEST(ComposeLayouts, ModularGridSpansAndAutoFlow) {
  // 4×4 modules, gutter 8, container 200×200 → module 44×44. Child 0 spans
  // 2×1 from (0,0); child 1 spans 1×3 from (3,0); children 2..3 auto-flow.
  Host host;
  layouts::ModularGrid grid;
  grid.columns = 4;
  grid.rows = 4;
  grid.gutter = 8;
  grid.spans = {{0, 0, 2, 1}, {3, 0, 1, 3}};
  host.composer.render(box().child(layout(grid)
                                       .width(pct(100))
                                       .grow(1)
                                       .child(box().key("a").fill(red()))
                                       .child(box().key("b").fill(blue()))
                                       .child(box().key("c").fill(green()))
                                       .child(box().key("d").fill(red()))));
  host.frame();
  auto a = host.composer.bounds("a");
  auto b = host.composer.bounds("b");
  auto c = host.composer.bounds("c");
  auto d = host.composer.bounds("d");
  ASSERT_TRUE(a && b && c && d);
  EXPECT_NEAR(a->width(), 44 * 2 + 8, 0.01f);  // 2-module span + gutter
  EXPECT_NEAR(a->left(), 0, 0.01f);
  EXPECT_NEAR(b->left(), (44 + 8) * 3, 0.01f);   // 4th column
  EXPECT_NEAR(b->height(), 44 * 3 + 16, 0.01f);  // 3 rows + 2 gutters
  EXPECT_NEAR(c->left(), 0, 0.01f);  // auto-flow starts at (0,0)… of the flow
  EXPECT_NEAR(c->width(), 44, 0.01f);
  EXPECT_NEAR(d->left(), 44 + 8, 0.01f);  // next module across
}

TEST(ComposeLayouts, BaselineGridSnapsBottomsAndBaselines) {
  // Non-text children anchor by BOTTOM: heights 15 & 27 on rhythm 20 land
  // their bottoms on grid lines 20 and 60 (flow 20+27=47 rounds up).
  Host host;
  host.composer.render(box().child(
      layout(layouts::BaselineGrid{.rhythm = 20})
          .width(pct(100))
          .grow(1)
          .child(box().key("a").width(40).height(15).fill(red()))
          .child(box().key("b").width(40).height(27).fill(blue()))));
  host.frame();
  auto a = host.composer.bounds("a");
  auto b = host.composer.bounds("b");
  ASSERT_TRUE(a && b);
  EXPECT_NEAR(a->bottom(), 20.0f, 0.01f);
  EXPECT_NEAR(b->bottom(), 60.0f, 0.01f);

  // A text child anchors by its FIRST BASELINE: with the baseline on the
  // 200 grid line, (200 - top) equals the baseline offset — strictly LESS
  // than the child's height (bottom-anchoring would make them equal).
  // Font-metric independent.
  host.composer.render(
      box().child(layout(layouts::BaselineGrid{.rhythm = 200})
                      .width(pct(100))
                      .grow(1)
                      .child(text(u8"Xylograph", styleAt(40)).key("t"))));
  host.frame();
  auto t = host.composer.bounds("t");
  ASSERT_TRUE(t.has_value());
  EXPECT_GT(200.0f - t->top(), 10.0f);               // sane baseline
  EXPECT_LT(200.0f - t->top(), t->height() - 0.5f);  // baseline, not bottom
}

TEST(ComposeLayouts, ScatterIsDeterministicAndContained) {
  auto centers = [&](uint32_t seed) {
    Host host;
    std::vector<Element> bits;
    for (int i = 0; i < 9; ++i)
      bits.push_back(
          box().width(12).height(12).fill(blue()).key("s" + std::to_string(i)));
    host.composer.render(box().child(layout(layouts::Scatter{.seed = seed})
                                         .width(200)
                                         .height(200)
                                         .children(bits)));
    host.frame();
    std::vector<SkPoint> out;
    for (int i = 0; i < 9; ++i) {
      auto r = host.composer.bounds(("s" + std::to_string(i)).c_str());
      out.push_back({r->centerX(), r->centerY()});
      EXPECT_GE(r->left(), -0.01f);
      EXPECT_GE(r->top(), -0.01f);
      EXPECT_LE(r->right(), 200.01f);
      EXPECT_LE(r->bottom(), 200.01f);
    }
    return out;
  };
  auto a1 = centers(5), a2 = centers(5), b = centers(6);
  EXPECT_EQ(a1, a2);  // same seed → same scatter
  EXPECT_NE(a1, b);   // new seed → new chaos
}

// ---------------------------------------------------------------------------
// Tile maps: atlas regions + chunked cache invalidation.

namespace {

/** 4-tile atlas, 8px cells: [red | green] / [blue | yellow]. */
std::shared_ptr<sigil::image::ImageAsset> fourTileAtlas() {
  SkBitmap src;
  src.allocN32Pixels(16, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 8, 8));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(8, 0, 8, 8));
  src.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 8, 8, 8));
  src.erase(SK_ColorYELLOW, SkIRect::MakeXYWH(8, 8, 8, 8));
  SkDynamicMemoryWStream stream;
  SkPngEncoder::Encode(&stream, src.pixmap(), {});
  return std::make_shared<sigil::image::ImageAsset>(
      *sigil::image::ImageAsset::decode(stream.detachAsData()));
}

struct ChunkProps {
  std::vector<int> tiles;  // 4x4 tile ids
  int chunkX = 0, chunkY = 0;
  bool operator==(const ChunkProps&) const = default;
};

constexpr float kTilePx = 12.0f;

Element tileChunk(const ChunkProps& p) {
  static auto atlas = fourTileAtlas();
  auto chunk = box().width(4 * kTilePx).height(4 * kTilePx);
  for (int i = 0; i < (int)p.tiles.size(); ++i) {
    const int id = p.tiles[(size_t)i];
    const float sx = (float)(id % 2) * 8, sy = (float)(id / 2) * 8;
    chunk.child(
        image(atlas)
            .region(SkRect::MakeXYWH(sx, sy, 8, 8))
            .absolute()
            .inset((float)(i % 4) * kTilePx, (float)(i / 4) * kTilePx, 0, 0)
            .width(kTilePx)
            .height(kTilePx));
  }
  return chunk;
}

}  // namespace

TEST(ComposeTiling, OnlyTouchedChunkRerecords) {
  Host host;
  // 2x2 chunks of 4x4 tiles; a checker-ish rule fills the ids.
  std::vector<ChunkProps> chunks(4);
  for (int c = 0; c < 4; ++c) {
    chunks[(size_t)c].chunkX = c % 2;
    chunks[(size_t)c].chunkY = c / 2;
    for (int i = 0; i < 16; ++i) chunks[(size_t)c].tiles.push_back((i + c) % 4);
  }
  auto maze = [&] {
    auto grid = box().row().wrapLines().width(2 * 4 * kTilePx);
    for (int c = 0; c < 4; ++c)
      grid.child(
          memo(chunks[(size_t)c], tileChunk).key("chunk" + std::to_string(c)));
    return box().child(grid);
  };

  host.composer.render(maze());
  host.frame();
  const size_t coldRecords = host.composer.stats().picturesRecorded;
  EXPECT_GE(coldRecords, 4u);  // every chunk baked (plus ancestors)

  // Pixel sanity: chunk 0 tile 0 is id 0 (red); chunk 1 tile 0 is id 1
  // (green) at x = 48.
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);

  host.composer.render(maze());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);  // all memo-warm

  chunks[0].tiles[0] = 3;  // mutate ONE tile in ONE chunk
  host.composer.render(maze());
  host.frame();
  // Only chunk 0 and its ancestor chain re-record; the other three
  // chunks' pictures replay untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 3u);
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorYELLOW);  // the mutated tile
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);  // neighbors intact
}

TEST(ComposeReconcile, StructuralPruneNeedsNoMemo) {
  // memo() is an optimisation for expensive DESCRIBES, not the thing that
  // makes pruning work. A subtree whose new description equals its old one
  // is skipped wholesale either way, so plain boxes, text and images built
  // from value-comparable props re-render for free.
  Host host;
  auto tree = [] {
    return box()
        .row()
        .gap(8)
        .padding(12)
        .child(box().width(40).height(40).corners({6}).fill(red()))
        .child(text(u8"static", styleAt(18)).key("t"))
        .child(box().grow(1).fill(blue()).opacity(0.9f));
  };
  host.composer.render(tree());
  host.frame();

  host.composer.render(tree());  // brand-new Elements, identical values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());  // hosts may skip the redraw
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

// ---------------------------------------------------------------------------
// Mount entrances, trim wrap, per-side insets, overflow-safe recording,
// stroke align, measure(), presets, marquee.

// ---------------------------------------------------------------------------
// The authoring grammar: animate(from(a).to(b)) / animate(through({…})).
// What is pinned is the VALUE each argument shape builds, because that value
// is the only thing the engine ever sees — the argument spellings are pure
// sugar over it.

TEST(ComposeMotion, EachArgumentShapeBuildsItsOwnTransitioned) {
  const Transition spec{200ms, &choreograph::easeNone, 40ms};

  const Transitioned<float> ramp = animate(to(1.0f), spec);
  EXPECT_EQ(ramp.value, 1.0f);
  EXPECT_FALSE(ramp.from.has_value()) << "to() alone is not an entrance";
  EXPECT_TRUE(ramp.waypoints.empty());
  EXPECT_EQ(ramp.spec.duration, 200ms);
  EXPECT_EQ(ramp.spec.delay, 40ms);

  const Transitioned<float> entrance = animate(from(0.0f).to(1.0f), spec);
  EXPECT_EQ(entrance.value, 1.0f);
  ASSERT_TRUE(entrance.from.has_value());
  EXPECT_EQ(*entrance.from, 0.0f);
  EXPECT_TRUE(entrance.waypoints.empty());
  EXPECT_EQ(entrance.spec.duration, 200ms);
  EXPECT_EQ(entrance.spec.delay, 40ms);
  EXPECT_FLOAT_EQ(entrance.spec.easing()(0.25f), 0.25f);

  const std::vector<std::pair<std::chrono::milliseconds, float>> path{
      {0ms, 40.0f}, {200ms, -20.0f}, {400ms, 0.0f}};
  const Transitioned<float> phrasedPath =
      animate(through(path), &choreograph::easeNone);
  EXPECT_EQ(phrasedPath.value, 0.0f);
  ASSERT_TRUE(phrasedPath.from.has_value());
  EXPECT_EQ(*phrasedPath.from, 40.0f);
  EXPECT_EQ(phrasedPath.waypoints, path);
  EXPECT_EQ(phrasedPath.spec.duration, 400ms);
  // The ease is the one field the waypoint overload writes itself —
  // dropping it would default to easeOutQuad silently.
  EXPECT_FLOAT_EQ(phrasedPath.spec.easing()(0.25f), 0.25f);
}

// A guard, not a reproduction: an indeterminate value can happen to hold the
// number this test wants, so a passing run is weaker evidence than usual.
// Both spellings are checked, and the pixel arm at the bottom is what makes
// the claim about behaviour rather than about one struct field.
TEST(ComposeMotion, AnEmptyKeyframePathIsDETERMINATE) {
  // An empty waypoint list is a degenerate ask that must still produce a
  // definite answer. `Transitioned<T>::value` has to be value-initialized:
  // default-initialized, `animate(through({}))` would leave a float property
  // reading whatever was on the stack — once, silently, with no failure to
  // observe anywhere. Zero is the answer.
  const Transitioned<float> empty = animate(through({}));
  EXPECT_EQ(empty.value, 0.0f);
  EXPECT_FALSE(empty.from.has_value());
  EXPECT_TRUE(empty.waypoints.empty());

  const std::vector<std::pair<std::chrono::milliseconds, float>> none;
  const Transitioned<float> phrased = animate(through(none));
  EXPECT_EQ(phrased.value, 0.0f);

  // And through the property slot: the node paints AT that determinate
  // value rather than at a number nobody chose.
  Host host;
  host.composer.render(box().child(
      box().width(80).height(80).fill(red()).opacity(animate(through({})))));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // opacity 0, not garbage
}

TEST(ComposeMotion, AnimateThroughDeducesAFloatPath) {
  // A nested braced list is a non-deduced context, so the generic form
  // normally has to be told `<float>`. This overload exists so it does not.
  // Compiling with no explicit template argument IS the test — the
  // assertions below only confirm it deduced the right thing.
  const Transitioned<float> t = animate(through({{0ms, 0.0f}, {100ms, 1.0f}}));
  ASSERT_EQ(t.waypoints.size(), 2u);
  EXPECT_EQ(t.waypoints.front().second, 0.0f);
  EXPECT_EQ(t.waypoints.back().second, 1.0f);
  ASSERT_TRUE(t.from.has_value());
  EXPECT_EQ(*t.from, 0.0f);
  EXPECT_EQ(t.value, 1.0f);
  EXPECT_EQ(t.spec.duration, 100ms);
}

TEST(ComposeMotion, AnimatePlaysEntranceOnMount) {
  Host host;
  auto tree = [] {
    return box().child(box().width(80).height(80).fill(red()).opacity(
        animate(from(0.0f).to(1.0f), {200ms, &choreograph::easeNone})));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);  // enters invisible
  host.frame(0.1);                               // half the linear ramp
  const SkColor mid = host.pixel(40, 40);
  EXPECT_GT(SkColorGetR(mid), 90u);
  EXPECT_LT(SkColorGetR(mid), 165u);
  EXPECT_EQ(SkColorGetG(mid), 0u);
  host.frame(0.2);  // settled
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);

  host.composer.render(tree());  // identical re-describe prunes clean
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}

TEST(ComposeMotion, AnimateColorSweepsOnMount) {
  Host host;
  host.composer.render(box().child(box().width(80).height(80).fill(
      Animatable<Fill>(animate(from(Fill::color({1, 1, 1, 1})).to(red()),
                               {200ms, &choreograph::easeNone})))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE);  // the declared "from"
  host.frame(0.3);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}

TEST(ComposeMask, WrapWindowCrossesTheSeam) {
  // A wrap window crossing the cycle seam must paint exactly the union of
  // its two clamped pieces — direction-agnostic pixel containment.
  auto strokedBox = [](Spans where) {
    return box().child(box()
                           .absolute()
                           .inset(50, 50, 50, 50)
                           .mask(by::spans(std::move(where)))
                           .foreground(util::stroke(6, green())));
  };
  Host wrap, pieceA, pieceB;
  wrap.composer.render(strokedBox(spans::wrap(0.9f, 1.15f)));
  pieceA.composer.render(strokedBox(spans::range(0.9f, 1.0f)));
  pieceB.composer.render(strokedBox(spans::range(0.0f, 0.15f)));
  wrap.frame();
  pieceA.frame();
  pieceB.frame();
  int unionCount = 0, wrapCount = 0, missing = 0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2) {
      const bool inUnion = pieceA.pixel(x, y) == SK_ColorGREEN ||
                           pieceB.pixel(x, y) == SK_ColorGREEN;
      const bool inWrap = wrap.pixel(x, y) == SK_ColorGREEN;
      unionCount += inUnion;
      wrapCount += inWrap;
      missing += inUnion && !inWrap;
    }
  EXPECT_GT(unionCount, 50);  // the pieces really painted
  EXPECT_LE(missing, 4);      // wrap covers the union (AA slack)
  EXPECT_NEAR(wrapCount, unionCount, unionCount / 5.0 + 8);
}

TEST(ComposeMask, WrapOffsetBindingMarchesTheWindow) {
  Host host;
  choreograph::Output<float> phase{0.0f};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .mask(by::spans(spans::wrap(0.0f, 0.25f).offset(&phase)))
                      .foreground(util::stroke(6, green()))));
  host.frame();
  std::vector<SkIPoint> lit0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2)
      if (host.pixel(x, y) == SK_ColorGREEN) lit0.push_back({x, y});
  ASSERT_GT(lit0.size(), 10u);

  phase = 0.5f;  // march half the cycle — no render()
  host.frame();
  int still = 0;
  for (const SkIPoint& p : lit0)
    still += host.pixel(p.x(), p.y()) == SK_ColorGREEN;
  // The window moved to the far side: (almost) none of the old pixels stay.
  EXPECT_LT((float)still, 0.2f * (float)lit0.size());
}

namespace {

/** A red 40x40 rect at x=150 recorded into a picture whose cull rect is
 *  the 100x100 box it escapes; replayed onto a 300x200 white surface.
 *  Returns the pixel the escaped rect would paint. */
sk_sp<SkPicture> escapingPicture(const SkRect& cull, SkBBHFactory* bbh) {
  SkPictureRecorder rec;
  SkCanvas* c = rec.beginRecording(cull, bbh);
  SkPaint p;
  p.setColor(SK_ColorRED);
  c->drawRect(SkRect::MakeXYWH(150, 10, 40, 40), p);
  return rec.finishRecordingAsPicture();
}

SkColor replayPixel(const sk_sp<SkPicture>& pic, int x, int y) {
  auto surf = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(300, 200));
  surf->getCanvas()->clear(SK_ColorWHITE);
  surf->getCanvas()->drawPicture(pic);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surf->readPixels(bm.pixmap(), x, y);
  return bm.getColor(0, 0);
}

}  // namespace

/** What a picture's cull rect actually does, established by experiment
 *  rather than assumed — because the intuitive reading ("ops outside the
 *  cull rect are dropped") is wrong, and `ownPaintBounds` is sized on the
 *  basis of the real behaviour.
 *
 *  An op outside the cull rect is NOT rejected at record time and NOT culled
 *  at plain playback. The cull rect only bites through a bounding-box
 *  hierarchy. What does clip in the compose paint path is saveLayer bounds
 *  and bake surfaces. Every arm below is asserted against its opposite, so
 *  the test cannot pass by agreeing with itself. */
TEST(ComposeCullRect, PictureCullDoesNotCullWithoutABbh) {
  // (1) recorded: the op survives RECORDING despite sitting wholly
  // outside the cull rect, and the picture keeps the rect it was given.
  sk_sp<SkPicture> pic = escapingPicture(SkRect::MakeWH(100, 100), nullptr);
  EXPECT_EQ(pic->approximateOpCount(true), 1);
  EXPECT_EQ(pic->cullRect(), SkRect::MakeWH(100, 100));
  // (2) and it survives PLAYBACK: the pixels land outside the cull rect.
  EXPECT_EQ(replayPixel(pic, 170, 20), SK_ColorRED);

  // (3) an EMPTY cull rect does not reject either — the zero-size-node
  // guard in Paint.cpp is justified by promotion, not by op rejection.
  sk_sp<SkPicture> empty = escapingPicture(SkRect::MakeWH(0, 0), nullptr);
  EXPECT_EQ(empty->approximateOpCount(true), 1);
  EXPECT_EQ(replayPixel(empty, 170, 20), SK_ColorRED);

  // (4) nor is the whole picture quick-rejected when its cull rect misses
  // the device entirely: an op inside the device still paints.
  {
    SkPictureRecorder rec;
    SkPaint p;
    p.setColor(SK_ColorRED);
    rec.beginRecording(SkRect::MakeXYWH(1000, 1000, 100, 100))
        ->drawRect(SkRect::MakeXYWH(20, 20, 40, 40), p);
    EXPECT_EQ(replayPixel(rec.finishRecordingAsPicture(), 30, 30), SK_ColorRED);
  }

  // (5) WITH a bbh the cull rect finally bites — still recorded, dropped
  // at playback, because the RTree clips op bounds to the cull rect. This
  // is the arm that makes (2) meaningful: same input, opposite outcome.
  SkRTreeFactory bbh;
  sk_sp<SkPicture> tree = escapingPicture(SkRect::MakeWH(100, 100), &bbh);
  EXPECT_EQ(tree->approximateOpCount(true), 1);
  EXPECT_EQ(replayPixel(tree, 170, 20), SK_ColorWHITE);

  // (6) saveLayer bounds, by contrast, are a genuine clip — this is the
  // mechanism recordBounds' child union is actually defending against.
  {
    auto surf = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(300, 200));
    surf->getCanvas()->clear(SK_ColorWHITE);
    const SkRect box = SkRect::MakeWH(100, 100);
    SkPaint layer;
    layer.setAlphaf(0.5f);
    SkPaint p;
    p.setColor(SK_ColorRED);
    surf->getCanvas()->saveLayer(&box, &layer);
    surf->getCanvas()->drawRect(SkRect::MakeXYWH(150, 10, 40, 40), p);
    surf->getCanvas()->restore();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surf->readPixels(bm.pixmap(), 170, 20);
    EXPECT_EQ(bm.getColor(0, 0), SK_ColorWHITE);
  }
}

TEST(ComposeCache, OverflowingChildSurvivesPictureCaching) {
  // A child translated beyond its parent's box must not be quick-rejected
  // by the parent's recording cull (the recordBounds fix).
  Host host(300, 200);
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).child(
          box().width(40).height(40).fill(red()).translateX(150.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 20), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);  // fully outside parent's box
  host.frame();                                 // cached replay path
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);
}

TEST(ComposeCache, OverflowingChildSurvivesGroupOpacityLayer) {
  // The clip that actually bites: a group opacity opens a saveLayer
  // BOUNDED by recordBounds, and saveLayer bounds are a real clip. Drop
  // the child union from recordBounds and the overflowing child is gone.
  Host host(300, 200);
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).opacity(0.5f).child(
          box().width(40).height(40).fill(red()).translateX(150.0f))));
  host.frame();
  EXPECT_GT(SkColorGetB(host.pixel(50, 20)), 100u);   // sanity: the parent
  EXPECT_GT(SkColorGetR(host.pixel(170, 20)), 100u);  // the escaped child
}

TEST(ComposeCache, OverflowingChildSurvivesTextureBake) {
  // The second real clip: Cache::Texture bakes into a surface sized from
  // recordBounds mapped to device, so anything the rect misses is
  // truncated by the surface itself — no picture cull involved.
  Host host(300, 200);
  host.composer.render(box().child(
      box()
          .width(100)
          .height(100)
          .fill(blue())
          .cache(Cache::Texture)
          .child(box().width(40).height(40).fill(red()).translateX(150.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 20), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);
  host.frame();  // cached blit path
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);
}

namespace {
/** A baseline that deliberately leaves the node's box: a ring centred well
 *  to the right of it. A comparable scheme rather than a raw callable, so
 *  the node can still prune and the cache under test is really reached. */
struct RingBesideTheBox {
  SkPath path(SkSize) const {
    SkPathBuilder builder;
    builder.addCircle(200, 100, 60);
    return builder.detach();
  }
  bool operator==(const RingBesideTheBox&) const = default;
};

/** How many pixels of `host` right of `fromX` carry ink. */
int litPixelsRightOf(Host& host, int fromX, int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  int lit = 0;
  for (int y = 0; y < height; ++y)
    for (int x = fromX; x < width; ++x)
      if (SkColorGetR(*bitmap.getAddr32(x, y)) > 40) ++lit;
  return lit;
}
}  // namespace

TEST(ComposeCache, TextOnAPathOutsideItsBoxSurvivesTheCull) {
  // A TextPath baseline resolves against the node's box but is not bounded
  // by it: this ring sits entirely beside the box, and `offset` would ride
  // the type further off it again. If the paint bounds stop at the box, the
  // bake surface is sized to the box and every glyph is truncated with no
  // diagnostic — the failure `bleed()` and `reach()` exist to prevent.
  // Cache::None is the ground truth: it re-paints straight to the canvas
  // with no surface to truncate against.
  const auto plate = [](Cache cache) {
    auto host = std::make_unique<Host>(300, 200);
    host->composer.render(box().child(text(u8"CIRCVMFERENTIA", whiteStyle(18))
                                          .width(100)
                                          .height(100)
                                          .onPath({.path = RingBesideTheBox{}})
                                          .cache(cache)
                                          .key("ring")));
    for (int i = 0; i < 4; ++i) host->frame(1.0 / 60.0);
    return host;
  };
  std::unique_ptr<Host> truth = plate(Cache::None);
  const int expected = litPixelsRightOf(*truth, 110, 300, 200);
  ASSERT_GT(expected, 40) << "the run never left the box: nothing is proven";

  std::unique_ptr<Host> baked = plate(Cache::Texture);
  EXPECT_GT(litPixelsRightOf(*baked, 110, 300, 200), expected * 9 / 10)
      << "the baked plate lost the glyphs the baseline put outside the box";
}

TEST(ComposeLayouts, RadialRadiusAtGivesEachChildItsOwnRing) {
  // `radiusAt` gives each child its own ring radius, so one Radial can draw
  // nested orbits rather than a single circle. The list may be shorter than
  // the child count: the tail falls back to `radiusFraction`, which is what
  // the second half of this case checks.
  Host host;
  std::vector<Element> dots;
  for (int i = 0; i < 4; ++i)
    dots.push_back(
        box().width(10).height(10).fill(red()).key("r" + std::to_string(i)));
  host.composer.render(box().child(
      layout(layouts::Radial{.radiusFraction = 0.8f, .radiusAt = {0.4f, 0.8f}})
          .width(200)
          .height(200)
          .children(dots)));
  host.frame();
  auto center = [&](const char* k) {
    auto r = host.composer.bounds(k);
    return SkPoint{r->centerX(), r->centerY()};
  };
  EXPECT_NEAR(center("r0").y(), 60, 1);   // top, INNER ring (0.4 → r=40)
  EXPECT_NEAR(center("r1").x(), 180, 1);  // right, outer (0.8 → r=80)
  EXPECT_NEAR(center("r2").y(), 180, 1);  // bottom, fallback 0.8
  EXPECT_NEAR(center("r3").x(), 20, 1);   // left, fallback 0.8
}

// ---------------------------------------------------------------------------
// MIXED TEXT — rich() spans, selector restyling, and the layout-option
// setters. The three together are the authoring surface for text that is not
// all set the same way, with no markup language anywhere in it.

namespace {
/** How many pixels of exactly this colour a region holds. */
int countColor(Host& host, SkIRect region, SkColor color) {
  int hits = 0;
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) == color) ++hits;
  return hits;
}
/** The shaped-word identity behind every placed run. The shape cache is
 *  content-addressed, so an unchanged pointer is proof that nothing about
 *  that word's shaping inputs moved. */
std::vector<const void*> runShapes(Host& host, const char* key) {
  std::vector<const void*> out;
  const auto* layout = host.composer.paragraphLayout(key);
  if (!layout) return out;
  for (const sigil::weave::PositionedRun& run : layout->runs)
    out.push_back(run.shaped.get());
  return out;
}
std::vector<SkPoint> runOrigins(Host& host, const char* key) {
  std::vector<SkPoint> out;
  const auto* layout = host.composer.paragraphLayout(key);
  if (!layout) return out;
  for (const sigil::weave::PositionedRun& run : layout->runs)
    out.push_back(run.origin);
  return out;
}
sigil::weave::TextStyle coloredStyle(float size, SkColor color) {
  sigil::weave::TextStyle s = styleAt(size);
  s.paint.foreground.setColor(color);
  return s;
}
}  // namespace

TEST(TextRich, MixedRunsPaintTheirOwnStyles) {
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(36, SK_ColorWHITE);
  const sigil::weave::TextStyle accent = coloredStyle(36, SK_ColorRED);
  host.composer.render(box().padding(10).child(
      text(rich(base).add(u8"AAA ").add(u8"BBB", accent)).key("t")));
  host.frame();
  const SkIRect band = SkIRect::MakeXYWH(0, 0, 400, 80);
  EXPECT_GT(countColor(host, band, SK_ColorWHITE), 20) << "the base run";
  EXPECT_GT(countColor(host, band, SK_ColorRED), 20) << "the accented run";
}

TEST(TextRich, AnIdenticalValuePrunesWhereAFreshPointerCannot) {
  // The whole reason rich() is a value: a component that rebuilds its spans
  // every describe must prune like a static leaf. The shared_ptr overload
  // cannot answer the question — a fresh make_shared is a fresh identity —
  // which is exactly the difference this pins.
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  const sigil::weave::TextStyle accent = coloredStyle(20, SK_ColorRED);
  auto describe = [&](std::u8string_view tail) {
    return box().child(
        text(rich(base).add(u8"Signal ").add(tail, accent)).key("t"));
  };
  host.composer.render(describe(u8"woven"));
  host.frame();
  host.composer.render(describe(u8"woven"));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical rich text did not prune";
  host.composer.render(describe(u8"noise"));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "a changed run pruned — equality is lying";

  auto byPointer = [&] {
    auto para = std::make_shared<sigil::weave::Paragraph>();
    para->appendText(std::u8string(u8"Signal woven"), base);
    return box().child(text(para).key("p"));
  };
  host.composer.render(byPointer());
  host.frame();
  host.composer.render(byPointer());
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "the pointer overload claimed a prune it cannot prove";
}

TEST(TextRich, AChangedRunStylePatchesToo) {
  Host host(400, 120);
  auto describe = [&](SkColor accentColor) {
    return box().child(text(rich(coloredStyle(20, SK_ColorWHITE))
                                .add(u8"Signal ")
                                .add(u8"woven", coloredStyle(20, accentColor)))
                           .key("t"));
  };
  host.composer.render(describe(SK_ColorRED));
  host.frame();
  host.composer.render(describe(SK_ColorRED));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.composer.render(describe(SK_ColorGREEN));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(TextRich, NamedRunsResolveThroughAStyleSet) {
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  sigil::weave::StyleSet reds;
  reds.set("accent", coloredStyle(20, SK_ColorRED));
  sigil::weave::StyleSet greens;
  greens.set("accent", coloredStyle(20, SK_ColorGREEN));

  const auto colorOf = [](const RichText& value, size_t run) {
    return value.runs()[run].style.paint.foreground.getColor();
  };

  const RichText supplied = rich(base).add(u8"x", "accent").styles(reds);
  EXPECT_EQ(colorOf(supplied, 0), SK_ColorRED);

  // styles() before the named run resolves identically: the order the two
  // are written in must not matter.
  const RichText suppliedFirst = rich(base).styles(reds).add(u8"x", "accent");
  EXPECT_EQ(colorOf(suppliedFirst, 0), SK_ColorRED);
  EXPECT_TRUE(supplied == suppliedFirst);

  {
    env::Provide<sigil::weave::StyleSet> ambient(reds);
    const RichText inherited = rich(base).add(u8"x", "accent");
    EXPECT_EQ(colorOf(inherited, 0), SK_ColorRED) << "the env set was ignored";
    const RichText overridden = rich(base).add(u8"x", "accent").styles(greens);
    EXPECT_EQ(colorOf(overridden, 0), SK_ColorGREEN)
        << "an explicit style set must beat the inherited one";
  }
  // Out of scope again: nothing is inherited, so the base answers.
  const RichText unbound = rich(base).add(u8"x", "accent");
  EXPECT_EQ(colorOf(unbound, 0), SK_ColorWHITE);
  // A name the set does not register resolves to rich()'s own base.
  const RichText unknown = rich(base).add(u8"x", "nope").styles(reds);
  EXPECT_EQ(colorOf(unknown, 0), SK_ColorWHITE);
}

TEST(TextSpans, SpanPaintRecolorsWithoutReshaping) {
  // Paint-only means paint-only: the glyphs are the glyphs the unrestyled
  // text shaped, at the positions it shaped them, drawn in another colour.
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(28, SK_ColorWHITE);
  const std::u8string body = u8"Count 1234 now";
  host.composer.render(box().padding(10).child(text(body, base).key("t")));
  host.frame();
  const std::vector<const void*> shapesBefore = runShapes(host, "t");
  const std::vector<SkPoint> originsBefore = runOrigins(host, "t");
  ASSERT_FALSE(shapesBefore.empty());

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sel::regex(u8"[0-9]+"),
                     sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  host.frame();
  EXPECT_EQ(runShapes(host, "t"), shapesBefore)
      << "a paint-only restyle re-shaped a word";
  EXPECT_EQ(runOrigins(host, "t"), originsBefore)
      << "a paint-only restyle moved a glyph";
  const SkIRect band = SkIRect::MakeXYWH(0, 0, 400, 80);
  EXPECT_GT(countColor(host, band, SK_ColorRED), 10) << "the digits";
  EXPECT_GT(countColor(host, band, SK_ColorWHITE), 10) << "everything else";
}

TEST(TextSpans, SpanStyleReshapesOnlyTheWordsItCovers) {
  Host host(400, 160);
  const sigil::weave::TextStyle base = coloredStyle(24, SK_ColorWHITE);
  const std::u8string body = u8"alpha beta gamma";
  host.composer.render(box().padding(10).child(text(body, base).key("t")));
  host.frame();
  const std::vector<const void*> before = runShapes(host, "t");
  ASSERT_EQ(before.size(), 3u);

  // The LAST word, so the two ahead of it keep their pen positions too.
  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanStyle(sel::text(u8"gamma"), coloredStyle(40, SK_ColorRED))
          .key("t")));
  host.frame();
  const std::vector<const void*> after = runShapes(host, "t");
  ASSERT_EQ(after.size(), 3u);
  EXPECT_EQ(after[0], before[0]) << "an uncovered word re-shaped";
  EXPECT_EQ(after[1], before[1]) << "an uncovered word re-shaped";
  EXPECT_NE(after[2], before[2]) << "the covered word did not re-shape";
}

TEST(TextSpans, ALaterRestyleWinsOnOverlap) {
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(28, SK_ColorWHITE);
  const std::u8string body = u8"alpha beta";
  const SkIRect band = SkIRect::MakeXYWH(0, 0, 400, 80);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sel::text(u8"beta"), sigil::weave::PaintStyle(SK_ColorRED))
          .spanPaint(sel::words(0, 2), sigil::weave::PaintStyle(SK_ColorGREEN))
          .key("t")));
  host.frame();
  EXPECT_EQ(countColor(host, band, SK_ColorRED), 0)
      << "the earlier narrow rule survived a later broad one";
  EXPECT_GT(countColor(host, band, SK_ColorGREEN), 20);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sel::words(0, 2), sigil::weave::PaintStyle(SK_ColorGREEN))
          .spanPaint(sel::text(u8"beta"), sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  host.frame();
  EXPECT_GT(countColor(host, band, SK_ColorRED), 10) << "the narrow exception";
  EXPECT_GT(countColor(host, band, SK_ColorGREEN), 10) << "the broad rule";
}

TEST(TextSpans, ALineSelectorAddressesTheLayout) {
  Host host(240, 200);
  const sigil::weave::TextStyle base = coloredStyle(24, SK_ColorWHITE);
  const std::u8string body = u8"one two three four five six seven eight";
  host.composer.render(
      box().padding(10).child(text(body, base).width(200).key("t")));
  host.frame();
  const auto* plain = host.composer.paragraphLayout("t");
  ASSERT_NE(plain, nullptr);
  ASSERT_GT(plain->lineCount, 1);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .width(200)
          .spanPaint(sel::line(0), sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  host.frame();
  const SkIRect all = SkIRect::MakeXYWH(0, 0, 240, 200);
  EXPECT_GT(countColor(host, all, SK_ColorRED), 10) << "the first line";
  EXPECT_GT(countColor(host, all, SK_ColorWHITE), 10) << "the rest";
}

// ---------------------------------------------------------------------------
// sel::style — the runs addressed by the NAME they were dressed in.
//
// The paragraph below is built so that the name and the words can disagree:
// two runs are written under "term", and a THIRD run says the same word with
// no name at all. Anything that resolves the name by matching text catches
// three; the selector must catch two.

namespace {

sigil::weave::StyleSet glossarySet(SkColor termColor, float termSize) {
  sigil::weave::StyleSet set{coloredStyle(24, SK_ColorWHITE)};
  set.set("term", coloredStyle(termSize, termColor));
  return set;
}

/** "alpha beta gamma beta delta beta", where the first and last `beta` are
 *  written under the name and the middle one is not. */
RichText glossaryCopy(const sigil::weave::StyleSet& set) {
  RichText copy = rich(set.base());
  copy.styles(set)
      .add(u8"alpha ")
      .add(u8"beta", "term")
      .add(u8" gamma ")
      .add(u8"beta")
      .add(u8" delta ")
      .add(u8"beta", "term");
  return copy;
}

}  // namespace

TEST(TextStyleSelector, AddressesTheNamedRunsAndNotTheirWords) {
  Host host(760, 140);
  const RichText copy = glossaryCopy(glossarySet(SK_ColorRED, 24));
  // Beats at WORD granularity number the units the track's own selection
  // resolved, so the beat list IS the addressed word list — and each beat's
  // rect says which word it is.
  const auto wordsAddressed = [&](Selector where) {
    host.composer.render(box().padding(10).child(text(copy).key("t").fx(
        {.where = std::move(where),
         .effect = fx::rise(0),
         .stagger = stagger(unit::Word, {.eachMs = 1, .durationMs = 1})})));
    host.frame();
    return host.composer.beatsOf("t", 0);
  };

  const std::vector<Beat> byName = wordsAddressed(sel::style("term"));
  const std::vector<Beat> byWords = wordsAddressed(sel::text(u8"beta"));
  ASSERT_EQ(byWords.size(), 3u) << "the three literal betas";
  ASSERT_EQ(byName.size(), 2u)
      << "the name caught a run nobody wrote it on — sel::style is matching "
         "text rather than the runs the content named";
  // …and they are the FIRST and THIRD of them, in place: the middle beta is
  // the one the name skips.
  EXPECT_EQ(byName[0].rect, byWords[0].rect);
  EXPECT_EQ(byName[1].rect, byWords[2].rect);
}

TEST(TextStyleSelector, ComposesUnderTheSelectorAlgebra) {
  Host host(760, 140);
  const RichText copy = glossaryCopy(glossarySet(SK_ColorRED, 24));
  // Beats at GLYPH granularity: one per addressed glyph, so the count is the
  // selection's size and the algebra can be checked as arithmetic.
  const auto glyphsAddressed = [&](Selector where) {
    host.composer.render(box().padding(10).child(text(copy).key("t").fx(
        {.where = std::move(where),
         .effect = fx::rise(0),
         .stagger = stagger(unit::Glyph, {.eachMs = 1, .durationMs = 1})})));
    host.frame();
    return host.composer.beatsOf("t", 0).size();
  };

  const size_t whole = glyphsAddressed(Selector{});
  const size_t named = glyphsAddressed(sel::style("term"));
  ASSERT_EQ(named, 8u) << "two four-letter runs";
  EXPECT_EQ(glyphsAddressed(sel::text(u8"beta")), 12u) << "three of them";

  // The three operators, against the same two runs.
  EXPECT_EQ(glyphsAddressed(sel::style("term") | sel::word(0)), named + 5u)
      << "alpha joined the union";
  EXPECT_EQ(glyphsAddressed(sel::style("term") & sel::word(1)), 4u)
      << "the intersection is the first named run alone";
  EXPECT_EQ(glyphsAddressed(!sel::style("term")), whole - named);
}

TEST(TextStyleSelector, PlainTextCarriesNoNamesAndSaysSoOnce) {
  // Only a named rich() run carries a name. Plain text has none, so the
  // selector addresses nothing — the silent-no-op rule, made audible.
  Host host(400, 120);
  ::testing::internal::CaptureStderr();
  const auto describe = [] {
    return box().padding(10).child(
        text(u8"alpha beta gamma", coloredStyle(24, SK_ColorWHITE))
            .key("t")
            .fx({.where = sel::style("unregistered-register"),
                 .effect = fx::rise(0),
                 .stagger = stagger(unit::Glyph, {.durationMs = 1})}));
  };
  host.composer.render(describe());
  host.frame();
  EXPECT_TRUE(host.composer.beatsOf("t", 0).empty())
      << "a name no run was written with addressed glyphs anyway";

  // A selector is re-resolved on every reflow, so a name that is wrong is
  // wrong every time — and must not report itself every time.
  host.composer.render(box().padding(11).child(
      text(u8"alpha beta gamma", coloredStyle(24, SK_ColorWHITE))
          .key("t")
          .fx({.where = sel::style("unregistered-register"),
               .effect = fx::rise(0),
               .stagger = stagger(unit::Glyph, {.durationMs = 1})})));
  host.frame();
  const std::string log = ::testing::internal::GetCapturedStderr();
  size_t seen = 0;
  for (size_t at = log.find("unregistered-register"); at != std::string::npos;
       at = log.find("unregistered-register", at + 1))
    ++seen;
  EXPECT_EQ(seen, 1u) << log;
}

TEST(TextStyleSelector, ReachesTheSpanRestylesToo) {
  // The span verbs resolve their selection as TEXT RANGES through a second
  // resolver. One vocabulary means one answer: the name must address the
  // same two runs there.
  Host host(760, 140);
  const RichText copy = glossaryCopy(glossarySet(SK_ColorWHITE, 24));

  // Where the three betas actually sit, read off the layout rather than
  // guessed, so the assertions below can name one of them.
  host.composer.render(box().padding(10).child(text(copy).key("t").fx(
      {.where = sel::text(u8"beta"),
       .effect = fx::rise(0),
       .stagger = stagger(unit::Word, {.eachMs = 1, .durationMs = 1})})));
  host.frame();
  const std::vector<Beat> betas = host.composer.beatsOf("t", 0);
  ASSERT_EQ(betas.size(), 3u);
  const auto bandOf = [](const Beat& b) {
    return SkIRect::MakeLTRB((int)std::floor(b.rect.left()), 0,
                             (int)std::ceil(b.rect.right()), 140);
  };

  const auto redsIn = [&](Selector where, const Beat& beat) {
    host.composer.render(box().padding(10).child(text(copy).key("t").spanPaint(
        std::move(where), sigil::weave::PaintStyle(SK_ColorRED))));
    host.frame();
    return countColor(host, bandOf(beat), SK_ColorRED);
  };

  EXPECT_GT(redsIn(sel::style("term"), betas[0]), 5) << "the first named run";
  EXPECT_GT(redsIn(sel::style("term"), betas[2]), 5) << "the last named run";
  EXPECT_EQ(redsIn(sel::style("term"), betas[1]), 0)
      << "the unnamed beta was repainted, so the restyle resolver matched "
         "the word rather than the run";
  EXPECT_GT(redsIn(sel::text(u8"beta"), betas[1]), 5)
      << "…which the literal selector does catch, as it must";

  // And through spanStyle, which re-shapes: exactly the named runs do.
  host.composer.render(box().padding(10).child(text(copy).key("t")));
  host.frame();
  const std::vector<const void*> before = runShapes(host, "t");
  ASSERT_FALSE(before.empty());
  const auto reshapedUnder = [&](Selector where) {
    host.composer.render(box().padding(10).child(text(copy).key("t").spanStyle(
        std::move(where), coloredStyle(34, SK_ColorGREEN))));
    host.frame();
    const std::vector<const void*> after = runShapes(host, "t");
    // A run list of a different LENGTH is not a re-shape count at all — the
    // restyle re-broke the passage, and the comparison below would be
    // pairing runs that are not each other's.
    if (after.size() != before.size()) return SIZE_MAX;
    size_t moved = 0;
    for (size_t i = 0; i < after.size(); ++i) moved += after[i] != before[i];
    return moved;
  };
  EXPECT_EQ(reshapedUnder(sel::style("term")), 2u);
  EXPECT_EQ(reshapedUnder(sel::text(u8"beta")), 3u);
}

TEST(TextStyleSelector, ANameOutlivesTheStyleItResolvedTo) {
  // The name is a handle on the RUN, not on the style span it produced — so
  // re-registering it against a different style, at a different size that
  // re-shapes and re-places everything, leaves the same runs addressed.
  Host host(760, 140);
  const auto namedGlyphs = [&](const sigil::weave::StyleSet& set) {
    host.composer.render(box().padding(10).child(
        text(glossaryCopy(set))
            .key("t")
            .fx({.where = sel::style("term"),
                 .effect = fx::rise(0),
                 .stagger =
                     stagger(unit::Glyph, {.eachMs = 1, .durationMs = 1})})));
    host.frame();
    return host.composer.beatsOf("t", 0).size();
  };
  EXPECT_EQ(namedGlyphs(glossarySet(SK_ColorRED, 24)), 8u);
  EXPECT_EQ(namedGlyphs(glossarySet(SK_ColorGREEN, 36)), 8u)
      << "a re-registered name stopped resolving, so the selector is keyed "
         "on the style rather than on the run that wears it";
}

// ---------------------------------------------------------------------------
// spanAxis — the advance-invariant middle between spanPaint and spanStyle

namespace {

/** A face carrying the advance-invariant GRAD axis, and that axis's own
 *  design range — null when this machine has none, or has one whose varied
 *  clone renders identically, because a test that cannot see the axis move
 *  would pass while checking nothing. */
sk_sp<SkTypeface> gradFace(float& lo, float& hi) {
  lo = hi = 0;
  sk_sp<SkFontMgr> manager = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> face;
  for (const char* family :
       {".AppleSystemUIFont", ".SF NS", "SF Pro Text", "SF Pro"}) {
    face = manager->matchFamilyStyle(family, SkFontStyle());
    if (face && fonts().axisIsAdvanceInvariant(face, "GRAD")) break;
    face = nullptr;
  }
  if (!face) return nullptr;
  const int count = face->getVariationDesignParameters({});
  if (count <= 0) return nullptr;
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
  face->getVariationDesignParameters({axes.data(), axes.size()});
  for (const auto& axis : axes)
    if (axis.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
      lo = axis.min;
      hi = axis.max;
    }
  if (hi <= lo) return nullptr;

  // The advance probe proves advances HOLD; it cannot prove the clone
  // RESPONDS. Rasterize one glyph at both ends and insist it does.
  const sigil::weave::FontVariation vLo("GRAD", lo), vHi("GRAD", hi);
  const auto ink = [&](const sigil::weave::FontVariation& v) {
    SkFont font(fonts().variedTypeface(face, {&v, 1}), 48);
    const SkGlyphID glyph = font.unicharToGlyph('W');
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
    surface->getCanvas()->clear(SK_ColorBLACK);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    paint.setAntiAlias(true);
    const SkPoint at{10, 60};
    surface->getCanvas()->drawGlyphs(SkSpan(&glyph, 1), SkSpan(&at, 1), {0, 0},
                                     font, paint);
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    surface->readPixels(bitmap.pixmap(), 0, 0);
    return bitmap;
  };
  const SkBitmap light = ink(vLo), heavy = ink(vHi);
  for (int y = 0; y < 80; ++y)
    for (int x = 0; x < 100; ++x)
      if (light.getColor(x, y) != heavy.getColor(x, y)) return face;
  return nullptr;
}

/** The whole surface, for a byte comparison against another frame. */
SkBitmap grab(Host& host, int w, int h) {
  SkBitmap out;
  out.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(out.pixmap(), 0, 0);
  return out;
}

int pixelsDiffering(const SkBitmap& a, const SkBitmap& b, int w, int h) {
  int changed = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) changed += a.getColor(x, y) != b.getColor(x, y);
  return changed;
}

}  // namespace

TEST(TextSpanAxis, AnInvariantAxisRedrawsWithoutReshaping) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  if (!face) GTEST_SKIP() << "no responsive advance-invariant GRAD face here";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(40, SK_ColorWHITE);
  base.shaping.typeface = face;
  const std::u8string body = u8"Count 1234 now";
  host.composer.render(box().padding(10).child(text(body, base).key("t")));
  host.frame();
  const std::vector<const void*> shapesBefore = runShapes(host, "t");
  const std::vector<SkPoint> originsBefore = runOrigins(host, "t");
  ASSERT_FALSE(shapesBefore.empty());
  const SkBitmap plain = grab(host, 400, 120);

  host.composer.render(box().padding(10).child(
      text(body, base).spanAxis(sel::regex(u8"[0-9]+"), "GRAD", hi).key("t")));
  host.frame();
  EXPECT_EQ(runShapes(host, "t"), shapesBefore)
      << "spanAxis re-shaped a word — the axis went into the shaping style";
  EXPECT_EQ(runOrigins(host, "t"), originsBefore) << "spanAxis moved a glyph";
  EXPECT_GT(pixelsDiffering(plain, grab(host, 400, 120), 400, 120), 20)
      << "the graded numerals are drawing exactly as the ungraded ones did";
}

TEST(TextSpanAxis, AnAdvanceVariantAxisIsRefusedAndSaysSoOnce) {
  // The instrument face whose wght genuinely interpolates advances, so the
  // gate has something to refuse. Loaded here rather than shared, so this
  // face's verdict is this test's own to observe.
  const SkFourByteTag wght = SkSetFourByteTag('w', 'g', 'h', 't');
  const sk_sp<SkTypeface> face = fonts().fontManager()->makeFromFile(
      SIGILCOMPOSE_TEST_ASSET_DIR "/AdvanceVariant.ttf");
  ASSERT_TRUE(face) << "test asset AdvanceVariant.ttf failed to load";
  ASSERT_GT(face->getVariationDesignParameters({}), 0);
  ASSERT_FALSE(fonts().axisIsAdvanceInvariant(face, "wght"))
      << "the instrument face's wght must move advances";
  (void)wght;

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(44, SK_ColorWHITE);
  base.shaping.typeface = face;
  const std::u8string body = u8"WEIGHT";
  // EVERY FRAME CARRIES A spanAxis, and they differ only in the coordinate.
  // A leaf carrying a track draws through the batched glyph path and one
  // without it draws through the paragraph's own, and the two round subpixel
  // coverage differently — so a bare paragraph as the baseline would measure
  // the path change and call it a weight.
  ::testing::internal::CaptureStderr();
  const auto at = [&](float weight) {
    host.composer.render(box().padding(10).child(
        text(body, base).spanAxis(Selector{}, "wght", weight).key("t")));
    host.frame();
    return grab(host, 400, 120);
  };
  const SkBitmap light = at(400.0f);
  const SkBitmap heavy = at(900.0f);
  const SkBitmap heavier = at(950.0f);
  const std::string log = ::testing::internal::GetCapturedStderr();

  int inked = 0;
  for (int y = 0; y < 120; y += 2)
    for (int x = 0; x < 400; x += 2)
      inked += light.getColor(x, y) != SK_ColorBLACK;
  ASSERT_GT(inked, 20) << "the text never drew";

  EXPECT_EQ(pixelsDiffering(light, heavy, 400, 120), 0)
      << "an advance-variant axis reached the draw — the glyphs kept the pen "
         "positions shaping gave them and wore an outline that does not fit "
         "them";
  EXPECT_EQ(pixelsDiffering(light, heavier, 400, 120), 0);
  // The warning is also the liveness proof: it can only have been written by
  // the gate this verb's coordinate reached. And the verdict is a property
  // of the face, probed once and remembered, so the two refusals after the
  // first must be silent.
  size_t said = 0;
  for (size_t found = log.find("moves advances"); found != std::string::npos;
       found = log.find("moves advances", found + 1))
    ++said;
  EXPECT_EQ(said, 1u) << log;
}

TEST(TextSpanAxis, TheCoordinateTakesTheSizeScaledLadder) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  if (!face) GTEST_SKIP() << "no responsive advance-invariant GRAD face here";

  // A FIXED WINDOW of design space, swept at a fixed number of samples, at
  // two rendered sizes. The ladder is cut per rendered size, so the same
  // window holds more rungs on the larger type — and a rung is a retained
  // varied clone, which is countable where a pixel difference of one rung
  // is not. Both sizes sit inside the ladder's proportional band, away from
  // its floor and its ceiling, so this reads the proportion and not a clamp.
  constexpr float kSmallPx = 24.0f, kLargePx = 112.0f;
  constexpr int kSamples = 61;
  const float window = (hi - lo) / 32.0f;
  const auto clonesAcrossTheWindow = [&](float pixelSize) {
    sigil::weave::FontContext local(sigil::weave::ports::systemFontManager());
    sigil::motion::Ticker ticker;
    Composer composer(ticker, local);
    composer.setSize({200, 200});
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
    sigil::weave::TextStyle style = coloredStyle(pixelSize, SK_ColorWHITE);
    style.shaping.typeface = face;
    for (int i = 0; i < kSamples; ++i) {
      const float value =
          lo + (hi - lo) * 0.5f + window * (float)i / (float)(kSamples - 1);
      composer.render(box().padding(4).child(
          // A default-constructed selector addresses every glyph.
          text(u8"888", style).key("t").spanAxis(Selector{}, "GRAD", value)));
      ticker.tick(1.0 / 60.0);
      surface->getCanvas()->clear(SK_ColorBLACK);
      composer.draw(*surface->getCanvas());
    }
    return local.variedTypefaceCount();
  };

  const size_t coarse = clonesAcrossTheWindow(kSmallPx);
  const size_t fine = clonesAcrossTheWindow(kLargePx);
  EXPECT_GT(coarse, 0u) << "the coordinate never reached a face at all";
  EXPECT_LT(coarse, (size_t)kSamples)
      << "every sample minted its own clone — the value is reaching the memo "
         "unsnapped";
  EXPECT_LT(coarse, fine)
      << "the same window of design space resolved to no more rungs at "
      << kLargePx << " px than at " << kSmallPx
      << " px, so the ladder is not cut by the rendered size";
}

TEST(TextSpanAxis, ALaterDeclarationWinsOnOverlap) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  if (!face) GTEST_SKIP() << "no responsive advance-invariant GRAD face here";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(46, SK_ColorWHITE);
  base.shaping.typeface = face;
  const auto drawn = [&](const std::function<Element(Element)>& dress) {
    host.composer.render(
        box().padding(10).child(dress(text(u8"GRADE", base).key("t"))));
    host.frame();
    return grab(host, 400, 120);
  };
  const SkBitmap light =
      drawn([&](Element t) { return t.spanAxis(Selector{}, "GRAD", lo); });
  const SkBitmap heavy =
      drawn([&](Element t) { return t.spanAxis(Selector{}, "GRAD", hi); });
  ASSERT_GT(pixelsDiffering(light, heavy, 400, 120), 20)
      << "the two ends of the axis draw the same, so nothing below is a test";

  const SkBitmap both = drawn([&](Element t) {
    return t.spanAxis(Selector{}, "GRAD", lo).spanAxis(Selector{}, "GRAD", hi);
  });
  EXPECT_EQ(pixelsDiffering(both, heavy, 400, 120), 0)
      << "the earlier declaration survived the later one — an axis is a "
         "substitution, and overlapping substitutions are last-one-wins";
}

TEST(TextOptionSetters, MaxLinesAndEllipsisClampTheText) {
  Host host(240, 200);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  const std::u8string body =
      u8"one two three four five six seven eight nine ten eleven twelve";
  host.composer.render(box().padding(10).child(
      text(body, base).width(180).maxLines(2).ellipsis(u8"...").key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(layout->lineCount, 2);
  EXPECT_TRUE(layout->overflowed());
  EXPECT_TRUE(layout->ellipsized) << "the marker never landed";
}

TEST(TextOptionSetters, HyphenationRendersTheHyphenAtASoftBreak) {
  Host host(260, 220);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  // A short word, then a long one carrying a discretionary break, in a
  // measure that cannot hold both whole. The line breaks at the soft hyphen
  // either way; what the option controls is whether the hyphen is DRAWN,
  // which shows up as one extra run on the broken line.
  const std::u8string body = u8"short extraordi\u00adnarily";
  const auto runsOnFirstLineWith = [&](bool enabled) {
    host.composer.render(
        box().padding(4).child(text(body, base)
                                   .width(150)
                                   .hyphenation({.enabled = enabled})
                                   .key("t")));
    host.frame();
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return 0;
    int runs = 0;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      if (run.lineIndex == 0) ++runs;
    return runs;
  };
  const int without = runsOnFirstLineWith(false);
  const int with = runsOnFirstLineWith(true);
  EXPECT_GT(with, without) << "the hyphenation setting never reached layout";
}

TEST(TextOptionSetters, SettersOverrideAPassedOptionsValueFieldByField) {
  // The contract for the full-control overload: a setter names one field and
  // leaves every other field of the passed options standing.
  Host host(300, 220);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  para->appendText(std::u8string(u8"one two three four five six seven eight"),
                   coloredStyle(20, SK_ColorWHITE));
  sigil::weave::ParagraphLayoutOptions passed;
  passed.alignment = sigil::weave::TextAlignment::kCenter;
  passed.overflow.maxLines = 5;
  host.composer.render(
      box().child(text(para, passed).width(200).maxLines(2).key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(layout->lineCount, 2) << "the setter did not override maxLines";
  ASSERT_FALSE(layout->runs.empty());
  EXPECT_GT(layout->runs.front().origin.fX, 1.0f)
      << "the passed alignment was clobbered rather than left alone";
}

TEST(TextOptionSetters, KnuthPlassBreaksARaggedParagraphDifferently) {
  Host host(320, 260);
  const sigil::weave::TextStyle base = coloredStyle(18, SK_ColorWHITE);
  // Deliberately uneven word lengths: the greedy breaker fills each line as
  // far as it goes, and the optimal one trades an early line short to keep
  // the whole paragraph even.
  const std::u8string body =
      u8"a longer word then tiny bits of text and an extraordinarily "
      u8"lengthy one to finish the measure";
  const auto lineStartsUnder = [&](sigil::weave::LineBreakStrategy strategy) {
    host.composer.render(box().padding(4).child(
        text(body, base).width(240).lineBreak(strategy).key("t")));
    host.frame();
    std::vector<uint32_t> starts;
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return starts;
    int previous = -1;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      if (run.lineIndex != previous) {
        starts.push_back(run.wordIndex);
        previous = run.lineIndex;
      }
    return starts;
  };
  const std::vector<uint32_t> greedy =
      lineStartsUnder(sigil::weave::LineBreakStrategy::kGreedy);
  const std::vector<uint32_t> optimal =
      lineStartsUnder(sigil::weave::LineBreakStrategy::kKnuthPlass);
  ASSERT_GT(greedy.size(), 1u);
  EXPECT_NE(greedy, optimal) << "the break strategy setter changed nothing";
}

// ---------------------------------------------------------------------------
// INLINE SLOTS — rich().slot() reserves the room, a keyed child fills it

namespace {
/** A caption with one reserved slot, and a pill child keyed for it. */
Element pillCaption(std::string childKey, float width, SkSize size = {34, 16}) {
  // The width lives on an inner box: the render root is always resized to
  // the composer's own size, so a width written there is overwritten.
  return box().child(box().padding(8).width(width).child(
      text(rich(coloredStyle(18, SK_ColorWHITE))
               .add(u8"press the archive key ")
               .slot("pill", size, 4)
               .add(u8" to continue the long descent"))
          .key("caption")
          .child(box().key(std::move(childKey)).fill(red()))));
}
}  // namespace

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
    return text(rich(coloredStyle(16, SK_ColorWHITE))
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

// ---------------------------------------------------------------------------
// fx::tint — the colour reveal, and the inversion it hides

TEST(ComposeTextFx, TintRampsColorMulBetweenTheTwoColoursInTimeOrder) {
  // The arguments read in TIME ORDER while the mechanism runs the other
  // way: colorMul MULTIPLIES, so the element is set in the destination and
  // the effect divides down toward the origin. At t = 1 the multiplier must
  // therefore be white — anything else tints a line that has arrived.
  const SkColor4f pale{0.9f, 0.8f, 0.4f, 1};
  const SkColor4f sung{0.3f, 0.6f, 0.8f, 1};
  const TextEffect ramp = fx::tint(pale, sung);
  GlyphInfo glyph;
  Rng rng(1);
  const GlyphMod start = ramp(glyph, 0.0f, rng);
  const GlyphMod end = ramp(glyph, 1.0f, rng);
  const GlyphMod middle = ramp(glyph, 0.5f, rng);

  // Origin: the multiplier that takes the DESTINATION to `pale`.
  EXPECT_NEAR(start.colorMul.fR * sung.fR, pale.fR, 1e-5f);
  EXPECT_NEAR(start.colorMul.fG * sung.fG, pale.fG, 1e-5f);
  EXPECT_NEAR(start.colorMul.fB * sung.fB, pale.fB, 1e-5f);
  // Destination: no tint at all.
  EXPECT_NEAR(end.colorMul.fR, 1.0f, 1e-5f);
  EXPECT_NEAR(end.colorMul.fG, 1.0f, 1e-5f);
  EXPECT_NEAR(end.colorMul.fB, 1.0f, 1e-5f);
  // And a monotone ramp between them on every channel, in whichever
  // direction that channel happens to run.
  for (auto lane : {&SkColor4f::fR, &SkColor4f::fG, &SkColor4f::fB}) {
    const float a = start.colorMul.*lane, b = middle.colorMul.*lane;
    EXPECT_GT((b - a) * (1.0f - a), 0.0f)
        << "the middle of the ramp is not between its ends";
  }
  // Alpha is left alone: a reveal that also fades is a separate track.
  EXPECT_FLOAT_EQ(start.colorMul.fA, 1.0f);
  // The value is comparable, which is what lets a re-described wipe prune.
  EXPECT_TRUE(fx::tint(pale, sung) == ramp);
  EXPECT_FALSE(fx::tint(sung, pale) == ramp);
  // A destination channel of zero cannot be departed from, and the ramp
  // says so by holding at 1 rather than dividing by nothing.
  const GlyphMod dark = fx::tint({1, 1, 1, 1}, {0, 0, 0, 1})(glyph, 0.0f, rng);
  EXPECT_FLOAT_EQ(dark.colorMul.fR, 1.0f);
}

TEST(ComposeTextFx, TintComposesWithAnotherTrackByMultiplying) {
  // Stacked tracks multiply their colour multipliers, so a tint under a
  // second tint is the product — not the last one to run. The letter is set
  // in white so the product is readable straight off the pixels.
  Host host(160, 120);
  host.composer.render(box().padding(8).child(
      text(u8"I", whiteStyle(64))
          .key("k")
          // Both tracks are AT REST (progress 0), where each contributes
          // its own origin: 0.5 on red and 0.5 on green.
          .fx({.effect = fx::tint({0.5f, 1, 1, 1}, {1, 1, 1, 1}),
               .stagger = {.eachMs = 0, .durationMs = 100},
               .progress = 0.0f})
          .fx({.effect = fx::tint({1, 0.5f, 1, 1}, {1, 1, 1, 1}),
               .stagger = {.eachMs = 0, .durationMs = 100},
               .progress = 0.0f})));
  host.frame();
  bool sawProduct = false;
  for (int y = 0; y < 120 && !sawProduct; ++y)
    for (int x = 0; x < 160; ++x) {
      const SkColor c = host.pixel(x, y);
      if (SkColorGetB(c) < 200) continue;  // not a glyph pixel
      // Half red AND half green, within the multiplier's quantization.
      if (std::abs((int)SkColorGetR(c) - 128) < 24 &&
          std::abs((int)SkColorGetG(c) - 128) < 24) {
        sawProduct = true;
        break;
      }
      EXPECT_FALSE(SkColorGetR(c) > 200 && SkColorGetG(c) > 200)
          << "one of the two tints never reached the glyph";
    }
  EXPECT_TRUE(sawProduct) << "the two tints did not multiply";
}

// ---------------------------------------------------------------------------
// The Debug.h instruments

TEST(ComposeDebug, TrackMeterDrawsACellPerBeatAtItsRect) {
  // The meter is beatsOf drawn: a cell on each unit's own rect, filled by
  // that unit's local time. Half way through a four-beat cascade, the first
  // cells are full, the last are empty, and every cell stands where its
  // letters do.
  Host host(300, 140);
  const auto describe = [&](bool withMeter) {
    Element root = box().padding(10).child(
        text(u8"ABCD", whiteStyle(28))
            .key("word")
            .fx({.effect = fx::rise(4),
                 .stagger = {.eachMs = 100, .durationMs = 100},
                 .progress = 0.5f}));
    if (withMeter)
      root.child(debug::trackMeter(host.composer, "word", 0, {1, 0, 0, 1},
                                   {0, 0, 1, 1})
                     .absolute()
                     .inset(0));
    return root;
  };
  host.composer.render(describe(false));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("word", 0);
  ASSERT_EQ(beats.size(), 4u);
  host.composer.render(describe(true));
  host.frame();

  // Every beat's rect carries a cell: bed where the beat has not run, fill
  // where it has, and the boundary between them at its localT.
  for (const Beat& beat : beats) {
    const int y = (int)beat.rect.centerY();
    const int left = (int)beat.rect.left() + 1;
    const int right = (int)beat.rect.right() - 1;
    ASSERT_LT(left, right) << "a beat rect with no width to draw in";
    const SkColor at = host.pixel(left, y);
    if (beat.localT > 0.05f)
      EXPECT_GT(SkColorGetR(at), 200u)
          << "a beat that has run shows no fill at its left edge";
    if (beat.localT < 0.95f)
      EXPECT_GT(SkColorGetB(host.pixel(right, y)), 200u)
          << "a beat that has not finished shows no bed at its right edge";
  }
  // …and outside the last beat's rect there is no meter at all: the cells
  // are the units' boxes and not one strip across the node.
  EXPECT_EQ(SkColorGetB(host.pixel((int)beats.back().rect.right() + 6,
                                   (int)beats.back().rect.centerY())),
            0u);
  // An unknown key is the query family's silent nothing, drawn: an overlay
  // with no cells in it, which measures as nothing rather than warning.
  Host empty(120, 80);
  empty.composer.render(box().child(
      debug::trackMeter(host.composer, "typo", 0, {1, 0, 0, 1}, {0, 0, 1, 1})
          .key("meter")
          .absolute()
          .inset(0)));
  empty.frame();
  for (int y = 0; y < 80; ++y)
    for (int x = 0; x < 120; ++x)
      ASSERT_EQ(empty.pixel(x, y), SK_ColorBLACK)
          << "an unknown key drew a meter anyway";
}

TEST(ComposeDebug, RestGhostDrawsTheSameWordUndeformedUnderTheMovingOne) {
  // The ghost is the rest position a deviation is measured against, so it
  // must be where the letters WOULD be — and must not be carrying the track
  // that moved them.
  Host host(300, 140);
  const SkColor4f ghostInk{0, 0, 1, 1};
  GlyphMod shove;
  shove.dx = 60.0f;
  host.composer.render(box().padding(10).child(
      debug::restGhost(text(u8"AB", whiteStyle(40))
                           .key("word")
                           .fx({.effect = fixed("shove", shove)}),
                       ghostInk)));
  host.frame();
  const auto countBlue = [&](SkIRect region) {
    int hits = 0;
    for (int y = region.top(); y < region.bottom(); ++y)
      for (int x = region.left(); x < region.right(); ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetB(c) > 180 && SkColorGetR(c) < 80) ++hits;
      }
    return hits;
  };
  const auto countWhite = [&](SkIRect region) {
    int hits = 0;
    for (int y = region.top(); y < region.bottom(); ++y)
      for (int x = region.left(); x < region.right(); ++x)
        if (host.pixel(x, y) == SK_ColorWHITE) ++hits;
    return hits;
  };
  // The ghost sits at rest, near the left; the moving copy is 60 px right
  // of it. Neither region may hold the other's ink.
  const SkIRect atRest = SkIRect::MakeXYWH(0, 0, 60, 140);
  const SkIRect shoved = SkIRect::MakeXYWH(66, 0, 120, 140);
  EXPECT_GT(countBlue(atRest), 20) << "no ghost at the rest position";
  EXPECT_EQ(countWhite(atRest), 0)
      << "the moving copy never left its rest position";
  EXPECT_GT(countWhite(shoved), 20) << "the moving copy did not move";
  EXPECT_EQ(countBlue(shoved), 0) << "the ghost is carrying the track too";
  // The ghost is addressable, and it is exactly as wide as the word.
  const SkRect ghost =
      host.composer.bounds("word-rest").value_or(SkRect::MakeEmpty());
  const SkRect moving =
      host.composer.bounds("word").value_or(SkRect::MakeEmpty());
  ASSERT_FALSE(ghost.isEmpty());
  EXPECT_NEAR(ghost.width(), moving.width(), 0.51f);
  EXPECT_NEAR(ghost.left(), moving.left(), 0.51f);
}

TEST(ComposeDebug, RestGhostCopiesTheTypeAndNotTheMarksOnIt) {
  // A text node's children are already on screen once. Ghosting them would
  // draw each of them twice under one key, which the composer's key index
  // cannot answer for — so the ghost is the type and nothing else.
  Host host(300, 140);
  host.composer.render(box().padding(10).child(debug::restGhost(
      text(u8"ALPHA BETA", whiteStyle(24))
          .key("word")
          .mark(sel::word(1), box().key("caret").width(4).fill(green())),
      {0, 0, 1, 1})));
  host.frame();
  const SkRect caret =
      host.composer.bounds("caret").value_or(SkRect::MakeEmpty());
  ASSERT_FALSE(caret.isEmpty()) << "the mark on the moving copy is gone";
  int greens = 0;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 300; ++x)
      if (host.pixel(x, y) == SK_ColorGREEN) ++greens;
  EXPECT_GT(greens, 0);
  // One mark, drawn once: every green pixel is inside the one rect the
  // query answers for.
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 300; ++x)
      if (host.pixel(x, y) == SK_ColorGREEN)
        ASSERT_TRUE(caret.contains((float)x + 0.5f, (float)y + 0.5f))
            << "the ghost carries a second copy of the mark";
}

// ---------------------------------------------------------------------------
// Element::mark — a sibling anchored to a unit

namespace {
/** The mark's rect, as the composer reports it. */
SkRect markRect(Host& host, std::string_view key) {
  const std::optional<SkRect> rect = host.composer.bounds(key);
  return rect.value_or(SkRect::MakeEmpty());
}
}  // namespace

TEST(ComposeTextFx, MarkPlacesAChildOnTheRectItsSelectorResolves) {
  // A mark's box IS the unit's rect when it says nothing about its own
  // placement — and it is the SAME rect the schedule read-back reports, so
  // a caret and a beat can never disagree about where a word is.
  Host host(400, 140);
  host.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA GAMMA", whiteStyle(24))
          .key("line")
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Word)})
          .mark(sel::word(1), box().key("caret").fill(green()))));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("line", 0);
  ASSERT_EQ(beats.size(), 3u);
  const SkRect caret = markRect(host, "caret");
  EXPECT_NEAR(caret.left(), beats[1].rect.left(), 0.01f);
  EXPECT_NEAR(caret.top(), beats[1].rect.top(), 0.01f);
  EXPECT_NEAR(caret.width(), beats[1].rect.width(), 0.01f);
  EXPECT_NEAR(caret.height(), beats[1].rect.height(), 0.01f);
  EXPECT_GT(caret.width(), 1.0f) << "the mark collapsed to nothing";

  // Its own placement longhand is read INSIDE that rect, which is the whole
  // difference from a slot: a 2 px caret pinned to the unit's leading edge
  // and hanging below it.
  Host pinned(400, 140);
  pinned.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA GAMMA", whiteStyle(24))
          .key("line")
          .mark(
              sel::word(1),
              box().key("caret").left(0).top(pct(100)).width(2).height(9).fill(
                  green()))));
  pinned.frame();
  const SkRect tick = markRect(pinned, "caret");
  EXPECT_NEAR(tick.left(), caret.left(), 0.01f);
  EXPECT_NEAR(tick.top(), caret.bottom(), 0.01f)
      << "pct(100) of the unit's rect is its bottom edge";
  EXPECT_FLOAT_EQ(tick.width(), 2.0f);
}

TEST(ComposeTextFx, MarkFollowsItsUnitWhenTheTextReflows) {
  // The rect is read off the placement, so a narrower box that pushes the
  // word onto the next line takes the mark with it — the reason to anchor a
  // caret rather than compute one.
  const auto placeAt = [](float width) {
    Host host(400, 200);
    host.composer.render(box().padding(10).child(
        text(u8"ALPHA BETA GAMMA DELTA", whiteStyle(24))
            .key("line")
            .width(width)
            .mark(sel::word(3), box().key("caret").fill(green()))));
    host.frame();
    return markRect(host, "caret");
  };
  const SkRect wide = placeAt(360);
  const SkRect narrow = placeAt(150);
  ASSERT_FALSE(wide.isEmpty());
  ASSERT_FALSE(narrow.isEmpty());
  EXPECT_GT(narrow.top(), wide.top())
      << "the word wrapped onto another line and the mark stayed behind";
}

TEST(ComposeTextFx, MarkStandsAtRestWhileACascadeDeviatesTheGlyphs) {
  // The rect is where the LAYOUT put the glyphs, not where a track has
  // thrown them: a deviation is per glyph and per track and several
  // compose, so there is no one place a moving unit "is". A mark that must
  // ride the motion reads beatsOf and drives its own transform.
  const auto placeAtProgress = [](float progress) {
    Host host(400, 200);
    host.composer.render(box().padding(10).child(
        text(u8"ALPHA BETA", whiteStyle(24))
            .key("line")
            .fx({.effect = fx::rise(40),
                 .stagger = {.eachMs = 0, .durationMs = 100},
                 .progress = progress})
            .mark(sel::word(1), box().key("caret").fill(green()))));
    host.frame();
    return markRect(host, "caret");
  };
  const SkRect early = placeAtProgress(0.0f);
  const SkRect settled = placeAtProgress(1.0f);
  ASSERT_FALSE(settled.isEmpty());
  EXPECT_NEAR(early.top(), settled.top(), 0.01f);
  EXPECT_NEAR(early.left(), settled.left(), 0.01f);
}

TEST(ComposeTextFx, MarkOnAPathRunStandsOnTheCurve) {
  // A path-laid run's marks stand on the CURVE the letters stand on: the
  // rect is the union of the advance boxes where the baseline placed them,
  // the same placement beatsOf reports — so a caret and a beat cannot
  // disagree on a ring any more than they can on a line. Resolved after
  // layout, because the curve resolves against the node's final box.
  Host host;
  host.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING IT GOES", whiteStyle(18))
          .key("ring")
          .width(180)
          .height(180)
          .onPath({.path = shapes::circle()})
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Word)})
          .mark(sel::word(2), box().key("caret").fill(green()))));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("ring", 0);
  ASSERT_GT(beats.size(), 2u);
  const SkRect caret = markRect(host, "caret");
  ASSERT_FALSE(caret.isEmpty()) << "the mark placed nothing on the curve";
  EXPECT_NEAR(caret.left(), beats[2].rect.left(), 0.01f);
  EXPECT_NEAR(caret.top(), beats[2].rect.top(), 0.01f);
  EXPECT_NEAR(caret.width(), beats[2].rect.width(), 0.01f);
  EXPECT_NEAR(caret.height(), beats[2].rect.height(), 0.01f);
  // And it IS the curved placement, not the straight flow line the run
  // does not use: the same content laid straight puts the word somewhere
  // else entirely.
  Host straight;
  straight.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING IT GOES", whiteStyle(18))
          .key("ring")
          .width(180)
          .height(180)
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Word)})
          .mark(sel::word(2), box().key("caret").fill(green()))));
  straight.frame();
  const SkRect flow = markRect(straight, "caret");
  EXPECT_TRUE(std::abs(caret.left() - flow.left()) > 1.0f ||
              std::abs(caret.top() - flow.top()) > 1.0f)
      << "the curved rect matched the straight one — the probe proves "
         "nothing at this size";
}

TEST(ComposeTextFx, MarkResolvingNothingPlacesNothing) {
  // The silent-no-op family's terms, with the warning that goes with them:
  // a style name no run carries selects nothing, and a mark on nothing must
  // draw nothing rather than land at the text node's origin.
  Host host(300, 140);
  host.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA", whiteStyle(24))
          .key("line")
          .mark(sel::style("nobody"),
                box().key("caret").width(30).height(30).fill(green()))));
  host.frame();
  EXPECT_TRUE(markRect(host, "caret").isEmpty())
      << "a mark on nothing took a box anyway";
  int greens = 0;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 300; ++x)
      if (host.pixel(x, y) == SK_ColorGREEN) ++greens;
  EXPECT_EQ(greens, 0) << "a mark on nothing drew something";
}

TEST(ComposeTextFx, MarkPrunesAndReResolvesWhenItMoves) {
  // A mark is a comparable selector plus the key of the child it anchors,
  // so a re-described identical mark must prune — and one pointed at a
  // different word must not, or the caret keeps the rect it had.
  Host host(400, 140);
  const auto describe = [](uint32_t word) {
    return box().padding(10).child(
        text(u8"ALPHA BETA GAMMA", whiteStyle(24))
            .key("line")
            .mark(sel::word(word), box().key("caret").fill(green())));
  };
  host.composer.render(describe(0));
  host.frame();
  const SkRect first = markRect(host, "caret");
  host.composer.render(describe(0));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged mark list did not prune";
  EXPECT_NEAR(markRect(host, "caret").left(), first.left(), 0.01f);

  host.composer.render(describe(2));
  host.frame();
  EXPECT_GT(markRect(host, "caret").left(), first.left() + 1.0f)
      << "the mark kept the rect the previous selector resolved";
}

TEST(ComposeTextFx, MarkIsNotASlotAndReservesNoSpaceInTheFlow) {
  // The distinction the header states: a slot's box is woven into the line
  // and the type after it starts further along; a mark is placed on a line
  // laid out as though it were not there.
  const auto widthOf = [](Element leaf) {
    Host host(400, 140);
    host.composer.render(box().padding(10).child(std::move(leaf).key("line")));
    host.frame();
    return host.composer.bounds("line").value_or(SkRect::MakeEmpty()).width();
  };
  const float bare = widthOf(text(u8"ALPHA BETA", whiteStyle(24)));
  const float marked =
      widthOf(text(u8"ALPHA BETA", whiteStyle(24))
                  .mark(sel::word(0), box().key("m").width(40).fill(green())));
  EXPECT_NEAR(marked, bare, 0.01f) << "the mark reserved space in the flow";
}
