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
  // the clutter bar O A I D V, each letter in its own cell of the strip
  struct Clutter {
    const char* letter;
    float y, h;
  };
  const Clutter kClutter[5] = {
      {"O", 3, 8}, {"A", 11, 7}, {"I", 18, 7}, {"D", 25, 8}, {"V", 33, 7}};
  // kbps / kHz readouts — each a small bordered window with its unit printed
  // outside it, exactly as MAIN.BMP bakes them
  const auto readout = [this](float x, float wN, const char* v) {
    return sunken(at(box(), x, 41, wN, 9).fill(hexColor(0x101020)),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.5f),
                  hexColor(0x08080E))
        .children({at(box(), 1, 2, wN - 2, 6)
                       .justifyContent(Justify::End)
                       .alignItems(Align::Center)
                       .children({t(v, pix(4.6f))})});
  };
  const auto unit = [this](float x, float wN, const char* words) {
    return at(box(), x, 43, wN, 6)
        .alignItems(Align::Center)
        .children({t(words, pix(4)).ink(hexColor(0x6E6E9A))});
  };
  const auto channels = [this](float x, float wN, const char* words, float size,
                               std::optional<SkColor4f> ink) {
    Element run = t(words, pix(size));
    if (ink) run.ink(*ink);
    return at(box(), x, 41, wN, 12)
        .alignItems(Align::Center)
        .justifyContent(Justify::Center)
        .children({std::move(run)});
  };
  // The display's lettering is TEXT.BMP's green: the marquee, the readouts and
  // STEREO inherit it; the captions beside them name dimmer colours.
  return raised(box().width(n(275)).height(n(116)).ink(kDisplay), kWellHi,
                kWellLo)
      .children(
          {// the brushed body, on its own leaf so the bake is a texture and
           // the window's live children never drag the grain shader back per
           // frame
           box().inset(0).fill(steel).cache(Cache::Texture),
           titleBar(275, "WINAMP", false),
           // the big display well (native x 0..275, y 21..58)
           sunken(at(box(), 0, 21, 275, 37).fill(lcdMat)),
           // the clutter bar on its own dark strip, running past the well,
           // with the specular glint that sweeps the stack once every 5 s
           sunken(at(box(), 10, 22, 8, 43).fill(hexColor(0x101020)),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.7f),
                  hexColor(0x080810))
               .children(
                   {each(kClutter,
                         [this](const Clutter& c) {
                           return at(box(), 0, c.y, 8, c.h)
                               .alignItems(Align::Center)
                               .justifyContent(Justify::Center)
                               .children(
                                   {t(c.letter, pix(3.4f)).ink(kCaption)});
                         }),
                    at(box(), 0, 0, 8, 6)
                        .fill(hexColor(0xCFE4FF, 0.55f))
                        .blendMode(SkBlendMode::kPlus)
                        .translateY(motion::bind(&glint).target(-n(6), n(43)))
                        .opacity(motion::bind(&glint)
                                     .offset(-0.5f)
                                     .scale(2.0f)
                                     .invert()
                                     .clamp(0.0f, 0.75f))}),
           // play-status LED (native 26,28,9,9)
           at(box(), 26, 28, 9, 9)
               .children({at(box(), 0, 1, 3, 7).fill(kGreen).opacity(&led),
                          at(box(), 4, 2, 5, 5)
                              .fill(kGreen)
                              .opacity(&led)
                              .shape(tri(0))}),
           // MM:SS. In the original the four NUMBERS.BMP digits are 9x13 cells
           // at native x 48/60 and 78/90; here the readout is one 54-wide box
           // holding five equal cells, so the colon gets a cell of its own and
           // the four digits land close to, but not exactly on, those
           // positions. The ghost "88:88" behind them, in VISCOLOR's
           // documented off-segment grey, is what turns green numerals into a
           // display.
           at(box(), 45, 26, 54, 13)
               .children({at(lcdCells("88:88", kUnlit), 0, 0, 54, 13),
                          box().inset(0).children({slot("time")})}),
           // the scrolling track title (TEXT.BMP, 5x6 cell) in its own sunken
           // box. The clip is 9 native px tall, not TEXT.BMP's 6: `pix(5)`
           // sizes the SUBSTITUTED face so its ADVANCE is 5 px, and that
           // face's line box is taller than its advance, so a 6- or 7-px
           // viewport cut the bottom scanline off every round glyph — E read
           // as F, L as I, U as II.
           sunken(at(box(), 109, 22, 158, 11).fill(hexColor(0x101020)),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.5f),
                  hexColor(0x08080E))
               .children(
                   {at(box(), 2, 1, 154, 9)
                        .overflow(Overflow::Clip)
                        .children({kit::marquee(t(marqueeText(), pix(5)),
                                                {.phase = &marqueePhase,
                                                 .gap = n(40),
                                                 .contentWidth = marqueeW})})}),
           readout(111, 17, "192"), unit(130, 20, "kbps"),
           readout(154, 13, "44"), unit(169, 18, "kHz"),
           channels(212, 28, "MONO", 4.4f, hexColor(0x3A3A5C)),
           channels(240, 29, "STEREO", 4.0f, std::nullopt),
           // the spectrum analyser well (native 24,43,76,16), with ONE atlas
           // stamp for 19x16 LED segments plus 19 peak-hold dots
           sunken(at(box(), 24, 43, 76, 16).fill(hexColor(0x000000)),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.6f),
                  hexColor(0x08080E))
               .children({box().inset(0).fill(visDots.material()),
                          box().inset(0).children({instancing::instances(
                              ledAtlas, ledPool, instancing::Mode::Live,
                              SkBlendMode::kPlus)})}),
           // The power-on tic: two 60 ms blinks before the display settles
           // lit. A shutter over the whole well, keyframed with easeNone so
           // each step is a hard cut — old displays do not fade in.
           at(box(), 0, 21, 275, 37)
               .fill(hexColor(0x090911))
               .opacity(animate(motion::through({{0ms, 1.0f},
                                                 {300ms, 1.0f},
                                                 {310ms, 0.0f},
                                                 {360ms, 0.0f},
                                                 {370ms, 1.0f},
                                                 {420ms, 1.0f},
                                                 {430ms, 0.0f}}),
                                &ch::easeNone)),
           // volume / balance, and the EQ+PL toggles
           at(box(), 107, 57, 108, 13).children({slot("sliders")}),
           eqPlToggle(),
           // position / seek bar (native 16,72,248,10). Two of playPos's
           // consumers are here: the elapsed underlay's scaleX, and the thumb
           // in pixels.
           sunken(at(box(), 16, 72, 248, 10).fill(hexColor(0x14141F)),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.7f),
                  hexColor(0x08080E))
               .children(
                   {at(box(), 1, 1, 246, 8)
                        .fill(hexColor(0x24243A))
                        .transformOrigin(pct(0), pct(50))
                        .scaleX(&playPos),
                    raised(at(box(), 1, 0, 29, 10)
                               .fill(mskia::Paint::linearUnit(
                                   {0, 0}, {0, 1},
                                   {{0.0f, mskia::lighten(kBtnFace, 0.12f)},
                                    {1.0f, dark(kBtnFace, 0.28f)}}))
                               .translateX(motion::bind(&playPos).target(
                                   0, n(248 - 31))))
                        .children(
                            {at(box(), 13, 2, 1, 6)
                                 .fill(mskia::withAlpha(kBtnLo, 0.8f)),
                             at(box(), 15, 2, 1, 6)
                                 .fill(mskia::withAlpha(kBtnHi, 0.7f))})}),
           // the six transport keys + shuffle / repeat
           transportRow(),
           // the baked Nullsoft bolt, bottom right — MAIN.BMP's bolt is a
           // muted orange-brown, not the bright yellow a lightning glyph
           // wants to be
           at(box(), 253, 91, 13, 15)
               .shape(bolt())
               .fill(mskia::Paint::linearUnit(
                   {0, 0}, {0, 1},
                   {{0.0f, hexColor(0xC98A32)}, {1.0f, hexColor(0x7A4208)}}))});
}

auto WinampBase::lamp(float x, float y, float w, float h, const char* label,
                      bool on, float pad, float size) -> Element {
  using namespace wa;
  // A LAMP AND ITS WORD IS ONE CONTROL, and the skin has five of them: the
  // two window toggles and the two play modes wear exactly this, a 3 px
  // square lit or dead with the name printed after it.
  return key(x, y, w, h, box())
      .row()
      .alignItems(Align::Center)
      .padding(0, 0, 0, n(pad))
      .children({box().width(n(3)).height(n(3)).fill(on ? wa::kGreen
                                                        : hexColor(0x3C4A58)),
                 box().width(n(pad < 3 ? 1.5f : 2.0f)), t(label, pix(size))});
}

auto WinampBase::eqPlToggle() -> Element {
  using namespace wa;
  return at(box(), 219, 58, 46, 12)
      .children({lamp(0, 0, 23, 12, "EQ", true, 2, 4.2f),
                 lamp(23, 0, 23, 12, "PL", true, 2, 4.2f)});
}

auto WinampBase::transportRow() -> Element {
  using namespace wa;
  // THE SIX TRANSPORT KEYS, each a key box over the parts its glyph is drawn
  // from — a transport glyph SPANS its whole key, so every part stands in that
  // key's own native coordinates.
  struct Transport {
    float x, y, w, h;
    std::vector<Element> glyph;
  };
  const std::vector<Transport> keys = {
      {16,
       0,
       23,
       18,
       {part(4, 5, 2, 8), part(7, 5, 6, 8, tri(1)),
        part(13, 5, 6, 8, tri(1))}},                           // prev |<<
      {39, 0, 23, 18, {part(8, 4, 8, 10, tri(0))}},            // play >
      {62, 0, 23, 18, {part(8, 5, 3, 8), part(13, 5, 3, 8)}},  // pause ||
      {85, 0, 23, 18, {part(8, 5, 8, 8)}},                     // stop []
      {108,
       0,
       22,
       18,
       {part(4, 5, 6, 8, tri(0)), part(10, 5, 6, 8, tri(0)),
        part(17, 5, 2, 8)}},                                            // next
      {136, 1, 22, 16, {part(7, 3, 8, 5, tri(2)), part(7, 9, 8, 3)}}};  // eject
  return at(box(), 0, 88, 275, 28)
      .children(
          {each(keys,
                [this](const Transport& k) {
                  return key(k.x, k.y, k.w, k.h, box().children(k.glyph));
                }),
           // SHUFFLE (47x15) and REP (28x15) — SHUFREP.BMP; the real CSS lets
           // these two sum to 75 native inside a 74-wide container, a 1 px
           // rounding slop in the ORIGINAL layout, kept.
           lamp(164, 1, 47, 15, "SHUFFLE", false, 3, 4.6f),
           lamp(211, 1, 28, 15, "REP", true, 3, 4.6f),
           // the "hardware self-test" light sweep — a single 150 ms pass over
           // the six keys with NO easing, entirely as mount keyframes
           at(box(), 0, 0, 8, 18)
               .fill(hexColor(0xE8F4FF, 0.55f))
               .blendMode(SkBlendMode::kPlus)
               .translateX(
                   animate(motion::through({{600ms, n(10)}, {750ms, n(162)}}),
                           &ch::easeNone))
               .opacity(animate(motion::through({{590ms, 0.0f},
                                                 {600ms, 1.0f},
                                                 {735ms, 1.0f},
                                                 {750ms, 0.0f}}),
                                &ch::easeNone))});
}

auto WinampBase::sliders(int vol, int bal) -> Element {
  using namespace wa;
  // THE THUMB both sliders share: a raised cap at its frame's own position,
  // with one scored line down it.
  const auto thumb = [](float travel, int frame) {
    return raised(at(box(), 0, 1, 14, 11)
                      .fill(mskia::Paint::linearUnit(
                          {0, 0}, {0, 1},
                          {{0.0f, mskia::lighten(kBtnFace, 0.12f)},
                           {1.0f, dark(kBtnFace, 0.30f)}}))
                      .translateX(n((travel - 14.0f) * (float)frame / 28.0f)))
        .children(
            {at(box(), 6, 2, 1, 7).fill(mskia::withAlpha(kBtnLo, 0.85f))});
  };
  const SkColor4f volColor = kVis[(size_t)std::clamp((vol * 15) / 28, 0, 15)];
  // BALANCE.BMP, the same 28-frame mechanism, but read as a DISTANCE FROM
  // CENTRE: the colour ramps outward in both directions from frame 14.
  const int b = std::abs(bal - 14);
  const SkColor4f balColor = kVis[(size_t)std::clamp((b * 15) / 14, 0, 15)];
  const auto trough = [](float x, float wN) {
    return sunken(at(box(), x, 0, wN, 13).fill(hexColor(0x1B1B2C)),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.6f),
                  hexColor(0x0A0A12));
  };
  return box().children(
      {trough(0, 68).children(
           {at(box(), 1, 3, 66, 3).fill(dark(volColor, 0.45f)),
            at(box(), 1, 6, 66, 3).fill(volColor),
            at(box(), 1, 9, 66, 2).fill(dark(volColor, 0.65f)),
            thumb(68.0f, vol)}),
       trough(70, 38).children(
           {at(box(), 1, 4, 36, 5).fill(dark(balColor, 0.55f)),
            at(box(), 18, 1, 2, 11).fill(mskia::withAlpha(balColor, 0.9f)),
            thumb(38.0f, bal)})});
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
  return box().row().width(n(54)).height(n(13)).ink(ink).children(
      each(s, [this, pitch](char ch) {
        Element cell = kit::centred().width(pitch).flexShrink(0);
        if (ch != ' ') cell.children({t(std::string(1, ch), pix(10))});
        return cell;
      }));
}
