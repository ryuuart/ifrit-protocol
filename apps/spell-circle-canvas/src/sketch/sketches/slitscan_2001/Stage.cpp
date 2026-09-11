#include "SlitScan2001.h"

auto SlitScan2001::header() -> Element {
  using namespace slit;
  Track rise{.effect = fx::rise(18.0f),
             .stagger = {.eachMs = 22},
             .progress = animate(to(1.0f), {440ms, ch::easeOutExpo, 120ms})};
  return box()
      .column()
      .height(Dim(kHeaderH))
      .gap(4)
      .child(
          t("TIME AS AN AXIS OF THE IMAGE", ui(10, kType2, 2.6f))
              .key("eyebrow")
              .opacity(animate(from(0.0f).to(1.0f), {260ms, ch::easeOutQuad}))
              .translateY(
                  animate(from(8.0f).to(0.0f), {260ms, ch::easeOutQuad})))
      .child(t("THE SLIT-SCAN MACHINE, 1966–68", uiB(40, kType, 0.4f))
                 .key("title")
                 .textStroke(0.6f, Fill::color(kInk))
                 .fx(std::move(rise)))
      .child(t("Douglas Trumbull — ‘Creating Special Effects for 2001: A "
               "Space Odyssey’, American Cinematographer 49(6):416–420, "
               "451–453, June 1968 (READ DIRECTLY) · Cinefex 85, April "
               "2001 · Super Panavision 70, 65 mm 5-perf spherical, "
               "2.20:1, 24 fps, f/1.8",
               ui(11, kType2))
                 .key("cite")
                 .opacity(animate(from(0.0f).to(1.0f),
                                  {240ms, ch::easeOutQuad, 400ms})));
}

auto SlitScan2001::filmFrame() -> Element {
  using namespace slit;
  const Shot& s = shotAt(shot);

  // THE ACCUMULATION. Two exposures, kPlus, one atlas stamp each.
  auto raw = [this] {
    return box()
        .inset(0)
        .child(instancing::instances(atlas, wallA, instancing::Mode::Live,
                                     SkBlendMode::kPlus))
        .child(instancing::instances(atlas, wallB, instancing::Mode::Live,
                                     SkBlendMode::kPlus));
  };
  Element accumulation =
      raw().effect(Effect::shader(transfer, {{"k", transferK()}}));
  // THE CORE. Where the two planes converge the camera is looking
  // straight down the corridor, and every stamp in both exposures has
  // been laid on top of every other: on the Star Gate frame the
  // vanishing point is the brightest thing in it. The accumulation
  // reaches it as a thin seam because a stamp's own width is finite, so
  // the last few inches of the sweep are added here as the light they
  // integrate to.
  // The core RIDES the vanishing point rather than being placed at it.
  // The frame is described once and the walls are rebuilt on the film
  // clock, so a position read at describe time freezes while the
  // convergence moves — and a core beside the convergence is worse than
  // no core.
  Element vanishing =
      box()
          .left(Dim(-150.0f))
          .top(Dim(-150.0f))
          .width(Dim(300.0f))
          .height(Dim(300.0f))
          .translateX(&coreX)
          .translateY(&coreY)
          // The core's own picture never changes — only where it is —
          // so it is baked once and the binding moves the bake. A
          // full-canvas kPlus radial re-evaluated per frame costs this
          // scene more than the two exposures do.
          .cache(Cache::Texture)
          .fill(Paint::glowUnit({0.5f, 0.5f}, 0.5f,
                                {{0.00f, {1.0f, 0.98f, 0.92f, 0.92f}},
                                 {0.12f, {1.0f, 0.94f, 0.80f, 0.42f}},
                                 {0.42f, {0.90f, 0.80f, 0.60f, 0.10f}},
                                 {1.00f, {0.6f, 0.5f, 0.4f, 0.0f}}}))
          .blend(SkBlendMode::kPlus);
  // HALATION. Film's own bloom: light scattering back off the base. The
  // SAME two pools read a second time, tone-curved softer, blurred and
  // added -- so it is still the accumulation, not a painted glow.
  Element halation =
      raw()
          .effect(Effect::shader(transfer, {{"k", transferK() * 0.55f}})
                      .then(Effect::filter(
                          SkImageFilters::Blur(9.0f, 9.0f, nullptr))))
          .blend(SkBlendMode::kPlus)
          .opacity(0.55f);

  auto hud = [&](const std::string& str, float l, float tp, float r, float b,
                 SkColor4f col) {
    Element e = t(str, mono(8, col, 0.6f)).absolute();
    if (l >= 0) e.left(Dim(l));
    if (r >= 0) e.right(Dim(r));
    if (tp >= 0) e.top(Dim(tp));
    if (b >= 0) e.bottom(Dim(b));
    return e;
  };

  const double machineSec = (double)filmNo * 120.0;  // 2880 : 1
  const long long mh = (long long)(machineSec / 3600.0);
  const long long mm = (long long)std::fmod(machineSec / 60.0, 60.0);

  return box()
      .width(Dim(kFilmW))
      .height(Dim(kFilmH))
      .shrink(0)
      .fill(kBlack)
      .clip()
      .stroke(stroke(1.0f, Fill::color(kRule)))
      .key("film")
      .mask(by::edge(
          0.0f, animate(from(0.0f).to(1.0f), {520ms, ch::easeOutCubic, 240ms})))
      .child(std::move(accumulation))
      .child(std::move(vanishing))
      .child(std::move(halation))
      // The shutter bar -- the ONLY thing in the plate driven by
      // addFixed's interpolant, and the caption says why.
      .child(box()
                 .left(Dim(0))
                 .top(Dim(0))
                 .width(Dim(kFilmW))
                 .height(2)
                 .fill(al(kCold, 0.4f))
                 .mask(by::edge(0.0f, bind(&frameAlpha))))
      .child(hud(s.name, 10, 10, -1, -1, al(kCold, 0.75f)))
      .child(
          hud(kit::formatted("FRAME %06lld · 24 fps · %d STAMPS/WALL · kPLUS",
                             filmNo, kKDisplay),
              -1, 10, 10, -1, al(kTick, 0.9f)))
      // The footer is ONE bottom-anchored column, not four absolute rows.
      // Two of these lines are long enough to wrap at this measure, and a
      // row placed by its own bottom offset grows upward into the row above
      // it — the stack has to own the spacing for the wrap to be safe.
      .child(box()
                 .absolute()
                 .left(Dim(10))
                 .right(Dim(10))
                 .bottom(Dim(6))
                 .column()
                 .gap(2)
                 .child(box()
                            .row()
                            .justify(Justify::SpaceBetween)
                            .child(t(kit::formatted(
                                         "MACHINE TIME %lld h %02lld m  @ "
                                         "2880 : 1%s",
                                         mh, mm, everClamped ? "  *" : ""),
                                     mono(8, al(kTick, 0.95f), 0.6f)))
                            .child(t("2.20 : 1 · 65 mm 5-PERF · f/1.8",
                                     mono(8, al(kTick, 0.95f), 0.6f))))
                 .child(t("THE FRAME IS HELD, NOT TWEENED. addFixed’s "
                          "INTERPOLANT DRIVES THE SHUTTER BAR AND NOTHING "
                          "IN THE PICTURE.",
                          mono(8, al(kTick, 0.8f), 0.6f)))
                 .child(t("THE SWEEP BEGINS 5 px FROM THE VANISHING POINT = "
                          "15 FEET FROM THE LENS, AND ENDS AT 600 px = 1½ "
                          "INCHES · THE FRAME IS THE SCAN, 120 : 1, DRAWN "
                          "TO ITS OWN SCALE — THE 5 px HOLE AT THE APEX IS "
                          "THAT FAR LIMIT, VISIBLE",
                          mono(8, al(kType2, 0.95f), 0.6f))));
}

auto SlitScan2001::rigStrip() -> Element {
  using namespace slit;
  return box()
      .width(Dim(kRigW))
      .height(Dim(kRigH))
      .shrink(0)
      .key("rig")
      // BOTH PROGRAMS BELOW ARE KEYLESS ON PURPOSE. They read the
      // carriage's live position and the artwork's live offset as they
      // paint, at Cache::None, so their picture is different every
      // frame; a key would name one drawing and replay it.
      .child(
          custom([this](SkCanvas& c, const PaintContext& p) { drawRig(c, p); })
              .left(Dim(0))
              .top(Dim(0))
              .width(Dim(kElevW))
              .height(Dim(kRigH))
              .clip()
              .cache(Cache::None))
      .child(custom([this](SkCanvas& c, const PaintContext& p) {
               drawArtworkPanel(c, p);
             })
                 .left(Dim(kRigW - kPanelStripW))
                 .top(Dim(0))
                 .width(Dim(kPanelStripW))
                 .height(Dim(kRigH))
                 .cache(Cache::None))
      // The "THIS EXPOSURE" monitor, in the elevation's upper-left where
      // there is nothing but sky. The only place you see a frame BEING
      // MADE rather than made, so it gets the good corner.
      .child(box()
                 .left(Dim(18))
                 .top(Dim(10))
                 .width(264)
                 .height(116)
                 .corners({4})
                 .fill(al(kPanelBg, 0.92f))
                 .stroke(stroke(1.0f, Fill::color(kRule)))
                 .clip()
                 .child(box()
                            .inset(0)
                            .child(instancing::instances(atlas, monA,
                                                         instancing::Mode::Live,
                                                         SkBlendMode::kPlus))
                            .child(instancing::instances(atlas, monB,
                                                         instancing::Mode::Live,
                                                         SkBlendMode::kPlus))
                            .effect(Effect::shader(transfer, {{"k", 2.4f}})))
                 .child(t("THIS EXPOSURE", mono(8, al(kCold, 0.85f), 1.4f))
                            .left(Dim(8))
                            .top(Dim(5)))
                 .child(slot("expo").left(Dim(8)).bottom(Dim(19)))
                 .child(t("ONE SWEEP / 3.0 s. THE MACHINE TOOK 45–60 s "
                          "[C85]. ×18.",
                          mono(7, al(kTick, 0.95f)))
                            .left(Dim(8))
                            .bottom(Dim(6))))
      .child(slot("readout").left(Dim(20)).top(Dim(130)));
}
