#include "WinampBase.h"

auto WinampBase::tracks() -> const std::array<Track, 25>& {
  static const std::array<Track, 25> v = {{
      {"DJ Mike Llama - Llama Whippin' Intro", "0:05", 5},
      {"Nine Inch Nails - The Perfect Drug", "5:15", 315},
      {"The Prodigy - Breathe", "5:35", 335},
      {"Fatboy Slim - Right Here, Right Now", "6:27", 387},
      {"Massive Attack - Teardrop", "5:29", 329},
      {"Portishead - Glory Box", "5:07", 307},
      {"Underworld - Born Slippy .NUXX", "9:44", 584},
      {"The Chemical Brothers - Block Rockin' Beats", "5:15", 315},
      {"Daft Punk - Around The World", "7:09", 429},
      {"Aphex Twin - Windowlicker", "6:07", 367},
      {"Orbital - Halcyon On And On", "9:27", 567},
      {"Leftfield - Phat Planet", "5:33", 333},
      {"Air - Sexy Boy", "4:58", 298},
      {"Boards Of Canada - Roygbiv", "2:31", 151},
      {"Autechre - Gantz Graf", "5:07", 307},
      {"Squarepusher - My Red Hot Car", "4:36", 276},
      {"Photek - Ni Ten Ichi Ryu (Two Swords)", "6:03", 363},
      {"Roni Size / Reprazent - Brown Paper Bag", "5:23", 323},
      {"Goldie - Inner City Life", "7:49", 469},
      {"LTJ Bukem - Horizons", "8:44", 524},
      {"Amon Tobin - Bricolage", "4:23", 263},
      {"DJ Shadow - Midnight In A Perfect World", "4:57", 297},
      {"Mr. Oizo - Flat Beat", "3:47", 227},
      {"Basement Jaxx - Where's Your Head At", "5:37", 337},
      {"Groove Armada - At The River", "3:24", 204},
  }};
  return v;
}

auto WinampBase::rockPreset() -> const std::array<float, 11>& {
  static const std::array<float, 11> v = {0.18f,  0.62f,  0.44f, -0.35f,
                                          -0.55f, -0.15f, 0.30f, 0.60f,
                                          0.72f,  0.72f,  0.70f};
  return v;
}

auto WinampBase::mainWindow() -> Element {
  using namespace wa;
  // The display's lettering is TEXT.BMP's green: the marquee, the readouts
  // and STEREO inherit it; the captions beside them name dimmer colours.
  Element w =
      box().width(Dimension(n(275))).height(Dimension(n(116))).ink(kDisplay);
  // The brushed body, on its own leaf so the bake is a texture and the
  // window's live children never drag the grain shader back per frame.
  w.child(box().inset(0).fill(steel).cache(Cache::Texture));
  raised(w, kWellHi, kWellLo);
  w.child(titleBar(275, "WINAMP", false));

  // ---- the big display well (native x 0..275, y 21..58) ---------------
  Element well = at(box(), 0, 21, 275, 37).fill(lcdMat);
  sunken(well);
  w.child(well);

  // clutter bar O A I D V — its own dark strip, running past the well
  Element clutter = at(box(), 10, 22, 8, 43).fill(hexColor(0x101020));
  sunken(clutter, mskia::withAlpha(hexColor(0x4A4A70), 0.7f),
         hexColor(0x080810));
  static const char* cl[5] = {"O", "A", "I", "D", "V"};
  static const float cy[5] = {3, 11, 18, 25, 33};
  static const float cht[5] = {8, 7, 7, 8, 7};
  for (int i = 0; i < 5; ++i)
    clutter.child(at(box(), 0, cy[i], 8, cht[i])
                      .justify(Justify::Center)
                      .alignItems(Align::Center)
                      .child(t(cl[i], pix(3.4f)).ink(kCaption)));
  // the specular glint that sweeps the stack once every 5 s
  clutter.child(
      at(box(), 0, 0, 8, 6)
          .fill(hexColor(0xCFE4FF, 0.55f))
          .blend(SkBlendMode::kPlus)
          .translateY(motion::bind(&glint).target(-n(6), n(43)))
          .opacity(
              motion::bind(&glint).offset(-0.5f).scale(2.0f).invert().clamp(
                  0.0f, 0.75f)));
  w.child(clutter);

  // play-status LED (native 26,28,9,9)
  Element status = at(box(), 26, 28, 9, 9);
  status.child(at(box(), 0, 1, 3, 7).fill(kGreen).opacity(&led));
  status.child(at(box(), 4, 2, 5, 5).fill(kGreen).opacity(&led).shape(tri(0)));
  w.child(status);

  // MM:SS. In the original the four NUMBERS.BMP digits are 9x13 cells at
  // native x 48/60 and 78/90; here the readout is one 54-wide box holding
  // five equal cells, so the colon gets a cell of its own and the four
  // digits land close to, but not exactly on, those positions. The ghost
  // "88:88" behind them, in VISCOLOR's documented off-segment grey, is
  // what turns green numerals into a display.
  Element clock = at(box(), 45, 26, 54, 13);
  clock.child(at(lcdCells("88:88", kUnlit), 0, 0, 54, 13));
  clock.child(box().inset(0).child(slot("time")));
  w.child(clock);

  // the scrolling track title (TEXT.BMP, 5x6 cell) in its own sunken box.
  // The clip is 9 native px tall, not TEXT.BMP's 6: `pix(5)` sizes the
  // SUBSTITUTED face so its ADVANCE is 5 px, and that face's line box is
  // taller than its advance, so a 6- or 7-px viewport cut the bottom
  // scanline off every round glyph — E read as F, L as I, U as II.
  Element titleWell = at(box(), 109, 22, 158, 11).fill(hexColor(0x101020));
  sunken(titleWell, mskia::withAlpha(hexColor(0x4A4A70), 0.5f),
         hexColor(0x08080E));
  Element title = at(box(), 2, 1, 154, 9).clip();
  title.child(kit::marquee(
      t(marqueeText(), pix(5)),
      {.phase = &marqueePhase, .gap = n(40), .contentWidth = marqueeW}));
  titleWell.child(title);
  w.child(titleWell);

  // kbps / kHz readouts — each a small bordered window with its unit
  // printed outside it, exactly as MAIN.BMP bakes them.
  auto readout = [&](float x, float wN, const char* v) {
    Element e = at(box(), x, 41, wN, 9).fill(hexColor(0x101020));
    sunken(e, mskia::withAlpha(hexColor(0x4A4A70), 0.5f), hexColor(0x08080E));
    e.child(at(box(), 1, 2, wN - 2, 6)
                .justify(Justify::End)
                .alignItems(Align::Center)
                .child(t(v, pix(4.6f))));
    return e;
  };
  w.child(readout(111, 17, "192"));
  w.child(at(box(), 130, 43, 20, 6)
              .alignItems(Align::Center)
              .child(t("kbps", pix(4)).ink(hexColor(0x6E6E9A))));
  w.child(readout(154, 13, "44"));
  w.child(at(box(), 169, 43, 18, 6)
              .alignItems(Align::Center)
              .child(t("kHz", pix(4)).ink(hexColor(0x6E6E9A))));
  w.child(at(box(), 212, 41, 28, 12)
              .justify(Justify::Center)
              .alignItems(Align::Center)
              .child(t("MONO", pix(4.4f)).ink(hexColor(0x3A3A5C))));
  w.child(at(box(), 240, 41, 29, 12)
              .justify(Justify::Center)
              .alignItems(Align::Center)
              .child(t("STEREO", pix(4.0f))));

  // ---- the spectrum analyser well (native 24,43,76,16) ---------------
  Element vis = at(box(), 24, 43, 76, 16).fill(hexColor(0x000000));
  sunken(vis, mskia::withAlpha(hexColor(0x4A4A70), 0.6f), hexColor(0x08080E));
  vis.child(box().inset(0).fill(visDots.material()));
  // ONE atlas stamp for 19x16 LED segments plus 19 peak-hold dots.
  vis.child(box().inset(0).child(instancing::instances(
      ledAtlas, ledPool, instancing::Mode::Live, SkBlendMode::kPlus)));
  w.child(vis);

  // The power-on tic: two 60 ms blinks before the display settles lit.
  // A shutter over the whole well, keyframed with easeNone so each step
  // is a hard cut — old displays do not fade in.
  w.child(at(box(), 0, 21, 275, 37)
              .fill(hexColor(0x090911))
              .opacity(animate(motion::through({{0ms, 1.0f},
                                                {300ms, 1.0f},
                                                {310ms, 0.0f},
                                                {360ms, 0.0f},
                                                {370ms, 1.0f},
                                                {420ms, 1.0f},
                                                {430ms, 0.0f}}),
                               &ch::easeNone)));

  // ---- volume / balance / EQ+PL toggles -------------------------------
  w.child(box()
              .left(Dimension(n(107)))
              .top(Dimension(n(57)))
              .width(Dimension(n(108)))
              .height(Dimension(n(13)))
              .child(slot("sliders")));

  w.child(eqPlToggle());

  // ---- position / seek bar (native 16,72,248,10) ----------------------
  Element pos = at(box(), 16, 72, 248, 10).fill(hexColor(0x14141F));
  sunken(pos, mskia::withAlpha(hexColor(0x4A4A70), 0.7f), hexColor(0x08080E));
  // Two of playPos's consumers, both here: the elapsed underlay's scaleX …
  pos.child(at(box(), 1, 1, 246, 8)
                .fill(hexColor(0x24243A))
                .transformOrigin(0, 0.5f)
                .scaleX(&playPos));
  // … and the thumb, in pixels.
  Element thumb =
      at(box(), 1, 0, 29, 10)
          .fill(
              mskia::Paint::linearUnit({0, 0}, {0, 1},
                                       {{0.0f, mskia::lighten(kBtnFace, 0.12f)},
                                        {1.0f, dark(kBtnFace, 0.28f)}}))
          .translateX(motion::bind(&playPos).target(0, n(248 - 31)));
  raised(thumb);
  thumb.child(at(box(), 13, 2, 1, 6).fill(mskia::withAlpha(kBtnLo, 0.8f)));
  thumb.child(at(box(), 15, 2, 1, 6).fill(mskia::withAlpha(kBtnHi, 0.7f)));
  pos.child(thumb);
  w.child(pos);

  // ---- the six transport keys + shuffle / repeat ----------------------
  w.child(transportRow());

  // the baked Nullsoft bolt, bottom right
  w.child(at(box(), 253, 91, 13, 15)
              .shape(bolt())
              .fill(mskia::Paint::linearUnit(
                  {0, 0}, {0, 1},
                  // MAIN.BMP's bolt is a muted orange-brown, not the
                  // bright yellow a lightning glyph wants to be.
                  {{0.0f, hexColor(0xC98A32)}, {1.0f, hexColor(0x7A4208)}})));
  return w;
}

auto WinampBase::eqPlToggle() -> Element {
  using namespace wa;
  Element g = at(box(), 219, 58, 46, 12);
  auto tog = [&](float x, const char* lbl, bool on) {
    Element e = key(x, 0, 23, 12, box());
    e.row().alignItems(Align::Center).padding(n(2), 0, 0, 0);
    e.child(box()
                .width(Dimension(n(3)))
                .height(Dimension(n(3)))
                .fill(on ? wa::kGreen : hexColor(0x3C4A58)));
    e.child(box().width(Dimension(n(1.5f))));
    e.child(t(lbl, pix(4.2f)));
    return e;
  };
  g.child(tog(0, "EQ", true));
  g.child(tog(23, "PL", true));
  return g;
}

auto WinampBase::transportRow() -> Element {
  using namespace wa;
  Element r = at(box(), 0, 88, 275, 28);

  // prev |<<
  Element prev = box();
  prev.child(part(4, 5, 2, 8));
  prev.child(part(7, 5, 6, 8, tri(1)));
  prev.child(part(13, 5, 6, 8, tri(1)));
  r.child(key(16, 0, 23, 18, prev));
  // play >
  Element play = box();
  play.child(part(8, 4, 8, 10, tri(0)));
  r.child(key(39, 0, 23, 18, play));
  // pause ||
  Element pause = box();
  pause.child(part(8, 5, 3, 8));
  pause.child(part(13, 5, 3, 8));
  r.child(key(62, 0, 23, 18, pause));
  // stop []
  Element stop = box();
  stop.child(part(8, 5, 8, 8));
  r.child(key(85, 0, 23, 18, stop));
  // next >>|
  Element next = box();
  next.child(part(4, 5, 6, 8, tri(0)));
  next.child(part(10, 5, 6, 8, tri(0)));
  next.child(part(17, 5, 2, 8));
  r.child(key(108, 0, 22, 18, next));
  // eject
  Element eject = box();
  eject.child(part(7, 3, 8, 5, tri(2)));
  eject.child(part(7, 9, 8, 3));
  r.child(key(136, 1, 22, 16, eject));

  // SHUFFLE (47x15) and REP (28x15) — SHUFREP.BMP; the real CSS lets
  // these two sum to 75 native inside a 74-wide container, a 1 px
  // rounding slop in the ORIGINAL layout, kept.
  Element shuf = key(164, 1, 47, 15, box());
  shuf.row().alignItems(Align::Center).padding(n(3), 0, 0, 0);
  shuf.child(box()
                 .width(Dimension(n(3)))
                 .height(Dimension(n(3)))
                 .fill(hexColor(0x3C4A58)));
  shuf.child(box().width(Dimension(n(2))));
  shuf.child(t("SHUFFLE", pix(4.6f)));
  r.child(shuf);

  Element rep = key(211, 1, 28, 15, box());
  rep.row().alignItems(Align::Center).padding(n(3), 0, 0, 0);
  rep.child(
      box().width(Dimension(n(3))).height(Dimension(n(3))).fill(wa::kGreen));
  rep.child(box().width(Dimension(n(2))));
  rep.child(t("REP", pix(4.6f)));
  r.child(rep);

  // the "hardware self-test" light sweep — a single 150 ms pass over the
  // six keys with NO easing, entirely as mount keyframes.
  r.child(
      at(box(), 0, 0, 8, 18)
          .fill(hexColor(0xE8F4FF, 0.55f))
          .blend(SkBlendMode::kPlus)
          .translateX(
              animate(motion::through({{600ms, n(10)}, {750ms, n(162)}}),
                      &ch::easeNone))
          .opacity(animate(
              motion::through(
                  {{590ms, 0.0f}, {600ms, 1.0f}, {735ms, 1.0f}, {750ms, 0.0f}}),
              &ch::easeNone)));
  return r;
}

auto WinampBase::sliders(int vol, int bal) -> Element {
  using namespace wa;
  Element g = box();

  const SkColor4f volColor = kVis[(size_t)std::clamp((vol * 15) / 28, 0, 15)];
  Element track = at(box(), 0, 0, 68, 13).fill(hexColor(0x1B1B2C));
  sunken(track, mskia::withAlpha(hexColor(0x4A4A70), 0.6f), hexColor(0x0A0A12));
  track.child(at(box(), 1, 3, 66, 3).fill(dark(volColor, 0.45f)));
  track.child(at(box(), 1, 6, 66, 3).fill(volColor));
  track.child(at(box(), 1, 9, 66, 2).fill(dark(volColor, 0.65f)));
  Element vt = at(box(), 0, 1, 14, 11)
                   .fill(mskia::Paint::linearUnit(
                       {0, 0}, {0, 1},
                       {{0.0f, mskia::lighten(kBtnFace, 0.12f)},
                        {1.0f, dark(kBtnFace, 0.30f)}}))
                   .translateX(n((68.0f - 14.0f) * (float)vol / 28.0f));
  raised(vt);
  vt.child(at(box(), 6, 2, 1, 7).fill(mskia::withAlpha(kBtnLo, 0.85f)));
  track.child(vt);
  g.child(track);

  // BALANCE.BMP, same 28-frame mechanism, but read as a distance from
  // centre: the colour ramps outward in both directions from frame 14.
  const int b = std::abs(bal - 14);
  const SkColor4f balColor = kVis[(size_t)std::clamp((b * 15) / 14, 0, 15)];
  Element btr = at(box(), 70, 0, 38, 13).fill(hexColor(0x1B1B2C));
  sunken(btr, mskia::withAlpha(hexColor(0x4A4A70), 0.6f), hexColor(0x0A0A12));
  btr.child(at(box(), 1, 4, 36, 5).fill(dark(balColor, 0.55f)));
  btr.child(at(box(), 18, 1, 2, 11).fill(mskia::withAlpha(balColor, 0.9f)));
  Element bt = at(box(), 0, 1, 14, 11)
                   .fill(mskia::Paint::linearUnit(
                       {0, 0}, {0, 1},
                       {{0.0f, mskia::lighten(kBtnFace, 0.12f)},
                        {1.0f, dark(kBtnFace, 0.30f)}}))
                   .translateX(n((38.0f - 14.0f) * (float)bal / 28.0f));
  raised(bt);
  bt.child(at(box(), 6, 2, 1, 7).fill(mskia::withAlpha(kBtnLo, 0.85f)));
  btr.child(bt);
  g.child(btr);
  return g;
}

auto WinampBase::marqueeText() -> std::string {
  const Track& tr = tracks()[(size_t)nowPlaying];
  std::string s = std::to_string(nowPlaying + 1);
  s += ". ";
  s += tr.title;
  s += "  ***  ";
  for (char& c : s)
    c = (char)std::toupper((unsigned char)c);  // TEXT.BMP has no lowercase
  return s;
}

auto WinampBase::lcdCells(const std::string& s, SkColor4f ink) const
    -> Element {
  using namespace wa;
  const float pitch = n(54) / (s.empty() ? 1.0f : (float)s.size());
  Element row =
      box().row().width(Dimension(n(54))).height(Dimension(n(13))).ink(ink);
  for (char ch : s) {
    Element cell = box()
                       .width(Dimension(pitch))
                       .shrink(0)
                       .justify(Justify::Center)
                       .alignItems(Align::Center);
    if (ch != ' ') cell.child(t(std::string(1, ch), pix(10)));
    row.child(std::move(cell));
  }
  return row;
}
