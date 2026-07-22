#pragma once
// The passive-tree study (REFERENCES.md section 5 — skill-tree / node-graph
// brushes between elements), rebuilt on REAL tree geometry.
//
// SkillTreeData.h carries a 39-node disc of Path of Exile's public tree
// export: group centres, the orbitRadii/skillsPerOrbit ladder, node kinds,
// and which links are same-group/same-orbit ARCS. That is the thing worth
// studying — no hand-placed graph produces PoE's characteristic rhythm of
// rosettes strung on straight spokes, and no hand-placed graph stresses the
// orbit router the way the real one does. Names and stat wording are ours.
//
// The grammar transcribed here:
//
//   group discs ......... every group with an occupied orbit gets the
//                         PSGroupBackground analogue: a soft radial disc
//                         plus one faint guide ring per occupied orbit.
//                         This is what makes a passive tree READ as orbits
//                         rather than as an arbitrary node soup.
//   the 3-state law ..... every edge is Normal / Intermediate / Active --
//                         brushes::rope(state, zoom) is the verified PoE
//                         ladder (#3A332A -> #6B5A40 -> #8A7248 + halo);
//                         Active needs BOTH ends allocated, Intermediate
//                         exactly one, and the whole rope scales with the
//                         cluster's zoom the way the game's line art does
//   frame ladder ........ four node chromes, not one: minor circle,
//                         notable double ring + notch rosette, mastery
//                         diamond, keystone octagon under an ornament ring.
//                         Size carries hierarchy, colour carries progress
//                         (the PoE width law, applied to sockets)
//   orbit routing ....... EVERY edge is a rail(...) with routers::orbit(the
//                         edge's own group centre): the 21 same-orbit pairs
//                         become arcs on their group's circle, the 22
//                         cross-group links stay straight spokes
//   wrap-mode comet ..... the widest group's orbit ring carries a Wrap-mode
//                         trim window that MARCHES around it and survives
//                         the mod-1 wrap across the path seam in two pieces
//   draw-on entrance .... the allocated spine (a real shortest path through
//                         the graph, notable -> keystone) enters with
//                         trim(0, withFrom(0 -> 1, 900ms))
//   pulse-travel ........ stock brushes::pulse() rides that spine
//   search pulse ........ Daripher's Passive-Skill-Tree lights matched
//                         nodes with a sin-driven alpha; ours rings them
//   selection ring ...... and rotates an ornament ring around the selected
//                         node, which is where the detail card points
//   detail card ......... the tooltip typography: name, kind rule, stat
//                         lines, italic flavour, on a framed scrim

#include "GalleryCore.h"
#include "SkillTreeData.h"

#include <sigilcompose/Brushes.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace compose_gallery {

namespace skill_tree {

namespace data = skill_tree_data;

// Near-black ground and the PoE ladder (REFERENCES.md section 5).
constexpr SkColor4f kBg{0.098f, 0.082f, 0.071f, 1};      // #191512
constexpr SkColor4f kBgLift{0.125f, 0.104f, 0.086f, 1};  // center lift
constexpr SkColor4f kBgSink{0.072f, 0.060f, 0.052f, 1};  // vignette edge
constexpr SkColor4f kSocket{0.129f, 0.106f, 0.082f, 1};  // socket well
constexpr SkColor4f kPewter{0.306f, 0.275f, 0.235f, 1};  // #4E463C
constexpr SkColor4f kRimLit{0.541f, 0.478f, 0.361f, 1};  // #8A7A5C
constexpr SkColor4f kGold{0.788f, 0.663f, 0.416f, 1};    // #C9A96A
constexpr SkColor4f kHalo{1.0f, 0.788f, 0.439f, 1};      // #FFC970
constexpr SkColor4f kBone{0.835f, 0.769f, 0.616f, 1};
constexpr SkColor4f kAsh{0.447f, 0.404f, 0.337f, 1};
constexpr SkColor4f kSearch{0.541f, 0.902f, 0.510f, 1}; // the search green

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

/** Visible diameter per node kind — hierarchy lives in size, progress in
 *  colour (the width law, applied to sockets rather than to lines). The
 *  cluster's tightest pair sits 43 px apart, which is what caps these. */
inline float diameterOf(data::Kind k) {
  switch (k) {
  case data::Kind::Keystone: return 46;
  case data::Kind::Notable:  return 30;
  case data::Kind::Mastery:  return 22;
  case data::Kind::Jewel:    return 24;
  case data::Kind::Minor:    break;
  }
  return 17;
}

/** How wide the rope art is drawn at this cluster's zoom. */
constexpr float kRopeScale = 0.62f;

/** An edge's rope state, the game's rule: Active when BOTH ends are
 *  allocated, Intermediate when exactly one is (the link you could travel),
 *  Normal otherwise — two allocatable neighbours are still just Normal. */
inline int edgeState(data::State a, data::State b) {
  const int allocated = (a == data::State::Allocated) +
                        (b == data::State::Allocated);
  return allocated == 2 ? 2 : allocated == 1 ? 1 : 0;
}

inline SkColor4f ringColor(data::State s) {
  return s == data::State::Allocated     ? kGold
         : s == data::State::CanAllocate ? kRimLit
                                         : kPewter;
}

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0, bool italic = false) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  if (italic)
    s.shaping.variations = {sigil::weave::FontVariation("slnt", -10)};
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** Full-circle outline for orbit guide rings, centered in the node. */
inline std::function<SkPath(SkSize)> circleOutline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.addCircle(s.width() * 0.5f, s.height() * 0.5f, s.width() * 0.5f);
    return b.detach();
  };
}

/** Horizontal line outline for the legend swatches. */
inline std::function<SkPath(SkSize)> hline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() * 0.5f);
    b.lineTo(s.width(), s.height() * 0.5f);
    return b.detach();
  };
}

/** The notch rosette a notable frame wears: `count` short radial ticks on
 *  the ring, the cheap analogue of PoE's cast-metal notable art. */
inline std::function<SkPath(SkSize)> notchRing(int count, float innerFrac,
                                               float outerFrac) {
  return [count, innerFrac, outerFrac](SkSize s) {
    SkPathBuilder b;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float half = s.width() * 0.5f;
    for (int i = 0; i < count; ++i) {
      const float a = 6.2831853f * (float)i / (float)count;
      const float c = std::cos(a), sn = std::sin(a);
      b.moveTo(cx + c * half * innerFrac, cy + sn * half * innerFrac);
      b.lineTo(cx + c * half * outerFrac, cy + sn * half * outerFrac);
    }
    return b.detach();
  };
}

/** A socket well: SDF chrome sized by the VISIBLE diameter --
 *  sdf::minBoxFor grows the box by the style's reserved pad so glow never
 *  clips, and centerAt() pins the measured box on the tree position. */
inline Element socket(const char *key, SkPoint at, float dia,
                      const sdf::Style &st,
                      const choreograph::Output<float> *breathingGlow =
                          nullptr,
                      int z = 3) {
  const float boxSize = sdf::minBoxFor(st, dia);
  Material m = sdf::material(sdf::circle(), st);
  if (breathingGlow)
    m.uniform("uGlowR", breathingGlow);
  Element e = box().width(Dim(boxSize)).height(Dim(boxSize))
                  .centerAt(at)
                  .fill(std::move(m))
                  .zIndex(z);
  if (key)
    e.key(key);
  return e;
}

} // namespace skill_tree

namespace treedata = skill_tree_data;

struct SkillTreeScene final : Scene {
  choreograph::Output<float> pulseS{0}, pulseE{0};
  choreograph::Output<float> ringPhase{0};
  choreograph::Output<float> breath{5.5f};
  choreograph::Output<float> searchPulse{0};
  choreograph::Output<float> selectSpin{0};

  const char *name() const override { return "passive tree"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    pulseS = 0;
    pulseE = 0;
    ringPhase = 0;
    breath = 5.5f;
    searchPulse = 0;
    selectSpin = 0;

    // canAllocate breathing; the traveling pulse (a [s, s+0.12] window
    // sweeping the allocated spine in 1.1 s then resting out a 2.8 s
    // cycle); the wrap comet at 0.5 rev/s; the search sin; the selection
    // ring's slow rotation.
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      breath = 5.5f + 3.5f * (float)std::sin(t * 2.1);
      const float cycle = (float)std::fmod(t, 2.8);
      const float u = (cycle - 1.1f) / 1.1f;
      pulseS = -0.12f + u * 1.12f;
      pulseE = pulseS + 0.12f;
      ringPhase = (float)std::fmod(t * 0.5, 1.0);
      searchPulse = 0.5f + 0.5f * (float)std::sin(t * 3.0);
      selectSpin = (float)std::fmod(t * 22.0, 360.0);
      return true;
    });

    composer.render(describe());
  }

  // ------------------------------------------------------------------
  // node identity: one stable key per index, so rails can anchor to them

  static std::string nodeKey(int i) { return "n" + std::to_string(i); }

  /** Nodes the search term lights up — Daripher's `searched` state. The
   *  term is fixed here; the point is the highlight, not a text field. */
  static bool searched(int i) {
    const std::string_view n = skill_tree_data::kNodes[i].name;
    return n.find("Fire") != std::string_view::npos ||
           n.find("Burning") != std::string_view::npos;
  }

  /** The one node the detail card describes. */
  static int selectedIndex() {
    for (int i = 0; i < skill_tree_data::kNodeCount; ++i)
      if (skill_tree_data::kNodes[i].kind == skill_tree_data::Kind::Keystone)
        return i;
    return 0;
  }

  // ------------------------------------------------------------------
  // the four node chromes

  void minorNode(Element &parent, int i) {
    namespace pt = skill_tree;
    const treedata::Node &n = treedata::kNodes[i];
    const float dia = pt::diameterOf(n.kind);
    const bool alloc = n.state == treedata::State::Allocated;
    const bool can = n.state == treedata::State::CanAllocate;
    const sdf::Style st{
        .fill = pt::kSocket,
        .borderWidth = alloc ? 2.6f : 1.9f,
        .borderColor = pt::ringColor(n.state),
        .glowRadius = can ? 12.0f : (alloc ? 11.0f : 0.0f),
        .glowColor = {pt::kHalo.fR, pt::kHalo.fG, pt::kHalo.fB,
                      alloc ? 0.45f : 0.40f}};
    // The declared glowRadius reserves the box pad (exp falloff reaches ~0
    // before the edge); the ACTUAL halo runs shorter, via a uniform.
    Element e = pt::socket(nullptr, {n.x, n.y}, dia, st,
                           can ? &breath : nullptr);
    if (alloc) {
      Material m = sdf::material(sdf::circle(), st);
      m.uniform("uGlowR", 5.5f);
      e.fill(std::move(m));
    }
    parent.child(e.key(nodeKey(i)));
  }

  void notableNode(Element &parent, int i) {
    namespace pt = skill_tree;
    const treedata::Node &n = treedata::kNodes[i];
    const SkPoint at{n.x, n.y};
    const float dia = pt::diameterOf(n.kind);
    const bool alloc = n.state == treedata::State::Allocated;
    const bool can = n.state == treedata::State::CanAllocate;
    const SkColor4f ring = pt::ringColor(n.state);
    const sdf::Style outer{
        .fill = pt::kSocket,
        .borderWidth = alloc ? 3.2f : 2.4f,
        .borderColor = ring,
        .glowRadius = 14,
        .glowColor = {pt::kHalo.fR, pt::kHalo.fG, pt::kHalo.fB,
                      alloc ? 0.5f : can ? 0.4f : 0.0f}};
    const std::string key = nodeKey(i);
    Element frame =
        pt::socket(key.c_str(), at, dia, outer, can ? &breath : nullptr);
    if (alloc) {
      Material m = sdf::material(sdf::circle(), outer);
      m.uniform("uGlowR", 7.0f);
      frame.fill(std::move(m));
    }
    parent.child(std::move(frame));
    // the inner ring and the notch rosette that make it read "notable"
    parent.child(pt::socket(nullptr, at, dia - 11,
                            {.fill = {0, 0, 0, 0},
                             .borderWidth = 1.6f,
                             .borderColor = ring},
                            nullptr, 4));
    parent.child(box().width(Dim(dia + 10)).height(Dim(dia + 10))
                     .centerAt(at)
                     .outline(pt::notchRing(8, 0.72f, 1.0f))
                     .stroke(util::stroke(
                         1.4f, Fill::color({ring.fR, ring.fG, ring.fB,
                                            alloc ? 0.9f : 0.5f})))
                     .zIndex(4));
    // The well is never empty in the real thing — a cast sigil sits in it.
    parent.child(box().width(Dim(dia * 0.50f)).height(Dim(dia * 0.50f))
                     .centerAt(at)
                     .outline(shapes::star(4, 0.34f))
                     .fill(Material::solid({ring.fR, ring.fG, ring.fB,
                                            alloc ? 0.95f : 0.6f}))
                     .zIndex(4));
  }

  void masteryNode(Element &parent, int i) {
    namespace pt = skill_tree;
    const treedata::Node &n = treedata::kNodes[i];
    const SkPoint at{n.x, n.y};
    const float dia = pt::diameterOf(n.kind);
    const SkColor4f ring = pt::ringColor(n.state);
    // A diamond, so a group's centre node never reads as one more socket.
    parent.child(box().width(Dim(dia)).height(Dim(dia))
                     .centerAt(at)
                     .key(nodeKey(i))
                     .outline(shapes::polygon(4))
                     .fill(Material::solid(pt::kSocket))
                     .stroke(util::stroke(1.8f, Fill::color(ring)))
                     .zIndex(3));
    parent.child(box().width(Dim(dia * 0.42f)).height(Dim(dia * 0.42f))
                     .centerAt(at)
                     .outline(shapes::polygon(4))
                     .fill(Material::solid(
                         {ring.fR, ring.fG, ring.fB, 0.75f}))
                     .zIndex(4));
  }

  void keystoneNode(Element &parent, int i) {
    namespace pt = skill_tree;
    const treedata::Node &n = treedata::kNodes[i];
    const SkPoint at{n.x, n.y};
    const float dia = pt::diameterOf(n.kind);
    const SkColor4f ring = pt::ringColor(n.state);
    const bool alloc = n.state == treedata::State::Allocated;
    // Halo well first, so the octagon frame sits inside its own light.
    parent.child(pt::socket(nullptr, at, dia - 6,
                            {.fill = pt::kSocket,
                             .borderWidth = 0,
                             .glowRadius = 22,
                             .glowColor = {pt::kHalo.fR, pt::kHalo.fG,
                                           pt::kHalo.fB,
                                           alloc ? 0.42f : 0.12f}},
                            nullptr, 2));
    parent.child(box().width(Dim(dia)).height(Dim(dia))
                     .centerAt(at)
                     .key(nodeKey(i))
                     .outline(shapes::polygon(8, 22.5f))
                     .fill(Material::radial({dia * 0.5f, dia * 0.5f},
                                            dia * 0.62f,
                                            {{0.0f, {0.20f, 0.16f, 0.12f, 1}},
                                             {1.0f, {0.07f, 0.06f, 0.05f, 1}}}))
                     .stroke(util::stroke(2.8f, Fill::color(ring)))
                     .zIndex(3));
    parent.child(box().width(Dim(dia - 11)).height(Dim(dia - 11))
                     .centerAt(at)
                     .outline(shapes::polygon(8, 22.5f))
                     .stroke(util::stroke(
                         1.2f, Fill::color({ring.fR, ring.fG, ring.fB, 0.6f})))
                     .zIndex(4));
    parent.child(box().width(Dim(dia + 16)).height(Dim(dia + 16))
                     .centerAt(at)
                     .outline(pt::notchRing(16, 0.86f, 1.0f))
                     .stroke(util::stroke(
                         1.3f, Fill::color({ring.fR, ring.fG, ring.fB, 0.55f})))
                     .zIndex(4));
    // A keystone's plate carries the heaviest sigil in the tree.
    parent.child(box().width(Dim(dia * 0.60f)).height(Dim(dia * 0.60f))
                     .centerAt(at)
                     .outline(shapes::star(6, 0.40f))
                     .fill(Material::radial(
                         {dia * 0.30f, dia * 0.30f}, dia * 0.34f,
                         {{0.0f, {pt::kHalo.fR, pt::kHalo.fG, pt::kHalo.fB,
                                  alloc ? 1.0f : 0.55f}},
                          {1.0f, {ring.fR, ring.fG, ring.fB,
                                  alloc ? 0.85f : 0.40f}}}))
                     .zIndex(4));
  }

  void jewelNode(Element &parent, int i) {
    namespace pt = skill_tree;
    const treedata::Node &n = treedata::kNodes[i];
    const SkPoint at{n.x, n.y};
    const float dia = pt::diameterOf(n.kind);
    const SkColor4f ring = pt::ringColor(n.state);
    parent.child(box().width(Dim(dia)).height(Dim(dia))
                     .centerAt(at)
                     .key(nodeKey(i))
                     .outline(shapes::polygon(4))
                     .stroke(util::stroke(2.0f, Fill::color(ring)))
                     .zIndex(3));
  }

  // ------------------------------------------------------------------

  /** Group discs + one faint guide ring per occupied orbit — the thing
   *  that makes the field read as orbits instead of as a node soup. */
  void groundwork(Element &root) {
    namespace pt = skill_tree;
    for (const treedata::Group &g : treedata::kGroups) {
      float widest = 0;
      for (float r : g.radius)
        widest = std::max(widest, r);
      if (widest <= 0)
        continue;
      const float discR = widest + 26;
      root.child(box().width(Dim(discR * 2)).height(Dim(discR * 2))
                     .centerAt({g.x, g.y})
                     .fill(Material::radial(
                         {discR, discR}, discR,
                         {{0.00f, {0.20f, 0.16f, 0.13f, 0.55f}},
                          {0.62f, {0.16f, 0.13f, 0.11f, 0.30f}},
                          {1.00f, {0.10f, 0.08f, 0.07f, 0.0f}}}))
                     .zIndex(0));
      for (float r : g.radius) {
        if (r <= 0)
          continue;
        root.child(box().width(Dim(r * 2)).height(Dim(r * 2))
                       .centerAt({g.x, g.y})
                       .outline(pt::circleOutline())
                       .stroke(util::stroke(
                           1.0f, Fill::color({pt::kPewter.fR, pt::kPewter.fG,
                                              pt::kPewter.fB, 0.30f})))
                       .zIndex(0));
      }
    }
  }

  /** The wrap comet: the widest group's orbit ring carries a fixed
   *  [0.92, 1.06] trim window whose offset is a bound wrapping phase.
   *  TrimMode::Wrap keeps BOTH pieces across the seam, so the highlight
   *  marches around the circle forever without a blink. */
  void cometRing(Element &root) {
    namespace pt = skill_tree;
    int best = -1;
    float bestR = 0;
    for (int i = 0; i < (int)(sizeof(treedata::kGroups) / sizeof(treedata::kGroups[0]));
         ++i)
      for (float r : treedata::kGroups[i].radius)
        if (r > bestR) {
          bestR = r;
          best = i;
        }
    if (best < 0)
      return;
    const treedata::Group &g = treedata::kGroups[best];
    root.child(box().width(Dim(bestR * 2)).height(Dim(bestR * 2))
                   .centerAt({g.x, g.y})
                   .outline(pt::circleOutline())
                   .trim(0.92f, 1.06f, &ringPhase, TrimMode::Wrap)
                   .stroke(brushes::pulse(
                       {pt::kHalo.fR, pt::kHalo.fG, pt::kHalo.fB, 0.22f},
                       {1, 1, 1, 0.75f}, 0.72f))
                   .zIndex(2));
  }

  /** Every link is a rail. Same-group/same-orbit pairs get that group's
   *  centre as the orbit router's focus and come out as arcs; everything
   *  else stays a straight spoke. The rope state is the WEAKER endpoint. */
  void edges(Element &root) {
    namespace pt = skill_tree;
    for (int i = 0; i < treedata::kEdgeCount; ++i) {
      const treedata::Edge &e = treedata::kEdges[i];
      const int state = pt::edgeState(treedata::kNodes[e.a].state,
                                      treedata::kNodes[e.b].state);
      RailRouter router;
      if (e.arc) {
        const treedata::Group &g = treedata::kGroups[e.group];
        router = routers::orbit({g.x, g.y});
      }
      root.child(rail({{nodeKey(e.a)}, {nodeKey(e.b)}}, std::move(router))
                     .inset(0)
                     .stroke(brushes::rope(state, pt::kRopeScale))
                     .zIndex(1));
    }
  }

  /** The allocated spine, drawn as ONE rail so it can draw itself on at
   *  mount, plus the packet that travels it. The spine's own links are
   *  already painted by edges(); this rides on top in Active rope. */
  void spine(Element &root) {
    namespace pt = skill_tree;
    using namespace std::chrono_literals;
    std::vector<Anchor> run;
    for (int i = 0; i < treedata::kNodeCount; ++i)
      if (treedata::kNodes[i].state == treedata::State::Allocated)
        run.push_back(Anchor{nodeKey(i)});
    if (run.size() < 2)
      return;
    // Ordered along the path: the table is sorted by position, so walk the
    // edge list to put the allocated run in graph order.
    std::vector<int> path;
    {
      std::vector<int> alloc;
      for (int i = 0; i < treedata::kNodeCount; ++i)
        if (treedata::kNodes[i].state == treedata::State::Allocated)
          alloc.push_back(i);
      auto degree = [&](int n) {
        int d = 0;
        for (int i = 0; i < treedata::kEdgeCount; ++i) {
          const treedata::Edge &e = treedata::kEdges[i];
          const bool aIn = treedata::kNodes[e.a].state == treedata::State::Allocated;
          const bool bIn = treedata::kNodes[e.b].state == treedata::State::Allocated;
          if (aIn && bIn && (e.a == n || e.b == n))
            ++d;
        }
        return d;
      };
      int start = alloc.front();
      for (int n : alloc)
        if (degree(n) == 1) {
          start = n;
          break;
        }
      std::vector<bool> used(treedata::kEdgeCount, false);
      path.push_back(start);
      for (;;) {
        bool advanced = false;
        for (int i = 0; i < treedata::kEdgeCount && !advanced; ++i) {
          if (used[i])
            continue;
          const treedata::Edge &e = treedata::kEdges[i];
          const int cur = path.back();
          const int other = e.a == cur ? e.b : e.b == cur ? e.a : -1;
          if (other < 0 ||
              treedata::kNodes[other].state != treedata::State::Allocated)
            continue;
          used[i] = true;
          path.push_back(other);
          advanced = true;
        }
        if (!advanced)
          break;
      }
    }
    if (path.size() < 2)
      return;
    std::vector<Anchor> anchors;
    for (int n : path)
      anchors.push_back(Anchor{nodeKey(n)});
    // Straight router: the spine crosses groups, and routers::orbit only
    // curves same-radius pairs anyway — a single focus would be a lie.
    root.child(rail(anchors)
                   .inset(0)
                   .trim(0.0f, withFrom(0.0f, 1.0f, {900ms}))
                   .stroke(brushes::rope(2, pt::kRopeScale))
                   .zIndex(2));
    root.child(rail(anchors)
                   .inset(0)
                   .trim(&pulseS, &pulseE)
                   .stroke(brushes::pulse({pt::kHalo.fR, pt::kHalo.fG,
                                           pt::kHalo.fB, 0.35f},
                                          {1, 1, 1, 0.9f}, 1.25f))
                   .zIndex(2));
  }

  /** Matched nodes get a breathing green ring; the selected node gets a
   *  slowly rotating ornament, the way Daripher's tree marks a favourite. */
  void highlights(Element &root) {
    namespace pt = skill_tree;
    for (int i = 0; i < treedata::kNodeCount; ++i) {
      if (!searched(i))
        continue;
      const treedata::Node &n = treedata::kNodes[i];
      const float d = pt::diameterOf(n.kind) + 13;
      root.child(box().width(Dim(d)).height(Dim(d))
                     .centerAt({n.x, n.y})
                     .outline(pt::circleOutline())
                     .opacity(&searchPulse)
                     .stroke(util::stroke(
                         1.6f, Fill::color({pt::kSearch.fR, pt::kSearch.fG,
                                            pt::kSearch.fB, 0.85f})))
                     .zIndex(5));
    }
    const treedata::Node &sel = treedata::kNodes[selectedIndex()];
    const float d = pt::diameterOf(sel.kind) + 30;
    root.child(box().width(Dim(d)).height(Dim(d))
                   .centerAt({sel.x, sel.y})
                   .rotate(&selectSpin)
                   .outline(shapes::star(12, 0.82f))
                   .stroke(util::stroke(
                       1.2f, Fill::color({pt::kHalo.fR, pt::kHalo.fG,
                                          pt::kHalo.fB, 0.55f})))
                   .zIndex(5));
  }

  /** The tooltip: name, kind rule, stat lines, italic flavour. Anchored in
   *  the empty bottom-left with a leader back to the selected node. */
  void detailCard(Element &root) {
    namespace pt = skill_tree;
    const int sel = selectedIndex();
    const treedata::Node &node = treedata::kNodes[sel];
    const treedata::Detail *detail = nullptr;
    for (int i = 0; i < treedata::kDetailCount; ++i)
      if (std::string_view(treedata::kDetails[i].name) == node.name) {
        detail = &treedata::kDetails[i];
        break;
      }
    if (!detail)
      return;

    using namespace std::chrono_literals;
    constexpr float kCardW = 268, kCardX = 34, kCardY = 428;
    Element card =
        box().width(Dim(kCardW)).left(kCardX).top(kCardY)
            .column().padding(16, 13).gap(0).corners({3})
            .fill(Material::linear({0, 0}, {0, 170},
                                   {{0.0f, {0.075f, 0.063f, 0.051f, 0.96f}},
                                    {1.0f, {0.043f, 0.036f, 0.031f, 0.96f}}}))
            .background(styles::dropShadow({0, 0, 0, 0.6f}, {0, 6}, 14))
            .foreground(util::stroke(
                1.2f, Fill::color({pt::kGold.fR, pt::kGold.fG, pt::kGold.fB,
                                   0.45f})))
            .zIndex(7)
            .opacity(withFrom(0.0f, 1.0f, {420ms}))
            .translateY(withFrom(10.0f, 0.0f, {520ms}))
            .child(text(toU8(detail->name), pt::type(17, pt::kHalo, 2.4f)))
            .child(text(toU8(detail->kind), pt::type(9.5f, pt::kAsh, 3.2f))
                       .margin(0, 3, 0, 0))
            .child(box().width(Dim(kCardW - 32)).height(Dim(1.0f))
                       .margin(0, 9, 0, 9)
                       .fill(Material::linear(
                           {0, 0}, {kCardW - 32, 0},
                           {{0.0f, {pt::kGold.fR, pt::kGold.fG, pt::kGold.fB,
                                    0.55f}},
                            {1.0f, {pt::kGold.fR, pt::kGold.fG, pt::kGold.fB,
                                    0.0f}}})));
    for (const char *line : detail->stats) {
      if (!line)
        continue;
      card.child(box().row().gap(7).margin(0, 0, 0, 5)
                     .child(box().width(Dim(3.0f)).height(Dim(3.0f))
                                .margin(0, 6, 0, 0).corners({1.5f})
                                .fill(Material::solid(
                                    {pt::kRimLit.fR, pt::kRimLit.fG,
                                     pt::kRimLit.fB, 0.9f})))
                     .child(text(toU8(line), pt::type(12, {0.62f, 0.68f, 0.90f,
                                                           1}, 0.2f))
                                .grow(1)));
    }
    if (detail->flavour)
      card.child(text(toU8(detail->flavour),
                      pt::type(11.5f, {0.42f, 0.38f, 0.32f, 1}, 0.3f, true))
                     .margin(0, 9, 0, 0));
    // the leader from the card back to the node it describes
    root.child(rail({{"detail"}, {nodeKey(sel)}})
                   .inset(0)
                   .stroke(util::stroke(
                       1.0f, Fill::color({pt::kGold.fR, pt::kGold.fG,
                                          pt::kGold.fB, 0.35f})))
                   .zIndex(6));
    root.child(card.key("detail"));
  }

  void hud(Element &root) {
    namespace pt = skill_tree;
    int allocated = 0, matched = 0;
    for (int i = 0; i < treedata::kNodeCount; ++i) {
      allocated += treedata::kNodes[i].state == treedata::State::Allocated;
      matched += searched(i);
    }
    char points[32];
    std::snprintf(points, sizeof(points), "%d / 123", allocated);
    char found[48];
    std::snprintf(found, sizeof(found), "%d matched", matched);

    root.child(box().column().top(30).left(38).zIndex(8)
                   .child(text(toU8("EMBERWOOD REACH"),
                               pt::type(24, pt::kBone, 3)))
                   .child(text(toU8("passive cluster \xe2\x80\x94 real orbit "
                                    "geometry, four frame states"),
                               pt::type(12, pt::kAsh, 1.0f))
                              .margin(0, 6, 0, 0)));
    root.child(box().column().alignItems(Align::End).top(30).right(36)
                   .zIndex(8)
                   .child(text(toU8(points), pt::type(21, pt::kGold, 2)))
                   .child(text(toU8("passive points"),
                               pt::type(10.5f, pt::kAsh, 1.5f))
                              .margin(0, 4, 0, 0)));
    // the search chip, Daripher's box with our palette
    root.child(box().row().alignItems(Align::Center).gap(8)
                   .bottom(28).left(38).zIndex(8)
                   .padding(10, 5).corners({3})
                   .fill(Material::solid({0.075f, 0.063f, 0.051f, 0.9f}))
                   .foreground(util::stroke(
                       1.0f, Fill::color({pt::kSearch.fR, pt::kSearch.fG,
                                          pt::kSearch.fB, 0.4f})))
                   .child(text(toU8("search"), pt::type(10, pt::kAsh, 1.8f)))
                   .child(text(toU8("fire"),
                               pt::type(12, pt::kSearch, 0.6f)))
                   .child(text(toU8(found), pt::type(10, pt::kAsh, 1.2f))));

    auto swatch = [&](int state, const char *label) {
      return box().row().alignItems(Align::Center).gap(8)
          .child(box().width(Dim(44.0f)).height(Dim(14.0f))
                     .outline(pt::hline())
                     .stroke(brushes::rope(state, 0.8f)))
          .child(text(toU8(label), pt::type(11, pt::kAsh, 0.8f)));
    };
    root.child(box().row().gap(20).alignItems(Align::Center)
                   .bottom(28).right(36).zIndex(8)
                   .child(swatch(0, "normal"))
                   .child(swatch(1, "intermediate"))
                   .child(swatch(2, "active")));
  }

  Element describe() {
    namespace pt = skill_tree;

    auto root = stack().fill(Material::radial(
        {pt::kW * 0.5f, pt::kH * 0.48f}, 600,
        {{0.0f, pt::kBgLift}, {0.55f, pt::kBg}, {1.0f, pt::kBgSink}}));

    groundwork(root);
    cometRing(root);
    edges(root);
    spine(root);

    for (int i = 0; i < treedata::kNodeCount; ++i) {
      switch (treedata::kNodes[i].kind) {
      case treedata::Kind::Minor:    minorNode(root, i);    break;
      case treedata::Kind::Notable:  notableNode(root, i);  break;
      case treedata::Kind::Mastery:  masteryNode(root, i);  break;
      case treedata::Kind::Keystone: keystoneNode(root, i); break;
      case treedata::Kind::Jewel:    jewelNode(root, i);    break;
      }
    }

    highlights(root);
    detailCard(root);
    hud(root);
    return root;
  }
};

} // namespace compose_gallery
