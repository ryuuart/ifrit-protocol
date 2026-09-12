// An animated MAGI voting plate. Three square modules stand over a circular
// bus; their labels rotate with them. A live material advances the infection
// through MELCHIOR and BALTHASAR, while the heading and verdict follow their
// own clock. The frame and headings remain independent of the infection.
//
// Capture at 2.5 seconds: MELCHIOR taken, BALTHASAR partly infected, CASPER
// clean. Later moments advance the verdict and countdown.

// TAGS: Interfaces/Film

#include "VotingPlate.h"

struct EvaMagiInterior : sketch::Sketch {
  sk_sp<SkRuntimeEffect> infectionFx = magi::infectionEffect();
  std::vector<magi::Panel> panels = magi::panels();
  std::vector<magi::Arrivals> arrivals;  // per panel, sorted by arrival
  SkPoint centre{613, 602};
  weave::FontContext* fonts = nullptr;

  // measured off the FLAT plate, inside the measured polygons
  static constexpr double kRefT = 2.5;
  static constexpr float kWant[3] = {0.000f, 0.302f, 1.000f};

  // --- animation: everything is a bound Output; nothing re-describes ------
  ch::Output<float> front0{0}, front1{0}, front2{0};
  ch::Output<float> kanjiHot{0};
  ch::Output<float> goldOn{1};
  ch::Output<float> creep{0};
  ch::Output<float> flicker{0};
  double clock = 0;

  std::array<bool, 3> taken{false, false, false};
  // A panel Ireul has not reached yet carries NO infection node at all:
  // a shader whose front is below every cell still runs over every pixel of
  // the panel to prove it, and CASPER stays clean for the first twelve
  // seconds of a twenty-six second loop. This is the one place the DATA path
  // beats a uniform.
  std::array<bool, 3> seeded{false, false, false};
  int verdictStep = -1;
  int countdown = -1;

  /** THE VERIFICATION, as one table. Every row's verdict is COMPUTED from
   *  the two values it reports, so a line that reads PASS cannot disagree
   *  with the arithmetic beside it, and `failures()` is what decides
   *  whether the plate carries a warning. */
  measure::Table verdict;

  // ==========================================================================
  // THE AUDIT — three copies of one square, equally spaced on a radial
  // register.
  void audit() {
    SkPoint c{0, 0};
    for (const auto& p : panels) {
      c.fX += p.box.centerX();
      c.fY += p.box.centerY();
    }
    centre = {c.fX / (float)panels.size(), c.fY / (float)panels.size()};
    verdict = {};
    verdict.add(measure::heading("MODULE RULE"));
    verdict.add(measure::reading(
        "module centroid",
        kit::formatted("(%.0f, %.0f)", (double)centre.fX, (double)centre.fY)));
    const auto distance = [](SkPoint a, SkPoint b) {
      return std::hypot(a.fX - b.fX, a.fY - b.fY);
    };
    const SkPoint p0{panels[0].box.centerX(), panels[0].box.centerY()};
    const SkPoint p1{panels[1].box.centerX(), panels[1].box.centerY()};
    const SkPoint p2{panels[2].box.centerX(), panels[2].box.centerY()};
    const float sides[3] = {distance(p0, p1), distance(p1, p2),
                            distance(p2, p0)};
    const float expectedRotation[3] = {60.0f, 0.0f, -60.0f};
    for (size_t i = 0; i < panels.size(); ++i) {
      const auto& p = panels[i];
      verdict.add(measure::check(
          kit::formatted("%s  square, w \xe2\x88\x92 h px", p.key),
          (double)p.box.width(), (double)p.box.height(), 0.1));
      verdict.add(measure::check(
          kit::formatted("%s  edge to the next module, px", p.key),
          (double)sides[(i + 1) % 3], (double)sides[i], 0.5));
      verdict.add(measure::check(kit::formatted("%s  rotation, deg", p.key),
                                 (double)expectedRotation[i],
                                 (double)p.rotation, 0.1));
    }
  }

  /** THE VERIFICATION, PAINTED — and only when it fails. The reference
   *  carries no drafting chrome, so a plate whose construction holds shows
   *  the construction and nothing else; a violated module rule is dealt
   *  across it in magenta where nobody can miss it. */
  Element failureCard() const {
    sketch::kit::Theme look;
    look.palette.ash = {0, 0, 0, 1};
    look.palette.figure = {0.32f, 0, 0, 1};
    look.type.sans = magi::latin();
    look.type.mono = magi::latin();
    look.type.captionNote = {19.0f, 0.2f};
    look.type.captionLabel = {19.0f, 0.2f, true};
    look.spacing.rowGap = 6;
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
        .top(360)
        .width(magi::kW)
        .height(140.0f + 25.0f * (float)rows.size())
        .fill(Fill::color({1, 0, 1, 0.94f}))
        .column()
        .padding(30)
        .gap(14)
        .child(text(u8"MODULE RULE VIOLATED")
                   .font({.face = magi::latin(), .size = 52.0f})
                   .ink({0, 0, 0, 1}))
        .child(sketch::kit::table(std::move(rows),
                                  {.columns = {{560}, {150, true}, {}},
                                   .gap = 16,
                                   .swatchSide = 13}));
  }

  // ==========================================================================
  // TYPE, SOLVED. metrics() gives capHeight without laying anything out and
  // capHeight is linear in size; capSlack() turns "the reference's ink starts
  // at (222, 692)" into a coordinate. The WIDTH is solved too, because Matisse
  // EB and the available Mincho fallback do not share a set width. Labels use
  // Helvetica CONDENSED — the frame is the authority on how much.
  // A Latin solve answers a PARTIAL (face, size, condensation): the words take
  // the ink where they land, and a probe is measured as a whole style. The Han
  // runs stay whole: a stand-in Mincho below extra-bold strokes its paint.

  weave::Type fitCap(const sk_sp<SkTypeface>& tf, float cap,
                     float* slack = nullptr) const {
    float size = cap * 1.4f;
    if (fonts) {
      const TextMetrics m =
          metrics(weave::textStyle({.face = tf, .size = 100.0f}), *fonts);
      if (m.capHeight > 1.0f) size = 100.0f * cap / m.capHeight;
    }
    const weave::Type st{.face = tf, .size = size};
    if (slack)
      *slack = fonts ? metrics(weave::textStyle(st), *fonts).capSlack()
                     : size * 0.22f;
    return st;
  }

  weave::Type fitRun(const sk_sp<SkTypeface>& tf, const std::u8string& s,
                     float cap, float inkW, float* slack = nullptr) const {
    weave::Type st = fitCap(tf, cap, slack);
    if (fonts && inkW > 1.0f) {
      const SkSize m =
          sigil::compose::intrinsicSize(text(s, weave::textStyle(st)), *fonts);
      if (m.width() > 1.0f)
        st.condense = std::clamp(inkW / m.width(), 0.40f, 1.8f);
    }
    return st;
  }

  weave::Type fitWithin(const sk_sp<SkTypeface>& tf, const std::u8string& s,
                        float cap, float maxWidth) const {
    weave::Type st = fitCap(tf, cap);
    if (!fonts) return st;
    const SkSize measured =
        sigil::compose::intrinsicSize(text(s, weave::textStyle(st)), *fonts);
    if (measured.width() > maxWidth && measured.width() > 1.0f)
      st.condense = std::min(maxWidth / measured.width(), 1.0f);
    return st;
  }

  /** A CJK run is sized by its ADVANCE — a Ming's em IS its advance, and the
   *  flat plate gives it directly: 提/訴 step 148 px, 決/議 130. */
  weave::TextStyle fitEmSpan(const std::u8string& s, float advanceSpan,
                             SkColor4f c, float* slack = nullptr) const {
    // The plate's Han is drawn WIDER than it is tall — the same horizontal
    // stretch itorr's recreation encodes as `transform: scale(1.2, 1)` — so
    // the stretch goes on BEFORE the measurement, or the solve fights it.
    constexpr float kStretch = 1.30f;
    float em = advanceSpan * 0.5f;
    if (fonts) {
      weave::TextStyle probe = evangelion::minchoDisplay(100.0f, c, kStretch);
      const SkSize m = sigil::compose::intrinsicSize(text(s, probe), *fonts);
      if (m.width() > 1.0f) em = 100.0f * advanceSpan / m.width();
    }
    weave::TextStyle st = evangelion::minchoDisplay(em, c, kStretch);
    if (slack) *slack = fonts ? metrics(st, *fonts).capSlack() : em * 0.10f;
    return st;
  }

  /** A run of type placed by its measured INK top-left. Nothing on this plate
   *  is rotated: the rectification says every baseline is horizontal to within
   *  1.3 deg, and the "varying roll" was the projection. */
  Element inked(std::u8string s, const weave::Type& st, SkPoint ink,
                float slack) {
    return box()
        .left(ink.fX)
        .top(ink.fY - slack)
        .child(text(std::move(s)).font(st));
  }

  // ==========================================================================
  // THE PLATE

  Element panelNode(int i) {
    const magi::Panel& p = panels[(size_t)i];
    const evangelion::MagiVoteLayout layout;
    const int number = 3 - i;
    const SkSize sz{p.box.width(), p.box.height()};
    const bool red = taken[(size_t)i];
    const SkPath circuit = magi::ownCircuitry(sz, 17 + i * 13, p.circuitRuns);
    const SkPath pads = magi::ownPads(sz, 41 + i * 7, p.circuitRuns ? 6 : 0);
    const ch::Output<float>* fr =
        i == 0 ? &front0 : (i == 1 ? &front1 : &front2);
    // The panel's ink is its label colour; the circuitry, laid only on a
    // clean panel, is drawn in it.
    const SkColor4f labelInk = red ? magi::kInkRed : magi::kInk;

    mskia::Paint infection =
        mskia::Paint::sksl(infectionFx)
            .child("uArrival", mskia::Paint::shader(arrivals[(size_t)i].field))
            .uniform("uCells",
                     std::array<float, 2>{std::ceil(sz.width() / magi::kCell),
                                          std::ceil(sz.height() / magi::kCell)})
            .uniform("uPour", magi::kRed)
            .uniform("uKey", magi::kTraceDark)
            .uniform("uFront", fr);

    Element node =
        box()
            .left(p.box.left())
            .top(p.box.top())
            .width(sz.width())
            .height(sz.height())
            .rotate(p.rotation)
            .transformOrigin(0.5f, 0.5f)
            .key(p.key)
            .shape(evangelion::panel({}))
            .fill(mskia::Paint::solid(red ? magi::kRed : magi::kMint))
            .ink(labelInk)
            .clip(true)
            .style(decorations::doubleBorder(
                decorations::border(5.0f, Fill::color(magi::kOrange), 0.0f),
                decorations::border(3.0f, Fill::color(magi::kTraceDark),
                                    8.0f)));

    if (!red && p.circuitRuns > 0) {
      // UNEQUAL offsets, unequal widths: a heavy run with a hairline beside it
      // at a different clearance, which is the thing lines::Line's symmetric
      // `parallels` cannot say at all.
      node.child(
          box()
              .inset(0)
              .fill(Fill::none())
              .shape(heldPath(circuit))
              .stroke(lines::Rails{.rails = {{.across = 6.0f,
                                              .width = 3.0f,
                                              .fill = Fill::currentInk(),
                                              .cap = SkPaint::kSquare_Cap,
                                              .join = SkPaint::kMiter_Join},
                                             {.across = -3.0f,
                                              .width = 1.8f,
                                              .fill = Fill::currentInk(),
                                              .cap = SkPaint::kSquare_Cap,
                                              .join = SkPaint::kMiter_Join}},
                                   .offsetStep = 6.0f}));
      node.child(box()
                     .inset(0)
                     .fill(Fill::none())
                     .shape(heldPath(pads))
                     .stroke(PathFormat{.width = 2.0f,
                                        .strokeFill = Fill::currentInk(),
                                        .join = SkPaint::kMiter_Join}));
    }
    if (!red && seeded[(size_t)i]) node.child(box().inset(0).fill(infection));
    node.child(text(toUtf8(p.number))
                   .font(fitCap(evangelion::voteNumeral(number), 86.0f))
                   .centerAt({sz.width() * 0.5f,
                              sz.height() * layout.numberSlotY(number)}));
    node.child(text(toUtf8(p.label))
                   .font(fitWithin(evangelion::moduleLabel(), toUtf8(p.label),
                                   31.0f, sz.width() - 44.0f))
                   .centerAt({sz.width() * 0.5f,
                              sz.height() * layout.nameSlotY(number)}));
    return node;
  }

  /** The headings are bracketed by three parallel green rules. */
  Element greenBand(float x0, float x1, float y) {
    Element band = box().inset(0);
    for (int i = -1; i <= 1; ++i)
      band.child(box()
                     .left(x0)
                     .top(y + (float)i * 7.0f)
                     .width(x1 - x0)
                     .height(2.5f)
                     .fill(mskia::Paint::solid(i == 0 ? magi::kGreenHi
                                                      : magi::kGreen)));
    return band;
  }

  Element plateFurniture() {
    Element g = box().inset(0);
    const evangelion::MagiVoteLayout layout;
    const SkRect frame = layout.frame();
    g.child(
        box()
            .left(frame.left())
            .top(frame.top())
            .width(frame.width())
            .height(frame.height())
            .fill(Fill::none())
            .foreground(decorations::border(7.0f, Fill::color(magi::kOrange))));
    g.child(kit::disc(layout.busCentre, layout.busRadius)
                .shape(shapes::circle())
                .fill(Fill::none())
                .foreground(decorations::border(
                    5.0f, Fill::color(magi::kOrangeDim), 0.0f)));
    g.child(greenBand(145.0f, 520.0f, 116.0f));
    g.child(greenBand(145.0f, 520.0f, 251.0f));
    g.child(greenBand(920.0f, 1295.0f, 116.0f));
    g.child(greenBand(920.0f, 1295.0f, 251.0f));
    return g;
  }

  /** The plate's Latin is orange, stated once; the whole-style Han names its
   *  own. */
  Element plateType() {
    Element g = box().inset(0).ink(magi::kOrange);
    float sCode = 0, sFile = 0;
    g.child(inked(u8"CODE : 132",
                  fitRun(magi::latin(), u8"CODE : 132", 45.0f, 270.0f, &sCode),
                  {151.0f, 294.0f}, sCode));
    const auto file =
        fitRun(magi::latin(), u8"EXTENTION:2048", 22.0f, 286.0f, &sFile);
    static const char* kBlock[5] = {"FILE:MAGI_SYS", "EXTENTION:2048",
                                    "EX_MODE:ON", "PRIORITY:A__", nullptr};
    for (int i = 0; kBlock[i]; ++i)
      g.child(inked(toUtf8(kBlock[i]), file,
                    {151.0f, 350.0f + (float)i * 32.0f}, sFile));

    const auto k1 = fitEmSpan(u8"提訴", 300.0f, magi::kKanji);
    const auto k1h = fitEmSpan(u8"提訴", 300.0f, magi::kKanjiHot);
    g.child(text(u8"提訴", k1).centerAt({332.5f, 184.0f}));
    g.child(box().inset(0).opacity(&kanjiHot).child(
        text(u8"提訴", k1h).centerAt({332.5f, 184.0f})));
    const auto k2 = fitEmSpan(u8"決議", 300.0f, magi::kKanji);
    g.child(text(u8"決議", k2).centerAt({1107.5f, 184.0f}));

    g.child(text(u8"MAGI")
                .font(fitCap(evangelion::magiWordmark(), 54.0f))
                .centerAt({720.0f, 535.0f}));
    return g;
  }

  /** 審議中 in its gold box: a REAL double border — gold outer rule, dark gap,
   *  thin inner rule. Measured 200 x 90; itorr builds the same object as a
   *  triple box-shadow at .03em / .07em / .1em. */
  Element verdictBox() {
    const auto st = fitEmSpan(u8"審議中", 188.0f, magi::kGoldPeak);
    return box()
        .left(995)
        .top(295)
        .width(275)
        .height(130)
        .opacity(&goldOn)
        .fill(mskia::Paint::solid(hexColor(0x140A02)))
        .style(decorations::doubleBorder(
            decorations::border(5.0f, Fill::color(magi::kGold), 0.0f),
            decorations::border(2.0f, Fill::color(magi::kGoldHot), 10.0f)))
        .child(text(u8"審議中", st).centerAt({137.5f, 65.0f}));
  }

  /** The verdict card — 否決 x4, then 可決, then the struck-through 否決 of
   *  "we are 1 sec. ahead". It appears only AFTER the reference moment, so the
   *  capture at 2.5 s still diffs against the anchor. */
  Element verdictCard() {
    if (verdictStep < 0) return box().absolute().width(0).height(0);
    const bool carried = verdictStep == 4;
    const SkColor4f ink = carried ? magi::kGoldPeak : magi::kRedHot;
    const auto st = fitEmSpan(carried ? u8"可決" : u8"否決", 150.0f, ink);
    Element card =
        box()
            .left(995)
            .top(295)
            .width(275)
            .height(130)
            .shape(shapes::chamfered(22.0f, shapes::Corner::Diagonal))
            .fill(mskia::Paint::solid(hexColor(0x0A0102)))
            .foreground(decorations::border(4.0f, Fill::color(ink), 3.0f))
            .child(text(carried ? u8"可決" : u8"否決", st)
                       .centerAt({137.5f, 65.0f}));
    if (verdictStep == 5)
      card.child(box().left(24).top(62).width(227).height(7).fill(
          mskia::Paint::solid(magi::kOrange)));
    return card;
  }

  /** The countdown. Helvetica Bold, bleeding off the frame — 20, 15, 10, then
   *  one a second from 9, and a 2 s stay at zero because "self-destruction
   *  will be executed 02 sec. after all three agree". */
  Element countdownNumeral() {
    if (countdown < 0) return box().absolute().width(0).height(0);
    const std::string buf = kit::formatted("%d", countdown);
    return box().left(1096).top(96).child(text(toUtf8(buf))
                                              .font({.face = magi::latin(),
                                                     .size = 260.0f,
                                                     .color = magi::kRedHot,
                                                     .condense = 1.2f}));
  }

  /** The HUD slot: everything that changes on a CLOCK rather than on the
   *  front. The panels carry live materials, and a re-describe puts their
   *  bakes at risk, so a countdown numeral changing once a second must not
   *  drag the whole tree through render(describe()) with it. */
  Element hud() {
    return box().inset(0).child(verdictCard()).child(countdownNumeral());
  }

  Element describe() {
    Element root = box().inset(0);
    Element picture = box().inset(0);

    // The bus is drawn first. The square modules are masks over it, and their
    // labels live inside their rotated local coordinate systems.
    picture.child(plateFurniture().cache(Cache::Texture).key("furniture"));
    for (int i = 0; i < 3; ++i) picture.child(panelNode(i));
    // Headings and state cards occupy the frontmost UI layer.
    picture.child(plateType().cache(Cache::Texture).key("ptype"));
    picture.child(verdictBox());
    picture.child(slot("hud"));
    root.child(
        std::move(picture).effect(evangelion::phosphor()).key("phosphor"));

    // --- the tube ----------------------------------------------------------
    root.child(box()
                   .left(0)
                   .top(-8)
                   .width(magi::kW)
                   .height(magi::kH + 16)
                   .fill(mskia::Paint::recipe(evangelion::tube()))
                   .translateY(&creep)
                   .cache(Cache::Texture)
                   .key("crt"));
    root.child(box()
                   .inset(0)
                   .fill(Fill::color({0, 0, 0, 1}))
                   .opacity(&flicker)
                   .key("flicker"));

    if (verdict.failures() > 0) root.child(failureCard());
    return root;
  }

  // ==========================================================================
  // Coverage selects a quantile of the same cell field sampled by the shader.
  // Whole trace cells make the requested fraction approximate.

  float frontAt(int i, double t) const {
    const magi::InfectionTiming timing = magi::kInfection[(size_t)i];
    double k = (t - timing.seedAt) / (timing.fullAt - timing.seedAt);
    k = std::clamp(k, 0.0, 1.0);
    // ease so the front decelerates as the panel fills, which is what the
    // episode's dialogue describes ("Balthazar is now taken over" lands late)
    double frac = k * k * (3.0 - 2.0 * k);
    // The material only changes when the front reaches another trace cell.
    // A small phase separates the panels' transition boundaries.
    frac =
        std::floor(frac * timing.levels + timing.phase + 1e-6) / timing.levels;
    frac = std::clamp(frac, 0.0, 1.0);
    return magi::frontFor(arrivals[(size_t)i], (float)frac);
  }

  void setup(sketch::SketchContext& ctx) override {
    // At the capture moment MELCHIOR is taken and BALTHASAR is about 30.2%
    // infected. By 6.0 s both panels are filled, hiding the ragged front.
    sketch::kit::stage(ctx, {.size = SkSize::Make(magi::kW, magi::kH),
                             .captureAt = 2.5,
                             .background = magi::kGround,
                             .nonlinearPicture = true});
    fonts = ctx.fonts;
    audit();

    arrivals.clear();
    verdict.add(measure::heading("THE FRONT, SOLVED"));
    verdict.add(measure::reading("arrival cell, px", (double)magi::kCell));
    for (int i = 0; i < 3; ++i) {
      seeded[(size_t)i] = false;
      const magi::Panel& pp = panels[(size_t)i];
      arrivals.push_back(magi::arrivalTable(
          pp.box, magi::kCell, pp.seed,
          evangelion::panel({})({pp.box.width(), pp.box.height()})));
      // report what the schedule actually lands on at the reference moment
      double k = 0;
      {
        const magi::InfectionTiming timing = magi::kInfection[(size_t)i];
        k = std::clamp(
            (kRefT - timing.seedAt) / (timing.fullAt - timing.seedAt), 0.0,
            1.0);
        k = k * k * (3.0 - 2.0 * k);
        k = std::floor(k * timing.levels + timing.phase + 1e-6) / timing.levels;
      }
      const auto& tab = arrivals.back();
      float reach = 0;
      for (const auto& c : tab.cells)
        if (c.first < 1e3f / magi::kArrivalScale) reach += c.second;
      const char* key = panels[(size_t)i].key;
      verdict.add(measure::reading(kit::formatted("%s  cells", key),
                                   (long)tab.cells.size()));
      verdict.add(measure::reading(kit::formatted("%s  reachable, %%", key),
                                   100.0 * reach / (double)tab.total));
      // The schedule has to land on the coverage measured off the flat
      // plate at the reference moment; that is what makes the still a
      // reconstruction rather than an impression of one.
      verdict.add(measure::check(
          kit::formatted("%s  coverage at %.1f s, %%", key, kRefT),
          100.0 * (double)kWant[i], 100.0 * k, 0.05));
      verdict.add(measure::check(
          kit::formatted("%s  rendered coverage, %%", key), 100.0 * k,
          100.0 * magi::renderedCoverage(infectionFx, tab, frontAt(i, kRefT)),
          0.15));
    }

    ctx.ticker.add([this](double dt) {
      clock += dt;
      const double t = std::fmod(clock, 26.0);
      front0 = frontAt(0, t >= 24.0 ? -1.0 : t);  // "we are 1 sec. ahead"
      front1 = frontAt(1, t);
      front2 = frontAt(2, t);
      // 提訴 — a proposal is FILED: 3 hard blinks, 220 on / 180 off.
      // ESTIMATED; a single frame cannot measure a blink, so it is flagged.
      kanjiHot = (t < 1.5 && std::fmod(t, 0.40) < 0.22) ? 1.0f : 0.0f;
      // 審議中 — itorr's --flash-time: .4s, step-end, dropping to .1s in EX
      // mode. EX mode here is "two MAGI taken".
      const double flash = (taken[2] && taken[1]) ? 0.10 : 0.40;
      goldOn = std::fmod(t, flash * 2.0) < flash ? 1.0f : 0.34f;
      creep = (float)((int)std::floor(clock * 0.5) % 4);
      flicker = std::fmod(clock, 4.0) < 0.04 ? 0.045f : 0.0f;
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("hud", hud());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // The DATA path, and the ONLY re-describe: a panel is taken (3x a loop),
    // the verdict card steps (6x), the countdown ticks (13x). The infection
    // itself never re-describes — it is one uniform.
    const double t = std::fmod(elapsed, 26.0);
    std::array<bool, 3> now{false, false, false};
    std::array<bool, 3> sow{false, false, false};
    for (int i = 0; i < 3; ++i) {
      const magi::InfectionTiming timing = magi::kInfection[(size_t)i];
      now[(size_t)i] = t >= timing.fullAt && t < 24.0;
      sow[(size_t)i] = t >= timing.seedAt && t < 24.0;
    }
    if (t < 12.0) now[0] = false;

    int vs = -1;
    if (t >= 6.5 && t < 8.5)
      vs = std::min(3, (int)((t - 6.5) / 0.5));
    else if (t >= 8.5 && t < 24.0)
      vs = 4;
    else if (t >= 24.0)
      vs = 5;

    int cd = -1;
    if (t >= 9.0 && t < 11.0)
      cd = 20;
    else if (t >= 11.0 && t < 13.0)
      cd = 15;
    else if (t >= 13.0 && t < 14.0)
      cd = 10;
    else if (t >= 14.0 && t < 23.0)
      cd = 9 - (int)(t - 14.0);
    else if (t >= 23.0 && t < 25.0)
      cd = 0;

    const bool structural = now != taken || sow != seeded;
    const bool hudChanged = vs != verdictStep || cd != countdown;
    if (!structural && !hudChanged) return;
    taken = now;
    seeded = sow;
    verdictStep = vs;
    countdown = cd;
    if (structural) ctx.composer.render(describe());
    ctx.composer.renderSlot("hud", hud());
  }
};

SIGIL_SKETCH(
    EvaMagiInterior, "Study \xc2\xb7 Film",
    "Evangelion Ep 13 under Ireul \xe2\x80\x94 the camera roll was the "
    "projection; the infection is a shader")
