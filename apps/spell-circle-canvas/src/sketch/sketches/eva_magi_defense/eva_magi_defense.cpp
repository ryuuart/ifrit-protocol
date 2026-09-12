// A tactical display built from six T-shaped MAGI installations and a
// continuous vertical colour field. Site silhouettes rotate toward the hub;
// their type stays upright. Labels hold their cap height and fit width by
// condensation. A continuous phosphor halo softens the lit edges.
//
// Capture at 2.5 seconds: five hostile sites, one friendly, the front still.
// The front climbs from 3 to 17 seconds; the network then repeats.

#include <sigilcompose/core/Measure.h>

#include "DefenseLayout.h"

struct EvaMagiDefense : sketch::Sketch {
  using Sketch::Sketch;

  ch::Output<float> creep{0.0f};    // scanline creep
  ch::Output<float> flicker{0.0f};  // phosphor dip (alpha of a black plane)
  ch::Output<float> blink{1.0f};    // COLLAPSING, hard on/off

  // A FALL IS A BOUND OPACITY. Every site carries both its states as two
  // bakes from the first frame, the hostile one over the friendly at this
  // rest alpha — drawn, so it is baked at load rather than on the frame
  // the site falls — and the fall is the ticker easing that alpha to one.
  // Nothing re-describes for a fall, and no bloom is ever painted live.
  static constexpr float kFallRest = 1.0f / 255.0f;
  ch::Output<float> fallAlpha[eva::kSiteN] = {{kFallRest}, {kFallRest},
                                              {kFallRest}, {kFallRest},
                                              {kFallRest}, {kFallRest}};
  /** The fall ladder, resolved for the five that fall. */
  motion::Cascade falls;
  // The front: the field's pan in whole px, negative as it climbs. Bound on
  // the funnel's material and on the ribbons' halo, so nothing re-describes.
  ch::Output<float> front{0.0f};
  sk_sp<SkImage> fieldStrip, haloStrip, ribbonHalo;  // baked once in setup
  std::vector<weave::Type> labelTypes;
  std::vector<weave::Type> alarmTypes;
  float numeralSize = 96.0f;
  weave::FontContext* fonts = nullptr;
  /** THE ROTATION RULE, ASSERTED. Every row's verdict is COMPUTED from the
   *  two values it reports, so a line that reads PASS cannot disagree with
   *  the arithmetic beside it, and `failures()` is what decides whether the
   *  plate carries a warning. */
  measure::Table verdict;
  SkPath funnel;

  // --- the construction rule, asserted ---------------------------------------
  void runAudit() {
    using namespace eva;
    verdict = {};
    // MAGI 01's target is the CENTROID of the five attackers; everyone else's
    // is the hub. Same rule, one substitution.
    SkPoint centroid{0, 0};
    for (const Site& s : kSites)
      if (s.falls) {
        centroid.fX += unroll(s.centre).fX / 5.0f;
        centroid.fY += unroll(s.centre).fY / 5.0f;
      }
    verdict.add(
        measure::heading("ROTATION RULE  theta = snap45(bearing(site "
                         "\xe2\x86\x92 target) \xe2\x88\x92 90)"));
    for (const Site& s : kSites) {
      const bool hub = s.falls;
      const SkPoint tgt = hub ? kHub : centroid;
      const SkPoint at = unroll(s.centre);
      const float bearing = deg(std::atan2(tgt.fY - at.fY, tgt.fX - at.fX));
      const float want = snap45(bearing - 90.0f);
      // the component's stem runs local +y; rotate it by the declared theta
      const float th = rad(s.rotation);
      const SkVector stem{-std::sin(th), std::cos(th)};
      const float stemDeg = deg(std::atan2(stem.fY, stem.fX));
      const float err = std::fabs(wrap180(stemDeg - bearing));
      // Two independent claims on one site: the declared angle IS the snap,
      // and the stem it turns actually points at the target. A single
      // combined boolean would report a wrong snap and a stem off by twenty
      // degrees with the same word.
      verdict.add(measure::check(
          kit::formatted("MAGI %s  (%.0f,%.0f) \xe2\x86\x92 (%.0f,%.0f), "
                         "declared rotation deg",
                         s.name, (double)at.fX, (double)at.fY, (double)tgt.fX,
                         (double)tgt.fY),
          0.0, (double)wrap180(want - s.rotation), 0.5));
      verdict.add(measure::check(
          kit::formatted("MAGI %s  stem %+.1f vs bearing %.2f, deg apart",
                         s.name, (double)stemDeg, (double)bearing),
          0.0, (double)err, 22.5));
    }
    // ...and the plate's other published number: the wall angle. The
    // polyline is authored pre-roll, so the check is "does the CAMERA put it
    // back on the 0.7386 the frame measures", which is what makes the roll
    // an honest transform rather than a decoration.
    {
      const float ax = kWallTop.fX - kWallBend.fX;  // authored run (leftward)
      const float ay = kWallBend.fY - kWallTop.fY;  // authored rise
      const float c = std::cos(rad(kRoll)), sn = std::sin(rad(kRoll));
      const float rx = -ax * c - ay * sn;  // rotated by the camera
      const float ry = -ax * sn + ay * c;
      const float rendered = std::fabs(rx / ry);
      verdict.add(measure::check(
          kit::formatted("WALL  authored %.4f + roll %.2f deg, dx:dy the "
                         "frame measures",
                         (double)(ax / ay), (double)kRoll),
          (double)kDiag, (double)rendered, 0.01));
    }
  }

  // --- one installation ------------------------------------------------------
  /** The site in one STATE, placed in canvas coordinates less @p origin —
   *  the top-left of the bake it stands on. A fall is not a change to this
   *  node: the hostile state is a second bake crossfaded over the friendly
   *  one, so neither is ever painted live with its bloom. */
  Element installation(int index, SkPoint origin, bool friendly) const {
    using namespace eva;
    const Site& s = kSites[index];
    const SkColor4f plateFill = friendly ? kFriendly : kHostile;
    const SkColor4f rim = friendly ? kRimFriendly : kRim;
    const SkColor4f ink = friendly ? kInkFriendly : kInkHostile;
    const auto& module = tre::kModule;

    // The measured centre is the rotated silhouette's bounding-box centre.
    // A T has empty lower corners, so rotating its rectangular slot would
    // shift the diagonal installations toward the outside of the network.
    SkMatrix turn;
    turn.setRotate(s.rotation, module.barWidth * 0.5f,
                   module.totalHeight() * 0.5f);
    const SkRect turned = module.outline()(SkSize::MakeEmpty())
                              .makeTransform(turn)
                              .computeTightBounds();
    const SkPoint at = unroll(s.centre) - origin -
                       SkVector{turned.centerX() - module.barWidth * 0.5f,
                                turned.centerY() - module.totalHeight() * 0.5f};
    auto plate =
        box()
            .left(at.fX - module.barWidth * 0.5f)
            .top(at.fY - module.totalHeight() * 0.5f)
            .width(module.barWidth)
            .height(module.totalHeight())
            .shape(siteSilhouette())
            .rotate(s.rotation)
            .fill(Fill::color(plateFill))
            .foreground(rimStroke(2.4f, rim))
            .key(std::string("site#") + s.name + (friendly ? "" : "#fallen"));

    // three cells: black, hard orange keyline, and the keyline blooms
    for (int n : {1, 2, 3}) {
      const SkRect r = module.cell(n);
      auto cell = box()
                      .left(r.left())
                      .top(r.top())
                      .width(r.width())
                      .height(r.height())
                      .corners({module.cellRadius})
                      .fill(Fill::color(kCell))
                      .foreground(rimStroke(3.2f, rim))
                      .ink(kNumeral);
      // The numeral is centred in the cell's TOP SQUARE, not in the cell:
      // measured, the glyph's centre sits at 30% of a 150 px cell, which is
      // 44 px — the middle of the 88 px width. And it stays UPRIGHT while
      // the plate turns.
      cell.child(text(toUtf8(std::string(1, (char)('0' + n))))
                     .font(type(numeralSize, 0.88f))
                     .centerAt({r.width() * 0.5f, r.width() * 0.5f + 3})
                     .rotate(-s.rotation));
      plate.child(std::move(cell));
    }

    // MAGI 0n knocked dark into the plate: two lines, two SIZES (cap 26 over
    // cap 36, measured), upright, no glow — it is a hole, not a light.
    //
    // The word is narrower than the installation number. Its width must
    // also clear the neighbouring cell when the plate turns sideways.
    plate.child(box()
                    .centerAt(module.labelCentre())
                    .column()
                    .alignItems(Align::Center)
                    .gap(-6)
                    .rotate(-s.rotation)
                    .ink(ink)
                    .child(text(u8"MAGI").font(type(36, 0.86f)))
                    .child(text(toUtf8(s.name)).font(type(50, 0.95f))));
    return plate;
  }

  // --- a pill ----------------------------------------------------------------
  Element pillOf(const eva::Label& L, const weave::Type& style, int keyIndex,
                 const char* keyTag, SkPoint origin) const {
    using namespace eva;
    const LabelRegister labelStyle = labelRegister(L.role);
    const SkColor4f ink = L.alarm ? kAlarm : kRim;
    const SkPoint at = unroll(L.centre) - origin;
    auto node = box()
                    .left(at.fX - L.w * 0.5f)
                    .top(at.fY - L.h * 0.5f)
                    .width(L.w)
                    .height(L.h)
                    .rotate(L.rotate)
                    .column()
                    .alignItems(L.role == LabelRole::Country ? Align::Start
                                                             : Align::Center)
                    .justify(Justify::Center)
                    .gap(labelStyle.lineGap)
                    .ink(ink)
                    .key(std::string(keyTag) + std::to_string(keyIndex));
    if (L.pill) {
      node.shape(pillSilhouette(L.cuts));
      node.fill(Fill::color(kCell));
      node.foreground(rimStroke(L.alarm ? 4.5f : 3.0f, ink));
    }
    const weave::TextStyle whole = weave::textStyle(style);  // the probes'
    const TextMetrics cap = metrics(whole, *fonts);
    const float inkShift =
        cap.lineHeight * 0.5f - cap.capSlack() - cap.capHeight * 0.5f;
    const int count = L.lines[1] ? (L.lines[2] ? 3 : 2) : 1;
    const float step = (L.h - 2.0f * labelStyle.insetY) / (float)count;
    for (int i = 0; i < count; ++i)
      node.child(
          text(toUtf8(L.lines[i]))
              .font(style)
              .centerAt(
                  {L.role == LabelRole::Country
                       ? intrinsicSize(text(toUtf8(L.lines[i]), whole), *fonts)
                                 .width() *
                             0.5f
                       : L.w * 0.5f,
                   labelStyle.insetY + step * ((float)i + 0.5f) + inkShift}));
    return node;
  }

  // --- the layers ------------------------------------------------------------
  /** The field's strip as a material, panned by the front. Nearest sampling
   *  and a whole-pixel pan: a row of the strip IS a row of the plate. */
  mskia::Paint field(const sk_sp<SkImage>& strip) const {
    mskia::Paint m = mskia::Paint::image(
        strip, SkTileMode::kRepeat, SkTileMode::kClamp, SkMatrix::I(),
        SkSamplingOptions(SkFilterMode::kNearest));
    m.offset(std::nullopt, &front);
    return m;
  }

  Element funnelLayer() const {
    using namespace eva;
    // Explicit size, not inset(0): the slot's own node has no dimensions to
    // stretch against, so an absolute child of it lays out 1920x0 — harmless
    // for an outline in absolute coordinates, and a lie in every query.
    // A bake, remade on each step of the pan and blitted between steps: a
    // recording would re-fill the funnel every frame.
    return box()
        .width(kW)
        .height(kH)
        .shape(heldPath(funnel))
        .fill(field(fieldStrip))
        .cache(Cache::Texture)
        .key("funnel");
  }

  /** The ribbons' halo: the feathered silhouette, coloured by the field's
   *  tail hue and panned with it. Laid over the ground with the plain
   *  blend — the mask cuts the ribbons out of it and the marks are drawn
   *  above it, so nothing it lands on is brighter than the halo itself —
   *  and baked at half resolution: a feathered field loses nothing to the
   *  upscale, and the bake is remade on every step of the pan. */
  Element ribbonGlow() const {
    using namespace eva;
    return box()
        .width(kW)
        .height(kH)
        .fill(mskia::Paint::blend(
            {{mskia::Paint::image(ribbonHalo), SkBlendMode::kSrc},
             {field(haloStrip), SkBlendMode::kSrcIn}}))
        .cache(Cache::Texture)
        .bakeScale(0.5f)
        .key("ribbonglow");
  }

  /** A MARK ON ITS OWN BAKE, with the bloom baked into it. The bake is the
   *  mark's turned box grown by the halo's reach — a layer effect reaches
   *  no further than the node's own box — so the halo is complete and the
   *  bake is the size of the mark, not of the canvas. The blit is exact at
   *  any angle: a still mark bakes on the device grid, the plate's roll
   *  included, and the recordings above it are pinned to that grid. */
  Element glowing(SkPoint centre, float w, float h, float degrees,
                  const std::string& key,
                  const std::function<Element(SkPoint origin)>& mark) const {
    using namespace eva;
    const SkRect bounds = turnedBounds(centre, w, h, degrees);
    const SkPoint origin{bounds.left() - kHaloReach, bounds.top() - kHaloReach};
    return box()
        .left(origin.fX)
        .top(origin.fY)
        .width(bounds.width() + 2.0f * kHaloReach)
        .height(bounds.height() + 2.0f * kHaloReach)
        .child(mark(origin))
        .effect(tubeBloom())
        .cache(Cache::Texture)
        .key(key);
  }

  Element art() const {
    using namespace eva;
    auto g = box().inset(0);
    // One bake PER MARK, not one over the group: this plate is mostly empty,
    // so bakes the size of the marks cover a fraction of the canvas for the
    // same picture — and a site that falls re-bakes its own halo alone.
    // A fall is a CROSSFADE between two bakes — the friendly state under
    // the hostile, whose opacity the ticker snaps from its rest to one.
    // Opacity rides the blit, so each state's bloom is baked once, at
    // load, and never painted live for a transition.
    const auto& module = tre::kModule;
    for (int i = 0; i < kSiteN; ++i) {
      const Site& s = kSites[i];
      g.child(glowing(
          unroll(s.centre), module.barWidth, module.totalHeight(), s.rotation,
          std::string("glow#") + s.name,
          [&, i](SkPoint origin) { return installation(i, origin, true); }));
      if (s.falls)
        g.child(glowing(unroll(s.centre), module.barWidth, module.totalHeight(),
                        s.rotation, std::string("glow#") + s.name + "#fallen",
                        [&, i](SkPoint origin) {
                          return installation(i, origin, false);
                        })
                    .opacity(&fallAlpha[i]));
    }
    for (int i = 0; i < kLabelN; ++i)
      g.child(glowing(unroll(kLabels[i].centre), kLabels[i].w, kLabels[i].h,
                      kLabels[i].rotate, "glowlab" + std::to_string(i),
                      [&, i](SkPoint origin) {
                        return pillOf(kLabels[i], labelTypes[(size_t)i], i,
                                      "lab", origin);
                      }));
    return g;
  }

  /** COLLAPSING blinks, so it is its own (volatile) node — and a TIGHT one:
   *  a full-canvas volatile layer would repaint the whole frame for two
   *  214 px pills. The blink rides the bake's blit; the bloom is inside. */
  Element collapsingLayer(int i) const {
    using namespace eva;
    const Label& L = kCollapsing[i];
    return glowing(unroll(L.centre), L.w, L.h, L.rotate,
                   "glowcol" + std::to_string(i),
                   [&, i](SkPoint origin) {
                     return pillOf(L, alarmTypes[(size_t)i], i, "col", origin);
                   })
        .opacity(&blink);
  }

  /** THE CAMERA, and its shape is dictated by what a cached blit costs.
   *
   *  The roll goes INSIDE the wrapper (on the child), so a Cache::Texture on
   *  the wrapper bakes the rotation into the pixels and the wrapper blits at
   *  identity. Put the same rotate on the OUTER node and every frame becomes a
   *  rotated resample of a canvas-sized texture.
   *
   *  The pivot is the FRAME CENTRE, not the hub: the coordinates were
   *  measured off an already-rolled frame, and rolling about the centre puts
   *  them back within ~1 px where rolling about the hub leaves the top of
   *  the plate 6 px out. */
  Element describe() {
    using namespace eva;
    auto camera = [](Element e) {
      return box().inset(0).child(
          std::move(e.rotate(kRoll).transformOriginPx({kW * 0.5f, kH * 0.5f})));
    };

    auto root = stack().inset(0);
    auto picture = stack().inset(0);

    // (no ground node: the host clears to ctx.background, and a full-canvas
    //  opaque fill on top of that is pure waste)

    // The ribbons: flat fills of one continuous field, panned by the front.
    // In a SLOT, so a fall's re-describe never reaches the funnel and the
    // funnel's pan never reaches the marks.
    picture.child(camera(slot("funnel")));
    // …their halo, screened over them, and the marks on their own bakes
    // above both — a panel hides the ribbon under it, halo and all.
    picture.child(camera(ribbonGlow()));
    picture.child(camera(art()));
    picture.child(camera(collapsingLayer(0)));
    picture.child(camera(collapsingLayer(1)));
    root.child(std::move(picture).key("phosphor"));

    // the photographed CRT: scanlines + vignette baked once, crept
    mskia::Paint crt = mskia::Paint::recipe(evangelion::tube());
    root.child(box()
                   .left(0)
                   .top(-8)
                   .width(kW)
                   .height(kH + 16)
                   .fill(crt)
                   .translateY(&creep)
                   .cache(Cache::Texture)
                   .key("crt"));
    // phosphor flicker: an alpha-0 plane 99% of the time, so it costs nothing
    root.child(box()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));

    if (verdict.failures() > 0) root.child(failureBanner());
    return root;
  }

  /** The rotation check, painted across the plate — and ONLY when it fails.
   *  The reference carries no drafting chrome, so a plate whose construction
   *  holds shows the construction and nothing else; a violated rule gets the
   *  whole table dealt in magenta where nobody can miss it. */
  Element failureBanner() const {
    sketch::kit::Theme look;
    look.palette.ash = {0, 0, 0, 1};
    look.palette.figure = {0.32f, 0, 0, 1};
    look.type.sans = eva::boldFace();
    look.type.mono = eva::boldFace();
    look.type.captionNote = {23.0f, 0.2f};
    look.type.captionLabel = {23.0f, 0.2f, true};
    look.spacing.rowGap = 7;
    std::vector<sketch::kit::Row> rows;
    for (const measure::Check& c : verdict.rows) {
      if (!c.judged()) continue;
      rows.push_back(
          {{toUtf8(c.label), toUtf8(c.actual),
            toUtf8(c.pass ? std::string("PASS") : "FAIL want " + c.expected)},
           Fill::color(c.pass ? SkColor4f{0, 0.30f, 0.14f, 1}
                              : SkColor4f{0.62f, 0, 0, 1})});
    }
    sketch::kit::Provide bound(look);
    return box()
        .left(0)
        .top(300)
        .width(eva::kW)
        .height(150.0f + 30.0f * (float)rows.size())
        .fill(Fill::color({1, 0, 1, 0.93f}))
        .column()
        .padding(26)
        .gap(10)
        .child(
            text(u8"ROTATION RULE VIOLATED — this plate is not one component")
                .font(eva::type(40, 0.95f))
                .ink({0, 0, 0, 1}))
        .child(sketch::kit::table(std::move(rows),
                                  {.columns = {{820}, {180, true}, {}},
                                   .gap = 18,
                                   .swatchSide = 15}));
  }

  // --- host ------------------------------------------------------------------
  void setup(sketch::SketchContext& ctx) override {
    using namespace eva;
    // The plate at exactly 2x. The canvas is the reference frame's own
    // 1920x1080, so halving the capture puts it on the frame directly.
    // The REFERENCE MOMENT this sketch is built to be diffed at: all five
    // outer MAGI fallen (last at 2.28), the hue front not yet moving (3.0).
    // 2.5 s sits inside that hold [2.28, 3.0).
    sketch::kit::stage(ctx, {.size = SkSize::Make(kW, kH),
                             .captureAt = 2.5,
                             .background = kGround,
                             .oversample = 2,
                             .nonlinearPicture = true});

    funnel = funnelPath();
    fieldStrip = eva::fieldStrip(0.0f);
    haloStrip = eva::fieldStrip(eva::kRibbonHueTurn);
    ribbonHalo = eva::ribbonHaloMask(funnel);
    front = 0.0f;
    runAudit();

    auto solve = [&](const Label& label) {
      const LabelRegister reg = labelRegister(label.role);
      const int lines = label.lines[1] ? (label.lines[2] ? 3 : 2) : 1;
      const float capHeight = std::min(
          reg.size, (label.h - 2.0f * reg.insetY) / (float)lines - reg.lineGap);
      const weave::TextStyle probe =
          atCapHeight(weave::textStyle(type(100.0f)), capHeight, *ctx.fonts);
      float widest = 0.0f;
      for (const char* line : label.lines)
        if (line)
          widest =
              std::max(widest, ctx.measure(text(toUtf8(line), probe)).width());
      // The cap height fixes the vertical register; condensation only fits
      // width.
      float condense = (label.w - 2.0f * reg.insetX) / widest;
      if (label.role == LabelRole::Country) condense = std::min(1.0f, condense);
      return type(probe.shaping.fontSize, condense);
    };
    fonts = ctx.fonts;
    labelTypes.clear();
    for (const auto& label : kLabels) labelTypes.push_back(solve(label));
    alarmTypes.clear();
    for (const auto& label : kCollapsing) alarmTypes.push_back(solve(label));
    // Numerals use the font's cap metric, independently of its line box.
    numeralSize = atCapHeight(weave::textStyle(type(100.0f)), 61.0f, *ctx.fonts)
                      .shaping.fontSize;

    // --- motion ---
    falls.build(eva::kFalls, eva::kFallN, 1);
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      // scanlines creep one WHOLE PIXEL at a time, 4 px per 8 s: a fractional
      // translate turns the cached CRT texture's blit into a resample.
      creep = (float)(motion::stepIndex(t, 0.5) % 4);
      // phosphor flicker: a 4 s cycle, 1% duty
      const double ph = std::fmod(t, 4.0);
      flicker = ph < 0.04 ? 0.04f : 0.0f;
      // COLLAPSING: hard on/off, 350 on / 250 off (ESTIMATED — a single frame
      // cannot measure a blink, so this rate is not read off the reference)
      blink = std::fmod(t, 0.6) < 0.35 ? 1.0f : 0.0f;
      // The falls: the cascade's own ladder, read one unit at a time. The
      // master is the seconds since the first fall over the span the
      // cascade says it needs, so the ladder and the clock cannot drift.
      const float master =
          std::clamp((float)((t - kFirstFall) * 1000.0 / (double)falls.totalMs),
                     0.0f, 1.0f);
      for (int i = 0; i < kFallN; ++i)
        fallAlpha[i] =
            kFallRest + (1.0f - kFallRest) * ch::easeOutQuad(falls.localTime(
                                                 master, (uint32_t)i, 0));
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("funnel", funnelLayer());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // The front is a bound pan and never re-describes: derived from
    // `elapsed`, in whole pixels, negative as the field climbs the plate.
    const double sweep = (elapsed - 3.0) / 14.0;
    const double k = sweep <= 0 ? 0.0 : (sweep >= 1 ? 1.0 : sweep);
    const double eased = choreograph::easeInOutQuad((float)k);
    front = -(float)(std::round(eased * eva::kFrontTravel * eva::kH /
                                eva::kFrontStep) *
                     eva::kFrontStep);
    (void)ctx;  // nothing re-describes: the falls are bound alphas too
  }
};

SIGIL_SKETCH(EvaMagiDefense, "Study \xc2\xb7 Film",
             "The End of Evangelion's MAGI plate (1997) \xe2\x80\x94 six "
             "installations are one component, rotated")
