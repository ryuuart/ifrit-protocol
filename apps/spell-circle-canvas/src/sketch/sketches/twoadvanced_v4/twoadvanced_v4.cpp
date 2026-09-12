// twoadvanced v4: scene assembly and animation.

// TAGS: Interfaces/Web

#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::describe() -> Element {
  using namespace tav;
  using namespace layouts;
  // THE PICTURE BELOW IS THE WHOLE OF THE PAGE'S GEOMETRY, and no panel
  // states a rectangle of its own: three columns over the 8 px gutter,
  // and one row per horizontal edge the reference draws at.
  Element sheet =
      layout(
          Grid{.columns = {px(596), fr(1), px(700)},
               .rows = {px(48), px(46), px(128), px(24), px(350), px(20),
                        px(168), px(242), px(64), px(96), px(10), px(96),
                        px(18), px(220)},
               // clang-format off
                  .areas = {".         .         masthead",
                            "audio     nav       masthead",
                            "audio     .         masthead",
                            ".         .         masthead",
                            "mainframe mainframe feature",
                            ".         .         .",
                            "aux       aux       press",
                            ".         .         press",
                            ".         .         .",
                            "subsys    subsys    subsys",
                            ".         .         .",
                            "legal     legal     legal",
                            ".         .         .",
                            "dock      dock      dock"},
               // clang-format on
               .gap = {8, 0}})
          .left(Dimension(24))
          .top(Dimension(0))
          .width(1892)
          .height(Dimension(1530));
  for (Element panel :
       {audioModule(), navBar(), masthead(), mainframe(), featureSystem(),
        auxiliary(), pressUpdates(), subSystem(), legalStrip(), footerDock()})
    sheet.child(panel);

  // sitebackground.gif is a 1×1600 strip tiled across the page. When
  // the real strip is loaded it IS the page — repeated in x, ×2 in y,
  // clamped so the canvas shows the strip's top 780 rows exactly as a
  // 780-CSS-px-tall page did. The fallback ramp is that same strip
  // read back off a loaded run and written down as stops, plus grain
  // for its vertical tooth, so a cold cache renders the same page a
  // warm one does rather than a lighter cousin of it.
  Element page = stack();
  if (siteBgGif) {
    page.fill(stretchFill(siteBgGif, 1940, 3200, SkTileMode::kRepeat));
  } else {
    // THE FALL-OFF IS READ OFF THE REAL STRIP, column by column, so
    // the two paths draw one page rather than two: down the middle the
    // loaded strip reads (87, 17, 25) for its top seventh — which is
    // kChrome exactly — then (82, 15, 23) at 0.40, (71, 10, 18) at
    // 0.55, (61, 6, 13) at 0.65, (37, 0, 2) at 0.80 and near black
    // under the footer. A two-stop ramp reads as flat maroon and gets
    // figure and ground backwards, because the PANELS are the light
    // thing on this page.
    page.fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                       {{0.00f, kChrome},
                                        {0.40f, hexColor(0x520F17)},
                                        {0.55f, hexColor(0x470A12)},
                                        {0.65f, hexColor(0x3D060D)},
                                        {0.80f, hexColor(0x250002)},
                                        {1.00f, kBgBot}}))
        .child(box().inset(0).fill(grain).opacity(0.07f).blend(
            SkBlendMode::kOverlay));
  }
  return page.child(rail(false))
      .child(rail(true))
      .child(statusBar())
      .child(sheet)
      .child(bootOverlay());
}

auto TwoAdvancedV4::setup(sketch::SketchContext& ctx) -> void {
  using namespace tav;
  sketch::kit::stage(ctx, {.size = {1940, 1560},
                           .captureAt = 6.0,
                           .background = hexColor(0x0A0000)});

  // The hero's world, baked at twice the panel's pixels because a plate
  // is taken at up to twice the canvas.
  heroPlate = bakeHero(2356, 632, ctx);

  // --- the production shell bitmaps, from the restoration host ----------
  // SigilIO's https path caches on disk (CacheFirst), so only the
  // very first run touches the network; each call returns null when the
  // fetch fails AND nothing is cached, which the use sites treat as
  // "draw the procedural stand-in".
  {
    sigil::io::Hub& hub = ctx.assets.hub();
    const std::string base = "https://v4prophecy.2advanced.com/images/";
    railLeftGif = hub.image(base + "leftsidepanel.gif");
    railRightGif = hub.image(base + "rightsidepanel.gif");
    siteBgGif = hub.image(base + "sitebackground.gif");
    footerGif = hub.image(base + "sitefooter.gif");
    logoBugSvg = hub.image(base + "2alogobug.svg", {.width = 124});
  }

  // --- generated materials, built ONCE and HELD (identity = pruning) ---
  hazard = patterns::stripes(6, 10, mskia::toColor(kChromeHi));
  hazard.rotate(45);
  hatchA = patterns::stripes(1, 7, mskia::toColor(mskia::withAlpha(kD4, 0.5f)));
  hatchA.rotate(45);
  hatchB =
      patterns::stripes(1, 7, mskia::toColor(mskia::withAlpha(kD1, 0.55f)));
  hatchB.rotate(-45);
  dither =
      patterns::checker(1.5f, mskia::toColor(mskia::withAlpha(kPanelSh, 0.28f)),
                        mskia::toColor(mskia::withAlpha(kPanelHi, 0.15f)));
  // LUMINANCE grain (one channel, not three), so the kOverlay pass
  // reads as LIGHT on the oxblood ramp instead of hue-shifting it —
  // `field::noise()` is fractal RGB and turns the page into rainbow
  // terrazzo. `stretch` gives the grain a slight vertical tooth, which
  // is what the real 1x1600 sitebackground.gif strip has.
  grain = mskia::Paint::recipe(field::grain(0.9f, 3, 4.0f, 1.25f, 1.6f));

  spectrum = mskia::Paint::sksl(spectrumFx(), {{"uBars", 32.0f}})
                 .uniform("uHot", kGlow)
                 .uniform("uCool", kTealBar)
                 .quantizeTime(10.0f);  // 10 steps a second, not a slide

  // ONE stripe material value, reused by the nav bar and four panel
  // headers; the pan is a bound uniform, not five redraw loops.
  stripesLive =
      mskia::Paint::sksl(stripeFx(), {{"uOn", 6.0f}, {"uPeriod", 16.0f}})
          .uniform("uColor", kChromeHi)
          .uniform("uBase", kChrome)
          .uniform("uPan", &stripePan);

  waterStreaks = mskia::Paint::sksl(waterFx());

  // measure the press entries at the well's own wrap width, so the
  // auto-scroll walks the REAL overflow rather than a guessed one
  pressOverflow = std::max(
      0.0f, ctx.measure(box().width(Dimension(kPressWellW)).child(pressList()))
                    .height() -
                kPressWellH);

  // --- the instanced chevron array in the footer dock ---
  dockAtlas = std::make_shared<instancing::Atlas>(2.0f);
  const int chev = dockAtlas->cell(
      box()
          .shape(keyedShape(std::string_view("dock-chevron"),
                            [](SkSize s) {
                              SkPathBuilder b;
                              b.moveTo(0, 0);
                              b.lineTo(s.width() * 0.62f, s.height() * 0.5f);
                              b.lineTo(0, s.height());
                              b.lineTo(s.width() * 0.30f, s.height() * 0.5f);
                              b.close();
                              return b.detach();
                            }))
          .fill(kD6),
      {12, 10});
  dockPool = std::make_shared<instancing::Pool>();
  instancing::place::grid(*dockPool, size_t{6} * 14, 14, {14, 12}, {0, 0},
                          {2, 4});
  {
    auto frames = dockPool->frames();
    auto tints = dockPool->tints();
    for (size_t i = 0; i < frames.size(); ++i) {
      frames[i] = chev;
      const float k = 0.35f + 0.65f * (float)((i * 7 + 3) % 11) / 10.0f;
      tints[i] = {1, 1, 1, k};
    }
    dockPool->commit();
  }

  // --- the idle motion, all of it driven from this one ticker -----------
  ctx.ticker.add([this, &ticker = ctx.ticker](double) {
    const double t = ticker.elapsed();
    const float s = (float)t;
    stripePan = s * 2.5f;                               // 20 px / 8 s
    portalGlow = 54.0f + 4.4f * std::sin(s * 1.5708f);  // ±8 %, period 4 s
    vuLeft = 0.45f + 0.42f * std::abs(std::sin(s * 3.1f));
    vuRight = 0.40f + 0.45f * std::abs(std::sin(s * 2.3f + 1.1f));

    // the section cycle: shutters, ACCESSING plate, selection mark
    {
      const tav::SectionCycle::At now = kCycle.at(t);
      const float ph = now.phase;  // transition phase, <0 outside a change
      int target = 2, from = 2;
      if (now.running) {
        target = cycleTarget(now.stop);
        from = now.previous < 0 ? 2 : cycleTarget(now.previous);
      }
      for (int i = 0; i < 6; ++i) {
        float cover = 0.0f;
        if (ph >= 0.0f) {
          // close L→R over the first 0.4, reopen R→L over the last 0.4
          const float closeAt = 0.04f * (float)i;
          const float openAt = 0.60f + 0.04f * (float)(5 - i);
          cover = std::clamp((ph - closeAt) / 0.14f, 0.0f, 1.0f) -
                  std::clamp((ph - openAt) / 0.14f, 0.0f, 1.0f);
        }
        shutter[(size_t)i] = cover;
      }
      shutterInfo = ph < 0.0f
                        ? 0.0f
                        : std::clamp((ph - 0.22f) / 0.08f, 0.0f, 1.0f) -
                              std::clamp((ph - 0.70f) / 0.08f, 0.0f, 1.0f);
      // the mark glides during the middle of the change
      const float glide =
          ph < 0.0f ? 1.0f : std::clamp((ph - 0.3f) / 0.4f, 0.0f, 1.0f);
      const float eased = glide * glide * (3.0f - 2.0f * glide);
      navIndX = navMarkX(from) + (navMarkX(target) - navMarkX(from)) * eased;
    }

    // seven tick clusters, each blinking its three dots in sequence,
    // phase-offset per panel so they never lock step
    for (int p = 0; p < 7; ++p) {
      const float off = (float)((p * 137) % 800) / 1000.0f;
      const float cyc = std::fmod(s + off, 2.7f);
      for (int i = 0; i < 3; ++i) {
        const bool on = cyc >= i * 0.4f && cyc < i * 0.4f + 0.32f;
        dot[(size_t)p * 3 + (size_t)i] = on ? 1.0f : 0.22f;
      }
    }

    // three gauges round-robin a radar sweep, 1 s each
    const int active = (int)std::fmod((double)s, 3.0);
    for (int i = 0; i < 3; ++i) {
      if (i == active) gauge[(size_t)i] = (float)std::fmod(s * 200.0, 360.0);
      gaugeAlpha[(size_t)i] = i == active ? 1.0f : 0.22f;
    }

    // PRESS UPDATES auto-scroll: walk the list a 62 px entry at a time,
    // clamped to the overflow measured in setup(), so the last step
    // parks the list bottom-flush instead of walking it off the top and
    // leaving the well empty. A list that fits its well holds still.
    const float span = 3.1f;
    const float overflow = pressOverflow;
    const int steps = (int)(overflow / 62.0f);
    if (steps > 0) {
      const float u =
          std::fmod(std::max(0.0f, s - 4.5f), span * (float)(steps + 1));
      const int idx = std::min((int)(u / span), steps);
      float f = (u - (float)idx * span - 2.5f) / 0.6f;
      f = std::clamp(f, 0.0f, 1.0f);
      f = f < 0.5f ? 2 * f * f : 1 - 2 * (1 - f) * (1 - f);  // easeInOutQuad
      pressScroll = -std::min(overflow, 62.0f * ((float)idx + f));
    } else {
      pressScroll = 0.0f;
    }
    return true;
  });

  ctx.composer.render(describe());
  ctx.composer.renderSlot("bootpct", bootReadout());
  ctx.composer.renderSlot("mfload", mfLoadReadout(mfSection = 2));
}

auto TwoAdvancedV4::update(double elapsed, sketch::SketchContext& ctx) -> void {
  // The ACCESSING readout's section name is text content, so it rides
  // the DATA path: re-rendered into its slot only when the cycle's
  // target changes.
  {
    const tav::SectionCycle::At now = kCycle.at(elapsed);
    const int target = now.running ? cycleTarget(now.stop) : 2;
    if (target != mfSection) {
      mfSection = target;
      ctx.composer.renderSlot("mfload", mfLoadReadout(target));
    }
  }
  // The boot percentage is TEXT CONTENT, not a paint property, so it
  // cannot be a binding. The slot() keeps the churn local — the rest
  // of the tree is untouched.
  if (booted) return;
  const double u = (elapsed - 0.55) / 0.80;
  const int pct = (int)std::lround(std::clamp(u, 0.0, 1.0) * 100.0);
  if (pct == bootPct) return;
  bootPct = pct;
  if (pct >= 100) booted = true;
  ctx.composer.renderSlot("bootpct", bootReadout());
}

SIGIL_SKETCH(TwoAdvancedV4, "Study \xc2\xb7 Screens",
             "2Advanced Studios v4 \"Prophecy\" (2003\xe2\x80\x93"
             "06) \xe2\x80\x94 chamfered Flash chrome, four deep")
