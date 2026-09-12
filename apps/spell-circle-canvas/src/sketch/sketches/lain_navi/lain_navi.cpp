// Translucent Copland OS panels layered over a moving field of type.

// TAGS: Interfaces/Film

#include "Navi.h"

struct LainNavi : sketch::Sketch {
  using Sketch::Sketch;

  ch::Output<float> creep{0};    // scanline creep, whole px
  ch::Output<float> flicker{0};  // phosphor dip
  ch::Output<float> breathe{0};  // the camera hunting focus, 0.15 Hz

  long long scrollLine = 0;  // console scroll, one line / 220 ms
  long long orbitStep = 0;   // hyperboloid twist, 6 Hz over 24 s
  long long phraseStep = 0;  // the Layer 07 sequence, 12 Hz
  float monoSize = 22.0f;
  float proseSize = 30.0f;

  // --- the console block: 15 lines, each with its own sigma ------------------
  Element consoleText() const {
    using namespace lain;
    auto g = box().inset(0);
    for (int i = 0; i < kLines; ++i) {
      const float y = kFirstBase + kPitch * (float)i;
      const int src =
          ((scrollLine + i + kScrollPhase) % kListingN + kListingN) % kListingN;
      // ONE colour for all fifteen lines. The whole focal plane is the
      // sigma, and the sigma is on the glyph MASK.
      const SkColor4f c = kConsoleInk;
      const float sigma = std::max(0.10f, focusSigma(y) + breathe.value());
      const std::u8string line = toUtf8(kListing[src]);
      // A PHOSPHOR HALO UNDER EVERY LINE. At 2x against the plate the sharp
      // line is not a clean glyph: it peaks at 222 with a wide soft skirt that
      // bleeds into the lines above and below. A second draw at sigma + 2.4
      // and 40% of the ink, DECLARED FIRST so the core paints over it, is that
      // skirt — and it costs nothing measurable, because Skia caches blurred
      // glyph masks per (font, sigma) and both passes hit the same cache.
      auto place = [&](Element e) {
        return e.at({kTextX, y - monoSize * 0.98f});
      };
      g.child(place(text(line, type(monoFace(), monoSize,
                                    mskia::scale(c, 0.40f), sigma + 2.4f)))
                  .key("halo" + std::to_string(i)));
      g.child(place(text(line, type(monoFace(), monoSize, c, sigma)))
                  .key("mips" + std::to_string(i)));
    }
    return g;
  }

  // --- the hyperboloid, one twist phase --------------------------------------
  Element wireframe() const {
    using namespace lain;
    // 24 s per revolution at 6 Hz = 144 steps. The twist SWEEPS about its
    // measured 62.8 deg rather than spinning through it, because a hyperboloid
    // whose phi passes 90 deg turns inside out.
    const float t = (float)orbitStep / 144.0f * 6.2831853f;
    const float phi = kPhi0 + 16.0f * std::sin(t);
    const float waist = kRim * std::cos(phi * 0.01745329f);
    const float tilt2 = kOrbit2Tilt + 5.0f * std::sin(t * 0.5f + 1.1f);

    auto g = box().inset(0).key("wire");

    // the ruling — straight, and stopping 7% short of both rims
    g.child(box()
                .inset(0)
                .shape(keyedShape(
                    phi, [phi](SkSize) { return generatrices(phi, 7); }))
                .foreground(add(1.5f, mskia::scale(kWire, 0.44f), 0.0f))
                .key("ruling"));

    // the two rims and the waist — DOTTED, never solid (1.6 on 4.4 with a
    // round cap is the frame's own broken hairline)
    const std::vector<SkScalar> dot{1.6f, 4.4f};
    auto ellArc = [&](SkPoint c, float a, float b, float tilt, SkColor4f col,
                      float w, float t0, float t1, const char* key) {
      g.child(box()
                  .inset(0)
                  .shape(keyedShape(std::tuple{c.fX, c.fY, a, b, tilt, t0, t1},
                                    [c, a, b, tilt, t0, t1](SkSize) {
                                      return ellipsePath(c, a, b, tilt, t0, t1);
                                    }))
                  .foreground(add(w, col, 0.0f, dot))
                  .key(key));
    };
    auto ell = [&](SkPoint c, float a, float b, float tilt, SkColor4f col,
                   float w, const char* key) {
      ellArc(c, a, b, tilt, col, w, 0.0f, 6.2831853f, key);
    };
    // The rims are PARTIAL. A full rim ellipse plus a full waist plus a full
    // tilted orbit is three concentric dotted rings and reads as a lampshade;
    // the plate shows arcs that leave frame and never close.
    ellArc({kAxis.fX, kAxis.fY - kHalfH}, kRim, kRim * kEcc, 0,
           mskia::scale(kWire, 0.72f), 1.7f, 3.55f, 6.60f, "rimTop");
    ellArc({kAxis.fX, kAxis.fY + kHalfH}, kRim, kRim * kEcc, 0,
           mskia::scale(kWire, 0.72f), 1.7f, 0.30f, 3.05f, "rimBot");
    ell(kAxis, waist, waist * kEcc, 0, kWire, 2.0f, "waist");
    ell(kOrbit2C, kOrbit2A, kOrbit2B, tilt2, kWire, 2.0f, "orbit2");

    // the axis: a REAL line, not a rendered artifact of the surface, and it
    // does not pass through the waist centre — measured x 501..505 against a
    // waist centred on 498.
    g.child(box()
                .inset(0)
                .shape(keyedShape(
                    std::string_view("wire-axis"),
                    [](SkSize) {
                      SkPathBuilder b;
                      b.moveTo(503 + kWireShift.fX, 28 + kWireShift.fY);
                      b.lineTo(503 + kWireShift.fX, 524 + kWireShift.fY);
                      return b.detach();
                    }))
                .foreground(add(2.4f, mskia::scale(kWire, 0.72f), 0.7f)));

    // `make me feel alright?` stands UPRIGHT beside the orbit's lower-left
    // arc rather than riding it. A run laid on the conic is turned per
    // glyph, and Layer 07's English is set as a title over the picture —
    // upright, large, in a plain face — so the arc places it and does not
    // shape it. The anchor is the run the conic was fitted through in the
    // first place, (185, 455) up to (840, 215).
    g.child(text(u8"make me feel alright?",
                 type(phraseFace(), 44.0f, kAlright, 1.2f))
                .centerAt({455, 392})
                .key("alright"));
    return g;
  }

  // --- the Layer 07 phrase sequence ------------------------------------------
  Element phrases() const {
    using namespace lain;
    const double t = std::fmod((double)phraseStep / 12.0, 13.6);
    auto g = box().inset(0).key("phrases");
    for (int i = 0; i < kPhraseN; ++i) {
      const Phrase& p = kPhrases[i];
      const double u = t - p.at;
      if (u < -0.1 || u > p.hold + 0.9) continue;
      // additive bloom in and out — 0.8 s overlap, so two phrases coexist and
      // ADD where they cross, which is the whole point of the law
      float k = 1.0f;
      if (u < 0.8)
        k = (float)std::max(0.0, u / 0.8);
      else if (u > p.hold)
        k = (float)std::max(0.0, 1.0 - (u - p.hold) / 0.9);
      if (k <= 0.01f) continue;
      k = k * k * (3.0f - 2.0f * k);
      const SkColor4f c = mskia::scale(kMinds, k);
      // the bloom is a second, blurred pass DECLARED FIRST so it paints under
      // the core; kPlus makes the order irrelevant for colour but not for the
      // core's own crispness
      // in-flow sharp CORE sizes the box; the bloom rides over it as an
      // absolute overlay (a stack() measures to nothing here and shoots the
      // run out of its own centre)
      g.child(box()
                  .centerAt(p.centre)
                  .key("ph" + std::to_string(i))
                  .child(text(std::u8string(p.text),
                              type(serifFace(), p.size, mskia::scale(c, 0.42f),
                                   6.5f))
                             .inset(0))
                  .child(text(std::u8string(p.text),
                              type(serifFace(), p.size, mskia::scale(c, 0.55f),
                                   2.2f))
                             .inset(0))
                  .child(text(std::u8string(p.text),
                              type(serifFace(), p.size, c, 0.7f))));
    }
    return g;
  }

  // --- the whole stack -------------------------------------------------------
  Element describe(sketch::SketchContext& ctx) {
    using namespace lain;
    auto root = stack().inset(0);

    // S0 — the photographic plate, and the ONLY node in the stack that does
    // not add. It is the BOTTOM: the #060719 ground is folded into its shader
    // and it composites kSrcOver, which is what keeps it off the every-frame
    // saveLayer that Cache::Texture plus .blend() would force.
    root.child(box()
                   .inset(0)
                   .fill(mskia::Paint::sksl(plateEffect()))
                   .cache(Cache::Texture)
                   .key("plate"));

    // S1 — the Japanese prose, FULL BLEED: it starts above the frame and runs
    // off all four edges. Leading 48-50 measured; nothing about it is aligned
    // to the window it will sit under.
    {
      auto g = box().inset(0).key("prose");
      for (int i = 0; i < kProseN; ++i) {
        const float y = -26.0f + 48.5f * (float)i;
        // the left edge wanders: no two lines start at the same x, which is
        // what a right-to-left vertical original looks like when it is set
        // horizontally by a compositor who did not care
        const float x = -34.0f + 14.0f * std::sin((float)i * 1.7f);
        g.child(text(std::u8string(kProseLines[i]),
                     type(minchoFace(), proseSize, kProse, 0.95f, 1.5f))
                    .at({x, y})
                    .key("prose" + std::to_string(i)));
      }
      root.child(std::move(g));
    }

    // S2 — the lightened panel. Measured x 190..470, y 100..380, and it is
    // soft-edged: a radial ramp to nothing rather than a rect with a blur.
    root.child(box()
                   .rect(SkRect::MakeXYWH(178, 88, 304, 304))
                   .fill(mskia::Paint::radialUnit(
                       {0.48f, 0.46f}, 0.95f,
                       {{0.0f, kPanel},
                        {0.55f, mskia::scale(kPanel, 0.86f)},
                        {0.86f, mskia::scale(kPanel, 0.30f)},
                        {1.0f, mskia::scale(kPanel, 0.0f)}}))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("panel"));

    // ---- S3, THE CONSOLE WINDOW ---------------------------------------------

    // the body: one radial pedestal that is also the eye's rings
    root.child(box()
                   .rect(SkRect::MakeXYWH(kBodyL, kBodyT, kBodyR - kBodyL,
                                          kBodyB - kBodyT))
                   .fill(pedestal())
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("body"));

    // the eye's remaining topology — eyelids, four satellites, the stem.
    // Enormously blurred: on the plate it is barely above the pedestal.
    //
    // THE ONE COMPOSITING TRAP IN THE FILE. A node whose DECORATION paints
    // kPlus must ALSO carry `.blend(kPlus)` if it is Texture-cached: the bake
    // happens onto transparent black, where kPlus is a no-op and the pass
    // lands correctly, but the BLIT then composites kSrcOver and paints the
    // dark blurred stroke straight over the plate. Drop the node-level blend
    // and the eyelids come back as two black lozenges. Blending has to hit
    // the real destination rather than the bake's transparent surface, which
    // is also why Texture is excluded from the direct-blend path.
    // Bounded to the eye's own box, so the bake covers the eye and not the
    // whole canvas.
    root.child(box()
                   .rect(SkRect::MakeXYWH(370, 150, 376, 400))
                   .shape(keyedShape(std::string_view("eye-furniture"),
                                     [](SkSize s) {
                                       return eyeFurniture({s.width() * 0.5f,
                                                            s.height() * 0.46f},
                                                           92.0f);
                                     }))
                   .foreground(LayeredBrush{{{24.0f,
                                              hexColor(0x070C17),
                                              13.0f,
                                              {},
                                              0,
                                              SkBlendMode::kPlus,
                                              true},
                                             {9.0f,
                                              hexColor(0x0A1120),
                                              5.0f,
                                              {},
                                              0,
                                              SkBlendMode::kPlus,
                                              true}}})
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::Texture)
                   .key("eye"));

    // the side rails: single hairlines at the body's own edges, dimmer than
    // the bars. No corner anywhere — the bars simply overhang them.
    root.child(box()
                   .inset(0)
                   .shape(keyedShape(std::string_view("side-rails"),
                                     [](SkSize) {
                                       SkPathBuilder b;
                                       b.moveTo(kBodyL, kBarTopB - 4);
                                       b.lineTo(kBodyL + 8, kBarBotT + 4);
                                       b.moveTo(kBodyR, kBarTopB - 4);
                                       b.lineTo(kBodyR - 6, kBarBotT + 4);
                                       return b.detach();
                                     }))
                   .foreground(add(2.0f, kRail, 0.8f))
                   .key("rails"));

    // the MIPS block, in its own slot: it re-describes 4.5 times a second and
    // nothing else in the frame should be dirtied by that
    root.child(slot("mips"));

    // the two chrome bars — parallelograms with opposite shear and an inverse
    // bevel each, the bottom one brighter. This is the only heavy element in
    // the interface and its 30 px against 2 px hairlines IS the contrast
    // structure.
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kBarTopL, kBarTopT, kBarTopR - kBarTopL,
                                   kBarTopB - kBarTopT))
            .shape(barOutline(kShearTop))
            .fill(barBevel(kBarTopHi, kBarTopLo, 0.72f))
            .blend(SkBlendMode::kPlus)
            .cache(Cache::Texture)
            .key("barTop"));
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kBarBotL, kBarBotT, kBarBotR - kBarBotL,
                                   kBarBotB - kBarBotT))
            .shape(barOutline(kShearBot))
            .fill(barBevel(kBarBotHi, kBarBotLo, 1.02f))
            .blend(SkBlendMode::kPlus)
            .cache(Cache::Texture)
            .key("barBot"));

    // the rotated Copland lockup, up the left margin at -55 deg. Documented
    // wordmark, verbatim off the boot plate and the ASCII transcription both.
    root.child(
        box()
            .centerAt({88, 300})
            .rotate(-55.0f)
            .column()
            .alignItems(Align::Center)
            .gap(1)
            .key("wordmark")
            .child(text(u8"Copland OS Enterprise",
                        type(serifItalicFace(), 34, kWordmark, 1.9f, 1.0f)))
            .child(text(u8"Produced By Tachibana Lab",
                        type(serifItalicFace(), 16,
                             mskia::scale(kWordmark, 0.7f), 1.6f, 0.8f))));

    // ---- S4..S8, the Layer 07 strata over the window ------------------------
    root.child(slot("wire"));

    // `cover me` — the only warm thing in the frame, set upright as a
    // title. x 576..884, y 136..229 measured.
    root.child(
        box()
            .centerAt({730, 182})
            .key("cover")
            .child(text(u8"cover me", type(phraseFace(), 62,
                                           mskia::scale(kCover, 0.5f), 6.5f))
                       .centerAt({0, 0}))
            .child(text(u8"cover me", type(phraseFace(), 62, kCover, 1.4f))));

    // the magenta streaks, x 466..869, y 483..639: horizontal smears, not
    // shapes — three bands of different length at different heights, blurred
    // hard along x only.
    //
    // These five bands fake a horizontal blur with hand-shaped gradient
    // ramps. Effect::directionalBlur(sigma, 0) now spells the same intent
    // directly, and a NEW streak should be written that way. These are not
    // converted: a real blur is a different picture than five authored ramps,
    // and this plate is kept as authored.
    {
      auto g = box().inset(0).key("magenta");
      const float bands[5][4] = {{474, 508, 128, 0.95f},
                                 {556, 528, 250, 0.72f},
                                 {498, 552, 74, 0.55f},
                                 {640, 574, 190, 0.85f},
                                 {742, 604, 118, 0.48f}};
      for (const auto& b : bands)
        g.child(box()
                    .rect(SkRect::MakeXYWH(b[0], b[1], b[2], 15))
                    .fill(mskia::Paint::linearUnit(
                        {0, 0}, {1, 0},
                        {{0.0f, mskia::scale(kMagenta, 0.0f)},
                         {0.30f, mskia::scale(kMagenta, b[3])},
                         {0.68f, mskia::scale(kMagenta, b[3] * 0.8f)},
                         {1.0f, mskia::scale(kMagenta, 0.0f)}}))
                    .blend(SkBlendMode::kPlus)
                    .cache(Cache::Texture));
      root.child(std::move(g));
    }

    root.child(slot("phrases"));

    // ---- the tube -----------------------------------------------------------
    // The creep rides this wrapper rather than the baked node beneath it.
    // Putting .translateY(&creep) directly on the Texture-cached node draws
    // the same picture at the same cost today, so this is not working around
    // a demonstrated defect — read it as a habit with a reason: a cached
    // node's transform belongs on a parent that owns no paint, so that a
    // moving transform can never become an input to the bake. The next study
    // reading this should not assume there is a bug behind it.
    root.child(box().inset(0).translateY(&creep).child(
        box()
            .rect(SkRect::MakeXYWH(0, -12, kW, kH + 24))
            .fill(mskia::Paint::recipe(crtTube()))
            .cache(Cache::Texture)
            .key("crt")));
    root.child(box()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));
    return root;
  }

  // --- host ------------------------------------------------------------------
  void setup(sketch::SketchContext& ctx) override {
    using namespace lain;
    // This sketch brings its own canvas size — the source frames' 1016x720,
    // so a capture diffs against them directly — and its own ground colour.
    // Both cycles are phased for the 2.5 s still: the frame's verbatim
    // `.frame $fp,40,$31` line, which every sharpness measurement above is
    // anchored on, sits at the focal plane, and "no double minds" is at full
    // bloom.
    sketch::kit::stage(ctx, {.size = SkSize::Make(kW, kH),
                             .captureAt = 2.5,
                             .background = kGround});

    // SOLVE the mono size from the measured advance rather than guessing it:
    // measure a 40-character run at 100 pt and scale.
    {
      const std::string probe(40, 'M');
      const SkSize m = ctx.measure(
          text(toUtf8(probe), type(monoFace(), 100.0f, kConsoleInk)));
      const float advAt100 = m.width() / 40.0f;
      monoSize = advAt100 > 1.0f ? 100.0f * kAdvance / advAt100 : 22.0f;
    }
    // and the prose size from the measured 48.5 px leading (CJK sets solid at
    // roughly 1.0 em, so the body size is the leading less the gap)
    proseSize = 28.0f;

    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      // whole-pixel creep: a fractional translate turns a cached blit into a
      // resample, so the creep steps in whole pixels and never lands between
      creep = (float)(motion::stepIndex(t, 0.5) % 6);
      const double ph = std::fmod(t, 4.0);
      flicker = ph < 0.05 ? 0.055f : 0.0f;
      // the camera hunting focus, +-0.4 px at 0.15 Hz
      breathe = 0.4f * (float)std::sin(t * 0.9424778);
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("mips", consoleText());
    ctx.composer.renderSlot("wire", wireframe());
    ctx.composer.renderSlot("phrases", phrases());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // Three independent rates, three slots. Nothing else re-describes at all.
    const long long line = motion::stepIndex(elapsed, 1.0 / 0.220);
    const long long orbit = motion::stepIndex(elapsed, 6.0);
    const long long phr = motion::stepIndex(elapsed, 12.0);
    if (line != scrollLine) {
      scrollLine = line;
      ctx.composer.renderSlot("mips", consoleText());
    }
    if (orbit != orbitStep) {
      orbitStep = orbit;
      ctx.composer.renderSlot("wire", wireframe());
    }
    if (phr != phraseStep) {
      phraseStep = phr;
      ctx.composer.renderSlot("phrases", phrases());
    }
  }
};

SIGIL_SKETCH(
    LainNavi, "Study \xc2\xb7 Film",
    "Serial Experiments Lain's Copland OS \xe2\x80\x94 no opaque window "
    "anywhere, and text through a fixed focal plane")
