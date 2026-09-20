// A feed of rows: what an append costs the rows already standing, the
// instance a surviving row keeps, the rows a window never mounts at all,
// and the stagger that delays only the ones that did mount.

#include <sigilcompose/core/Feed.h>

#include "support/CoreTestSupport.h"

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
    ring.append({"boot sequence line " + std::to_string(i)});
  const feed::TextOptions options = feedOptions(10);
  Host host(200, 400);
  auto describe = [&] {
    return box().padding(6).children({feed::feed(ring, options)});
  };
  host.composer.render(describe());
  host.frame();  // records the visible window

  ring.append({u8"intrusion detected"});
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);  // the new tail only
  host.frame();
  // Ancestor chain re-records plus the tail's own picture; the nine
  // surviving rows replay their cached pictures untouched. Stated against
  // the window size rather than against a number: re-recording the window
  // would cost at least one picture per row in it.
  const unsigned recordedForOneAppend = host.composer.stats().picturesRecorded;
  EXPECT_GT(recordedForOneAppend, 0u) << "the append recorded nothing at all";
  EXPECT_LT(recordedForOneAppend, 10u)
      << "the whole ten-row window re-recorded for one appended line";

  // The price is CONSTANT, which is the whole claim: a second append costs
  // exactly what the first did, and the retained tree does not grow.
  const size_t liveAfterFirst = host.composer.stats().instances;
  ring.append({u8"second intrusion"});
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, recordedForOneAppend)
      << "the second append cost more than the first";
  EXPECT_EQ(host.composer.stats().instances, liveAfterFirst)
      << "the window is bounded: one mount in, one unmount out";
}

TEST(ComposeFeed, ASurvivingRowKeepsItsInstanceRatherThanReentering) {
  // The sharper form of the same property. A row whose factory declares a
  // mount entrance re-runs that entrance whenever it MOUNTS, so a row that
  // was silently remounted by an append would flash back to nothing. Every
  // row here is fully lit before the append, and must still be after it.
  feed::TextRing ring;
  for (int i = 0; i < 4; ++i) ring.append({u8"row"});
  const feed::TextOptions options = feedOptions(6, 16.0f);
  auto lit = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .opacity(animate(motion::from(0.0f).to(1.0f),
                         {200ms, &choreograph::easeNone}));
  };
  Host host(160, 200);
  auto describe = [&] {
    return box().padding(4).children({feed::feed(ring, options.window, lit)});
  };

  host.composer.render(describe());
  host.frame(0.4);  // every mounted row has finished its entrance
  for (uint64_t sequence = 1; sequence <= 4; ++sequence) {
    const std::optional<SkRect> band =
        host.composer.bounds(feed::rowKey(sequence));
    ASSERT_TRUE(band.has_value()) << sequence;
    EXPECT_GT(brightestIn(host, *band), 150) << "row " << sequence;
  }

  ring.append({u8"tail"});
  host.composer.render(describe());
  host.frame(0.016);
  for (uint64_t sequence = 1; sequence <= 4; ++sequence) {
    const std::optional<SkRect> band =
        host.composer.bounds(feed::rowKey(sequence));
    ASSERT_TRUE(band.has_value()) << sequence;
    EXPECT_GT(brightestIn(host, *band), 150)
        << "row " << sequence << " re-entered: the append remounted it";
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
  for (int i = 0; i < 300; ++i) ring.append({"line " + std::to_string(i)});
  const feed::TextOptions options = feedOptions(8);
  Host host(200, 200);
  host.composer.render(box().children({feed::feed(ring, options)}));
  host.frame();

  EXPECT_FALSE(host.composer.bounds(feed::rowKey(1)).has_value())
      << "a row outside the window was mounted";
  EXPECT_FALSE(host.composer.bounds(feed::rowKey(292)).has_value());
  EXPECT_TRUE(host.composer.bounds(feed::rowKey(293)).has_value())
      << "the window is the NEWEST visible rows";
  EXPECT_TRUE(host.composer.bounds(feed::rowKey(300)).has_value());

  const size_t live = host.composer.stats().instances;
  for (int i = 0; i < 200; ++i) ring.append({"more " + std::to_string(i)});
  host.composer.render(box().children({feed::feed(ring, options)}));
  host.frame();
  EXPECT_EQ(host.composer.stats().instances, live)
      << "the retained tree grew with the ring instead of with the window";
}

TEST(ComposeFeed, TheEntranceStaggerDelaysOnlyTheRowsThatMount) {
  // Options::entrance is a motion::Spread — the same value the glyph
  // engine and staggerChildren speak — with a ROW as the beat. The initial
  // describe cascades the window; an append is the only new mount in its patch,
  // so it enters AT ONCE instead of inheriting a full window's worth of steps,
  // and no row already on screen re-enters.
  feed::TextRing ring;
  for (int i = 0; i < 3; ++i) ring.append({u8"row"});
  feed::TextOptions options = feedOptions(6, 16.0f);
  options.window.entrance = {.eachMs = 400};
  auto lit = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .opacity(animate(motion::from(0.0f).to(1.0f),
                         {200ms, &choreograph::easeNone}));
  };
  Host host(160, 200);
  auto describe = [&] {
    return box().padding(4).children({feed::feed(ring, options.window, lit)});
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
  ring.append({u8"tail"});
  host.composer.render(describe());
  host.frame(0.25);
  const std::optional<SkRect> r4 = host.composer.bounds(feed::rowKey(4));
  ASSERT_TRUE(r4.has_value());
  EXPECT_GT(brightestIn(host, *r4), 150)
      << "the appended row inherited the window's cascade";
}
