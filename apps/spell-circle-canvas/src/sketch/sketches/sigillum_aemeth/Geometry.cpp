#include "SigillumAemeth.h"

namespace {

/** ONE RUN SET ROUND A CIRCLE about @p centre at radius @p r, turned to
 *  @p thDeg: the box the run is measured in is that circle's own square,
 *  which is where `TextPath` strikes its normal from. */
Element onCircle(Utf8 words, SkPoint centre, float r, float thDeg,
                 TextPath::Orient orient) {
  return text(std::move(words))
      .rect(path::centred(centre, {2 * r, 2 * r}))
      .textOnPath(TextPath{.path = shapes::circle(),
                           .at = frac(thDeg),
                           .align = TextPath::Align::Center,
                           .offset = 0.0f,
                           .autoFlip = false,
                           .orient = orient});
}

/** ONE RUN SET ALONG SIDE @p k of the heptagon at radius @p r, centred on
 *  that side — seven sides addressed as one continuous arc-length. */
Element onSide(Utf8 words, int k, float r) {
  return text(std::move(words))
      .cover()
      .textOnPath(TextPath{.path = heptChords(r, 0.0f),
                           .at = ((float)k + 0.5f) / 7.0f,
                           .align = TextPath::Align::Center,
                           .offset = 0.0f,
                           .autoFlip = false,
                           .orient = TextPath::Orient::Tangent});
}

}  // namespace

auto SigillumAemeth::waxGround() -> Element {
  const SkPoint hub{kRR, kRR};
  return box().inset(0).children(
      {// the cake: rim, body, and the tool-marks of a warm knife
       kit::disc(hub, kWaxEdge * 1.055f * kR)
           .shape(shapes::annulus(0.90f))
           .fill(Paint::radialUnit({0.5f, 0.5f}, 1.0f,
                                   {{0.0f, hexColor(0x05070a, 0.0f)},
                                    {0.905f, hexColor(0x05070a, 0.0f)},
                                    {0.945f, hexColor(0x05070a, 0.62f)},
                                    {1.0f, hexColor(0x05070a, 0.0f)}}))
           .translateX(5)
           .translateY(11)
           .key("waxshadow"),
       kit::dot(hub, kWaxEdge * kR,
                Paint::blend({{Paint::radialUnit({0.42f, 0.36f}, 1.05f,
                                                 {{0.0f, kWaxPale},
                                                  {0.45f, kWaxLit},
                                                  {0.82f, kWaxMid},
                                                  {1.0f, kWaxDeep}}),
                               SkBlendMode::kSrcOver},
                              {waxGrain, SkBlendMode::kOverlay},
                              {waxSpeck.material(), SkBlendMode::kMultiply}}))
           .foreground(lines::presets::hatch(
               Fill::color(hexColor(0x6d5228, 0.10f)), 11.0f, 0.9f, -24.0f))
           .foreground(PathFormat{
               .width = 9.0f,
               .strokeFill = grooveFill(kWaxEdge * kR, 9.0f, 0.55f, 0.42f),
               .align = PathFormat::Align::Inner})
           .cache(Cache::Texture)
           .key("wax"),
       // the burnish left by the shew-stone. A ball of quartz stood on the
       // middle of this figure for its whole working life.
       kit::dot(hub, 0.33f * kR,
                Paint::radialUnit({0.42f, 0.38f}, 1.0f,
                                  {{0.0f, hexColor(0xfff6dd, 0.34f)},
                                   {0.55f, hexColor(0xffeec6, 0.14f)},
                                   {1.0f, hexColor(0x000000, 0.0f)}}))
           .blendMode(SkBlendMode::kScreen)
           .key("shew"),
       kit::ring(hub, 0.335f * kR,
                 stroke(2.0f, Fill::color(hexColor(0x7d5f2c, 0.20f))))
           .key("shewring")});
}

auto SigillumAemeth::circumferenceRules() -> Element {
  // the greatest Circle and its hairline companion — heavy OUTSIDE, hair
  // INSIDE. lines::Line cannot say this; lines::Rails can.
  const auto rule = [](float rNorm, float heavy, float hair, float gap,
                       const char* key, bool dotted) {
    std::vector<lines::Rail> set{
        {.across = 0.0f,
         .width = heavy,
         .fill = grooveFill(rNorm * kR, heavy, 0.95f, 0.55f)},
        {.across = -gap,
         .width = hair,
         .fill = Fill::color(hexColor(0x4a3418, 0.72f)),
         .dash =
             dotted ? std::vector<float>{1.2f, 5.0f} : std::vector<float>{}}};
    lines::Rails rails = lines::rails(std::move(set));
    rails.offsetStep = 7.0f;
    return kit::disc(SkPoint{kRR, kRR}, rNorm * kR)
        .shape(wobbled(shapes::circle(), 1582))
        .fill(Fill::none())
        .key(key)
        .stroke(std::move(rails))
        .mask(by::spans(spans::upTo(
            animate(from(0.0f).to(1.0f), ramp(tPlate * 1000, 900)))));
  };
  // THE COMPASS PRICKS. Forty divisions are not measured, they are STEPPED
  // round with dividers, and the point leaves a mark at every step. The inner
  // circle's contour starts due east, which is a cell CENTRE, so an Interval
  // scatter with no phase lands its first stamp half a cell along — exactly on
  // the boundary — and every one after.
  const float step = 2.0f * SK_FloatPI * rBandIn * kR / 40.0f;
  return box().inset(0).children(
      {kit::disc(SkPoint{kRR, kRR}, rGreat * kR)
           .shape(shapes::annulus(rBandIn / rGreat))
           .fill(Fill::none())
           .foreground(lines::RadialHatch{
               .strokeFill = Fill::color(hexColor(0x6d5228, 0.11f)),
               .spokes = 320,
               .rings = 0,
               .width = 0.8f,
               .holeFraction = rBandIn / rGreat})
           .opacity(animate(from(0.0f).to(1.0f), ramp(tCells * 1000, 700)))
           .key("bandhatch"),
       rule(rGreat, 5.6f, 1.2f, 11.0f, "great", false),
       rule(rBandIn, 3.4f, 0.9f, -8.0f, "second", true),
       kit::ring(SkPoint{kRR, kRR}, rBandIn * kR,
                 brush::Scatter{
                     .art = box()
                                .width(5)
                                .height(5)
                                .shape(shapes::polygon(4))
                                .fill(Fill::color(hexColor(0x2c1c06, 0.85f))),
                     .spacing = step,
                     .alignToPath = true,
                     .bleedPx = 8.0f})
           .key("pricks")});
}

auto SigillumAemeth::circumferenceCells() -> Element {
  // WHAT THE 40 DIVISIONS ACTUALLY ARE: the cells, and only the cells. The
  // letters stand upright-radial and their numbers above or below — the
  // ring's face, the letter's size and its ink are the cells' own, and a
  // number is set smaller and browner.
  std::vector<Element> cells;
  for (int i = 0; i < 40; ++i) {
    const Cell& c = kRing[(size_t)i];
    const float th = (float)i * 9.0f;
    Element letter = onCircle(c.glyph, {kRR, kRR}, rCellLet * kR, th,
                              TextPath::Orient::Radial)
                         .key("cl" + std::to_string(i));
    if (!visited[(size_t)i])  // never reached by the walk: it dims
      letter.opacity(
          animate(to(0.30f), ramp(tDark * 1000 + (float)i * 9, 700)));
    cells.push_back(std::move(letter));
    if (c.number > 0)
      cells.push_back(onCircle(std::to_string(c.number), {kRR, kRR},
                               (c.step > 0 ? rNumOut : rNumIn) * kR, th,
                               TextPath::Orient::Radial)
                          .styleClass("cellNumber")
                          .key("cn" + std::to_string(i)));
  }
  // the 40 radial dividers: INTERRUPTED rules that stop short of both
  // circles. One node, forty contours, one trim window on the stroke.
  return box()
      .inset(0)
      .transformOrigin(pct(50), pct(50))
      .font({.face = faceRing, .size = 0.060f * kR})
      .ink(hexColor(0x241603, 1.0f))
      .styleSheet(weaveNs::StyleSheet{
          {"cellNumber",
           weaveNs::Type{.size = 0.031f * kR,
                         .color = sigil::material::skia::toSkColor(
                             hexColor(0x4a3210, 1.0f))}}})
      .children(
          {box()
               .inset(0)
               .shape(keyedShape(std::string_view("band-dividers"),
                                 [] {
                                   SkPathBuilder b;
                                   for (int i = 0; i < 40; ++i) {
                                     const float th = (float)i * 9.0f - 4.5f;
                                     b.moveTo(P(th, rBandIn));
                                     b.lineTo(P(th, rGreat));
                                   }
                                   return b.detach();
                                 }))
               .fill(Fill::none())
               .stroke(PathFormat{
                   .width = 1.9f,
                   .strokeFill = Fill::color(hexColor(0x2c1c06, 1.0f)),
                   .cap = SkPaint::kRound_Cap,
                   .trimStart = 0.09f,
                   .trimEnd = 0.91f})
               .opacity(animate(from(0.0f).to(1.0f), ramp(tCells * 1000, 620)))
               .key("dividers"),
           cells});
}

auto SigillumAemeth::angles() -> Element {
  // THE SEVEN "SEGMENTS OF CIRCLES" — annular plates, radially hatched, with
  // brush::Pattern corner tiles: "at each corner of these segments of
  // circles, to make little Crosses."
  //
  // A CROSS TURNED 45 DEGREES IS AN X, so `cornerAlign` is stated here and
  // not left to the default. The art below is a Greek cross drawn with its
  // arms on local +x/+y, and every corner of an annular sector is a RIGHT
  // ANGLE — the radial leg meets the arc at 90 degrees — so aligning the
  // stamp to either leg (Outgoing) puts one arm along the arc and one along
  // the radius, at all four corners of all seven plates, which is what the
  // record draws. The BISECTOR of a right angle is 45 degrees off both, and
  // the same 90-fold symmetry that makes Outgoing corner-agnostic makes
  // Bisector uniformly wrong here: not a tilt to notice, a saltire. Any art
  // whose own axes carry meaning has to name the frame it was drawn in.
  const Element crossTile =
      box()
          .width(13)
          .height(13)
          .shape(keyedShape(std::string_view("cross-tile"),
                            [](SkSize s) {
                              SkPathBuilder b;
                              const float w = s.width(), h = s.height();
                              const float t = w * 0.20f;
                              b.moveTo(w * 0.5f - t, 0);
                              b.lineTo(w * 0.5f + t, 0);
                              b.lineTo(w * 0.5f + t, h * 0.5f - t);
                              b.lineTo(w, h * 0.5f - t);
                              b.lineTo(w, h * 0.5f + t);
                              b.lineTo(w * 0.5f + t, h * 0.5f + t);
                              b.lineTo(w * 0.5f + t, h);
                              b.lineTo(w * 0.5f - t, h);
                              b.lineTo(w * 0.5f - t, h * 0.5f + t);
                              b.lineTo(0, h * 0.5f + t);
                              b.lineTo(0, h * 0.5f - t);
                              b.lineTo(w * 0.5f - t, h * 0.5f - t);
                              b.close();
                              return b.detach();
                            }))
          .fill(Fill::color(hexColor(0x402c10, 0.92f)));
  // ONE PLATE, as the sector the record cuts it as; the bare form is the
  // outline every reading of it shares
  const auto plate = [](int k) {
    const float half = 360.0f / 7.0f * 0.5f - 1.1f;
    return kit::disc(SkPoint{kRR, kRR}, 0.868f * kR)
        .shape(shapes::sector(skAngle(((float)k + 0.5f) * 360.0f / 7.0f - half),
                              2 * half, 0.720f / 0.868f));
  };
  return box()
      .inset(0)
      .transformOrigin(pct(50), pct(50))
      .font({.face = faceSeal, .size = 0.049f * kR, .track = 0.034f * kR})
      .ink(hexColor(0x201404, 1.0f))
      .children(
          {box()
               .inset(0)
               .cache(Cache::Texture)
               .children(each(
                   std::views::iota(0, 7),
                   [&plate, crossTile](int k) {
                     return plate(k)
                         .fill(Fill::color(hexColor(0xd9bd88, 0.30f)))
                         .foreground(lines::RadialHatch{
                             .strokeFill =
                                 Fill::color(hexColor(0x6d5228, 0.13f)),
                             .spokes = 96,
                             .rings = 0,
                             .width = 0.9f,
                             .holeFraction = 0.70f})
                         .stroke(Brush{}
                                     .layer(stroke(1.5f, Fill::color(hexColor(
                                                             0x4a3418, 0.55f))))
                                     .layer(brush::Pattern{
                                         .side = box().width(20).height(3).fill(
                                             Fill::none()),
                                         .corner =
                                             brush::CornerArt{
                                                 crossTile,
                                                 brush::CornerAlign::Outgoing},
                                         .advance = 22.0f,
                                         .cornerAngleDeg = 40.0f,
                                         .bleedPx = 16.0f}))
                         .key("plate" + std::to_string(k));
                   })),
           // THE 49 LETTERS: ONE text run per row on a SEVEN-CONTOUR chord
           // path. The entrance is a plain ramp that FINISHES, and it has to
           // be: a keyframe path that is still running counts as live
           // volatility even while its value is constant, so a long hold on
           // one of these seven text-on-path nodes would keep all of them, and
           // their parents, painting live and out of any texture for the whole
           // hold. The birds' later arrival is therefore marked by a separate
           // cheap rule on each plate rather than by holding this run's
           // opacity.
           each(7,
                [](int k) {
                  std::string row;
                  for (int c = 0; c < 7; ++c) row += kAngles[k][c];
                  return onSide(row, k, rAngleHept)
                      .key("ang" + std::to_string(k));
                }),
           // the bird lands: its angle-plate takes a rule it did not have
           each(7, [&plate](int k) {
             return plate(k)
                 .fill(Fill::none())
                 .stroke(stroke(2.2f, Fill::color(hexColor(0x402c10, 0.85f)),
                                PathFormat::Align::Inner))
                 .opacity(animate(from(0.0f).to(1.0f),
                                  ramp(tBirds * 1000 + (float)k * 260, 420)))
                 .key("birdlit" + std::to_string(k));
           })});
}

auto SigillumAemeth::heptagonNames() -> Element {
  // THE HEPTAGON and the seven Names of God, written along its sides with a
  // quill — the seven sides come out at seven weights from one nib. The
  // quill's face, size, tracking and ink are the band's; the Latin gloss
  // inside it is the italic, untracked.
  return box()
      .inset(0)
      .transformOrigin(pct(50), pct(50))
      .font({.face = faceQuill, .size = 0.048f * kR, .track = 0.026f * kR})
      .ink(hexColor(0x201404, 1.0f))
      .styleSheet(weaveNs::StyleSheet{
          {"gloss", weaveNs::Type{.face = faceItalic,
                                  .size = 0.022f * kR,
                                  .color = sigil::material::skia::toSkColor(
                                      hexColor(0x53380f, 0.88f)),
                                  .track = 0.0f}}})
      .children(
          {box()
               .inset(0)
               .shape(wobbled(heptChords(rHept, 0.0f), 30, 30.0f, 0.45f))
               .fill(Fill::none())
               .stroke(Brush{}
                           .layer(brush::presets::calligraphic(
                               34.0f, 6.8f,
                               Fill::color(hexColor(0x291a05, 0.95f)), 0.22f))
                           .layer(stroke(
                               0.9f, Fill::color(hexColor(0xf7e9c4, 0.35f)))))
               .key("heptrule"),
           // the second, inner heptagon rule — the Names sit between the two
           box()
               .inset(0)
               .shape(heptChords(rNameHept - 0.043f, 0.0f))
               .fill(Fill::none())
               .stroke(lines::rails(
                   {{.across = 0.0f,
                     .width = 2.2f,
                     .fill = Fill::color(hexColor(0x4a3418, 0.72f))},
                    {.across = -5.0f,
                     .width = 0.8f,
                     .fill = Fill::color(hexColor(0x4a3418, 0.45f)),
                     .dash = {1.4f, 4.6f}}}))
               .key("heptrule2"),
           each(7,
                [](int k) {
                  std::string row;
                  for (auto gl : kGodNames[(size_t)k].glyphs)
                    row += (gl[0] == '*') ? "ɛ" : gl;  // the 21/8 ligature
                  return onSide(row, k, rNameHept)
                      .key("god" + std::to_string(k));
                }),
           each(7, [](int k) {
             return onSide(kGodNames[(size_t)k].gloss, k, rNameHept - 0.056f)
                 .styleClass("gloss")
                 .key("gloss" + std::to_string(k));
           })});
}

auto SigillumAemeth::heptagram() -> Element {
  Weave w = weave;
  PaintProgram paint = [w](SkCanvas& c, const PaintContext& ctx) {
    const float bandW = 0.038f * kR;
    auto seg = [&](int i) {
      SkPathBuilder b;
      b.moveTo(w.v[i]);
      b.lineTo(w.v[(i + 1) % 7]);
      return b.detach();
    };
    // ONE LIMB, CUT INTO THE WAX. A groove is a floor with two walls:
    // under a light from the upper left the far wall catches it and the
    // near one is in shadow, so the two rails are UNEQUAL and the band
    // between them is DARKER than the surface, not lighter. Drawn the
    // other way round — a pale band with a dark line either side and a
    // shadow cast onto its neighbour — the same geometry reads as a
    // batten laid on top, and the seal stops being engraved.
    auto limb = [&](SkCanvas& cv, int i, bool over) {
      const SkPath p = seg(i);
      SkPaint body;
      body.setAntiAlias(true);
      body.setStyle(SkPaint::kStroke_Style);
      body.setStrokeWidth(bandW);
      body.setStrokeCap(SkPaint::kButt_Cap);
      body.setColor4f(sigil::material::skia::toSkColor(kCutDark), nullptr);
      cv.drawPath(p, body);
      decorations::paintOn(
          cv, ctx, p,
          lines::rails(
              {// the lit wall
               {.across = bandW * 0.5f - 1.2f,
                .width = 2.4f,
                .fill = Fill::color(kCutLite)},
               // the shadowed wall, and the lip of wax pushed up beside it
               {.across = 1.2f - bandW * 0.5f,
                .width = 2.4f,
                .fill = Fill::color(hexColor(0x1a1409, 0.85f))},
               {.across = 1.6f - bandW * 0.5f - 2.4f,
                .width = 1.0f,
                .fill = Fill::color(hexColor(0xbfae76, 0.45f))}}));
      (void)over;
    };
    for (int i = 0; i < 7; ++i) limb(c, i, false);
    // THE WEAVE, CUT. At a crossing the graver takes the over-limb
    // through and stops the under-limb's walls short of it, so the
    // interlace is in the cut rather than in a cast shadow: redraw the
    // over limb clipped to the lens where the two marks actually overlap,
    // and its floor and walls close over the other's. The cap is half the
    // arc distance to the next crossing on the same strand, which is what
    // stops two neighbouring lenses merging into one contour and handing
    // the whole run to a single strand.
    const path::CrossingRule& rule = w.rule;
    const float reach = bandW * 0.5f + 1.6f;
    for (const path::Crossing& x : w.crossings) {
      const size_t over = rule.decide(x) == path::Order::Over ? x.a : x.b;
      const size_t under = over == x.a ? x.b : x.a;
      c.save();
      c.clipPath(path::crossingPatch(w.strands[over], reach, w.strands[under],
                                     reach, x.at, w.reachCap[x.index]),
                 true);
      limb(c, (int)over, true);
      c.restore();
    }
  };
  return box().inset(0).background(paint).key("weave");
}

auto SigillumAemeth::innerRings() -> Element {
  // Concentric circles and a crosshatched annular recess: invariant under
  // rotation about their own centre, so they stay OUT of the turning layer,
  // where they would be resampled every frame and look identical.
  return box().inset(0).children(
      {// the deepest recesses — crosshatched wax between the star's limbs
       box()
           .inset(0)
           .shape(keyedShape(std::string_view("heptagram"),
                             [] {
                               SkPathBuilder b;
                               b.setFillType(SkPathFillType::kEvenOdd);
                               for (int k = 0; k < 7; ++k) {
                                 const SkPoint v = heptVertex(k, rHept);
                                 k == 0 ? b.moveTo(v) : b.lineTo(v);
                               }
                               b.close();
                               for (int k = 0; k < 14; ++k) {
                                 const float rr =
                                     (k % 2 == 0) ? rHept : rHept * kStar72;
                                 const SkPoint v =
                                     P((float)k * 360.0f / 14.0f, rr);
                                 k == 0 ? b.moveTo(v) : b.lineTo(v);
                               }
                               b.close();
                               return b.detach();
                             }))
           .fill(Fill::color(hexColor(0x7d5f2c, 0.10f)))
           .foreground(lines::presets::crosshatch(
               Fill::color(hexColor(0x5a4218, 0.16f)), 8.0f, 0.8f, 22.0f))
           .key("recess"),
       // the concentric rules that cut the points into cells
       each(5, [](int i) {
         const float rr = kCellRings[i];
         const float width = i == 4 ? 2.4f : 1.5f;
         return kit::disc(SkPoint{kRR, kRR}, rr * kR)
             .shape(wobbled(shapes::circle(), (uint32_t)(7 + i), 22.0f, 0.30f))
             .fill(Fill::none())
             .stroke(spans::upTo(animate(
                         from(0.0f).to(1.0f),
                         ramp(tInner * 1000 + 200 + (float)i * 90, 620))),
                     PathFormat{.width = width,
                                .strokeFill =
                                    grooveFill(rr * kR, width, 0.50f, 0.34f)})
             .key("ring" + std::to_string(i));
       })});
}

auto SigillumAemeth::inner() -> Element {
  // THE FOUR ORDERS OF THE CHILDREN OF LIGHT, each with the tablet the record
  // gives it: an arc-segment worn in the forehead, a round gold plate on the
  // breast, a four-square white ivory, a three-cornered green. Four
  // silhouettes, cut in the same wax as everything else — not four tinted
  // rectangles. Every letter inside the heptagram is the seal's face in one
  // ink; only the size differs between an order's ring and ZABATHIEL's.
  struct Order {
    const char* const* names;
    float radius;
    std::optional<Shape> tablet;  // unset is the box's own four-square
    float size;
  };
  const Order orders[4] = {
      {kFiliaeLucis, rFiliaeLucis, std::nullopt, 0.027f},
      {kFiliiLucis, rFiliiLucis, shapes::circle(), 0.025f},
      {kFiliaeFil, rFiliaeFil, std::nullopt, 0.024f},
      {kFiliiFil, rFiliiFil, shapes::polygon(3, 180.0f), 0.023f}};
  const sigil::material::Color kTabletFace[4] = {
      hexColor(0xe4cd9e, 0.62f), hexColor(0xecd7a8, 0.66f),
      hexColor(0xe8d2a2, 0.60f), hexColor(0xdfc793, 0.58f)};
  const sigil::material::Color kTabletRule = hexColor(0x2c1c06, 0.95f);
  std::vector<Element> marks;
  for (int o = 0; o < 4; ++o) {
    const Order& ord = orders[o];
    for (int k = 0; k < 7; ++k) {
      const float th = (float)k * 360.0f / 7.0f;
      const float em = 0.034f * kR;            // the tablet itself
      const float rTab = ord.radius - 0.032f;  // it sits below the name
      Element tablet = box()
                           .foreground(lines::presets::hatch(
                               Fill::color(hexColor(0x4a3418, 0.30f)), 3.6f,
                               0.7f, 20.0f + (float)o * 40.0f))
                           .fill(Fill::color(kTabletFace[o]))
                           .stroke(stroke(1.7f, Fill::color(kTabletRule)))
                           .key("tab" + std::to_string(o * 7 + k));
      // the forehead's arc-segment is a SECTOR of the ring it is worn on, so
      // it takes the ring's own box; the other three are their own small box,
      // turned to the ray they stand on
      if (o == 0)
        tablet
            .rect(path::centred({kRR, kRR},
                                {2 * rTab * 1.05f * kR, 2 * rTab * 1.05f * kR}))
            .shape(shapes::sector(skAngle(th - 5.2f), 10.4f, 0.905f));
      else {
        tablet.rect(path::centred(P(th, rTab), {em, em})).rotate(th);
        if (ord.tablet) tablet.shape(*ord.tablet);
      }
      marks.push_back(std::move(tablet));
      const std::string nm = ord.names[k];
      marks.push_back(onCircle(nm == "*" ? "Eɛ" : nm, {kRR, kRR},
                               ord.radius * kR, th, TextPath::Orient::Radial)
                          .font({.size = ord.size * kR})
                          .key("chl" + std::to_string(o * 7 + k)));
    }
  }
  // ZABATHIEL — "this name must be distributed in his letters into 7 sides of
  // that innermost Heptagonum. So have you just 7 places."
  return box()
      .inset(0)
      .font({.face = faceSeal})
      .ink(hexColor(0x201404, 1.0f))
      .children({marks,
                 box()
                     .inset(0)
                     .shape(heptChords(rInnerHept, 0.0f))
                     .fill(Fill::none())
                     .stroke(lines::rails(
                         {{.across = 0.0f,
                           .width = 2.2f,
                           .fill = Fill::color(hexColor(0x3f2c12, 0.88f))},
                          {.across = 4.0f,
                           .width = 0.7f,
                           .fill = Fill::color(hexColor(0xfbf0d0, 0.40f))}}))
                     .key("zabhept"),
                 each(7, [](int k) {
                   const std::string s = kZabathiel[k];
                   return onSide(s == "I*" ? "Iɛ" : s, k, rInnerHept - 0.028f)
                       .font({.size = 0.030f * kR})
                       .key("zab" + std::to_string(k));
                 })});
}

auto SigillumAemeth::pentagram() -> Element {
  // "Set Z, of Zedekieil within the angle which standeth up toward the
  // begynning of the greatest Circle" — point-up, aligned on division 1. The
  // initials take the seal face, size and ink; a name's tail is the quill.
  const SkPoint hub{kHp, kHp};
  const auto lit = [](float delay) {
    return animate(from(0.0f).to(1.0f), ramp(delay, 420));
  };
  return box()
      .rect(path::centred({kRR, kRR}, {2 * kHp, 2 * kHp}))
      .transformOrigin(pct(50), pct(50))
      .font({.face = faceSeal, .size = 0.052f * kR})
      .ink(hexColor(0x241704, 1.0f))
      .styleSheet(weaveNs::StyleSheet{
          {"tail", weaveNs::Type{.face = faceQuill,
                                 .size = 0.024f * kR,
                                 .color = sigil::material::skia::toSkColor(
                                     hexColor(0x40300f, 0.92f))}}})
      .children(
          {kit::disc(hub, rPenta * kR)
               .shape(wobbled(shapes::star(5, 0.382f), 5, 16.0f, 0.30f))
               .fill(Fill::color(hexColor(0xe6cf9e, 0.18f)))
               .stroke(lines::rails(
                   {{.across = 0.0f,
                     .width = 3.0f,
                     .fill = Fill::color(hexColor(0x3f2c12, 0.92f))},
                    {.across = 3.2f,
                     .width = 0.8f,
                     .fill = Fill::color(hexColor(0xfbf0d0, 0.45f))}}))
               // THE MARKS, and not the wash under them: the reveal runs on
               // the rails only, and the 18%-alpha ground is simply there from
               // the start. Sweeping a wash that faint across its own
               // background is a change too small to read as an entrance.
               .mask(parts::marks(),
                     by::spans(spans::upTo(animate(
                         from(0.0f).to(1.0f), ramp(tInner * 1000 + 500, 800)))))
               .key("penta"),
           each(5,
                [hub, lit](int k) {
                  return onCircle(kPentaNames[(size_t)k].initial, hub,
                                  rPentaInit * kR, (float)k * 72.0f,
                                  TextPath::Orient::Radial)
                      .key("pi" + std::to_string(k))
                      .opacity(lit(tInner * 1000 + 900 + (float)k * 40));
                }),
           // the rest of the name runs circularly outward into the exterior
           // angle
           each(5, [hub, lit](int k) {
             return onCircle(kPentaNames[(size_t)k].tail, hub, rPentaTail * kR,
                             (float)k * 72.0f + 38.0f,
                             TextPath::Orient::Tangent)
                 .styleClass("tail")
                 .key("pt" + std::to_string(k))
                 .opacity(lit(tInner * 1000 + 980 + (float)k * 40));
           })});
}

auto SigillumAemeth::centreCross() -> Element {
  // LE · VA · NA · el, on the arms — Levanael read left, top, right, foot
  struct Arm {
    const char* s;
    float th, r;
  };
  const Arm kArms[4] = {{"VA", 0.0f, rCross * 0.72f},
                        {"NA", 90.0f, rCross * 1.02f},
                        {"el", 180.0f, rCross * 1.02f},
                        {"LE", 270.0f, rCross * 1.02f}};
  const float arm = rCross * kR;
  return box()
      .rect(path::centred({kRR, kRR}, {2 * kHc, 2 * kHc}))
      .font({.face = faceSeal, .size = 0.025f * kR})
      .ink(hexColor(0x2b1d08, 1.0f))
      .children(
          {box()
               .rect(path::centred({kHc, kHc}, {2.4f * arm, 2.4f * arm}))
               .shape(keyedShape(std::string_view("crux"),
                                 [](SkSize s) {
                                   SkPathBuilder b;
                                   const float w = s.width(), h = s.height();
                                   const float t = w * 0.085f;
                                   const float top = h * 0.06f;
                                   b.moveTo(w * 0.5f - t, top);
                                   b.lineTo(w * 0.5f + t, top);
                                   b.lineTo(w * 0.5f + t, h * 0.34f - t);
                                   b.lineTo(w * 0.90f, h * 0.34f - t);
                                   b.lineTo(w * 0.90f, h * 0.34f + t);
                                   b.lineTo(w * 0.5f + t, h * 0.34f + t);
                                   b.lineTo(w * 0.5f + t, h * 0.96f);
                                   b.lineTo(w * 0.5f - t, h * 0.96f);
                                   b.lineTo(w * 0.5f - t, h * 0.34f + t);
                                   b.lineTo(w * 0.10f, h * 0.34f + t);
                                   b.lineTo(w * 0.10f, h * 0.34f - t);
                                   b.lineTo(w * 0.5f - t, h * 0.34f - t);
                                   b.close();
                                   return b.detach();
                                 }))
               .fill(Fill::color(hexColor(0xe9d4a4, 0.34f)))
               .stroke(stroke(2.4f, Fill::color(hexColor(0x3f2c12, 0.92f))))
               .key("crux"),
           each(kArms, [](const Arm& a, size_t i) {
             return onCircle(a.s, {kHc, kHc}, a.r * kR, a.th,
                             TextPath::Orient::Upright)
                 .key("lev" + std::to_string(i))
                 .opacity(animate(from(0.0f).to(1.0f),
                                  ramp(tInner * 1000 + 1200, 500)));
           })});
}
