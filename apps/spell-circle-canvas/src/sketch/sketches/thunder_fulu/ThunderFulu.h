#pragma once

#include "Settings.h"

struct ThunderFulu : sketch::Sketch {
  sk_sp<SkTypeface> faceSerif, faceItalic, faceMono, faceDisplay;

  // ONE Output writes the entire plate: it is the score position in seconds,
  // and every stroke's beat is a window() on it.
  ch::Output<float> scribe{0.0f};
  double clockT = 0;

  feed::TextRing logA{64}, logB{64}, logC{64}, logD{64};

  /** WHAT THE CONSOLE'S PASS AND FAIL MARKS ARE READ OFF. A row's verdict
   *  is COMPUTED from the two values it reports — the stroke count the
   *  doctrine publishes and the one the median data carries — so a line
   *  marked pass cannot disagree with the number printed in it, and
   *  `failures()` is the run's verdict away from the screen. */
  sigil::measure::Table verdict;

  /** @p want against @p got, added to the table, and the console tag its
   *  verdict spells. */
  const char* claim(std::string label, long want, long got) {
    verdict.add(sigil::measure::check(std::move(label), want, got));
    return verdict.rows.back().pass ? "pass" : "fail";
  }

  Paint ironGrain;
  Pattern ironSpeck;
  Element footPrint;  // brush::Scatter art, held for pointer stability
  Element hammerTile, hammerCorner;

  // --- the ink ------------------------------------------------------------
  struct Stroke {
    SkPath path;   // plate-local, already cloud-wandered where it should be
    SkRect frame;  // the node's own box — sized to content, never to plate
    float len = 1;
    float w0 = 6;
    int cls = TURN;
    float t0 = 0, t1 = 1;
    bool dry = false;  // 飛白
    SkColor4f ink = kCinnabar;
    std::string key;
    // the foot is ONE contour of 75 spans: 38 strokes and 37 ligatures
    std::vector<std::array<float, 3>>
        spans;  // {d0, d1, w0}  (w0 = 0 → nothing)
  };
  std::vector<Stroke> strokes;
  int nHead = 0, nBody = 0, nGall = 0, nFootStrokes = 0;

  // --- verification -------------------------------------------------------
  int kvgAgree = 0, kvgTotal = 0;
  std::vector<std::string> kvgMiss;
  std::array<int, 10> gangCls{};

  // =========================================================================
  // BUILDING THE INK

  /** One stroke → its node. The whole brush model is here. */
  Element inkStroke(const Stroke& s) const;

  /** Register a stroke: measure it, size its box to its own content. */
  void push(SkPath p, float w0, int cls, float t0, float t1, SkColor4f ink,
            std::string key, bool dry = false,
            std::vector<std::array<float, 3>> spans = {});

  /** 雲篆. Strokes 「盤曲如雲」 — twisted like winding cloud. The wander is a
   *  REAL median displaced, never an invented squiggle:
   * geometry::path::displace offsets perpendicular to the tangent, so the graph
   * keeps its topology and loses its legibility, which is exactly the intent of
   * the script. */
  static SkPath cloud(const SkPath& p, float amp, float wl) {
    return geometry::path::displace(p, amp, wl, false);
  }

  /** A composite cloud graph: components stacked vertically in one box, each
   *  component's real medians, the whole column wandered. */
  void cloudGraph(const std::vector<std::pair<int, float>>& parts, SkPoint at,
                  float size, float t, float each, float amp, int tag);

  // =========================================================================
  // THE PLATE

  Element ironGround();

  /** The grain wash that unifies ink and iron — the flying-white streaks are
   *  painted in the iron's own mid-tone, and this wash over the top is what
   *  stops them reading as paint. The bake is asked for BY NAME because the
   *  library will not promote a leaf that carries both an opacity and a
   *  blend mode on its own: compositing a baked layer rounds a second time,
   *  so it declines rather than change the pixels. Here the wash is a soft
   *  full-plate gradient where that second rounding is invisible, and it is
   *  the most expensive single node on the plate if it repaints. */
  Element ironWash();

  // =========================================================================
  // THE INK, ASSEMBLED IN ORDER

  void buildStrokes();

  Element inkLayer() {
    auto g = box().inset(0).key("ink");
    for (const Stroke& s : strokes) g.child(inkStroke(s));
    return g;
  }

  // =========================================================================
  // 虛書 — the graphs written by NOTHING. 《法海遺珠》 DZ 1166 卷六:
  // "with the left eye's light write one talismanic graph; with the right
  // eye's light write one; with the tongue write one; visualise the three
  // graphs as blue, red and white." No brush touches the plate, so they are
  // drawn as light and they fade into the aperture.

  Element voidWriting();

  // =========================================================================
  // 法印 — the master's seal, square, in relief, vermilion. The seal script
  // deforms a graph to FILL its cell: 五 and 雷 stretched into two halves of
  // a square, cut in relief, with the negative space crosshatched the way a
  // stone impression breaks up.

  Element sealBlock();

  // =========================================================================
  // THE PLATE, ASSEMBLED

  Element plate();

  // =========================================================================
  // THE TREAD — 步罡踏斗 on the real Dipper.

  Element tread();

  // =========================================================================
  // MARGINALIA

  Element chantPanel();

  feed::TextOptions logStyle();

  Element consolePanel();

  Element tempoPanel();

  /** The margin column: the chants that are said WHILE the stroke goes down,
   *  and the two things that decide how every stroke on this plate is drawn
   *  — the width law, plotted, and the six recovered classes as specimens.
   *
   *  The chants are windowed on the same Output as the strokes themselves,
   *  which is what keeps them in step: the 六句 of 罡 must finish exactly as
   *  the tenth stroke lands. */
  Element marginColumn();

  Element furniture();

  // =========================================================================

  Element describe(sketch::SketchContext&);

  // =========================================================================
  // THE CHECKS

  void runChecks();

  void validateClassifier();

  // =========================================================================

  Font font;

  void setup(sketch::SketchContext& ctx) override;

  void update(double, sketch::SketchContext&) override {}
};
