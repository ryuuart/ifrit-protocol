#include "ComposeTestSupport.h"

namespace {
/** A flow band's width law: 166 px at the start, 12 at the end. The shape
 *  `widthFn` used to need a `std::function` and a hand-set `widthMax` for,
 *  and the shape that proved the trap — Minard's retreat band runs 166 px
 *  at Kowno on a ribbon whose default widthStart/widthEnd declare 10. */
struct FlowLaw {
  float start = 166.0f, end = 12.0f;
  float across(float along) const { return start + (end - start) * along; }
  float max() const { return std::max(start, end); }
  bool operator==(const FlowLaw &) const = default;
};
} // namespace

TEST(ComposeBrushes, ARibbonsReachIsDERIVEDFromItsProfile) {
  // bleed() grows the recording cull. It could not look inside the deleted
  // `widthFn`, so a width function returning 166 on a ribbon whose
  // widthStart/widthEnd were the defaults declared 10 px of reach and the
  // band was silently CLIPPED — the failure read as a rendering bug rather
  // than a missing declaration, and the fix was a second field
  // (`widthMax`) that nobody had to set.
  //
  // On the profile seam `max()` is REQUIRED by the concept, so the number
  // cannot go unsaid: the trap is structurally impossible, not merely
  // documented. Asserted on bleed() directly — an earlier version of this
  // test tried to observe the cull through rendered pixels and measured
  // the same number either way, because what reaches the canvas also
  // depends on cache-mode decisions the test was not pinning.
  brush::Ribbon plain;
  plain.widthStart = 12.0f;
  plain.widthEnd = 4.0f;
  EXPECT_FLOAT_EQ(plain.bleed(), 12.0f);

  brush::Ribbon flow = plain;
  flow.width = Profile(FlowLaw{});
  EXPECT_FLOAT_EQ(flow.bleed(), 166.0f)
      << "the profile knows its own reach; nobody had to declare it";

  // A profile is not optional-with-a-fallback: once set it OWNS the reach,
  // so the fixed widths underneath it never inflate the cull either.
  flow.width = Profile(FlowLaw{2.0f, 2.0f});
  EXPECT_FLOAT_EQ(flow.bleed(), 2.0f);

  // And it participates in equality, so changing the law repatches.
  brush::Ribbon a = plain, b = plain;
  b.width = Profile(FlowLaw{});
  EXPECT_FALSE(a == b);
}

TEST(ComposeFx, EdgeGateOnAZeroMeasuredBoxRevealsRatherThanHides) {
  // A container of absolutely-positioned children measures zero, and a
  // half-plane built from an empty box is empty — so a FULL reveal hid the
  // whole subtree. A reveal at 1 must never hide anything.
  auto tree = [](bool withWipe) {
    Element outer = box().absolute().left(0).top(0); // no dims: measures 0
    outer.child(box()
                    .absolute()
                    .left(40)
                    .top(40)
                    .width(80)
                    .height(80)
                    .fill(Fill::color({1, 0, 0, 1})));
    if (withWipe)
      outer.mask(by::edge(90.0f, 1.0f));
    return box().child(std::move(outer));
  };
  auto ink = [](Host &host) {
    int n = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x)
        n += SkColorGetR(host.pixel(x, y)) > 180;
    return n;
  };

  Host plain(200, 200), wiped(200, 200);
  plain.composer.render(tree(false));
  plain.frame();
  wiped.composer.render(tree(true));
  wiped.frame();
  EXPECT_GT(ink(plain), 5000);
  EXPECT_EQ(ink(wiped), ink(plain)); // a full reveal changes nothing
}

TEST(ComposeText, RingWindingDecidesWhichWayTheGlyphsFace) {
  // Direction is not a detail on a text baseline. onPath orients to the
  // tangent, so a clockwise ring puts glyph-up radially OUTWARD
  // (Nightingale's 1858 plate) and a counter-clockwise one puts it INWARD
  // (Chevreul's 1864 limb) — both uniform engraver's conventions,
  // opposite in sign. Half of all ring inscriptions were hand-rolling an
  // OutlineFn because of a default nobody chose.
  //
  // Asserted as the two properties that matter and that do not depend on
  // knowing which quadrant Skia's addOval starts in: the directed
  // overload at kCW is EXACTLY the undirected one (a strict superset, not
  // a near-miss), and kCCW is observably different. My first three
  // attempts each asserted a position I had inferred rather than
  // measured, and each was wrong in a different way.
  auto render = [](std::function<SkPath(SkSize)> path) {
    auto host = std::make_unique<Host>(300, 300);
    host->composer.render(box().child(
        text(u8"RING INSCRIPTION", whiteStyle(30))
            .width(240).height(240).absolute().left(30).top(30)
            .onPath({.path = std::move(path), .at = 0.25f,
                     .align = TextPath::Align::Center, .offset = 0.0f})));
    host->frame();
    return host;
  };
  auto differing = [](Host &a, Host &b) {
    int n = 0;
    for (int y = 0; y < 300; ++y)
      for (int x = 0; x < 300; ++x)
        n += a.pixel(x, y) != b.pixel(x, y);
    return n;
  };
  auto inked = [](Host &h) {
    int n = 0;
    for (int y = 0; y < 300; ++y)
      for (int x = 0; x < 300; ++x)
        n += h.pixel(x, y) != SK_ColorBLACK;
    return n;
  };

  auto cw = render(shapes::circle(SkPathDirection::kCW));
  auto ccw = render(shapes::circle(SkPathDirection::kCCW));
  auto plain = render(shapes::circle());

  ASSERT_GT(inked(*cw), 300);
  ASSERT_GT(inked(*ccw), 300);
  // The winding is observable — the run faces the other way.
  EXPECT_GT(differing(*cw, *ccw), 500);
  // …and the directed overload's default IS the undirected one.
  EXPECT_EQ(differing(*cw, *plain), 0);
}

TEST(ComposeInstances, APerInstanceUVWindowAddressesInsideACell) {
  // The per-sprite texture rect existed all the way down —
  // drawSpriteAtlas reads tex[i] per sprite — and the only narrowing was
  // that a Pool could name a cell INDEX and never a RECT. So a strip of
  // artwork crawling behind a slit, or a sprite scrolling within its own
  // cell, was out of reach for want of a lane, not a draw path.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->filter(SkFilterMode::kNearest);
  // One cell, four vertical quarters: red, green, blue, white.
  atlas->cell(box().width(16).height(64).column()
                  .child(box().width(16).height(16).fill(Fill::color({1,0,0,1})))
                  .child(box().width(16).height(16).fill(Fill::color({0,1,0,1})))
                  .child(box().width(16).height(16).fill(Fill::color({0,0,1,1})))
                  .child(box().width(16).height(16).fill(Fill::color({1,1,1,1}))),
              {16, 64});

  auto quarterAt = [&](float top) {
    auto pool = std::make_shared<instancing::Pool>();
    pool->add({100, 100});
    pool->texWindows()[0] = SkRect::MakeXYWH(0.0f, top, 1.0f, 0.25f);
    pool->sizes()[0] = {1.0f, 0.25f}; // draw the window at its own aspect
    pool->commit();
    Host host(200, 200);
    host.composer.render(box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data)));
    host.frame();
    return host.pixel(100, 100);
  };

  // Sliding the window down the cell selects each band in turn — one
  // pool, one cell, four different sprites.
  const SkColor a = quarterAt(0.00f), b = quarterAt(0.25f);
  const SkColor c = quarterAt(0.50f), d = quarterAt(0.75f);
  EXPECT_GT(SkColorGetR(a), 180); EXPECT_LT(SkColorGetG(a), 80);
  EXPECT_GT(SkColorGetG(b), 180); EXPECT_LT(SkColorGetR(b), 80);
  EXPECT_GT(SkColorGetB(c), 180); EXPECT_LT(SkColorGetR(c), 80);
  EXPECT_GT(SkColorGetR(d), 180); EXPECT_GT(SkColorGetB(d), 180);

  // The lane is opt-in: a pool that never asks keeps the whole cell.
  auto plain = std::make_shared<instancing::Pool>();
  plain->add({100, 100});
  EXPECT_FALSE(plain->hasTexWindows());
}

TEST(ComposeText, AliasedTextHasHardEdges) {
  // Skia takes glyph edging from the FONT, never the paint, so
  // `paint.foreground.setAntiAlias(false)` is silently ignored on text
  // and there was no other way to ask. An X11 core font is 1-bit and a
  // 1995 desktop is ~100% 13 px UI type, so a period reconstruction had
  // to leave SigilWeave entirely — a raw kAlias SkFont in a decoration on
  // a hand-measured box, forfeiting shaping, bidi, fallback and
  // flowAround. This is the cheap half of "no bitmap-font path": one
  // field, not a face.
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
        partial += r > 15 && r < 240; // an antialiased edge pixel
      }
    return std::pair<int, int>{full, partial};
  };

  const auto soft = greys(false);
  const auto hard = greys(true);
  ASSERT_GT(soft.first, 200); // both actually drew
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
    shell.child(box()
                    .key("bar")
                    .absolute()
                    .left(20)
                    .top(20)
                    .width(40)
                    .height(40)
                    .fill(red()));
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
  // Layout runs inside draw(), so a query issued in the same update() as
  // the render() before it reads an unlaid tree — and used to hand back
  // left=0, top=0, width=NaN for EVERY key. A study lost an iteration
  // and a debug harness localising that.
  Host host(200, 200);
  host.composer.render(box().child(
      box().key("cell").absolute().left(10).top(10).width(50).height(50)));
  // No frame() yet: nothing has been laid out.
  const auto before = host.composer.bounds("cell");
  if (before)
    EXPECT_TRUE(before->isFinite()); // if it answers, the answer is real

  host.frame();
  const auto after = host.composer.bounds("cell");
  ASSERT_TRUE(after.has_value());
  EXPECT_TRUE(after->isFinite());
  EXPECT_FLOAT_EQ(after->width(), 50.0f);

  // A key that was never in the tree is still absent, not NaN.
  EXPECT_FALSE(host.composer.bounds("nope").has_value());
}

TEST(ComposeSlots, ASlotSurvivesItsContentCarryingTheSameKey) {
  // slot(name) sets node->key = name, and renderSlot used to resolve
  // through the same byKey map the whole tree shares. Give the slot's
  // CONTENT a root .key(name) and — since a child is indexed after its
  // parent, last writer wins — the content Box shadowed the slot, every
  // later renderSlot() returned silently, and the slot froze on its
  // first value. No warning. A study lost forty minutes and a printf to
  // it. Slots now have their own index.
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
  // The other half of §26b's trap. A slot's NAME is its key — one field —
  // so `.key()` on a slot renames the mount, renderSlot() on the original
  // name no-ops, and the symptom is a W x 0 layout rather than an error.
  // §26b diagnosed it from the renderSlot side; this is the warning at the
  // call that CAUSES it, where both names are still in hand.
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
  // Three studies hand-rolled the same forty lines to check a claim
  // against the pixels rather than the description that produced them.
  // The F16 default is the non-obvious half: a slit-scan study measuring
  // an intensity falloff had its outer streak at 1/120 of the apex,
  // which N32 quantises to two levels — an 8-bit read-back gives a
  // confident wrong exponent rather than an obviously broken one.
  const auto r = debug::rasterize(box().absolute().inset(0).fill(
                                      Fill::color({1.0f, 0.25f, 0.0f, 1})),
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
  const auto dim = debug::rasterize(
      box().absolute().inset(0).fill(Fill::color({faint, faint, faint, 1})),
      fonts(), {8, 8});
  ASSERT_TRUE(dim.valid());
  EXPECT_NEAR(dim.at(4, 4).fR, faint, faint * 0.25f);

  // Out of bounds is transparent rather than undefined.
  EXPECT_EQ(r.at(-1, 0).fA, 0.0f);
  EXPECT_EQ(r.at(0, 999).fA, 0.0f);
}

// ---------------------------------------------------------------------------
// The extraction layer: placement, the prelude, the console plate, and the
// proving primitive. See EXTRACT.md for the counts these answer.

#include <sigilcompose/Console.h>
#include <sigilcompose/Debug.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/Util.h>

TEST(ComposePlacement, RectIsTheLonghandAndPrunesIdentically) {
  // THE load-bearing test for Element::rect(). 287 sketch + 29 gallery sites
  // spell .absolute().left().top().width().height(), and nine studies wrote
  // the three-line helper themselves under four names. The whole safety
  // argument is that rect() describes the SAME node — so a re-describe that
  // swaps one spelling for the other must prune to zero patches, and the
  // pixels must not move.
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
  EXPECT_EQ(host.pixel(45, 65), SK_ColorRED);   // inside
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK); // above the top edge
  EXPECT_EQ(host.pixel(95, 65), SK_ColorBLACK); // right of the right edge

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
  host.composer.render(
      box().child(box().key("plate").rect(SkRect::MakeXYWH(41, 60, 50, 30))
                      .fill(red())));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "the patch counter is not live, so the zeroes above prove nothing";
  EXPECT_EQ(host.pixel(45, 55), SK_ColorBLACK);
  EXPECT_EQ(host.composer.bounds("plate")->fLeft, 41.0f);
}

TEST(ComposePlacement, AtPinsTheCornerAndLeavesTheNodeToSizeItself) {
  // The 187-site half of the longhand that carries no box: .left().top()
  // on a node that measures itself from its content.
  Host host(300, 200);
  auto longhand = [] {
    return box().child(
        text(u8"Wm", styleAt(20)).key("cap").absolute().left(Dim(30)).top(
            Dim(40)));
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
  // layout.absolute itself (Compose.cpp:95-129), so `.absolute()` before or
  // after one writes a bool that is already written. 1,316 calls across both
  // populations depend on this being exactly true — and on its NOT being
  // true for a node that pins no edge, which is the shape a blind sweep
  // would silently un-absolute.
  Host host(200, 200);

  auto withRedundant = [] {
    return box().child(
        box().key("p").absolute().left(Dim(30)).top(Dim(30)).width(20).height(
            20).fill(red()));
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
  flow.composer.render(box().row()
                           .child(box().width(60).height(20).fill(green()))
                           .child(box().key("q").absolute().width(20).height(20)
                                      .fill(red())));
  flow.frame();
  ASSERT_TRUE(flow.composer.bounds("q").has_value());
  EXPECT_FLOAT_EQ(flow.composer.bounds("q")->fLeft, 0.0f);

  flow.composer.render(box().row()
                           .child(box().width(60).height(20).fill(green()))
                           .child(box().key("q").width(20).height(20)
                                      .fill(red())));
  flow.frame();
  EXPECT_FLOAT_EQ(flow.composer.bounds("q")->fLeft, 60.0f)
      << "if this is still 0 then .absolute() alone is ALSO redundant and "
         "the sweep's predicate is over-cautious; if it is 60 the predicate "
         "is exactly right";
}

TEST(ComposeStudio, TheColourOpsAreTheBodiesTheCorpusWroteTwentyFourTimes) {
  // hex() is defined 24 times across 64 files under three names with
  // byte-identical bodies and no shared brief between the groups.
  constexpr SkColor4f rubric = studio::hex(0x8C2F22);
  static_assert(studio::hex(0xFFFFFF).fR == 1.0f, "must stay constexpr — the "
                                                  "palettes are constexpr");
  EXPECT_FLOAT_EQ(rubric.fR, 0x8C / 255.0f);
  EXPECT_FLOAT_EQ(rubric.fG, 0x2F / 255.0f);
  EXPECT_FLOAT_EQ(rubric.fB, 0x22 / 255.0f);
  EXPECT_FLOAT_EQ(rubric.fA, 1.0f);
  EXPECT_FLOAT_EQ(studio::hex(0x000000, 0.25f).fA, 0.25f);

  // alpha() and mul() are two names on purpose: 45 gallery sites override
  // alpha and 16 scale channels, and they are different operations.
  const SkColor4f faded = studio::alpha(rubric, 0.4f);
  EXPECT_FLOAT_EQ(faded.fR, rubric.fR);
  EXPECT_FLOAT_EQ(faded.fA, 0.4f);

  const SkColor4f lit = studio::mul(rubric, 1.5f);
  EXPECT_FLOAT_EQ(lit.fR, rubric.fR * 1.5f);
  EXPECT_FLOAT_EQ(lit.fA, rubric.fA) << "a < 0 must KEEP the source alpha";
  EXPECT_FLOAT_EQ(studio::mul(rubric, 0.5f, 0.2f).fA, 0.2f);
  // Deliberately unclamped: SkColor4f is float and >1 is meaningful.
  EXPECT_GT(studio::mul(SkColor4f{0.9f, 0.9f, 0.9f, 1}, 2.0f).fR, 1.0f);

  const SkColor4f half =
      studio::mix({0, 0, 0, 0}, {1.0f, 0.5f, 0.25f, 1.0f}, 0.5f);
  EXPECT_FLOAT_EQ(half.fR, 0.5f);
  EXPECT_FLOAT_EQ(half.fG, 0.25f);
  EXPECT_FLOAT_EQ(half.fA, 0.5f) << "mix() interpolates alpha too";

  // phase() wraps and never NaNs on a zero period.
  EXPECT_FLOAT_EQ(studio::phase(0.0, 4.0), 0.0f);
  EXPECT_FLOAT_EQ(studio::phase(3.0, 4.0), 0.75f);
  EXPECT_FLOAT_EQ(studio::phase(9.0, 4.0), 0.25f);
  EXPECT_FLOAT_EQ(studio::phase(1.0, 0.0), 0.0f);

  EXPECT_EQ(studio::fmt("%s %d %.2f", "n", 7, 1.5), "n 7 1.50");
  // Sizes the result rather than truncating into a fixed stack buffer, which
  // all seven hand-rolled versions did.
  EXPECT_EQ(studio::fmt("%s", std::string(2000, 'x').c_str()).size(), 2000u);
}

TEST(ComposeStudio, TypeCarriesWhatTheShippedPositionalHelperCouldNot) {
  // GalleryCore.h:35 already ships styleAt(size, SkColor) and sixteen
  // gallery scene headers wrote their own type() anyway — because they
  // needed a face, or tracking, or condensation, or a wght variation
  // (ScenesInventory.h:99), or slnt instead (ScenesSkillTree.h:122). This
  // test asserts exactly the fields a positional two-argument helper could
  // not reach; if it ever shrinks to size+colour, the extraction has failed
  // the same way its predecessor did.
  const sigil::weave::TextStyle s = studio::type({.size = 18.0f,
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
  EXPECT_TRUE(studio::type({.size = 12}).shaping.variations.empty());

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
  EXPECT_TRUE(s == byHand)
      << "studio::type() is not the six-statement core the corpus wrote";

  // And it actually lays out — a TextStyle that measures to nothing would
  // satisfy every field assertion above.
  const SkSize measured = measure(text(u8"Wm", studio::type({.size = 40})),
                                  fonts());
  EXPECT_GT(measured.width(), 10.0f);
  EXPECT_GT(measured.height(), 10.0f);
}

TEST(ComposeConsole, PanelIsTheBorderedPlateSevenStudiesBuiltByHand) {
  // Seven independent hand-builds differing only in count, axis and colour.
  // The plate does NOT place itself — it takes the rect, which is the
  // Layouts.h rule.
  Host host(240, 120);
  console::LineRing a, b;
  a.append(u8"alpha", 0);
  b.append(u8"beta", 0);

  console::Style style;
  style.text = studio::type({.size = 9, .color = {1, 1, 1, 1}});
  style.palette = {studio::type({.size = 9, .color = {1, 1, 1, 1}})};
  style.gap = 1.0f;

  auto plate = [&] {
    return box().child(console::panel({.rings = {&a, &b},
                                       .style = style,
                                       .paddingX = 10,
                                       .paddingY = 6,
                                       .gap = 8,
                                       .fill = Fill::color({0, 0, 0.5f, 1}),
                                       .border = green(),
                                       .divider = red()})
                           .key("plate")
                           .rect(SkRect::MakeXYWH(20, 20, 200, 80)));
  };
  host.composer.render(plate());
  host.frame();

  ASSERT_TRUE(host.composer.bounds("plate").has_value());
  EXPECT_EQ(*host.composer.bounds("plate"), SkRect::MakeXYWH(20, 20, 200, 80));
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
  EXPECT_EQ(redColumns, 1) << "a row panel of two rings has exactly one "
                              "divider between them";

  // Re-describing an unchanged plate prunes: the panel is composition over
  // the kernel and adds no volatility of its own.
  host.composer.render(plate());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);

  // A column panel puts the divider on the other axis, and the same call
  // site says so with one field.
  Host col(240, 120);
  col.composer.render(box().child(
      console::panel({.rings = {&a, &b},
                      .style = style,
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
    if (SkColorGetR(col.pixel(120, y)) > 180 && SkColorGetG(col.pixel(120, y)) < 80)
      ++redRows;
  EXPECT_EQ(redRows, 1);

  // monoStyle builds one TextStyle per palette colour off one face and size
  // — the block six studies wrote five times over. The palette entries mean
  // nothing to it, deliberately: the six plates do not agree on their order.
  const console::Style mono =
      console::monoStyle(nullptr, 10.5f, {1, 1, 1, 1},
                         {{0.5f, 0.5f, 0.5f, 1}, {0, 1, 0, 1}, {1, 0, 0, 1}});
  EXPECT_FLOAT_EQ(mono.text.shaping.fontSize, 10.5f);
  ASSERT_EQ(mono.palette.size(), 3u);
  EXPECT_FLOAT_EQ(mono.palette[1].shaping.fontSize, 10.5f);
  EXPECT_FLOAT_EQ(mono.palette[2].paint.foreground.getColor4f().fR, 1.0f);
  EXPECT_FLOAT_EQ(mono.palette[2].paint.foreground.getColor4f().fG, 0.0f);
}

TEST(ComposeConsole, VisibleLinesHasAHeightAndThreeRingsFitOnePanel) {
  // ROADMAP §21, filed by dunhuang_star_chart: three LineRings with hairline
  // dividers inside ONE fixed-height panel meant hand-tuning that height
  // against font size × line count. console::height() is that number, and
  // the shape below is the study's (consolePanel(), 9.2 px mono, gap 1,
  // visibleLines 12, padY 9, column gap 6, two 1 px dividers).
  console::Style st;
  st.text = studio::type({.size = 9.2f, .color = {1, 1, 1, 1}});
  st.gap = 1.0f;
  st.visibleLines = 12;

  const float rows = console::height(st, fonts());
  EXPECT_GT(rows, 12.0f * 9.2f) << "twelve 9.2 px rows plus eleven gaps";
  EXPECT_LT(rows, 12.0f * 9.2f * 3.0f);

  // The window CLAMPS: the console shows the last visibleLines, so asking
  // for more rows than the style shows is the same height.
  EXPECT_FLOAT_EQ(console::height(st, 400, fonts()), rows);
  EXPECT_LT(console::height(st, 6, fonts()), rows);

  // The snapshot()/measure() rule — those size by the root's CHILDREN, not
  // the root's own dims — is DEMONSTRATED, not assumed: console() returns a
  // panel that sets neither width nor height, so the shell box the
  // implementation wraps it in cannot change the answer.
  console::LineRing full;
  for (int i = 0; i < 20; ++i)
    full.append(u8"the ring outruns its window");
  EXPECT_FLOAT_EQ(measure(console::console(full, st), fonts()).height(), rows)
      << "the un-shelled spelling disagrees — console() grew its own dims";

  // And it is the height the console ACTUALLY takes when laid out: three
  // rings and two dividers in a column panel sized from the answer, with no
  // room to spare, must not shrink. (A wrong answer here is SILENT — flex
  // shrink absorbs the deficit and every ring loses rows, which is exactly
  // how the study lost its iteration.)
  console::LineRing a, b, c;
  for (int i = 0; i < 20; ++i) {
    a.append(u8"alpha");
    b.append(u8"beta");
    c.append(u8"gamma");
  }
  const float padY = 9.0f, gap = 6.0f, div = 1.0f;
  const float panelH = 2 * padY + 3 * rows + 4 * gap + 2 * div;

  Host host(320, (int)std::ceil(panelH) + 40);
  auto divider = [&] { return box().height(div).fill(green()); };
  host.composer.render(box().child(
      box()
          .key("panel")
          .padding(12.0f, padY)
          .column()
          .gap(gap)
          .child(console::console(a, st).key("ringA"))
          .child(divider())
          .child(console::console(b, st).key("ringB"))
          .child(divider())
          .child(console::console(c, st).key("ringC"))
          .rect(SkRect::MakeXYWH(10, 10, 300, panelH))));
  host.frame();

  ASSERT_TRUE(host.composer.bounds("panel").has_value());
  EXPECT_FLOAT_EQ(host.composer.bounds("panel")->height(), panelH);
  for (const char *k : {"ringA", "ringB", "ringC"}) {
    ASSERT_TRUE(host.composer.bounds(k).has_value()) << k;
    EXPECT_FLOAT_EQ(host.composer.bounds(k)->height(), rows)
        << k << " shrank: console::height() is not the laid-out height";
  }
  // The three rings tile the interior exactly — the last one ends on the
  // padding, with nothing clipped and nothing left over.
  EXPECT_FLOAT_EQ(host.composer.bounds("ringC")->bottom(),
                  10.0f + panelH - padY);
}

TEST(ComposeConsole, LineIsTheRowTheComponentBuildsAndCanBeGivenAnEntrance) {
  // ROADMAP §14: "console::console() admits no entrance choreography — it
  // builds its line Elements internally, so staggerChildren() on the
  // returned panel is a no-op". The mechanism is right: staggerChildren
  // delays the animate() mount transitions a child DECLARES, and a plain
  // text() declares none. console::line() is the entry's own second remedy.
  console::Style st;
  st.text = studio::type({.size = 20, .color = {1, 1, 1, 1}});
  st.palette = {studio::type({.size = 20, .color = {1, 0, 0, 1}})};
  st.gap = 4.0f;
  console::LineRing ring;
  ring.append(u8"AAAA");
  ring.append(u8"BBBB", 0);

  // (a) The hand-rebuild recipe in line()'s doc comment IS the component:
  //     a panel spelled column().gap(style.gap).clip() with one line() per
  //     row reconciles onto console() with zero patched nodes. Note what
  //     this can and cannot falsify — console() DELEGATES to line(), so
  //     breaking line() breaks both sides equally and leaves this green;
  //     what it pins is the PANEL spelling an author has to reproduce.
  //     (b) and (c) below are what hold line() itself.
  Host host(220, 90);
  host.composer.render(box().child(console::console(ring, st)));
  host.frame();
  auto byHand = [&](bool staggered) {
    auto p = box().column().gap(st.gap).clip();
    if (staggered)
      p.staggerChildren(400ms);
    for (const console::Line &l : ring.lines()) {
      Element row = console::line(l, st);
      if (staggered)
        row.opacity(animate(from(0.0f).to(1.0f),
                            {200ms, &choreograph::easeNone}));
      p.child(std::move(row));
    }
    return box().child(std::move(p));
  };
  host.composer.render(byHand(false));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "console()'s panel is not the spelling line()'s doc tells "
         "authors to write";

  // (b) It carries the con#<seq> key and honours Line::paletteIndex.
  ASSERT_TRUE(host.composer.bounds("con#1").has_value());
  ASSERT_TRUE(host.composer.bounds("con#2").has_value());
  const SkRect band = *host.composer.bounds("con#2");
  int redInk = 0;
  for (int y = (int)band.top(); y < (int)band.bottom(); ++y)
    for (int x = (int)band.left(); x < (int)band.right(); ++x) {
      const SkColor c = host.pixel(x, y);
      redInk += SkColorGetR(c) > 150 && SkColorGetG(c) < 80;
    }
  EXPECT_GT(redInk, 10) << "the paletteIndex row did not take palette[0]";

  // (c) And now the console types out: each row's OWN mount animation is
  //     what staggerChildren has to delay, so row 2 is still dark while
  //     row 1 has finished.
  auto brightest = [](Host &h, SkRect r) {
    int best = 0;
    for (int y = (int)r.top(); y < (int)r.bottom(); ++y)
      for (int x = (int)r.left(); x < (int)r.right(); ++x)
        best = std::max(best, (int)SkColorGetR(h.pixel(x, y)));
    return best;
  };
  Host typed(220, 90);
  typed.composer.render(byHand(true));
  typed.frame(0.25); // row 1's 200 ms is done; row 2 waits out its 400 ms
  const SkRect r1 = *typed.composer.bounds("con#1");
  const SkRect r2 = *typed.composer.bounds("con#2");
  EXPECT_GT(brightest(typed, r1), 150);
  EXPECT_LT(brightest(typed, r2), 40) << "the stagger did not delay row 2";
  typed.frame(0.5); // t = 0.75 — row 2 is 350 ms into its own 200 ms
  EXPECT_GT(brightest(typed, r2), 150);
}

TEST(ComposeDebug, CheckPrintsTheVerdictItComputed) {
  // Both proving plates in the corpus prove themselves on screen and neither
  // can be falsified by its own output: 53 fmt() calls producing strings
  // whose truth is not connected to the assertion. The fix is that the
  // printed verdict is DERIVED from the two values it prints —
  // minard_1869.cpp:2580 invented exactly this as a lambda.
  const debug::Check ok = debug::check("northern column", 422000 - 22000,
                                       400000);
  EXPECT_TRUE(ok.pass);
  EXPECT_NE(ok.line().find("400000"), std::string::npos);
  EXPECT_NE(ok.line().find("PASS"), std::string::npos);
  EXPECT_EQ(ok.line().find("FAIL"), std::string::npos);

  const debug::Check bad = debug::check("Berezina", 20000 + 30000, 49000);
  EXPECT_FALSE(bad.pass);
  EXPECT_NE(bad.line().find("FAIL want 50000"), std::string::npos)
      << "a failing check must print what it wanted, or the plate says "
         "nothing an author can act on: " << bad.line();

  // A long label is not truncated — sigillum_aemeth.cpp:1719 documents four
  // checks silently losing their units to a console column that clipped.
  const std::string wide =
      debug::check(std::string(80, 'L'), 1, 1).line(44);
  EXPECT_NE(wide.find(std::string(80, 'L')), std::string::npos);

  // Floats need a tolerance the STUDY chooses; there is no default.
  EXPECT_TRUE(debug::check("R", 257.972, 257.9715, 0.001).pass);
  EXPECT_FALSE(debug::check("R", 257.972, 257.9, 0.001).pass);
  EXPECT_NE(debug::check("R", 257.972, 257.9, 0.001).line().find("\xc2\xb1"),
            std::string::npos);

  EXPECT_TRUE(debug::check("winding", std::string_view("kCW"),
                           std::string_view("kCW")).pass);
  EXPECT_FALSE(debug::check("closed", false).pass);
  EXPECT_TRUE(debug::check("closed", true).pass);

  const debug::Check checks[] = {ok, bad, debug::check("x", true)};
  EXPECT_EQ(debug::failures(checks), 1);
  EXPECT_EQ(debug::failures(std::span<const debug::Check>{checks, 1}), 0);

  // report() lands the line in the ring under the palette index the VERDICT
  // chose — that link is the whole primitive.
  console::LineRing ring;
  debug::report(ring, ok, 1, 2);
  debug::report(ring, bad, 1, 2);
  ASSERT_EQ(ring.lines().size(), 2u);
  EXPECT_EQ(ring.lines()[0].paletteIndex, 1u);
  EXPECT_EQ(ring.lines()[1].paletteIndex, 2u);
  EXPECT_NE(ring.lines()[1].text.find(u8"FAIL"), std::u8string::npos);

  // And it renders: a plate whose checks never reach the screen is the
  // situation this replaces.
  Host host(200, 60);
  console::Style style;
  style.text = studio::type({.size = 9, .color = {1, 1, 1, 1}});
  style.palette = {studio::type({.size = 9, .color = {0, 1, 0, 1}}),
                   studio::type({.size = 9, .color = {1, 0, 0, 1}})};
  host.composer.render(box().fill(Fill::color({0, 0, 0, 1}))
                           .child(console::console(ring, style).at({4, 4})));
  host.frame();
  int inked = 0;
  for (int y = 0; y < 40; ++y)
    for (int x = 0; x < 200; ++x)
      if (host.pixel(x, y) != SK_ColorBLACK)
        ++inked;
  EXPECT_GT(inked, 50) << "the reported checks drew nothing";
}

TEST(ComposeUtil, CentredBuildsTheRectFifteenSitesComputeByHand) {
  const SkRect r = util::centred({100, 50}, 40, 20);
  EXPECT_EQ(r, SkRect::MakeXYWH(80, 40, 40, 20));
  EXPECT_EQ(util::centred({100, 50}, SkSize{40, 20}), r);

  // The point of it being a VALUE: rect() takes it, and so does everything
  // else that wants the same geometry.
  Host host(200, 200);
  host.composer.render(box().child(box().key("d").rect(r).fill(red())));
  host.frame();
  ASSERT_TRUE(host.composer.bounds("d").has_value());
  EXPECT_EQ(*host.composer.bounds("d"), r);
  EXPECT_EQ(host.pixel(100, 50), SK_ColorRED);
}

// ---------------------------------------------------------------------------
// The stroke grammar (ROADMAP §33 stage one): shape(), spans, band().

