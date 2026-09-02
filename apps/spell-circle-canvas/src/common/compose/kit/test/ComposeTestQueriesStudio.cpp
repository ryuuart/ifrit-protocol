#include "support/StudioTestSupport.h"

namespace {

/** A flow band's width law: wide at the start, narrow at the end, with the
 *  wide end far larger than any ribbon's default widths. That gap is the
 *  point — it is what makes an under-declared reach visible. */
struct FlowLaw {
  float start = 166.0f, end = 12.0f;
  float across(float along) const { return start + (end - start) * along; }
  float max() const { return std::max(start, end); }
  bool operator==(const FlowLaw&) const = default;
};

}  // namespace

TEST(ComposeBrushes, ARibbonsReachIsDERIVEDFromItsProfile) {
  // bleed() grows the recording cull, so it has to know how far the widest
  // part of a ribbon reaches. A width supplied as a callable cannot be
  // asked, so the declared reach falls back to the fixed widths underneath
  // it and a much wider band is silently CLIPPED — which reads as a
  // rendering bug rather than as a missing declaration.
  //
  // `max()` is REQUIRED by the Profile concept, so the number cannot go
  // unsaid: the trap is structurally impossible rather than merely
  // documented.
  //
  // Asserted on bleed() DIRECTLY. Observing the cull through rendered pixels
  // gives the same answer either way, because what reaches the canvas also
  // depends on cache-mode decisions this test is not pinning.
  brush::Ribbon plain;
  plain.widthStart = 12.0f;
  plain.widthEnd = 4.0f;
  EXPECT_FLOAT_EQ(plain.bleed(), 12.0f);

  brush::Ribbon flow = plain;
  flow.width = geometry::path::Profile(FlowLaw{});
  EXPECT_FLOAT_EQ(flow.bleed(), 166.0f)
      << "the profile knows its own reach; nobody had to declare it";

  // A profile is not optional-with-a-fallback: once set it OWNS the reach,
  // so the fixed widths underneath it never inflate the cull either.
  flow.width = geometry::path::Profile(FlowLaw{2.0f, 2.0f});
  EXPECT_FLOAT_EQ(flow.bleed(), 2.0f);

  // And it participates in equality, so changing the law repatches.
  brush::Ribbon a = plain, b = plain;
  b.width = geometry::path::Profile(FlowLaw{});
  EXPECT_FALSE(a == b);
}

TEST(ComposeFx, EdgeGateOnAZeroMeasuredBoxRevealsRatherThanHides) {
  // A container of absolutely-positioned children measures zero, and a
  // half-plane built from an empty box is empty — so a FULL reveal hid the
  // whole subtree. A reveal at 1 must never hide anything.
  auto tree = [](bool withWipe) {
    Element outer = box().absolute().left(0).top(0);  // no dims: measures 0
    outer.child(box().absolute().left(40).top(40).width(80).height(80).fill(
        Fill::color({1, 0, 0, 1})));
    if (withWipe) outer.mask(by::edge(90.0f, 1.0f));
    return box().child(std::move(outer));
  };
  auto ink = [](Host& host) {
    int n = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x) n += SkColorGetR(host.pixel(x, y)) > 180;
    return n;
  };

  Host plain(200, 200), wiped(200, 200);
  plain.composer.render(tree(false));
  plain.frame();
  wiped.composer.render(tree(true));
  wiped.frame();
  EXPECT_GT(ink(plain), 5000);
  EXPECT_EQ(ink(wiped), ink(plain));  // a full reveal changes nothing
}

TEST(ComposeText, RingWindingDecidesWhichWayTheGlyphsFace) {
  // Direction is not a detail on a text baseline. onPath orients to the
  // tangent, so a clockwise ring puts glyph-up radially OUTWARD
  // (Nightingale's 1858 plate) and a counter-clockwise one puts it INWARD
  // (Chevreul's 1864 limb) — both uniform engraver's conventions,
  // opposite in sign, which is why a ring inscription so often ends up
  // hand-rolling an OutlineFn over a default nobody chose.
  //
  // The two assertions are chosen so as NOT to depend on knowing which
  // quadrant Skia's addOval starts in: the directed overload at kCW is
  // EXACTLY the undirected one — a strict superset, not a near-miss — and
  // kCCW is observably different. Anything that names a specific expected
  // position is asserting an inference about Skia rather than a property of
  // this library.
  auto render = [](std::function<SkPath(SkSize)> path) {
    auto host = std::make_unique<Host>(300, 300);
    host->composer.render(
        box().child(text(u8"RING INSCRIPTION", whiteStyle(30))
                        .width(240)
                        .height(240)
                        .absolute()
                        .left(30)
                        .top(30)
                        .onPath({.path = std::move(path),
                                 .at = 0.25f,
                                 .align = TextPath::Align::Center,
                                 .offset = 0.0f})));
    host->frame();
    return host;
  };
  auto differing = [](Host& a, Host& b) {
    int n = 0;
    for (int y = 0; y < 300; ++y)
      for (int x = 0; x < 300; ++x) n += a.pixel(x, y) != b.pixel(x, y);
    return n;
  };
  auto inked = [](Host& h) {
    int n = 0;
    for (int y = 0; y < 300; ++y)
      for (int x = 0; x < 300; ++x) n += h.pixel(x, y) != SK_ColorBLACK;
    return n;
  };

  auto cw = render(geometry::shapes::circle(SkPathDirection::kCW));
  auto ccw = render(geometry::shapes::circle(SkPathDirection::kCCW));
  auto plain = render(geometry::shapes::circle());

  ASSERT_GT(inked(*cw), 300);
  ASSERT_GT(inked(*ccw), 300);
  // The winding is observable — the run faces the other way.
  EXPECT_GT(differing(*cw, *ccw), 500);
  // …and the directed overload's default IS the undirected one.
  EXPECT_EQ(differing(*cw, *plain), 0);
}

TEST(ComposeText, AliasedTextHasHardEdges) {
  // Skia takes glyph edging from the FONT, never the paint, so
  // `paint.foreground.setAntiAlias(false)` is silently ignored on text.
  // Without a field on the shaping style there is no way to ask for aliased
  // type at all, and the only recourse is a raw kAlias SkFont drawn inside a
  // decoration on a hand-measured box — which forfeits shaping, bidi,
  // fallback and flowAround. One field buys it back; this is not a
  // bitmap-font path, just an edging switch.
  auto greys = [](bool aliased) {
    auto style = whiteStyle(40);
    style.shaping.aliased = aliased;
    Host host(240, 100);
    host.composer.render(box().padding(12).child(text(u8"AVWM", style)));
    host.frame();
    int partial = 0, full = 0;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 240; ++x) {
        const int r = SkColorGetR(host.pixel(x, y));
        full += r > 250;
        partial += r > 15 && r < 240;  // an antialiased edge pixel
      }
    return std::pair<int, int>{full, partial};
  };

  const auto soft = greys(false);
  const auto hard = greys(true);
  ASSERT_GT(soft.first, 200);  // both actually drew
  ASSERT_GT(hard.first, 200);
  // Antialiased type is fringed with partial coverage; aliased type is
  // not — every pixel is on or off.
  EXPECT_GT(soft.second, 300);
  EXPECT_LT(hard.second, soft.second / 8);
}

TEST(ComposeQuery, AKeyedShellCanOptOutOfHitTesting) {
  // hitTest returns any keyed node whose box contains the point, whether
  // or not it paints. That is correct, and it means a keyed full-bleed
  // layout SHELL with no fill swallows every hit in the frame: a study's
  // four stat-bar groups were transparent containers carrying their
  // bars' keys, and every point on screen came back as the last of them.
  // Silent and total.
  auto tree = [](bool shellTestable) {
    Element shell = box().key("shell").absolute().inset(0);
    shell.hitTestable(shellTestable);
    shell.child(
        box().key("bar").absolute().left(20).top(20).width(40).height(40).fill(
            red()));
    return box().child(std::move(shell));
  };

  Host greedy(200, 200);
  greedy.composer.render(tree(true));
  greedy.frame();
  EXPECT_EQ(greedy.composer.hitTest({150, 150}), "shell");
  EXPECT_EQ(greedy.composer.hitTest({40, 40}), "bar");

  Host polite(200, 200);
  polite.composer.render(tree(false));
  polite.frame();
  // The shell no longer answers for empty space…
  EXPECT_FALSE(polite.composer.hitTest({150, 150}).has_value());
  // …and its CHILDREN are still tested, which is the whole distinction.
  EXPECT_EQ(polite.composer.hitTest({40, 40}), "bar");
}

TEST(ComposeQuery, BoundsIsAbsentRatherThanNaNBeforeLayout) {
  // Layout runs inside draw(), so a query issued in the same update() as the
  // render() before it is reading an UNLAID tree. It must answer "no value"
  // rather than a rect full of NaN: a NaN rect propagates silently into
  // whatever arithmetic the caller does with it.
  Host host(200, 200);
  host.composer.render(box().child(
      box().key("cell").absolute().left(10).top(10).width(50).height(50)));
  // No frame() yet: nothing has been laid out.
  const auto before = host.composer.bounds("cell");
  if (before)
    EXPECT_TRUE(before->isFinite());  // if it answers, the answer is real

  host.frame();
  const auto after = host.composer.bounds("cell");
  ASSERT_TRUE(after.has_value());
  EXPECT_TRUE(after->isFinite());
  EXPECT_FLOAT_EQ(after->width(), 50.0f);

  // A key that was never in the tree is still absent, not NaN.
  EXPECT_FALSE(host.composer.bounds("nope").has_value());
}

TEST(ComposeSlots, ASlotSurvivesItsContentCarryingTheSameKey) {
  // slot(name) sets node->key = name, so resolving renderSlot through the
  // shared key index makes a slot collidable with ordinary keys. Give the
  // slot's CONTENT a root .key(name) and — a child being indexed after its
  // parent, last writer wins — the content shadows the slot: every later
  // renderSlot() returns silently and the slot freezes on its first value,
  // with no warning. Slots therefore keep their own index.
  Host host(200, 200);
  host.composer.render(box().child(slot("readout").absolute().inset(0)));
  host.composer.renderSlot(
      "readout", box().key("readout").absolute().inset(0).fill(red()));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);

  // Control: with NO colliding key the second update has always worked.
  Host control(200, 200);
  control.composer.render(box().child(slot("r2").absolute().inset(0)));
  control.composer.renderSlot("r2", box().absolute().inset(0).fill(red()));
  control.frame();
  control.composer.renderSlot("r2", box().absolute().inset(0).fill(green()));
  control.frame();
  ASSERT_GT(SkColorGetG(control.pixel(100, 100)), 180)
      << "the plain slot path is broken, not the collision fix";

  // The second update must land. Before the fix this returned silently.
  host.composer.renderSlot(
      "readout", box().key("readout").absolute().inset(0).fill(green()));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(100, 100)), 180);
  EXPECT_LT(SkColorGetR(host.pixel(100, 100)), 80);

  // And a slot's name still answers bounds(), so the two indexes coexist.
  EXPECT_TRUE(host.composer.bounds("readout").has_value());
}

TEST(ComposeSlots, KeyOnASlotRenamesItAndSaysSoOnce) {
  // A slot's NAME is its key — one field — so `.key()` on a slot RENAMES
  // the mount. renderSlot() on the original name then no-ops and the symptom
  // is a zero-height layout rather than an error. The warning has to fire at
  // the call that causes it, which is the only place both names are still in
  // hand.
  ::testing::internal::CaptureStderr();
  {
    Host quiet(200, 200);
    quiet.composer.render(box().child(slot("gauges").absolute().inset(0)));
    quiet.composer.renderSlot("gauges", box().absolute().inset(0).fill(red()));
    quiet.frame();
    EXPECT_GT(SkColorGetR(quiet.pixel(100, 100)), 180);
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "naming a slot once must be silent";

  ::testing::internal::CaptureStderr();
  Element renamed = slot("dials").key("panel");
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("dials"), std::string::npos) << log;
  EXPECT_NE(log.find("panel"), std::string::npos) << log;

  // Once per rename, not per frame — the same call in a describe loop
  // must not print sixty lines a second.
  ::testing::internal::CaptureStderr();
  (void)slot("dials").key("panel");
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");

  // And the warning is telling the truth: the mount answers to the NEW
  // name only.
  Host host(200, 200);
  host.composer.render(box().child(renamed.absolute().inset(0)));
  host.composer.renderSlot("panel", box().absolute().inset(0).fill(green()));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(100, 100)), 180);
}

TEST(ComposeDebug, RasterizeReadsBackWhatWasDrawn) {
  // Checking a claim against PIXELS rather than against the description that
  // produced them otherwise means hand-rolling a surface, a draw and a
  // read-back at every call site.
  //
  // The F16 default is the non-obvious half. Measuring a falloff whose tail
  // sits at a small fraction of its peak, an 8-bit read-back quantises that
  // tail to a couple of levels — which yields a confident wrong exponent
  // rather than an obviously broken one.
  const auto r = test::rasterize(
      box().absolute().inset(0).fill(Fill::color({1.0f, 0.25f, 0.0f, 1})),
      fonts(), {32, 32});
  ASSERT_TRUE(r.valid());
  EXPECT_EQ(r.width(), 32);
  const SkColor4f c = r.at(16, 16);
  EXPECT_NEAR(c.fR, 1.0f, 0.02f);
  EXPECT_NEAR(c.fG, 0.25f, 0.02f);
  EXPECT_NEAR(c.fB, 0.0f, 0.02f);

  // The point of F16: a ratio far below 8-bit resolution survives.
  // 1/500 of full scale is 0.51 of a 255-step — it quantises to 0 or 1
  // in N32 and is measurable in float.
  const float faint = 1.0f / 500.0f;
  const auto dim = test::rasterize(
      box().absolute().inset(0).fill(Fill::color({faint, faint, faint, 1})),
      fonts(), {8, 8});
  ASSERT_TRUE(dim.valid());
  EXPECT_NEAR(dim.at(4, 4).fR, faint, faint * 0.25f);

  // Out of bounds is transparent rather than undefined.
  EXPECT_EQ(r.at(-1, 0).fA, 0.0f);
  EXPECT_EQ(r.at(0, 999).fA, 0.0f);
}

// ---------------------------------------------------------------------------
// The extraction layer: placement, the prelude, the feed plate, and the
// proving primitive.

#include <sigilcompose/core/Feed.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmotion/Animation.h>

TEST(ComposePlacement, RectIsTheLonghandAndPrunesIdentically) {
  // rect() is sugar for .absolute().left().top().width().height(), and the
  // entire safety argument is that it describes the SAME node. So a
  // re-describe that swaps one spelling for the other must prune to zero
  // patches and must not move a pixel — anything less and the two spellings
  // are different nodes wearing one name.
  Host host(200, 200);
  const SkRect r = SkRect::MakeXYWH(40, 60, 50, 30);

  auto longhand = [&] {
    return box().child(box()
                           .key("plate")
                           .absolute()
                           .left(Dim(40))
                           .top(Dim(60))
                           .width(Dim(50))
                           .height(Dim(30))
                           .fill(red()));
  };
  auto terse = [&] {
    return box().child(box().key("plate").rect(r).fill(red()));
  };

  host.composer.render(longhand());
  host.frame();
  const auto boundsLonghand = host.composer.bounds("plate");
  ASSERT_TRUE(boundsLonghand.has_value());
  EXPECT_EQ(*boundsLonghand, r);
  EXPECT_EQ(host.pixel(45, 65), SK_ColorRED);    // inside
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);  // above the top edge
  EXPECT_EQ(host.pixel(95, 65), SK_ColorBLACK);  // right of the right edge

  // Re-describe with rect(). Equal props => the reconciler prunes it.
  host.composer.render(terse());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "rect() described a node the reconciler considered DIFFERENT from "
         "the longhand — it is not writing the same LayoutProps fields";
  EXPECT_EQ(host.composer.bounds("plate"), boundsLonghand);
  EXPECT_EQ(host.pixel(45, 65), SK_ColorRED);
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);

  // And back the other way, so neither direction is the privileged one.
  host.composer.render(longhand());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);

  // NEGATIVE CONTROL — without this the two assertions above pass on a
  // composer that never patches anything, which is exactly the vacuous
  // shape this program keeps finding. A different rect MUST patch.
  host.composer.render(box().child(
      box().key("plate").rect(SkRect::MakeXYWH(41, 60, 50, 30)).fill(red())));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "the patch counter is not live, so the zeroes above prove nothing";
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);
  EXPECT_EQ(require(host.composer.bounds("plate")).fLeft, 41.0f);
}

TEST(ComposePlacement, AtPinsTheCornerAndLeavesTheNodeToSizeItself) {
  // The 187-site half of the longhand that carries no box: .left().top()
  // on a node that measures itself from its content.
  Host host(300, 200);
  auto longhand = [] {
    return box().child(text(u8"Wm", styleAt(20))
                           .key("cap")
                           .absolute()
                           .left(Dim(30))
                           .top(Dim(40)));
  };
  auto terse = [] {
    return box().child(text(u8"Wm", styleAt(20)).key("cap").at({30, 40}));
  };

  host.composer.render(longhand());
  host.frame();
  const auto measured = host.composer.bounds("cap");
  ASSERT_TRUE(measured.has_value());
  EXPECT_FLOAT_EQ(measured->fLeft, 30.0f);
  EXPECT_FLOAT_EQ(measured->fTop, 40.0f);
  // Sized by its content, not by the caller: this is what rect() cannot do
  // and is why at() exists separately (ScenesPersona.h:438-447 is the
  // gallery case that rect() cannot serve at all).
  EXPECT_GT(measured->width(), 1.0f);

  host.composer.render(terse());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "at() is not left().top()";
  EXPECT_EQ(host.composer.bounds("cap"), measured);

  // Negative control, as above.
  host.composer.render(
      box().child(text(u8"Wm", styleAt(20)).key("cap").at({31, 40})));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeLayout, AbsoluteBeforeAnEdgeSetterIsDeadButAloneItIsNot) {
  // The corpus-wide subtraction rests on one claim: every edge setter sets
  // layout.absolute itself (Element.cpp), so `.absolute()` before or
  // after one writes a bool that is already written. 1,316 calls across both
  // populations depend on this being exactly true — and on its NOT being
  // true for a node that pins no edge, which is the shape a blind sweep
  // would silently un-absolute.
  Host host(200, 200);

  auto withRedundant = [] {
    return box().child(box()
                           .key("p")
                           .absolute()
                           .left(Dim(30))
                           .top(Dim(30))
                           .width(20)
                           .height(20)
                           .fill(red()));
  };
  auto without = [] {
    return box().child(
        box().key("p").left(Dim(30)).top(Dim(30)).width(20).height(20).fill(
            red()));
  };
  host.composer.render(withRedundant());
  host.frame();
  const auto pinned = host.composer.bounds("p");
  ASSERT_TRUE(pinned.has_value());
  EXPECT_EQ(*pinned, SkRect::MakeXYWH(30, 30, 20, 20));

  host.composer.render(without());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "dropping a redundant .absolute() changed the description";
  EXPECT_EQ(host.composer.bounds("p"), pinned);
  EXPECT_EQ(host.pixel(35, 35), SK_ColorRED);

  // The 48-call shape the sweep must NOT touch: absolute with a size and no
  // pinned edge. Here .absolute() is the only thing taking it out of flow,
  // so removing it moves the node behind its sibling.
  Host flow(200, 200);
  flow.composer.render(
      box()
          .row()
          .child(box().width(60).height(20).fill(green()))
          .child(box().key("q").absolute().width(20).height(20).fill(red())));
  flow.frame();
  ASSERT_TRUE(flow.composer.bounds("q").has_value());
  EXPECT_FLOAT_EQ(require(flow.composer.bounds("q")).fLeft, 0.0f);

  flow.composer.render(
      box()
          .row()
          .child(box().width(60).height(20).fill(green()))
          .child(box().key("q").width(20).height(20).fill(red())));
  flow.frame();
  EXPECT_FLOAT_EQ(require(flow.composer.bounds("q")).fLeft, 60.0f)
      << "if this is still 0 then .absolute() alone is ALSO redundant and "
         "the sweep's predicate is over-cautious; if it is 60 the predicate "
         "is exactly right";
}

TEST(ComposeStudio, TheColourOpsAreTheBodiesTheCorpusWroteTwentyFourTimes) {
  // hex() is defined 24 times across 64 files under three names with
  // byte-identical bodies and no shared brief between the groups.
  constexpr SkColor4f rubric = hex(0x8C2F22);
  static_assert(hex(0xFFFFFF).fR == 1.0f,
                "must stay constexpr — the "
                "palettes are constexpr");
  EXPECT_FLOAT_EQ(rubric.fR, 0x8C / 255.0f);
  EXPECT_FLOAT_EQ(rubric.fG, 0x2F / 255.0f);
  EXPECT_FLOAT_EQ(rubric.fB, 0x22 / 255.0f);
  EXPECT_FLOAT_EQ(rubric.fA, 1.0f);
  EXPECT_FLOAT_EQ(hex(0x000000, 0.25f).fA, 0.25f);

  // alpha() and scaleRgb() are two names on purpose: 45 gallery sites override
  // alpha and 16 scale channels, and they are different operations.
  const SkColor4f faded = alpha(rubric, 0.4f);
  EXPECT_FLOAT_EQ(faded.fR, rubric.fR);
  EXPECT_FLOAT_EQ(faded.fA, 0.4f);

  const SkColor4f lit = scaleRgb(rubric, 1.5f);
  EXPECT_FLOAT_EQ(lit.fR, rubric.fR * 1.5f);
  EXPECT_FLOAT_EQ(lit.fA, rubric.fA) << "a < 0 must KEEP the source alpha";
  EXPECT_FLOAT_EQ(scaleRgb(rubric, 0.5f, 0.2f).fA, 0.2f);
  // Deliberately unclamped: SkColor4f is float and >1 is meaningful.
  EXPECT_GT(scaleRgb(SkColor4f{0.9f, 0.9f, 0.9f, 1}, 2.0f).fR, 1.0f);

  const SkColor4f half = mix({0, 0, 0, 0}, {1.0f, 0.5f, 0.25f, 1.0f}, 0.5f);
  EXPECT_FLOAT_EQ(half.fR, 0.5f);
  EXPECT_FLOAT_EQ(half.fG, 0.25f);
  EXPECT_FLOAT_EQ(half.fA, 0.5f) << "mix() interpolates alpha too";

  // phase() wraps and never NaNs on a zero period.
  EXPECT_FLOAT_EQ(motion::phase(0.0, 4.0), 0.0f);
  EXPECT_FLOAT_EQ(motion::phase(3.0, 4.0), 0.75f);
  EXPECT_FLOAT_EQ(motion::phase(9.0, 4.0), 0.25f);
  EXPECT_FLOAT_EQ(motion::phase(1.0, 0.0), 0.0f);
}

TEST(ComposeStudio, TypeCarriesWhatTheShippedPositionalHelperCouldNot) {
  // GalleryCore.h:35 already ships styleAt(size, SkColor) and sixteen
  // gallery scene headers wrote their own type() anyway — because they
  // needed a face, or tracking, or condensation, or a wght variation
  // (ScenesInventory.h:99), or slnt instead (ScenesSkillTree.h:122). This
  // test asserts exactly the fields a positional two-argument helper could
  // not reach; if it ever shrinks to size+colour, the extraction has failed
  // the same way its predecessor did.
  const sigil::weave::TextStyle s =
      weave::textStyle({.size = 18.0f,
                        .color = {0.2f, 0.4f, 0.6f, 1},
                        .track = 1.25f,
                        .condense = 0.94f,
                        .weight = 650.0f,
                        .slant = -10.0f,
                        .aliased = true});
  EXPECT_FLOAT_EQ(s.shaping.fontSize, 18.0f);
  EXPECT_FLOAT_EQ(s.shaping.letterSpacing, 1.25f);
  EXPECT_FLOAT_EQ(s.shaping.scaleX, 0.94f);
  EXPECT_TRUE(s.shaping.aliased);
  ASSERT_EQ(s.shaping.variations.size(), 2u);
  EXPECT_EQ(std::string(s.shaping.variations[0].tag, 4), "wght");
  EXPECT_FLOAT_EQ(s.shaping.variations[0].value, 650.0f);
  EXPECT_EQ(std::string(s.shaping.variations[1].tag, 4), "slnt");
  const SkColor4f c = s.paint.foreground.getColor4f();
  EXPECT_FLOAT_EQ(c.fB, 0.6f);

  // Defaults leave design space alone — an unvaried style must not carry a
  // wght entry, or every default style occupies its own varied-face memo.
  EXPECT_TRUE(weave::textStyle({.size = 12}).shaping.variations.empty());

  // It equals a hand-built style, so a study migrating to it prunes.
  sigil::weave::TextStyle byHand;
  byHand.shaping.fontSize = 18.0f;
  byHand.shaping.letterSpacing = 1.25f;
  byHand.shaping.scaleX = 0.94f;
  byHand.shaping.aliased = true;
  byHand.paint.foreground.setColor4f({0.2f, 0.4f, 0.6f, 1}, nullptr);
  byHand.paint.foreground.setAntiAlias(true);
  byHand.variation("wght", 650.0f);
  byHand.variation("slnt", -10.0f);
  EXPECT_TRUE(s == byHand) << "type() does not build the TextStyle it declares";

  // And it actually lays out — a TextStyle that measures to nothing would
  // satisfy every field assertion above.
  const SkSize measured =
      measure(text(u8"Wm", weave::textStyle({.size = 40})), fonts());
  EXPECT_GT(measured.width(), 10.0f);
  EXPECT_GT(measured.height(), 10.0f);
}

TEST(ComposeFeed, PlateIsTheBorderedStripSevenStudiesBuiltByHand) {
  // A bordered plate holding N feeds with dividers between them. The plate
  // does NOT place itself: it takes the rect, following the same rule every
  // layout scheme does.
  Host host(240, 120);
  feed::TextRing a, b;
  a.append({u8"alpha"});
  b.append({u8"beta"});

  feed::TextOptions style;
  style.styles.base(weave::textStyle({.size = 9, .color = {1, 1, 1, 1}}));
  style.window.gap = 1.0f;

  auto strip = [&] {
    return box().child(
        kit::plate({.columns = {feed::feed(a, style), feed::feed(b, style)},
                    .paddingX = 10,
                    .paddingY = 6,
                    .gap = 8,
                    .fill = Fill::color({0, 0, 0.5f, 1}),
                    .border = green(),
                    .divider = red()})
            .key("plate")
            .rect(SkRect::MakeXYWH(20, 20, 200, 80)));
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
  col.composer.render(box().child(
      kit::plate({.columns = {feed::feed(a, style), feed::feed(b, style)},
                  .column = true,
                  .paddingX = 10,
                  .paddingY = 6,
                  .gap = 8,
                  .fill = Fill::color({0, 0, 0.5f, 1}),
                  .divider = red()})
          .rect(SkRect::MakeXYWH(20, 20, 200, 80))));
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
  const sigil::weave::StyleSet mono =
      kit::tinted(nullptr, 10.5f, {1, 1, 1, 1},
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
  st.styles.base(weave::textStyle({.size = 9.2f, .color = {1, 1, 1, 1}}));
  st.window.gap = 1.0f;
  st.window.visible = 12;

  const float rows = feed::height(st, fonts());
  EXPECT_GT(rows, 12.0f * 9.2f) << "twelve 9.2 px rows plus eleven gaps";
  EXPECT_LT(rows, 12.0f * 9.2f * 3.0f);

  // The window CLAMPS: the feed shows the newest visible rows, so asking
  // for more rows than the options show is the same height.
  EXPECT_FLOAT_EQ(feed::height(st, 400, fonts()), rows);
  EXPECT_LT(feed::height(st, 6, fonts()), rows);

  // The snapshot()/measure() rule — those size by the root's CHILDREN, not
  // the root's own dims — is DEMONSTRATED, not assumed: feed() returns a
  // column that sets neither width nor height, so the shell box the
  // implementation wraps it in cannot change the answer.
  feed::TextRing full;
  for (int i = 0; i < 20; ++i) full.append({u8"the ring outruns its window"});
  EXPECT_FLOAT_EQ(measure(feed::feed(full, st), fonts()).height(), rows)
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
  host.composer.render(
      box().child(box()
                      .key("panel")
                      .padding(12.0f, padY)
                      .column()
                      .gap(gap)
                      .child(feed::feed(a, st).key("feedA"))
                      .child(divider())
                      .child(feed::feed(b, st).key("feedB"))
                      .child(divider())
                      .child(feed::feed(c, st).key("feedC"))
                      .rect(SkRect::MakeXYWH(10, 10, 300, panelH))));
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
  st.styles.base(weave::textStyle({.size = 20, .color = {1, 1, 1, 1}}))
      .set("alert", weave::textStyle({.size = 20, .color = {1, 0, 0, 1}}));
  st.window.gap = 4.0f;
  feed::TextRing ring;
  ring.append({u8"AAAA"});
  ring.append({u8"BBBB", "alert"});

  Host host(220, 90);
  host.composer.render(box().child(feed::feed(ring, st)));
  host.frame();
  auto byHand = [&](bool staggered) {
    auto column = box().column().gap(st.window.gap).clip();
    if (staggered) column.staggerChildren(400ms);
    for (const feed::Row<feed::TextRow>& r : ring.rows()) {
      Element row = feed::textRow(r.value, st.styles);
      row.key(feed::rowKey(r.seq));
      if (staggered)
        row.opacity(animate(motion::from(0.0f).to(1.0f),
                            {200ms, &choreograph::easeNone}));
      column.child(std::move(row));
    }
    return box().child(std::move(column));
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

TEST(ComposeDebug, CheckPrintsTheVerdictItComputed) {
  // A figure that prints its own verification is worthless if the printed
  // verdict is written by hand next to the numbers: the string and the claim
  // are then unconnected, and the plate cannot be falsified by its own
  // output. test::check() derives the verdict FROM the two values it
  // prints, so a wrong number changes the word beside it.
  const test::Check ok = test::check("northern column", 422000 - 22000, 400000);
  EXPECT_TRUE(ok.pass);
  EXPECT_NE(ok.line().find("400000"), std::string::npos);
  EXPECT_NE(ok.line().find("PASS"), std::string::npos);
  EXPECT_EQ(ok.line().find("FAIL"), std::string::npos);

  const test::Check bad = test::check("Berezina", 20000 + 30000, 49000);
  EXPECT_FALSE(bad.pass);
  EXPECT_NE(bad.line().find("FAIL want 50000"), std::string::npos)
      << "a failing check must print what it wanted, or the plate says "
         "nothing an author can act on: "
      << bad.line();

  // A long label is not truncated — sigillum_aemeth.cpp:1719 documents four
  // checks silently losing their units to a feed column that clipped.
  const std::string wide = test::check(std::string(80, 'L'), 1, 1).line(44);
  EXPECT_NE(wide.find(std::string(80, 'L')), std::string::npos);

  // Floats need a tolerance the STUDY chooses; there is no default.
  EXPECT_TRUE(test::check("R", 257.972, 257.9715, 0.001).pass);
  EXPECT_FALSE(test::check("R", 257.972, 257.9, 0.001).pass);
  EXPECT_NE(test::check("R", 257.972, 257.9, 0.001).line().find("\xc2\xb1"),
            std::string::npos);

  EXPECT_TRUE(
      test::check("winding", std::string_view("kCW"), std::string_view("kCW"))
          .pass);
  EXPECT_FALSE(test::check("closed", false).pass);
  EXPECT_TRUE(test::check("closed", true).pass);

  const test::Check checks[] = {ok, bad, test::check("x", true)};
  EXPECT_EQ(test::failures(checks), 1);
  EXPECT_EQ(test::failures(std::span<const test::Check>{checks, 1}), 0);

  // report() lands the line in the feed under the style name the VERDICT
  // chose — that link is the whole primitive.
  feed::TextRing ring;
  test::report(ring, ok, "pass", "fail");
  test::report(ring, bad, "pass", "fail");
  ASSERT_EQ(ring.size(), 2u);
  EXPECT_EQ(ring.rows()[0].value.style, "pass");
  EXPECT_EQ(ring.rows()[1].value.style, "fail");
  EXPECT_NE(ring.rows()[1].value.text.find(u8"FAIL"), std::u8string::npos);

  // And it renders: a plate whose checks never reach the screen is the
  // situation this replaces.
  Host host(200, 60);
  feed::TextOptions style;
  style.styles = kit::tinted(nullptr, 9, {1, 1, 1, 1},
                             {{"pass", {0, 1, 0, 1}}, {"fail", {1, 0, 0, 1}}});
  host.composer.render(box()
                           .fill(Fill::color({0, 0, 0, 1}))
                           .child(feed::feed(ring, style).at({4, 4})));
  host.frame();
  int inked = 0;
  for (int y = 0; y < 40; ++y)
    for (int x = 0; x < 200; ++x)
      if (host.pixel(x, y) != SK_ColorBLACK) ++inked;
  EXPECT_GT(inked, 50) << "the reported checks drew nothing";
}

TEST(ComposeUtil, CentredBuildsTheRectFifteenSitesComputeByHand) {
  const SkRect r = geometry::path::centred({100, 50}, 40, 20);
  EXPECT_EQ(r, SkRect::MakeXYWH(80, 40, 40, 20));
  EXPECT_EQ(geometry::path::centred({100, 50}, SkSize{40, 20}), r);

  // The point of it being a VALUE: rect() takes it, and so does everything
  // else that wants the same geometry.
  Host host(200, 200);
  host.composer.render(box().child(box().key("d").rect(r).fill(red())));
  host.frame();
  ASSERT_TRUE(host.composer.bounds("d").has_value());
  EXPECT_EQ(require(host.composer.bounds("d")), r);
  EXPECT_EQ(host.pixel(100, 50), SK_ColorRED);
}

// ---------------------------------------------------------------------------
// The stroke grammar: shape(), spans, band().
