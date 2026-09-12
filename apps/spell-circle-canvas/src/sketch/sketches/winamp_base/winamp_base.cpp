// winamp base: scene assembly and animation.

#include "WinampBase.h"

auto WinampBase::describe() -> Element {
  using namespace wa;
  Element root = stack().width(Dimension(1320)).height(Dimension(1947));

  // the desktop: flat teal + one baked dither pass
  root.child(box().inset(0).fill(deskMat).cache(Cache::Texture));

  // Main — pops in at its final position, scale 0.9 -> 1 on outBack.
  root.child(
      mainWindow()
          .left(Dimension(60))
          .top(Dimension(60))
          .transformOrigin(0.5f, 0.5f)
          .scale(animate(motion::from(0.9f).to(1.0f),
                         {200ms, motion::ease::outBack(), 100ms}))
          .opacity(animate(
              motion::through({{0ms, 0.0f}, {99ms, 0.0f}, {100ms, 1.0f}}),
              &ch::easeNone)));

  // Equalizer — docking snap from 60 px above, the same outBack value.
  root.child(
      eqWindow()
          .left(Dimension(60))
          .top(Dimension(408))
          .translateY(animate(motion::from(-60.0f).to(0.0f),
                              {250ms, motion::ease::outBack(), 900ms}))
          .opacity(animate(
              motion::through({{0ms, 0.0f}, {899ms, 0.0f}, {900ms, 1.0f}}),
              &ch::easeNone)));

  // Playlist — same snap, 1.25 s later.
  root.child(
      playlistWindow()
          .left(Dimension(60))
          .top(Dimension(756))
          .translateY(animate(motion::from(-60.0f).to(0.0f),
                              {250ms, motion::ease::outBack(), 2150ms}))
          .opacity(animate(
              motion::through({{0ms, 0.0f}, {2149ms, 0.0f}, {2150ms, 1.0f}}),
              &ch::easeNone)));
  return root;
}

auto WinampBase::setup(sketch::SketchContext& ctx) -> void {
  // Inside the llama beat, which runs 7.0-7.8 s: past its bounce and
  // before its fade. Every other second of the loop is missing it.
  using namespace wa;
  // The plate at exactly 2x. One native skin pixel is three canvas px and
  // six device px, so every BMP cell — the 5x6 TEXT.BMP glyph included —
  // keeps its edges on whole pixels.
  sketch::kit::stage(ctx, {.size = {1320, 1947},
                           .captureAt = 7.4,
                           .background = kDesk,
                           .oversample = 2});
  buildMaterials();

  // Declaring again runs this on the same instance over an empty tree, so
  // forget what the discrete slots were last pushed for: the values they
  // are compared against have to describe THIS declaration's tree.
  volSprite = balSprite = -1;
  shownSec = -1;
  lastNow = lastSel = -1;

  // The substituted monospace faces, measured rather than assumed.
  {
    auto probe = [&](const sk_sp<SkTypeface>& tf) {
      const float w =
          ctx.measure(text(u8"MMMMMMMMMM", type(tf, 100.0f, kGreen))).width();
      return w > 1.0f ? w / 1000.0f : 0.602f;
    };
    monoEm = probe(mono());
    boldEm = probe(monoBold());
  }

  // --- the LED atlas: ONE cell, a 3x1 native quad, tinted per instance.
  ledAtlas = std::make_shared<instancing::Atlas>(2.0f);
  ledAtlas->cell(box().fill(SkColor4f{1, 1, 1, 1}), {n(3), n(1)});
  ledPool = std::make_shared<instancing::Pool>();
  ledPool->resize((size_t)kCols * (size_t)kRows + (size_t)kCols);
  {
    auto pos = ledPool->positions();
    auto tint = ledPool->tints();
    // Not arrange::cellAt: this analyser is indexed COLUMN-major and
    // its rows count up from the floor, which is neither of the two
    // things that function answers.
    for (int c = 0; c < kCols; ++c)
      for (int r = 0; r < kRows; ++r) {
        const size_t i = (size_t)c * (size_t)kRows + (size_t)r;
        pos[i] = {n(4.0f * (float)c + 1.5f), n((float)(kRows - 1 - r) + 0.5f)};
        tint[i] = mskia::withAlpha(kVis[(size_t)r], 0.0f);
      }
    for (int c = 0; c < kCols; ++c) {
      const size_t i = (size_t)kCols * (size_t)kRows + (size_t)c;
      pos[i] = {n(4.0f * (float)c + 1.5f), n(0.5f)};
      tint[i] = mskia::withAlpha(kPeak, 0.0f);
    }
  }

  // --- playlist row backgrounds: three tint states, one stamp.
  rowAtlas = std::make_shared<instancing::Atlas>(1.0f);
  rowAtlas->cell(box().fill(SkColor4f{1, 1, 1, 1}), {n(368), n(13)});
  rowPool = std::make_shared<instancing::Pool>();
  rowPool->resize(25);
  {
    auto pos = rowPool->positions();
    for (int i = 0; i < 25; ++i)
      pos[(size_t)i] = {n(184), n(13.0f * (float)i + 6.5f)};
  }

  // --- the marquee wants its content width measured once. A one-shot
  // measure resolves no cascade, so the run is handed the whole style.
  marqueeW = ctx.measure(text(toUtf8(marqueeText()), weave::textStyle(pix(5))))
                 .width();
  if (marqueeW < 1) marqueeW = n(300);

  // --- one steppable drives every idle loop.
  ctx.ticker.add([this](double dt) {
    step(dt);
    return true;
  });

  ctx.composer.render(describe());
  pushSlots(ctx, true);
}

auto WinampBase::pushSlots(sketch::SketchContext& ctx, bool force) -> void {
  const int sec =
      (int)(playPos.value() * (float)tracks()[(size_t)nowPlaying].seconds);
  if (force || sec != shownSec) {
    shownSec = sec;
    ctx.composer.renderSlot("time", timeReadout(sec));
  }
  const int v = (int)std::lround(volFrame.value());
  const int b = (int)std::lround(balFrame.value());
  if (force || v != volSprite || b != balSprite) {
    volSprite = v;
    balSprite = b;
    ctx.composer.renderSlot("sliders", sliders(v, b));
  }
  if (force || nowPlaying != lastNow || selected != lastSel) {
    lastNow = nowPlaying;
    lastSel = selected;
    ctx.composer.renderSlot("tracks", trackList());
  }
}

auto WinampBase::hash(int a, int b) -> float {
  // The lattice mixer, asked for a plane of it: a third coordinate of
  // zero adds nothing to the sum.
  const uint32_t h = sigil::core::noise::lattice(0, a, b, 0);
  return (float)(h & 0xffffffu) / (float)0xffffff;
}

auto WinampBase::step(double dt) -> void {
  using namespace wa;
  const double t = (elapsedNow += dt);

  // ---- playback position: the one source every readout reads ---------
  const float len = (float)tracks()[(size_t)nowPlaying].seconds;
  playPos = std::fmod(0.42f + (float)std::max(0.0, t - 0.45) / len, 1.0f);

  // ---- play LED, from 0.45 s -----------------------------------------
  led = t > 0.45 ? 1.0f : 0.0f;

  // ---- marquee: the title holds through the boot sequence, then runs
  // at the real ~30 native px/s with a 1 s pause at the loop seam ------
  {
    const float span = marqueeW + n(40);
    const double period = (double)(span / n(30)) + 1.0;
    const double u = std::fmod(std::max(0.0, t - 4.0), period);
    const double run = (double)(span / n(30));
    marqueePhase = u < run ? -(float)(u * n(30)) : -span;
  }

  // ---- the eleven EQ gains: staggered elastic to the Rock preset -----
  for (int i = 0; i < 11; ++i) {
    const double start = 1.15 + 0.04 * (double)i;
    const float target = rockPreset()[(size_t)i];
    float v = 0.0f;
    if (t > start) {
      const float u = (float)std::min(1.0, (t - start) / 0.70);
      v = target * ch::easeOutElastic(u, 1.0f, 0.4f);
      if (t > start + 0.70) {
        // "live audio": an 8 Hz quantised wobble on the settled preset
        const int k = (int)std::floor((t - start) * 8.0);
        v = target + (hash(i * 7 + 3, k) - 0.5f) * 0.055f;
      }
    }
    gain[(size_t)i] = std::clamp(v, -1.0f, 1.0f);
  }
  graphDraw = (float)std::clamp((t - 1.85) / 0.30, 0.0, 1.0);

  // ---- the 28-frame sliders: quantisation, not a lerp -----------------
  // The volume steps once, silent to 78%, at 3.5 s; the balance holds at
  // centre for the whole run. Rounding to a frame index here is what makes
  // the thumbs land on sprite positions rather than sliding between them.
  {
    const float pctV = t < 3.5 ? 0.0f : 0.78f;
    const float pctB = 0.5f;
    // The player's own mapping, round(pct * 28): 29 positions 0..28, so
    // 50% lands exactly on the centre frame 14 and the two ends are
    // symmetric about it.
    volFrame = std::round(pctV * 28.0f);
    balFrame = std::round(pctB * 28.0f);
  }

  // ---- the analyser: hard 12 Hz steps, no easing anywhere ------------
  if (t >= 3.2) {
    // WHICH step, not the held seconds: the levels are reseeded once
    // per tick and the seed is the tick's own number, so `stepIndex` is
    // the verb and `quantizeTime` — which re-emits t — is not.
    const long long stepT = motion::stepIndex(t, 12.0);
    if (stepT != lastRoll) {
      lastRoll = stepT;
      for (int c = 0; c < kCols; ++c) {
        const float shape =
            0.30f + 0.70f * std::sin(3.14159f * (float)c / (float)kCols);
        const float a = hash(c, (int)stepT);
        const float bb = hash(c * 3 + 11, (int)stepT / 3);
        const float lvl = (0.18f + 0.82f * a * (0.35f + 0.65f * bb)) * shape;
        colLevel[(size_t)c] = lvl * (float)kRows;
        colPeak[(size_t)c] =
            std::max(colPeak[(size_t)c] - 0.35f, colLevel[(size_t)c]);
      }
    }
  }
  {
    auto tint = ledPool->tints();
    auto pos = ledPool->positions();
    for (int c = 0; c < kCols; ++c) {
      const int lit = (int)colLevel[(size_t)c];
      for (int r = 0; r < kRows; ++r) {
        const size_t i = (size_t)c * (size_t)kRows + (size_t)r;
        tint[i] = mskia::withAlpha(kVis[(size_t)r], r < lit ? 1.0f : 0.0f);
      }
      const size_t pi = (size_t)kCols * (size_t)kRows + (size_t)c;
      const float pk = colPeak[(size_t)c];
      pos[pi] = {n(4.0f * (float)c + 1.5f),
                 n((float)kRows - std::clamp(pk, 0.0f, (float)kRows) + 0.5f)};
      tint[pi] = mskia::withAlpha(kPeak, pk > 0.6f ? 0.85f : 0.0f);
    }
  }

  // ---- playlist rows: reveal in bands of four ------------------------
  {
    auto tint = rowPool->tints();
    for (int i = 0; i < 25; ++i) {
      const int group = i / 4;
      const double start = 2.4 + 0.13 * (double)group;
      const float u = (float)std::clamp((t - start) / 0.13, 0.0, 1.0);
      rowIn[(size_t)i] = u * u;
      SkColor4f c = i == selected     ? kPlSel
                    : i == nowPlaying ? SkColor4f{1, 1, 1, 0.10f}
                                      : SkColor4f{0, 0, 0, 0};
      c.fA *= u;
      tint[(size_t)i] = c;
    }
  }

  // ---- the clutter-bar glint, once every 5 s -------------------------
  glint =
      (float)std::fmod(t, 5.0) < 0.2 ? (float)(std::fmod(t, 5.0) / 0.2) : 0.0f;

  // ---- the LLAMA easter egg, every ~9 s for 800 ms -------------------
  {
    const double u = std::fmod(t + 2.0, 9.0);
    if (u < 0.8) {
      const float f = (float)(u / 0.8);
      llama = f < 0.12f ? f / 0.12f : (f > 0.88f ? (1.0f - f) / 0.12f : 1.0f);
      llamaPop = 0.85f + 0.15f * ch::easeOutBounce(std::min(1.0f, f * 4.0f));
    } else {
      llama = 0.0f;
      llamaPop = 1.0f;
    }
  }
}

SIGIL_SKETCH(WinampBase, "Study \xc2\xb7 Screens",
             "Winamp 2.91's Base skin \xe2\x80\x94 a bitmap skin as generated "
             "material, 28 quantised frames")
