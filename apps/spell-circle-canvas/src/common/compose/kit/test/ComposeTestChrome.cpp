// The bevel: where each of the four edges lands and in which tone, what
// sunken does to that, where a doubled ring's gap is, how the two mitres
// hand the off corners over, the stipple's lattice, and what each era's
// token set resolves to.

#include <include/core/SkColor.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilmaterial/pattern/Patterns.h>

#include "support/Rows.h"
#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;
namespace styles = sigil::compose::styles;
namespace path = sigil::geometry::path;

namespace {

// The tones are the three primaries so a pixel names the band it came
// from with no tolerance anywhere: red is the lit edge, blue the shaded
// one, green the face they are drawn on, white and black the inner ring.
constexpr sigil::material::Color kLit{1, 0, 0, 1};
constexpr sigil::material::Color kShade{0, 0, 1, 1};
constexpr sigil::material::Color kFace{0, 1, 0, 1};

constexpr int kX = 20, kY = 20, kW = 100, kH = 60;

/** A 100 × 60 face at (20, 20) in a 140 × 100 host, wearing @p b as an
 *  overlay — over the fill, under nothing, which is the slot a bevel
 *  wants. */
Element panel(const kit::Bevel& b) {
  return box().padding(kX).children(
      {box().width(kW).height(kH).fill(kFace).overlay(b)});
}

/** The pixel at (x, y) of the FACE's own coordinates. */
SkColor at(Host& host, int x, int y) { return host.pixel(kX + x, kY + y); }

kit::Bevel plain() {
  kit::Bevel b;
  b.light = kLit;
  b.shadow = kShade;
  b.depth = 2;
  b.antiAlias = false;
  return b;
}

}  // namespace

namespace {

/** The doubled edge of a key: a plain outer ring and an inner ring on the
 *  two shaded sides alone, six pixels in, ending as @p ends says. */
kit::Bevel halfInnerRing(styles::BevelEnds ends) {
  kit::Bevel b = plain();
  b.inner = kit::BevelInner{.gap = 6,
                            .light = {1, 1, 1, 1},
                            .shadow = {0, 0, 0, 1},
                            .depth = 1,
                            .shadowDepth = 1,
                            .edges = path::Edge::Bottom | path::Edge::Right,
                            .ends = ends};
  return b;
}

}  // namespace

TEST(KitChrome, AStippleTakesEveryOtherCellInsideTheOutline) {
  sigil::compose::test::checkRows({
      {"AStippleWashedInAPropertyTakesTheColourThePropertyHolds",
       [&] {
         // The stipple is the mask and the property the colour laid through it,
         // so one lattice serves every colour a theme puts there and a change
         // of the property alone re-colours it.
         const Element washed =
             box().width(kW).height(kH).fill(kFace).foreground(
                 sigil::compose::decorations::wash(
                     var("shade"), sigil::material::pattern::stipple()));
         const auto under = [&](sigil::material::Color shade) {
           return box().padding(kX).var("shade", shade).children({washed});
         };
         Host host(140, 100);
         host.composer.render(under(kLit));
         host.frame();
         for (int y = 0; y < 4; ++y)
           for (int x = 0; x < 4; ++x)
             EXPECT_EQ(at(host, x, y),
                       ((x + y) % 2 == 0) ? SK_ColorRED : SK_ColorGREEN)
                 << x << "," << y;
         host.composer.render(under(kShade));
         host.frame();
         EXPECT_EQ(at(host, 0, 0), SK_ColorBLUE);
         EXPECT_EQ(at(host, 1, 0), SK_ColorGREEN);
       }},
      {"ThePixelLatticeStippleDrawsInsideTheOutline",
       [&] {
         Host host(140, 100);
         host.composer.render(
             box().padding(kX).children({box().width(kW).height(kH).fill(
                 sigil::material::from(kFace).layer(
                     sigil::material::pattern::stipple(
                         {.color = {1, 0, 0, 1}})))}));
         host.frame();
         // Nothing outside the face: a tile drawn over the bounds and clipped
         // to nothing would flood the host.
         EXPECT_EQ(host.pixel(kX - 1, kY - 1), SK_ColorBLACK);
         EXPECT_EQ(host.pixel(kX + kW, kY + kH), SK_ColorBLACK);
       }},
  });
}

// ---------------------------------------------------------------------------
// The eras

TEST(KitChrome, TheFourEraTokenSetsResolveToTheirOwnEdge) {
  // Motif: a mitred two-tone shadow on the lattice, both bands the same
  // depth, and the etched form the same at half the depth twice with the
  // inner ring turned over.
  const kit::Bevel motif = kit::bevels::motif(kLit, kShade, 2);
  EXPECT_EQ(motif.corner, styles::BevelCorner::Mitre);
  EXPECT_FALSE(motif.antiAlias);
  EXPECT_EQ(motif.depth, 2);
  EXPECT_EQ(motif.shadowDepth, 2);
  EXPECT_FALSE(motif.inner.has_value());
  EXPECT_EQ(motif.softness, 0);

  const kit::Bevel etched = kit::bevels::motifEtched(kLit, kShade, 2);
  EXPECT_EQ(etched.corner, styles::BevelCorner::MitreFar);
  EXPECT_EQ(etched.depth, 1);
  ASSERT_TRUE(etched.inner.has_value());
  EXPECT_EQ(etched.inner->gap, 1);
  EXPECT_EQ(etched.inner->depth, 1);
  EXPECT_TRUE(etched.inner->inverted);
  // A groove needs two rings and one pixel cannot hold two: below two
  // deep the etched form IS the plain shadow.
  EXPECT_FALSE(kit::bevels::motifEtched(kLit, kShade, 1).inner.has_value());

  // Flash: an antialiased asymmetric pair derived from the face, and a
  // second fainter pair where a gap is asked for.
  const sigil::material::Color mid{0.4f, 0.4f, 0.5f, 1};
  const kit::Bevel flat = kit::bevels::flash(mid);
  EXPECT_TRUE(flat.antiAlias);
  EXPECT_EQ(flat.depth, 3);
  EXPECT_EQ(flat.shadowDepth, 2);
  EXPECT_FALSE(flat.inner.has_value());
  EXPECT_GT(flat.light.g, mid.g);   // the lift walks toward white
  EXPECT_LT(flat.shadow.g, mid.g);  // the drop keeps the hue
  const kit::Bevel doubled = kit::bevels::flash(mid, 6);
  ASSERT_TRUE(doubled.inner.has_value());
  EXPECT_EQ(doubled.inner->gap, 6);
  EXPECT_LT(doubled.inner->light.a, 1.0f);
  EXPECT_FALSE(doubled.inner->inverted);

  // A skin: stated tones, one depth, square, nothing derived.
  const kit::Bevel part = kit::bevels::skin(kLit, kShade, 2);
  EXPECT_EQ(part.corner, styles::BevelCorner::Square);
  EXPECT_EQ(part.depth, 2);
  EXPECT_EQ(part.shadowDepth, 2);
  EXPECT_EQ(part.light, kLit);
  EXPECT_EQ(part.softness, 0);

  // A stamped plate: the only one of the four whose edge is moulded.
  const kit::Bevel stamped = kit::bevels::plate(kLit, kShade);
  EXPECT_GT(stamped.softness, 0);
  EXPECT_EQ(stamped.angleDegrees, 118);
  EXPECT_TRUE(kit::bevels::plate(kLit, kShade, 1.4f, 1.8f, true).sunken);
}

TEST(KitChrome, ABevelTakesTheTonesInForceWhereItPaints) {
  sigil::compose::test::checkRows({
      {"TheTonesInForceDressEveryPanelUnderThem",
       [&] {
         // Every use site says "the tones of the window I am in" and nothing
         // else: the window sets them as custom properties and the cascade
         // carries them.
         kit::Bevel fallback = plain();
         fallback.light = {0, 1, 0, 1};
         fallback.shadow = {0, 1, 0, 1};
         const auto panel = [&] {
           return box().width(kW).height(kH).fill(kFace).overlay(
               kit::bevels::inherited(fallback));
         };
         Host host(140, 100);
         host.composer.render(
             box()
                 .padding(kX)
                 .var("bevel-light", material::Color{1, 0, 0, 1})
                 .var("bevel-shadow", material::Color{0, 0, 1, 1})
                 .children({panel()}));
         host.frame();
         EXPECT_EQ(at(host, 50, 0), SK_ColorRED);
         EXPECT_EQ(at(host, 50, kH - 1), SK_ColorBLUE);

         // Where no ancestor sets them, the bevel's own tones stand: a panel
         // outside any era is not undrawn.
         Host bare(140, 100);
         bare.composer.render(box().padding(kX).children({panel()}));
         bare.frame();
         EXPECT_EQ(at(bare, 50, 0), SK_ColorGREEN);
         EXPECT_EQ(at(bare, 50, kH - 1), SK_ColorGREEN);
       }},
      {"AnEtchedGrooveTakesTheTonesInForceOnBothRings",
       [&] {
         // The groove's inner ring is the outer pair the other way up, so under
         // a window that set the pair both rings are drawn in it and neither
         // token tone shows anywhere — whether the value is one overlay or is
         // dressed ring by ring.
         constexpr sigil::material::Color kTokenLight{1, 1, 0, 1};
         constexpr sigil::material::Color kTokenShadow{1, 0, 1, 1};
         const kit::Bevel groove = kit::bevels::inherited(
             kit::bevels::motifEtched(kTokenLight, kTokenShadow, 4));
         Element overlaid =
             box().width(kW).height(kH).fill(kFace).overlay(groove);
         Element dressed = box().width(kW).height(kH).fill(kFace);
         kit::bevelled(dressed, groove);
         for (const Element* panel : {&overlaid, &dressed}) {
           Host host(140, 100);
           host.composer.render(box()
                                    .padding(kX)
                                    .var("bevel-light", kLit)
                                    .var("bevel-shadow", kShade)
                                    .children({*panel}));
           host.frame();
           int lit = 0, shaded = 0;
           for (int y = 0; y < 100; ++y)
             for (int x = 0; x < 140; ++x) {
               const SkColor pixel = host.pixel(x, y);
               EXPECT_NE(pixel, SK_ColorYELLOW)
                   << "at (" << x << ", " << y << ")";
               EXPECT_NE(pixel, SK_ColorMAGENTA)
                   << "at (" << x << ", " << y << ")";
               lit += pixel == SK_ColorRED;
               shaded += pixel == SK_ColorBLUE;
             }
           EXPECT_GT(lit, 0);
           EXPECT_GT(shaded, 0);
           // The inner ring, two pixels in, is drawn too, the other way up: its
           // top band is the shaded tone.
           EXPECT_EQ(at(host, 50, 2), SK_ColorBLUE);
         }
       }},
  });
}

TEST(KitChrome, EachBevelTokenPutsItsBandsWhereItSays) {
  sigil::compose::test::checkRows({
      {"ADoubledBevelLeavesTheGapBetweenItsTwoRings",
       [&] {
         Host host(140, 100);
         kit::Bevel b = plain();
         b.inner = kit::BevelInner{6, {1, 1, 1, 1}, {0, 0, 0, 1}, 1, 1, false};
         host.composer.render(panel(b));
         host.frame();

         EXPECT_EQ(at(host, 50, 0), SK_ColorRED);
         EXPECT_EQ(at(host, 50, 1), SK_ColorRED);
         for (int y = 2; y < 6; ++y)
           EXPECT_EQ(at(host, 50, y), SK_ColorGREEN) << y;
         EXPECT_EQ(at(host, 50, 6), SK_ColorWHITE);
         EXPECT_EQ(at(host, 50, 7), SK_ColorGREEN);
         // The far side of the inner ring is the inner ring's own shadow, six
         // pixels in from the far side of the outer one.
         EXPECT_EQ(at(host, 50, kH - 7), SK_ColorBLACK);
         EXPECT_EQ(at(host, 50, kH - 8), SK_ColorGREEN);
       }},
      {"AnInnerRingOnTwoSidesCarriesNoLightOppositeIt",
       [&] {
         Host host(140, 100);
         host.composer.render(panel(halfInnerRing(styles::BevelEnds::Mitred)));
         host.frame();

         // The two sides the mask names are drawn and the two it leaves out are
         // the face — not a fainter ring, not a ring in the other tone. A pair
         // there would light the top of the control, which is the whole reason
         // a second ring is masked rather than dimmed.
         EXPECT_EQ(at(host, 50, kH - 7), SK_ColorBLACK);
         EXPECT_EQ(at(host, kW - 7, 30), SK_ColorBLACK);
         EXPECT_EQ(at(host, 50, 6), SK_ColorGREEN);
         EXPECT_EQ(at(host, 6, 30), SK_ColorGREEN);
         // The outer ring is untouched by the inner one's mask.
         EXPECT_EQ(at(host, 50, 0), SK_ColorRED);
         EXPECT_EQ(at(host, 0, 30), SK_ColorRED);
         EXPECT_EQ(at(host, 50, kH - 1), SK_ColorBLUE);
         EXPECT_EQ(at(host, kW - 1, 30), SK_ColorBLUE);
       }},
      {"SlicedEndsStopAHalfRingWhereTheMissingBandWouldMeetIt",
       [&] {
         Host mitred(140, 100);
         mitred.composer.render(
             panel(halfInnerRing(styles::BevelEnds::Mitred)));
         mitred.frame();
         Host sliced(140, 100);
         sliced.composer.render(
             panel(halfInnerRing(styles::BevelEnds::Sliced)));
         sliced.frame();

         // The two corners the mask left half open. Mitred, each band runs into
         // the corner the whole ring would have made; sliced, it stops one
         // missing band's depth short and the corner is the face.
         EXPECT_EQ(at(mitred, kW - 7, 6), SK_ColorBLACK);
         EXPECT_EQ(at(sliced, kW - 7, 6), SK_ColorGREEN);
         EXPECT_EQ(at(sliced, kW - 7, 7), SK_ColorBLACK) << "one pixel short";
         EXPECT_EQ(at(mitred, 6, kH - 7), SK_ColorBLACK);
         EXPECT_EQ(at(sliced, 6, kH - 7), SK_ColorGREEN);
         EXPECT_EQ(at(sliced, 7, kH - 7), SK_ColorBLACK);
         // The corner both bands own stands either way, and so do their runs.
         EXPECT_EQ(at(mitred, kW - 7, kH - 7), SK_ColorBLACK);
         EXPECT_EQ(at(sliced, kW - 7, kH - 7), SK_ColorBLACK);
         EXPECT_EQ(at(sliced, 50, kH - 7), SK_ColorBLACK);
         EXPECT_EQ(at(sliced, kW - 7, 30), SK_ColorBLACK);
       }},
      {"AMaskedMitredRingDrawsOnlyTheSidesItNames",
       [&] {
         // The mask on a mitred ring is a clip, since the ring is two fills
         // over the whole box rather than four strokes. Mitred ends keep the
         // corner the whole ring would have made — which at a mitre is the
         // OTHER band's — and sliced ends cut it away with the rest of the
         // missing band.
         const auto masked = [](styles::BevelEnds ends) {
           kit::Bevel b = plain();
           b.corner = styles::BevelCorner::Mitre;
           b.edges = path::Edge::Bottom | path::Edge::Right;
           b.ends = ends;
           return b;
         };
         Host mitred(140, 100);
         mitred.composer.render(panel(masked(styles::BevelEnds::Mitred)));
         mitred.frame();
         Host sliced(140, 100);
         sliced.composer.render(panel(masked(styles::BevelEnds::Sliced)));
         sliced.frame();

         for (Host* host : {&mitred, &sliced}) {
           EXPECT_EQ(at(*host, 50, 0), SK_ColorGREEN) << "no top band";
           EXPECT_EQ(at(*host, 0, 30), SK_ColorGREEN) << "no left band";
           EXPECT_EQ(at(*host, 50, kH - 1), SK_ColorBLUE);
           EXPECT_EQ(at(*host, kW - 1, 30), SK_ColorBLUE);
         }
         // The mitre hands the top-right corner pixel to the band along the
         // top, which the mask did not draw: mitred, the ring still gives it
         // away and it stands lit inside the shaded band; sliced, that end of
         // the band is gone and nothing lit is left on the panel at all.
         EXPECT_EQ(at(mitred, kW - 1, 0), SK_ColorRED);
         EXPECT_EQ(at(sliced, kW - 1, 0), SK_ColorGREEN);
         EXPECT_EQ(at(sliced, kW - 1, 1), SK_ColorGREEN);
         EXPECT_EQ(at(sliced, kW - 1, 2), SK_ColorBLUE);
       }},
      {"ABevelUnderASpanGateKeepsItsRingInsideTheShape",
       [&] {
         // A span gate narrows a decoration's boundary to the run revealed so
         // far, and a partial run is OPEN: it bounds no area. The ring's clip
         // is the SHAPE that run was cut from, so the revealed part of the ring
         // stands inside the panel; clipping to the run itself would discard
         // the whole ring until the reveal came back round.
         //
         // The perimeter runs left, top, right, bottom, so a tenth of a 100 x
         // 60 panel's 320 px is the lower half of the LEFT edge and nothing
         // else. Its implicit closure is a straight line enclosing nothing at
         // all.
         auto face = [](float reveal) {
           Element panel =
               box().width(kW).height(kH).fill(kFace).overlay(plain());
           panel.mask(by::spans(spans::upTo(reveal)));
           return box().padding(kX).children({std::move(panel)});
         };
         Host host(140, 100);
         host.composer.render(face(0.1f));
         host.frame();
         // Two pixels of the lit band down the revealed half of the left edge,
         // and the third pixel in is not the band — the ring is inside the
         // shape, not fattening it and not gone.
         EXPECT_EQ(at(host, 0, 50), SK_ColorRED);
         EXPECT_EQ(at(host, 1, 50), SK_ColorRED);
         EXPECT_NE(at(host, 2, 50), SK_ColorRED);
         EXPECT_EQ(at(host, 0, 40), SK_ColorRED);
         // …and nothing where the reveal has not reached: the top edge is the
         // run after this one.
         EXPECT_NE(at(host, 0, 1), SK_ColorRED);
         EXPECT_NE(at(host, 50, 1), SK_ColorRED);

         // A SETTLED REVEAL IS THE UNSPANNED RING, pixel for pixel: a gate that
         // claims the whole perimeter carries no shape at all, so nothing about
         // the ring can differ from the one no gate narrowed.
         Host gated(140, 100), bare(140, 100);
         gated.composer.render(face(1.0f));
         gated.frame();
         bare.composer.render(panel(plain()));
         bare.frame();
         for (int y = 0; y < kH; ++y)
           for (int x = 0; x < kW; ++x)
             ASSERT_EQ(at(gated, x, y), at(bare, x, y))
                 << "at (" << x << ", " << y << ")";
       }},
  });
}
