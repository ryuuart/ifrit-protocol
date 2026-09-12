#pragma once

#include "Settings.h"

struct WinampBase : sketch::Sketch {
  using Out = ch::Output<float>;

  // ---- THE bound outputs. Every idle motion is declared; only discrete
  // state (the digits, the 28-frame sliders, the track list) re-describes.
  Out playPos{0.42f};   // [0,1] through the current track. One source for the
                        // seek thumb's position in px, the elapsed underlay's
                        // scaleX, the MM:SS readout and the playlist's
                        // running-time line, so none of them can disagree.
  Out marqueePhase{0};  // px, wraps
  Out volFrame{0};      // a HARD integer in 0..28: round(pct * 28), the
                        // player's 29 thumb positions over the sprite strip
  Out balFrame{0};      // the same, and 14 is dead centre
  std::array<Out, 11> gain{};  // preamp + 10 bands, [-1,1]; drives the fader
                               // thumbs AND the response graph
  Out graphDraw{0};
  Out llama{0}, llamaPop{1};    // the title-bar easter egg
  Out glint{0};                 // clutter-bar specular sweep
  Out led{0};                   // play-status LED
  std::array<Out, 25> rowIn{};  // playlist row reveal, in bands of four

  // ---- generated materials, held so their identity prunes ----
  mskia::Paint steel, deskMat, lcdMat, faderTrack, graphMat;
  Pattern gripTile, visDots, graphGrid, previewCheck;

  // ---- instancing: the spectrum analyser LEDs and the playlist rows ----
  static constexpr int kCols = 19;  // 19 bars x (3 px bar + 1 px gap) = 76
  static constexpr int kRows = 16;  // VISCOLOR gives exactly 16 ramp stops
  std::shared_ptr<instancing::Atlas> ledAtlas;
  std::shared_ptr<instancing::Pool> ledPool;
  std::shared_ptr<instancing::Atlas> rowAtlas;
  std::shared_ptr<instancing::Pool> rowPool;
  std::array<float, kCols> colLevel{}, colPeak{};
  long long lastRoll = -1;

  // ---- discrete state, the describe path ----
  int volSprite = -1, balSprite = -1;
  // What the track-list slot was last pushed for. On the instance beside the
  // sprite indices, so a fresh session starts with nothing shown and pushes
  // its first list.
  int lastNow = -1, lastSel = -1;
  int shownSec = -1;
  int nowPlaying = 8, selected = 13;
  double elapsedNow = 0;

  // ---- the measured faces, probed once per instance in setup() ----
  /** The advance/em of the substituted monospace faces, in em. Probed rather
   *  than assumed: pix() divides by this to hit an exact cell width, so a
   *  value off by a thousandth is enough to push the LCD digits out of their
   *  well. The initialisers are only the fallback for a probe that measures
   *  nothing. Per instance, because they are a property of the faces this
   *  session resolved. */
  float monoEm = 0.602f;
  float boldEm = 0.602f;

  /** TEXT.BMP's 5x6 native cell, approximated: a monospace sized so its
   *  advance plus tracking lands on exactly `cellN` NATIVE px. Uppercase only
   *  — the real font has no lowercase glyphs at all. A PARTIAL: the run's
   *  colour is the ink in force where it lands. */
  sigil::weave::Type pix(float cellN, bool bold = false,
                         float trackN = 0.0f) const {
    const float em = bold ? boldEm : monoEm;
    return {.face = bold ? wa::monoBold() : wa::mono(),
            .size = wa::n(cellN - trackN) / em,
            .track = wa::n(trackN)};
  }

  // paragraph identities held so the playlist prunes across slot renders
  std::vector<std::shared_ptr<sigil::weave::Paragraph>> rowPara;

  float marqueeW = 1;

  // =========================================================================
  // Data — a plausible 2003 playlist. Track 1 is the real one Winamp
  // shipped with.

  struct Track {
    const char* title;
    const char* time;
    int seconds;
  };
  static const std::array<Track, 25>& tracks();

  /** The real stock "Rock" curve: boosted bass and treble, recessed mids.
   *  [0] is the preamp. */
  static const std::array<float, 11>& rockPreset();

  static std::string mmss(int seconds) {
    const std::string buf =
        kit::formatted("%d:%02d", seconds / 60, seconds % 60);
    return buf;
  }
  /** The readout is four fixed 9x13 cells, so a one-digit minute leaves the
   *  tens cell dark rather than shifting the run. */
  static std::string mmssCells(int seconds) {
    const std::string buf =
        kit::formatted("%2d:%02d", (seconds / 60) % 100, seconds % 60);
    return buf;
  }

  // =========================================================================
  // Materials

  void buildMaterials();

  // =========================================================================
  // Small parts

  /** A transport / menu key: the two-stroke bevel over a steel-blue face
   *  with a vertical ramp, zero radius, content centred. */
  Element key(float x, float y, float w, float h, Element glyph);

  /** A glyph part inside a key, in native px local to the key. */
  static Element part(float x, float y, float w, float h, Shape shape = {});

  /** A tiny beveled text button — ON / AUTO / PRESETS / ADD / REM / … */
  Element textKey(float x, float y, float w, float h, const char* label,
                  float cell = 4.0f);

  // ---- title bar ----------------------------------------------------------

  /** The title bar. The wordmark is baked pixel-art logotype in the real
   *  TITLEBAR.BMP; this is a deliberate tight-monospace approximation in
   *  the sampled gold, cross-faded with the real "IT REALLY WHIPS THE
   *  LLAMA'S ASS!" state that the same bitmap carries. */
  Element titleBar(float wN, const char* label, bool wide, bool hasMin = true,
                   float hN = 14.0f);

  // ---- main window --------------------------------------------------------

  Element mainWindow();

  Element eqPlToggle();

  Element transportRow();

  // ---- the 28-frame sliders: rebuilt through describe, not bound ---------

  /** VOLUME.BMP is 28 stacked 15 px frames and the player picks one with
   *  `round(percent * 28)`: the bar's COLOUR is the level. So this is a
   *  discrete frame swap, re-described when the frame index changes —
   *  never a lerped fill. */
  Element sliders(int vol, int bal);

  std::string marqueeText();

  /** The five NUMBERS.BMP cells, ONE GLYPH PER FIXED CELL, pitch 54/5.
   *
   *  Per-cell placement is the whole point: the live readout has to register
   *  with the unlit "88:88" ghost painted behind it, and that is the only
   *  reason the ghost is there. Centring the run as a single text node
   *  instead would let the leading blank of " 3:02" slide the lit digits
   *  sideways off the ghost. The ghost and the live readout are built by
   *  this same function, so they cannot disagree about the pitch. */
  Element lcdCells(const std::string& s, SkColor4f ink) const;

  Element timeReadout(int seconds) {
    return lcdCells(mmssCells(seconds), wa::kGreen);
  }

  // ---- equalizer window ---------------------------------------------------

  Element eqWindow();

  /** The response curve. Its shape depends on ten live Outputs that move
   *  every frame, and an outline() is resolved once at layout, so the path
   *  cannot be a shape: it is rebuilt inside a custom leaf at paint time,
   *  where the Outputs' current values are readable. The same leaf clips the
   *  left-to-right reveal, since there is no trim to animate on a path that
   *  does not exist until it is drawn. */
  Element eqCurve();

  // ---- playlist window ----------------------------------------------------

  Element playlistWindow();

  std::string runningTime();

  /** The list rows. Twenty-five rows of REAL text — the one place classic
   *  Winamp uses a proportional font — with SigilWeave doing the ellipsis
   *  the real CSS asks for. Backgrounds come from the instancing pool
   *  underneath; the Pool has no text lane, so the rows themselves stay
   *  elements. */
  Element trackList();

  /** One clipped, ellipsised row title. The paragraph is held so pointer
   *  identity keeps the shaping cache warm across slot re-renders. */
  Element ellipsized(int idx, const std::string& s,
                     const sigil::weave::TextStyle& st, float w);

  // =========================================================================

  Element describe();

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  void update(double elapsed, sketch::SketchContext& ctx) override {
    elapsedNow = elapsed;
    pushSlots(ctx, false);
  }

  /** The three discrete-state slots. Everything continuous is bound to an
   *  Output and never re-described; these three are text content and a
   *  28-frame sprite index, neither of which an Animatable can carry, so
   *  they are pushed as fresh subtrees only when their value changes. */
  void pushSlots(sketch::SketchContext& ctx, bool force);

  // =========================================================================
  // The frame loop. Nothing here re-describes.

  static float hash(int a, int b);

  void step(double dt);
};
