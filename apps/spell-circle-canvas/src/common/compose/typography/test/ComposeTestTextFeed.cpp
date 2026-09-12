// A FEED OF TEXT ROWS: a typed-on row that paints live while its track runs
// and caches when it settles, and a structured row appended at its own
// constant cost.
//
// The text binary's share of the content suites, one file per subject.

#include <sigilcompose/core/Feed.h>

#include "DressedTypeProbes.h"

namespace {

/** Plain rows, white on the Host's black ground so ink reads as brightness. */
feed::TextOptions feedOptions(size_t visible, float size = 12.0f) {
  feed::TextOptions options;
  options.styles.base(whiteStyle(size));
  options.window.visible = visible;
  options.window.gap = 2.0f;
  return options;
}

}  // namespace

TEST(ComposeFeed, ATypedOnRowPaintsLiveThenCachesWhenItsTrackSettles) {
  // A glyph entrance on a feed row is affordable because it ENDS: while the
  // track's progress moves the row paints live, and once it settles the row
  // is a static leaf again, cached like every row above it. A track that
  // never settled would pin the whole window volatile.
  feed::TextRing ring;
  ring.append({toUtf8("daemon bound port 6042")});
  const feed::TextOptions options = feedOptions(8, 16.0f);
  auto typed = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .fx({.effect = fx::typeOn(),
             .stagger = {.eachMs = 12, .durationMs = 40},
             .progress = animate(motion::from(0.0f).to(1.0f),
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
  // A richer row — a severity stripe beside ONE weave::rich() leaf whose runs
  // speak named styles, entered by a track that settles — is a small
  // subtree rather than a single text node. That changes the CONSTANT in
  // the append price, never its shape: an append patches exactly the new
  // row's own nodes, however many rows the window holds, and the price
  // repeats append after append.
  sigil::weave::StyleSheet styles(whiteStyle(12));
  styles.set("ts", sigil::weave::Type{.size = 10});
  styles.set("tag", sigil::weave::Type{.size = 11});
  feed::Ring<StructuredRow> ring;
  for (int i = 0; i < 30; ++i)
    ring.append({"0412.50", "AUTH", "row " + std::to_string(i)});
  auto rowEl = [&](const StructuredRow& r) {
    auto line = sigil::weave::rich(styles.base())
                    .styles(styles)
                    .add(toUtf8(r.ts + "  "), "ts")
                    .add(toUtf8(r.tag + "  "), "tag")
                    .add(toUtf8(r.body));
    return box()
        .row()
        .gap(6)
        .child(box().width(3).height(10).fill(Fill::color(SkColors::kRed)))
        .child(text(std::move(line))
                   .fx({.effect = fx::typeOn(),
                        .stagger = {.eachMs = 5, .durationMs = 30},
                        .progress = animate(motion::from(0.0f).to(1.0f),
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
