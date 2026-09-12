// Four constellation charts assembled as an illuminated book spread.

// TAGS: Geometry/Diagrams, Interfaces/Game

#include "Constellations.h"

struct AstralTome : sketch::Sketch {
  // TEN Outputs for 93 twinkling primitives: an Output per primitive is
  // a write per primitive per frame, and the twinkle reads the same at a
  // tenth of them shared round.
  std::array<ch::Output<float>, at::kDivCount> bright;
  ch::Output<float> arrowScale{1.0f};

  std::vector<int> divisors;  // the seeded list, long enough for any chart
  sk_sp<SkTypeface> serif, mono;
  double clock = 0;

  // ------------------------------------------------------------------ type
  Element label(const char* s, float x, float y, float size, SkColor4f col,
                float track = 0.0f, bool useMono = false) const {
    return box().at({x, y}).child(
        text(toUtf8(s), weave::textStyle({.face = useMono ? mono : serif,
                                          .size = size,
                                          .color = col,
                                          .track = track,
                                          .aliased = useMono})));
  }
  Element label(const std::string& s, float x, float y, float size,
                SkColor4f col, float track = 0.0f, bool useMono = false) const {
    return label(s.c_str(), x, y, size, col, track, useMono);
  }
  /** The tome's own tooltip idiom: type that has to sit ON the star field gets
   *  a dark sill under it (GuiScreen.drawHoveringText fills a plate before it
   *  prints). Without one, an annotation lands on a link and is gone. */
  Element scrimLabel(const std::string& s, float x, float y, float size,
                     SkColor4f col, float track = 0.0f) const {
    return label(s, x, y, size, col, track, true)
        .padding(3.0f)
        .fill(Fill::color({0.02f, 0.02f, 0.035f, 0.74f}));
  }

  // ------------------------------------------------------------- the plate
  /** Layer 0: the tome's leather, bleeding off every edge. guijspacebook is
   *  57% transparent (the window onto the page) over a gilt-brown ground; the
   *  opaque part is #0A0800 .. #9B7A2D. Generated as a radial ground plus
   *  grain, baked to PIXELS — a full-canvas material is a shader, and a
   *  picture replays the draw call, not the result. */
  Element leather() const {
    Element e =
        box()
            .inset(0, 0, 0, at::kBandH)
            .key("leather")
            .cache(Cache::Texture)
            .fill(Paint::blend({{Paint::radialUnit({0.5f, 0.5f}, 0.95f,
                                                   {{0.0f, at::kLeatherWarm},
                                                    {0.55f, at::kLeatherMid},
                                                    {1.0f, at::kLeatherDark}}),
                                 SkBlendMode::kSrcOver},
                                {Paint::recipe(field::grain(2.6f, 4, 21.0f)),
                                 SkBlendMode::kOverlay}}));
    return e;
  }

  /** Layer 1: drawCstBackground (Cluster:113). texBlack over GUI 15,10 ..
   *  405,260 at full white, then guiresbgcst over the same rect at
   *  (0.8, 0.8, 1.0, 0.7) with UV cropped to 0.1..0.9. The measured product
   *  is a #0B080B ground with white points reaching #8F8FB3 over ~0.95% of
   *  the area — reconstructed as a nebula ramp plus four brush::Scatter passes
   *  on lissajous routes, seeded so the field is identical every frame. */
  Element pagePlate() const {
    const float x = at::gx(15), y = at::gy(10);
    const float w = at::g(at::kGuiW - 30), h = at::g(at::kGuiH - 20);
    Element p =
        box()
            .rect(SkRect::MakeXYWH(x, y, w, h))
            .key("page")
            .cache(Cache::Texture)
            .fill(Paint::blend(
                {{Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
                 {Paint::radialUnit({0.42f, 0.38f}, 0.85f,
                                    {{0.0f, mskia::scale(at::kNebula, 2.2f)},
                                     {0.5f, at::kNebula},
                                     {1.0f, {0, 0, 0, 1}}}),
                  SkBlendMode::kPlus},
                 {Paint::radialUnit({0.78f, 0.74f}, 0.55f,
                                    {{0.0f, mskia::scale(at::kNebula, 1.6f)},
                                     {1.0f, {0, 0, 0, 0}}}),
                  SkBlendMode::kPlus}}));
    // The field. Six scatter runs on lissajous routes with a wide normal
    // jitter — a brush, seeded, not a table of hand-placed dots. The measured
    // budget is the constraint: ~0.95% of the plate's pixels exceed L = 60, so
    // the runs are deliberately sparse and mostly sub-pixel-faint, with one
    // bright pass carrying the visible points.
    struct Run {
      float a, b, delta;
      float spacing;
      float mag;
      float alpha;
    };
    static constexpr Run kRuns[6] = {
        {3, 4, 40, 78, 0.9f, 0.30f},  {5, 6, 90, 96, 0.6f, 0.20f},
        {7, 5, 20, 112, 0.5f, 0.14f}, {2, 9, 65, 134, 1.5f, 0.55f},
        {9, 8, 15, 88, 0.7f, 0.24f},  {4, 11, 75, 168, 2.1f, 0.75f}};
    for (int i = 0; i < 6; ++i) {
      const Run& r = kRuns[i];
      Element art =
          box()
              .width(1.5f * r.mag)
              .height(1.5f * r.mag)
              .shape(shapes::star(4, 0.26f, 0.14f))
              .fill(Fill::color(mskia::scale(at::kFieldStar, 1.0f, r.alpha)));
      p.child(box()
                  .inset(-60)
                  .key(std::string("field") + std::to_string(i))
                  .shape(shapes::lissajous(r.a, r.b, r.delta, 1600))
                  .stroke(brush::Scatter{.art = std::move(art),
                                         .spacing = r.spacing,
                                         .seed = 8000u + (uint32_t)i * 37u,
                                         .jitterAlong = r.spacing * 0.49f,
                                         .jitterNormal = 300.0f,
                                         .jitterScale = 0.85f,
                                         .alignToPath = false,
                                         .bleedPx = 330.0f}));
    }
    return p;
  }

  // -------------------------------------------------------------- the links
  /** One connection pass, as the mod draws it: a band of half-width
   *  `linebreadth` about the segment, at the constellation's colour, at the
   *  pass's own brightness. Rendered as a Ribbon body (tapered to 40% at each
   *  end — the departure) under a five-rail engraved rule whose three inner
   *  alphas are integrated off connectionperks.png. */
  /** THE PAGE'S OWN RANGE. `RenderConstellation` tints by tier, and the
   *  tier colours are saturated — but on the cluster page the four charts
   *  are drawn pale, close together, and the tint is what tells them
   *  apart rather than what the eye reads first. Four fully saturated
   *  hues on one spread makes the page a colour key. This mixes each
   *  tier's own colour most of the way to the page's silver and keeps its
   *  hue as the difference. */
  static SkColor4f paled(SkColor4f c) {
    constexpr float k = 0.66f;
    constexpr SkColor4f kSilver{0.80f, 0.84f, 0.93f, 1.0f};
    return {c.fR + (kSilver.fR - c.fR) * k, c.fG + (kSilver.fG - c.fG) * k,
            c.fB + (kSilver.fB - c.fB) * k, c.fA};
  }

  Element linkPass(const at::Con& c, int li, int pass, int key) const {
    const SkPoint a = at::starAt(c, c.links[(size_t)li].first);
    const SkPoint b = at::starAt(c, c.links[(size_t)li].second);
    const SkColor4f col = paled(at::hexColor(c.color));
    const float half = at::g(at::kLineBreadth);  // 6 canvas px
    const float band = half * 2.0f;              // 12 canvas px

    // The three measured bands of connectionperks.png, solved back through
    // alpha-over so the composite matches the sprite's own profile. The
    // sprite's v-profile is 0.097 over the full band, 0.309 over the middle
    // 25/64, 0.580 over the middle 9/64 — those three numbers, verbatim.
    lines::Rails rails;
    rails.rails = {
        {.across = 0,
         .width = band * (25.0f / 64.0f),
         .fill = Fill::color(mskia::scale(col, 1.0f, 0.309f))},
        {.across = 0,
         .width = band * (9.0f / 64.0f),
         .fill = Fill::color(mskia::scale(col, 1.45f, 0.580f))},
        // the two dotted flanks — the departure, and the per-rail phase test:
        // same width, same fill, same dash, HALF A PERIOD apart, so the two
        // rows of dots interleave down the link the way a plate's register
        // marks do. Nothing before Rails could spell this.
        // NOTE the 2.2 px "on" interval, and do not shorten it to chase
        // rounder dots. A near-zero dash such as {0.01f, 11.0f} is placed
        // CORRECTLY — geometry::path::parallel samples getPosTan at two
        // distances and displaces both along the normal, and a 0.01-long
        // segment still has a well-defined tangent — but it carries almost no
        // ink: under a 1.4 px round cap its peak coverage never reaches 1, and
        // here it lands over a band already carrying two brighter rails, so it
        // simply disappears. The remedy is more ink, either a wider rail or an
        // "on" interval long enough to build full coverage; 2.2 px is the
        // latter. Worth knowing when reading lines::dottedCore, whose default
        // dash is exactly that near-zero kind.
        {.across = -half * 1.9f,
         .width = 1.4f,
         .fill = Fill::color(mskia::scale(col, 1.35f, 0.52f)),
         .dash = {2.2f, 9.4f},
         .cap = SkPaint::kRound_Cap},
        {.across = half * 1.9f,
         .width = 1.4f,
         .fill = Fill::color(mskia::scale(col, 1.35f, 0.52f)),
         .dash = {2.2f, 9.4f},
         .dashPhase = 5.8f,
         .cap = SkPaint::kRound_Cap},
    };

    // The band's own soft shoulder, tapered to 40% at both ends so the link
    // reads as drawn FROM star TO star. Two Ribbons: a wide bloom and the
    // sprite's own 12-px body.
    brush::Ribbon bloom;
    bloom.fill = Fill::color(mskia::scale(col, 1.0f, 0.055f));
    bloom.step = 6.0f;
    bloom.width = at::LinkTaper{band * 2.1f};

    brush::Ribbon body;
    body.fill = Fill::color(mskia::scale(col, 1.0f, 0.135f));
    body.step = 4.0f;
    body.width = at::LinkTaper{band};

    const SkRect box2 =
        SkRect::MakeLTRB(std::min(a.fX, b.fX), std::min(a.fY, b.fY),
                         std::max(a.fX, b.fX), std::max(a.fY, b.fY));
    SkPathBuilder spineBuilder;
    spineBuilder.moveTo(a.fX - box2.left(), a.fY - box2.top());
    spineBuilder.lineTo(b.fX - box2.left(), b.fY - box2.top());
    const SkPath spine = spineBuilder.detach();
    return box()
        .rect(SkRect::MakeXYWH(box2.left(), box2.top(),
                               std::max(box2.width(), 1.0f),
                               std::max(box2.height(), 1.0f)))
        .key(std::string("lk") + std::to_string(key) + "_" +
             std::to_string(pass))
        .shape(heldPath(spine))
        .stroke(std::move(bloom))
        .stroke(std::move(body))
        .stroke(std::move(rails));
  }

  // -------------------------------------------------------------- the stars
  /** star1.png is a four-pointed star with concave arms reaching the full
   *  32 px of its cell: shapes::star(4, 0.24, 0.16). The quad the mod draws is
   *  2*ulength square; the radius here is modulated by LINK DEGREE, which is
   *  the one piece of magnitude information the graph actually carries. */
  Element starEl(const at::Con& c, int si, int key) const {
    const SkPoint p = at::starAt(c, si);
    const SkColor4f col = paled(at::hexColor(c.color));
    const int deg = at::degreeOf(c, si);
    const float base = at::g(at::kUlen * 2.0f);  // 18.39 canvas px
    const float r = base * (0.74f + 0.15f * (float)std::min(deg, 4));
    const float side = r * 3.4f;

    // A TIGHT box, and it must stay one. The twinkle rides an .opacity()
    // binding, a bound opacity is a saveLayer over the node's box, and this
    // page has ninety-odd of them. Hanging the stars off a shared full-canvas
    // group instead would allocate a canvas-sized layer per group per frame;
    // the layer has to be the size of the thing that twinkles.
    Element grp = box().width(side).height(side).centerAt(p).key(
        std::string("st") + std::to_string(key));
    // the halo — a gradient, which lowers to a SIMD blitter, not a blur
    // star1.png is a BLURRED point, and its falloff is most of what makes
    // a chart read as a sky rather than as a dot diagram: the halo carries
    // further and holds more of the light than the glyph does.
    grp.child(box().inset(0).fill(
        Paint::glowUnit({0.5f, 0.5f}, 0.62f,
                        {{0.0f, mskia::scale(col, 1.0f, 0.60f)},
                         {0.22f, mskia::scale(col, 1.0f, 0.30f)},
                         {0.55f, mskia::scale(col, 1.0f, 0.09f)},
                         {1.0f, mskia::scale(col, 1.0f, 0.0f)}})));
    // the glyph
    grp.child(
        box()
            .rect(SkRect::MakeXYWH((side - r) * 0.5f, (side - r) * 0.5f, r, r))
            .shape(shapes::star(4, 0.24f, 0.16f))
            .fill(Fill::color(mskia::scale(col, 1.15f, 0.74f))));
    // the white-hot core. The one kPlus on this canvas, declared as a
    // departure on the plate: the source is GL_SRC_ALPHA/ONE_MINUS_SRC_ALPHA
    // throughout (Blending.java:23).
    const float cr = r * 0.34f;
    grp.child(box()
                  .rect(SkRect::MakeXYWH((side - cr) * 0.5f, (side - cr) * 0.5f,
                                         cr, cr))
                  .blend(SkBlendMode::kPlus)
                  .shape(shapes::circle())
                  .fill(Fill::color({0.92f, 0.94f, 1.0f, 0.52f})));
    return grp;
  }

  // ---------------------------------------------------------------- arrows
  /** guijarrow is a 45 x 26 sheet read as a 2x2 UV grid, so one arrow is a
   *  22.5 x 13 cell drawn into a 30 x 15 rect. Olive, #7D6C00 over #574E25.
   *  Idle scale sin(t/4)/32 + 1 (Cluster:163); hover snaps to a flat 1.1. */
  Element arrow(float guiX, float guiY, bool flip, bool hovered,
                const char* k) const {
    Element e =
        box()
            .rect(SkRect::MakeXYWH(at::gx(guiX), at::gy(guiY), at::g(30),
                                   at::g(15)))
            .key(k)
            .transformOrigin(0.5f, 0.5f)
            .shape(shapes::arrow(0.34f, 0.42f))
            .rotate(flip ? 180.0f : 0.0f)
            .fill(Paint::linearUnit(
                {0, 0}, {0, 1}, {{0.0f, at::kOlive}, {1.0f, at::kOliveDim}}))
            .foreground(decorations::border(
                1.2f, Fill::color(mskia::scale(at::kGilt, 1.0f, 0.7f))));
    if (hovered)
      e.scale(1.1f);
    else
      e.scale(bind(&arrowScale));
    return e;
  }

  // ------------------------------------------------------------ bookmarks
  /** GuiScreenJournal:113 — the rail hangs at guiLeft + guiWidth - 17.25 and
   *  runs DOWN THE RIGHT EDGE at an 18 px pitch, 67 x 15 each, five px WIDER
   *  when not selected. Almost all of it is off the page: the tab sticks out
   *  past the tome's own right edge, which is why it bleeds off this canvas.
   *  Labels from ClientProxy:196-205. */
  /** THE CAPTION BAND: what the plate proves, under the plate. */
  Element captionBand() const {
    Element band = box()
                       .key("caption")
                       .left(0)
                       .right(0)
                       .bottom(0)
                       .height(at::kBandH)
                       .column()
                       .justify(Justify::Center)
                       .padding(34.0f, 0.0f)
                       .gap(5.0f)
                       .zIndex(20)
                       .fill(Fill::color({0.031f, 0.027f, 0.023f, 1.0f}));
    band.child(
        text(toUtf8("ASTRAL SORCERY \xc2\xb7 "
                    "GuiJournalConstellationCluster, PAGE 1 OF 4"),
             weave::textStyle({.face = mono,
                               .size = 13.0f,
                               .color = SkColor4f{0.72f, 0.66f, 0.50f, 1.0f},
                               .track = 2.6f})));
    band.child(text(
        toUtf8("Four charts on one page at the mod's own numbers: a 95x95 "
               "SQUARE render box hung on an 80x110 hit cell, the offsetMap's "
               "zig-zag placing them, and every star's twinkle on its own "
               "divisor between 12 and 21."),
        weave::textStyle({.face = mono,
                          .size = 11.0f,
                          .color = SkColor4f{0.50f, 0.46f, 0.38f, 1.0f},
                          .track = 0.4f})));
    return band;
  }

  Element bookmarkRail() const {
    static constexpr const char* kNames[4] = {"RESEARCH", "CONSTELLATIONS",
                                              "PERKS", "KNOWLEDGE"};
    Element rail = box().inset(0).key("bm").zIndex(12);
    for (int i = 0; i < 4; ++i) {
      const bool sel = i == 1;  // bookmarkIndex 20 = Constellations
      const float w = 67.0f + (sel ? 0.0f : 5.0f);
      const float y = 20.0f + 18.0f * (float)i;
      rail.child(box()
                     .rect(SkRect::MakeXYWH(at::gx(at::kGuiW - 17.25f),
                                            at::gy(y), at::g(w), at::g(15)))
                     .key(std::string("bmk") + std::to_string(i))
                     .shape(shapes::notched(at::g(9.0f), at::g(4.0f),
                                            shapes::Corner::TopRight |
                                                shapes::Corner::BottomRight))
                     .fill(Paint::linearUnit(
                         {0, 0}, {1, 0},
                         {{0.0f, sel ? at::kLeatherWarm : at::kLeatherMid},
                          {0.6f, mskia::scale(at::kLeatherMid, 0.8f)},
                          {1.0f, at::kLeatherDark}}))
                     .foreground(decorations::border(
                         1.2f,
                         Fill::color(
                             mskia::scale(at::kGilt, 1.25f, sel ? 1.0f : 0.6f)),
                         1.0f)));
      (void)kNames;  // the label rides 15 GUI px into a tab that starts
                     // 2.75 px from the tome's right edge: at 3x it is
                     // entirely off-canvas, so the tab bleeds and the name
                     // does not print as a clipped fragment.
    }
    return rail;
  }

  // --------------------------------------------------------------- setup

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = SkSize::Make(at::kCanvasW, at::kCanvasH),
                             .captureAt = 6.0,
                             .background = SkColor4f{0, 0, 0, 1}});

    serif = weave::ports::face({"Baskerville", "Charter", "Palatino",
                                "Times New Roman", "Georgia", "Helvetica"});
    mono = weave::ports::face(
        {"Menlo", "SF Mono", "Monaco", "Courier New", "Helvetica"});
    divisors = at::divisorSequence(64);

    // ---- motion ---------------------------------------------------------
    ctx.ticker.add([this](double dt) {
      clock += dt;
      // Cluster:243 / Render:331. clientTick runs at 20 Hz; partialTicks is
      // the sub-tick fraction, so (tick + partial) is simply t*20.
      const double t = std::fmod(clock * 20.0, 12000.0);  // dayLength/2
      for (int i = 0; i < at::kDivCount; ++i)
        bright[(size_t)i] =
            (float)(0.3 + 0.7 * (std::sin(t / (double)(at::kDivMin + i)) / 4.0 +
                                 0.375));
      // Cluster:163 — sin(t/4)/32 + 1 on the client tick.
      arrowScale = (float)(std::sin(t / 4.0) / 32.0 + 1.0);
      return true;
    });

    // ---- the tree -------------------------------------------------------
    Element root = box().inset(0);
    root.child(leather().zIndex(0));
    root.child(pagePlate().zIndex(1));

    // THE TWINKLE. Ten divisors is the whole of it (12 + rand.nextInt(10)),
    // so ten ch::Output<float> drive 31 stars and 62 connection passes — but
    // the BINDING goes on each primitive's own tight box, not on ten shared
    // full-canvas groups. A bound opacity is a saveLayer over the node's box,
    // so grouping would put twenty canvas-sized layers in every frame for the
    // same ten Outputs, the same draw order and the same pixels.
    // The draw-on: each chart is its own container so staggerChildren can
    // cascade the four in offsetMap order — the zig-zag reveals as a zig-zag —
    // and the links inside a chart cascade again at 25 ms. Containers with no
    // opacity of their own allocate no layer, so this costs nothing at rest.
    Element links =
        box().inset(0).key("links").zIndex(3).staggerChildren(160ms);
    Element stars =
        box().inset(0).key("stars").zIndex(4).staggerChildren(160ms);

    int lkKey = 0, stKey = 0;
    for (int ci = 0; ci < 4; ++ci) {
      const at::Con& c = at::kPage0[(size_t)ci];
      const SkPoint o = at::kOffsets[(size_t)ci];
      const SkPoint org{at::gx(o.fX), at::gy(o.fY)};
      // Cluster:222 — the pivot is (width/2, width/2) = (40, 40) GUI, i.e.
      // 7.5 px up and left of the 95x95 chart's own centre. Chart 1 (armara,
      // the dense one) is the hovered chart, and it grows down-right off it.
      const bool hovered = ci == 1;
      const float pivot = at::g(at::kCellW * 0.5f);
      Element chartLinks = box()
                               .inset(0)
                               .key(std::string("cl") + std::to_string(ci))
                               .staggerChildren(25ms);
      Element chartStars = box()
                               .inset(0)
                               .key(std::string("cs") + std::to_string(ci))
                               .staggerChildren(25ms);
      auto place = [&](Element e) {
        e.translateX(org.fX).translateY(org.fY);
        if (hovered) {
          e.scale(1.1f);
          e.transformOriginPx({org.fX + pivot, org.fY + pivot});
        }
        return e;
      };

      // Connections. TWO sibling passes per link, at the two divisors the
      // seeded sequence hands them — Render:243's `for (j = 0; j < 2; j++)`
      // calls getBrightness() again on the second lap, so the two passes do
      // NOT share an alpha and the composite beats between two periods.
      for (int li = 0; li < c.linkCount; ++li)
        for (int pass = 0; pass < 2; ++pass) {
          const int d =
              divisors[(size_t)pass * (size_t)c.linkCount + (size_t)li] -
              at::kDivMin;
          chartLinks.child(place(linkPass(c, li, pass, lkKey++)
                                     .mask(by::spans(spans::upTo(animate(
                                         from(0.0f).to(1.0f), {520ms}))))
                                     .opacity(bind(&bright[(size_t)d]))));
        }

      // Stars. The star loop runs after both connection laps, so it picks up
      // at index 2*C in the same sequence.
      for (int si = 1; si <= c.starCount; ++si) {
        const int d =
            divisors[(size_t)(2 * c.linkCount + si - 1)] - at::kDivMin;
        chartStars.child(place(starEl(c, si, stKey++)
                                   .scale(animate(from(0.0f).to(1.0f), {380ms}))
                                   .opacity(bind(&bright[(size_t)d]))));
      }
      links.child(std::move(chartLinks));
      stars.child(std::move(chartStars));
    }
    root.child(std::move(links));
    root.child(std::move(stars));

    // The names. Cluster:252-253 centres on x = 40 of an 80-wide cell — 7.5
    // GUI px left of the 95-wide chart — and drops the baseline at y = 90,
    // inside the chart's own star field.
    for (int ci = 0; ci < 4; ++ci) {
      const at::Con& c = at::kPage0[(size_t)ci];
      const SkPoint o = at::kOffsets[(size_t)ci];
      const float w = (float)std::char_traits<char>::length(c.name) * 8.6f;
      root.child(label(c.name, at::gx(o.fX + at::kCellW * 0.5f) - w * 0.5f,
                       at::gy(o.fY + 90.0f), 19.0f,
                       mskia::scale(at::kInk, 1.0f, kInkAlphaOf()), 2.4f)
                     .key(std::string("nm") + std::to_string(ci))
                     .zIndex(6));
    }

    root.child(arrow(367, 125, false, false, "arrowNext").zIndex(10));
    root.child(arrow(197, 230, true, false, "arrowBack").zIndex(10));
    root.child(bookmarkRail());

    // THE CAPTION BAND, outside the tome. Everything this study has to
    // say about the page — the 95-against-80 overhang, the offsetMap's
    // zig-zag, the twinkle's divisor ladder, the sprite's own three-band
    // alpha profile, the corrections the mod's source forced — was drawn
    // ONTO the page as tick labels, leaders, insets and a corrections
    // block, so a still of it was a still of a diagram about a page
    // rather than of the page. The measurements live in this file's
    // header now, and what stands under the artefact is one band naming
    // what the plate is.
    root.child(captionBand());

    ctx.composer.render(root);
  }

  static float kInkAlphaOf() { return at::kInkAlpha; }

  void update(double, sketch::SketchContext&) override {}
};

SIGIL_SKETCH(AstralTome, "Study \xc2\xb7 Game UI",
             "Astral Sorcery's constellation spread \xe2\x80\x94 the chart is "
             "square, the CELL is stretched")
