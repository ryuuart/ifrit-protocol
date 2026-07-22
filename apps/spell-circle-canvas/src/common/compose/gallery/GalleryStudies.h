#pragma once
// The study sketches, as gallery scenes.
//
// Each study under sketch/sketches/ rebuilds something that actually
// existed — a shipped game screen, a real website, a published plate, a
// paving you can walk on — with nothing but SigilCompose. They are the
// library's acceptance tests in the only form that finds real gaps.
// SIGIL_SKETCH_STATIC (Sketch.h) compiles them into this binary and
// registers a factory under the file's stem, so this header adds only the
// gallery's own metadata: which folder a study belongs in and one line
// about what it puts under load. Nothing here can drift out of step with
// the file people hot-reload, because it IS that file.

#include "GalleryCore.h"

#include <sigilsketch/Sketch.h>

#include <memory>

#ifndef SIGIL_SKETCH_ASSET_DIR
#define SIGIL_SKETCH_ASSET_DIR ""
#endif

namespace compose_gallery {

namespace sketch = sigil::compose::sketch;

/** One study's gallery identity. `key` is the sketch file's stem and the
 *  key SIGIL_SKETCH_STATIC registered it under; `name` is what the sidebar
 *  shows, what `--scene` matches, and what the capture is named. */
struct StudyInfo {
  const char *key;
  const char *name;
  const char *category;
  const char *tag;
};

inline constexpr StudyInfo kStudies[] = {
    // ---- Game UI: the screens that stress the library hardest ----
    {"ds2_bench", "ds2 bench", "Study \xc2\xb7 Game UI",
     "Dead Space 2's Nanocircuit bench (2011) \xe2\x80\x94 routers, rails, "
     "a diegetic hologram"},
    {"fallout2_charsheet", "fallout2 charsheet", "Study \xc2\xb7 Game UI",
     "Fallout 2's character screen (1998) at 2\xc3\x97 \xe2\x80\x94 ~134 runs, "
     "five alignment regimes"},
    {"ksp_mapview", "ksp mapview", "Study \xc2\xb7 Game UI",
     "Kerbal Space Program's map view \xe2\x80\x94 real conics, a navball in "
     "one SkSL pass"},
    {"psx_doom_fire", "psx doom fire", "Study \xc2\xb7 Game UI",
     "The DOOM PlayStation title flame (1995) \xe2\x80\x94 an automaton at a "
     "fixed 27 Hz"},
    {"xcom_battlescape", "xcom battlescape", "Study \xc2\xb7 Game UI",
     "X-COM: UFO Defense (1994) at 4\xc3\x97 \xe2\x80\x94 115 colours, all of "
     "them in the palette"},

    // ---- Screens and software people actually used ----
    {"cde_motif", "cde motif", "Study \xc2\xb7 Screens",
     "CDE 1.0 on OSF/Motif 2.1 (1995) \xe2\x80\x94 XmGetColors reproduced "
     "byte-exact"},
    {"spacejam_1996", "spacejam 1996", "Study \xc2\xb7 Screens",
     "spacejam.com, still live \xe2\x80\x94 HTML auto table layout as a "
     "LayoutScheme, to 0.11 px"},
    {"twoadvanced_v4", "twoadvanced v4", "Study \xc2\xb7 Screens",
     // The literal breaks after the en dash on purpose: \x93 followed by a
     // digit would swallow it into one out-of-range hex escape.
     "2Advanced Studios v4 \"Prophecy\" (2003\xe2\x80\x93"
     "06) \xe2\x80\x94 chamfered Flash chrome, four deep"},
    {"winamp_base", "winamp base", "Study \xc2\xb7 Screens",
     "Winamp 2.91's Base skin \xe2\x80\x94 a bitmap skin as generated "
     "material, 28 quantised frames"},

    // ---- Science and data: plates that argue something ----
    {"chaucer_astrolabe", "chaucer astrolabe", "Study \xc2\xb7 Science",
     "A planispheric astrolabe for Oxford 51\xc2\xb0 50\xe2\x80\xb2 "
     "\xe2\x80\x94 an instrument that tells the time"},
    {"chevreul_circle", "chevreul circle", "Study \xc2\xb7 Science",
     "Chevreul's 1er cercle chromatique, Plate V, 1864 \xe2\x80\x94 a study "
     "whose content is a palette"},
    {"chladni_tab1", "chladni tab1", "Study \xc2\xb7 Science",
     "Chladni's Tab. I (1786) \xe2\x80\x94 9,580 instanced grains onto twelve "
     "nodal geometries"},
    {"minard_1869", "minard 1869", "Study \xc2\xb7 Science",
     "Minard's BnF presentation copy \xe2\x80\x94 the plate audited against "
     "its own printed legend"},
    {"nightingale_coxcomb", "nightingale coxcomb", "Study \xc2\xb7 Science",
     "Nightingale's 1858 coxcomb \xe2\x80\x94 polar-area wedges from the real "
     "mortality table"},

    // ---- Pattern and tradition: constructions, not tracings ----
    {"black_watch", "black watch", "Study \xc2\xb7 Pattern",
     "The Government sett \xe2\x80\x94 24 integers and a mod-4 rule, 63,504 "
     "emergent cells"},
    {"kumiko_asanoha", "kumiko asanoha", "Study \xc2\xb7 Pattern",
     "A hinoki asanoha ranma \xe2\x80\x94 514 mitred boards, per-piece "
     "assembly staggering"},
    {"penrose_paving", "penrose paving", "Study \xc2\xb7 Pattern",
     "Penrose's 2012 P3 paving, Oxford \xe2\x80\x94 549 setts from de Bruijn's "
     "pentagrid"},

    // ---- Motion and film: frames that are functions of time ----
    {"genesis_fire", "genesis fire", "Study \xc2\xb7 Motion",
     "The Genesis Demo wall of fire (Lucasfilm, 1982) \xe2\x80\x94 the first "
     "particle system"},
    {"hitman_verlet", "hitman verlet", "Study \xc2\xb7 Motion",
     "Jakobsen's Advanced Character Physics (GDC 2001) \xe2\x80\x94 motion "
     "with state and contact"},
    {"slitscan_2001", "slitscan 2001", "Study \xc2\xb7 Motion",
     "Trumbull's slit-scan machine (1966\xe2\x80\x93"
     "68) \xe2\x80\x94 a frame that is a time integral"},
    {"vertigo_titles", "vertigo titles", "Study \xc2\xb7 Motion",
     "Bass and Whitney's Vertigo titles (1958) \xe2\x80\x94 a Lissajous off an "
     "M-5 gun director"},

    // ---- Run 2: film, game and esoteric interfaces ----
    // These were registered in SIGIL_SKETCH_STUDIES one at a time as they
    // shipped, which compiled them in — and then were absent from the
    // gallery for hours, because THE COMPILED SET AND THE LISTED SET ARE
    // TWO LISTS. The build knows what exists; this table knows what is
    // shown; nothing checks that they agree. Same two-name identity that
    // cost three agents an hour elsewhere today (ROADMAP §22), wearing
    // its third face: not two spellings of one name, but two registries
    // of one fact.
    {"thaumonomicon", "thaumonomicon", "Study \xc2\xb7 Game UI",
     "Thaumcraft 6's research browser (2018) \xe2\x80\x94 edges that are "
     "stamped art, not strokes"},
    {"vagrant_story_target", "vagrant story target", "Study \xc2\xb7 Game UI",
     "Vagrant Story's targeting overlay (2000) \xe2\x80\x94 a projected "
     "sphere, and a hidden +10 the printed number omits"},
    {"astral_tome", "astral tome", "Study \xc2\xb7 Game UI",
     "Astral Sorcery's constellation spread \xe2\x80\x94 the chart is "
     "square, the CELL is stretched"},
    {"bg3_dice_roll", "bg3 dice roll", "Study \xc2\xb7 Game UI",
     "Baldur's Gate 3's ability check \xe2\x80\x94 an icosahedron, and an "
     "enum whose ordinals are 5e's skill table"},
    {"eva_magi_defense", "eva magi defense", "Study \xc2\xb7 Film",
     "The End of Evangelion's MAGI plate (1997) \xe2\x80\x94 six "
     "installations are one component, rotated"},
    {"eva_magi_interior", "eva magi interior", "Study \xc2\xb7 Film",
     "Evangelion Ep 13 under Ireul \xe2\x80\x94 the camera roll was the "
     "projection; the infection is a shader"},
    {"lain_navi", "lain navi", "Study \xc2\xb7 Film",
     "Serial Experiments Lain's Copland OS \xe2\x80\x94 no opaque window "
     "anywhere, and text through a fixed focal plane"},
    {"sigillum_aemeth", "sigillum aemeth", "Study \xc2\xb7 Esoteric",
     "Dee's Sigillum Dei Aemeth (1582) \xe2\x80\x94 solved from the "
     "angels' own jump rule, 33 of 40 cells"},
    {"thunder_fulu", "thunder fulu", "Study \xc2\xb7 Esoteric",
     "A Thunder-Rite talisman, WRITTEN \xe2\x80\x94 real stroke medians, "
     "and the foot at 7.1\xc3\x97 the body's tempo"},
    {"dunhuang_star_chart", "dunhuang star chart", "Study \xc2\xb7 Esoteric",
     "The Dunhuang star chart (c. 649\xe2\x80\x93" "684) reprojected from "
     "1,460 real stars \xe2\x80\x94 and it refuses to answer"},

    // ---- The kit itself ----
    {"stroke_atlas", "stroke atlas", "Kit",
     "The line, border and corner specimen plate \xe2\x80\x94 every rule "
     "captioned with the call that made it"},
    {"hello", "hello", "Kit", "The starter sketch. Copy it."},
    {"stock_materials", "stock materials", "Kit",
     "One of every stock SkSL material \xe2\x80\x94 also the split-Skia ctest "
     "guard"},
};
inline constexpr int kStudyCount = (int)(sizeof(kStudies) /
                                         sizeof(kStudies[0]));

/** The assets root a study reaches for when it wants something it did not
 *  generate. Leaked like fonts(): Assets owns Skia-backed images, and a
 *  static destructor racing Skia teardown is a class of crash worth not
 *  having. The directory need not exist — a missing file yields the
 *  magenta placeholder and heals when one appears. */
inline sketch::Assets &studyAssets() {
  static auto *assets = new sketch::Assets(SIGIL_SKETCH_ASSET_DIR);
  return *assets;
}

/** A study sketch driven as a gallery scene. The sketch surface and the
 *  Scene surface are close enough that this is mostly plumbing — the one
 *  real difference is the canvas: a sketch DECLARES its size from inside
 *  setup(), so canvasSize() only answers truthfully afterwards, which is
 *  why GalleryStage reads it back at the end of activate(). */
class StudyScene final : public Scene {
public:
  StudyScene(const StudyInfo &info, sketch::SketchFactory factory)
      : m_info(&info), m_sketch(factory ? factory() : nullptr) {}

  const char *name() const override { return m_info->name; }
  SkSize canvasSize() const override { return m_spec.size; }
  SkColor4f background() const override { return m_spec.background; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    m_composer = &composer;
    m_ticker = &ticker;
    if (!m_sketch)
      return;
    sketch::SketchContext ctx = context();
    m_sketch->setup(ctx);
  }

  void update(double elapsed, Composer &) override {
    if (!m_sketch)
      return;
    sketch::SketchContext ctx = context();
    m_sketch->update(elapsed, ctx);
  }

private:
  // Rebuilt per call rather than stored: SketchContext holds references, and
  // the host's contract is that ctx.canvas()/ctx.background() write through
  // to the spec the host then reads. Keeping one around would only invite it
  // to outlive a composer.
  sketch::SketchContext context() {
    return sketch::SketchContext{*m_composer, *m_ticker,  studyAssets(),
                                 m_spec.size, &m_spec,    &fonts()};
  }

  const StudyInfo *m_info;
  std::unique_ptr<sketch::Sketch> m_sketch;
  sketch::CanvasSpec m_spec;
  Composer *m_composer = nullptr;
  sigil::motion::Ticker *m_ticker = nullptr;
};

/** The scene for `info`, or nullptr when this binary was built without that
 *  study linked in (which the gallery test treats as a failure — the table
 *  and the link line must agree). */
inline std::unique_ptr<Scene> makeStudy(const StudyInfo &info) {
  if (sketch::SketchFactory factory = sketch::findStaticSketch(info.key))
    return std::make_unique<StudyScene>(info, factory);
  return nullptr;
}

} // namespace compose_gallery
