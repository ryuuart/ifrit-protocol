#pragma once

/** @file
 * SigilShape pop — POP-style point combinators as VALUES. A Chain is
 * a description (nondestructive: edit a field, re-describe), the
 * TouchDesigner lesson over shape's own Cloud vocabulary. The LANGUAGE
 * lives here, backend-neutral; executors live downstream: cook() is
 * the CPU reference (a Cloud, ready for points::instance / panels /
 * drawBillboards on the Skia painter), and SigilWorld runs the same
 * Chain as compute dispatches over GPU lanes. The two must agree —
 * same hashes, same spline, same formulas.
 */

#include <cstdint>
#include <variant>
#include <vector>

#include "sigilshape/Curves.h"
#include "sigilshape/Points.h"

namespace sigil::shape {

struct pop {
  enum class Lane : int32_t { P = 0, Dir = 1, Color = 2, Scale = 3, T = 4 };
  /** TouchDesigner's real superpower, adopted: operators address
   *  attributes BY NAME. The conventional lanes ("P", "T", "Dir",
   *  "Scale", "Color", plus "Tex" for texture hinting) are just
   *  well-known names — any other name creates a custom float4
   *  attribute on first write, flows through every filter, and
   *  exports on the cooked Cloud. The Lane enum remains as sugar for
   *  the builtins. */
  struct AttrRef {
    std::string name = "P";
    AttrRef() = default;
    AttrRef(Lane lane)  // NOLINT: implicit by design
        : name(lane == Lane::P       ? "P"
               : lane == Lane::Dir   ? "Dir"
               : lane == Lane::Color ? "Color"
               : lane == Lane::Scale ? "Scale"
                                     : "T") {}
    AttrRef(const char* n) : name(n) {}             // NOLINT: implicit
    AttrRef(std::string n) : name(std::move(n)) {}  // NOLINT
  };
  /** The GPU executor's packed-lane index for a builtin — "Tex"
   *  included, slot 5; -1 for custom names, whose chains the GPU
   *  executor declines gracefully. */
  static int32_t builtinIndex(const AttrRef& attr) {
    if (attr.name == "P") return 0;
    if (attr.name == "T") return 1;
    if (attr.name == "Dir") return 2;
    if (attr.name == "Scale") return 3;
    if (attr.name == "Color") return 4;
    if (attr.name == "Tex") return 5;
    return -1;
  }
  static constexpr int32_t kBuiltinSlots = 6;
  /** Generator: scatter count points along a window of a closed
   *  loop — writes P, T, Dir (the tangent), Scale = 1. */
  struct SplineScatter {
    std::vector<glm::vec3> loop;
    int count = 10000;
    float head = 1, span = 1;
    float radius = 0;  ///< stable per-point offset in the normal plane
    uint32_t seed = 1;
  };
  /** Filter: lane += a stable random cube offset per point. */
  struct Jitter {
    AttrRef lane = Lane::P;
    float amplitude = 10;
    uint32_t seed = 7;
  };
  /** Filter: lane += a smooth sin-field drift sampled at P. */
  struct Noise {
    AttrRef lane = Lane::P;
    float amplitude = 10;
    float frequency = 0.01f;
    float seed = 0;
  };
  /** Filter: lane = lerp(from, to) by the T attribute. */
  struct Ramp {
    AttrRef lane = Lane::Color;
    glm::vec4 from = {1, 1, 1, 1};
    glm::vec4 to = {1, 1, 1, 1};
  };
  /** Filter: lane.x = base * (1 + spread * (hash * 2 - 1)). */
  struct Vary {
    AttrRef lane = Lane::Scale;
    float base = 1;
    float spread = 0.5f;
    uint32_t seed = 11;
  };
  /** Filter: Dir = normalize(target - P) — billboards, gazes. */
  struct LookAt {
    glm::vec3 target = {0, 0, 0};
  };
  /** Filter: lane = lane * mul + add, per component. */
  struct Math {
    AttrRef lane = Lane::P;
    glm::vec4 mul = {1, 1, 1, 1};
    glm::vec4 add = {0, 0, 0, 0};
  };
  /** Filter: neighborhood smoothing — each point eases toward its
   *  chain-order neighbors' midpoint (ends clamp). The op the ribbon
   *  example demanded: it heals Noise kinks before a swept sink so
   *  parallel-transport frames stop tearing. Double-buffered on both
   *  executors, so order can't leak. */
  struct Relax {
    AttrRef lane = Lane::P;
    float strength = 0.5f;  ///< 0 = off, 1 = full midpoint
    int iterations = 1;
  };
  /** Generator: scatter count points ON a formed model's surface.
   *  Seeds a chain from a Mesh — sweep a tube, scatter on it, form
   *  again: pops build on pops' results. CPU-cooked today (the GPU
   *  executor declines mesh-led chains). */
  struct MeshScatter {
    Mesh mesh;
    int count = 10000;
    uint32_t seed = 1;
  };
  /** Creator (TD's Attribute Create): fill an attribute — customs
   *  spring into being on first write. */
  struct Set {
    AttrRef attr = "Tex";
    glm::vec4 value = {0, 0, 1, 1};
  };
  /** Texture hint: pick a sprite-atlas cell per point (stable hash)
   *  and write "Tex" = {uOffset, vOffset, uScale, vScale}. The
   *  stamps sink applies it to each stamped point's uvs. */
  struct Atlas {
    int cols = 2, rows = 2;
    uint32_t seed = 17;
  };
  /** Filter, PRIMITIVE class (TD/Houdini's Attribute Promote,
   *  point -> prim): bake a point attribute onto the PRIMITIVES the
   *  chain's forming sink builds — Mesh::prims[to], one float4 per
   *  triangle. The reserved source name "Id" writes the owning point's
   *  index instead of reading a lane.
   *
   *  Class boundaries, stated: inert on the point sink (a Cloud has no
   *  primitives) and on the swept sinks (their triangles ride
   *  RESAMPLED cross-sections, not points — there is no owning point
   *  to promote from); honoured by the stamping sink cookMesh(). The
   *  GPU executor cooks POINTS only and declines any chain holding
   *  this op outright rather than dropping it silently. */
  struct Promote {
    AttrRef from = Lane::Color;
    std::string to;  ///< primitive lane name; empty = the source's name
  };
  /** Filter (TouchDesigner's Lookup): DRIVE one attribute from another
   *  through a table of stops. The key is dot(from, weights); it is
   *  remapped from [low, high] onto the table's span and sampled with
   *  linear interpolation, so the table is a curve, not a palette.
   *
   *  This is `fade` generalized — Ramp is the two-stop case driven by
   *  T with no domain. Any source attribute, any number of stops, any
   *  range: "colour by height", "size by density", a non-linear
   *  falloff on a custom lane. Per-point and count-invariant, so BOTH
   *  executors run it; an empty table is a no-op on both. */
  struct Lookup {
    AttrRef from = Lane::T;
    glm::vec4 weights = {1, 0, 0, 0};  ///< key = dot(from, weights)
    AttrRef to = Lane::Color;
    std::vector<glm::vec4> stops = {{0, 0, 0, 1}, {1, 1, 1, 1}};
    float low = 0, high = 1;  ///< the source range the table spans
  };
  /** Filter, PERMUTATION class (TouchDesigner's Sort): reorder the
   *  whole point set by dot(by, weights). Every lane travels with its
   *  point — a permutation, not a rewrite, so the count never moves.
   *
   *  Chain ORDER is meaning here: the point sink draws in it (painter
   *  order for transparent sprites, which is what the Skia sink has
   *  instead of a depth buffer), the swept sinks thread their path
   *  through it, and Relax smooths along it. Sorting is therefore an
   *  authoring verb, not a display trick.
   *
   *  CPU-only, and stated as a boundary rather than a gap: a
   *  permutation is not a per-point map, so it does not fit the GPU
   *  executor's one-kernel-per-op arena model (it would want a sorting
   *  NETWORK — log^2(n) dispatches and a ping-pong — which is a
   *  different dispatch shape, not a different formula). SigilWorld
   *  declines any chain holding one, the way it declines MeshScatter
   *  and Promote. */
  struct Sort {
    AttrRef by = Lane::P;
    glm::vec4 weights = {0, 0, 1, 0};  ///< key = dot(by, weights)
    bool descending = false;
  };
  /** Variant ORDER IS ABI: SigilWorld maps each op's variant index to
   *  a compute PSO. New ops are APPENDED, never inserted. */
  using Op =
      std::variant<SplineScatter, Jitter, Noise, Ramp, Vary, LookAt, Math,
                   Relax, MeshScatter, Set, Atlas, Promote, Lookup, Sort>;
  using Chain = std::vector<Op>;

  /** The artist's spelling — TouchDesigner ergonomics over the same
   *  values: one entry verb, chained INTENT verbs with loud defaults
   *  (seeds auto-vary; every parameter is optional), and the result
   *  IS the Chain — nothing hides in the builder, so reach into any
   *  op afterwards and re-cook. Sinks form directly:
   *
   *    Mesh comet = pop::on(loop).count(9000).window(0.9f, 0.3f)
   *                     .spread(40).noise(18)
   *                     .fade(pink, cyan).tube(9);
   */
  class Builder {
   public:
    explicit Builder(std::vector<glm::vec3> loop) {
      SplineScatter scatter;
      scatter.loop = std::move(loop);
      m_chain.push_back(scatter);
    }
    /** Compose: build ON another chain — its cooked P becomes this
     *  chain's path. Pops feed pops; positions are the currency.
     *  (Cooked on the CPU reference at build time; a GPU-resident
     *  chain-to-chain feed is the queued next step.) */
    explicit Builder(const Chain& upstream);
    explicit Builder(MeshScatter scatter) {
      m_chain.push_back(std::move(scatter));
    }
    Builder& count(int n) {
      if (auto* s = std::get_if<SplineScatter>(&m_chain.front()))
        s->count = n;
      else if (auto* m = std::get_if<MeshScatter>(&m_chain.front()))
        m->count = n;
      return *this;
    }
    /** Loop entries only; a surface entry has no window. */
    Builder& window(float head, float span) {
      if (auto* s = std::get_if<SplineScatter>(&m_chain.front())) {
        s->head = head;
        s->span = span;
      }
      return *this;
    }
    Builder& spread(float radius) {
      if (auto* s = std::get_if<SplineScatter>(&m_chain.front()))
        s->radius = radius;
      return *this;
    }
    Builder& seed(uint32_t v) {
      if (auto* s = std::get_if<SplineScatter>(&m_chain.front()))
        s->seed = v;
      else if (auto* m = std::get_if<MeshScatter>(&m_chain.front()))
        m->seed = v;
      return *this;
    }
    Builder& jitter(float amplitude, AttrRef attr = Lane::P) {
      m_chain.push_back(Jitter{std::move(attr), amplitude, nextSeed()});
      return *this;
    }
    Builder& noise(float amplitude, float frequency = 0.01f,
                   AttrRef attr = Lane::P) {
      m_chain.push_back(
          Noise{std::move(attr), amplitude, frequency, (float)nextSeed()});
      return *this;
    }
    Builder& vary(float spread, float base = 1, AttrRef attr = Lane::Scale) {
      m_chain.push_back(Vary{std::move(attr), base, spread, nextSeed()});
      return *this;
    }
    Builder& fade(glm::vec4 from, glm::vec4 to) {
      m_chain.push_back(Ramp{Lane::Color, from, to});
      return *this;
    }
    Builder& tint(glm::vec4 color) { return fade(color, color); }
    Builder& lookAt(glm::vec3 target) {
      m_chain.push_back(LookAt{target});
      return *this;
    }
    Builder& move(glm::vec3 offset) {
      m_chain.push_back(
          Math{Lane::P, {1, 1, 1, 1}, {offset.x, offset.y, offset.z, 0}});
      return *this;
    }
    /** Create/fill any attribute — customs included. */
    Builder& set(AttrRef attr, glm::vec4 value) {
      m_chain.push_back(Set{std::move(attr), value});
      return *this;
    }
    /** Texture hint: a stable per-point sprite-atlas cell in "Tex". */
    Builder& atlas(int cols, int rows) {
      m_chain.push_back(Atlas{cols, rows, nextSeed()});
      return *this;
    }
    /** Drive one attribute from another through a table of stops —
     *  `fade` grown up: pick the source, pick which of its components
     *  reads, give the range it spans, hand over as many stops as the
     *  curve needs. `.rampBy(Lane::P, 1, {deep, shallow}, 0, 200)` is
     *  "colour by height". */
    Builder& rampBy(AttrRef from, int component, std::vector<glm::vec4> stops,
                    float low = 0, float high = 1, AttrRef to = Lane::Color) {
      m_chain.push_back(Lookup{std::move(from), componentWeight(component),
                               std::move(to), std::move(stops), low, high});
      return *this;
    }
    /** The loud-default spelling: a multi-stop gradient down T. */
    Builder& rampBy(std::vector<glm::vec4> stops = {{0, 0, 0, 1},
                                                    {1, 1, 1, 1}}) {
      return rampBy(Lane::T, 0, std::move(stops));
    }
    /** Put the points in order along an axis — farthest-first painter
     *  order for transparent sprites, or a re-threading of the path
     *  the swept sinks follow. Pass the camera's forward vector and
     *  `descending` for back-to-front. */
    Builder& order(glm::vec3 axis = {0, 0, 1}, bool descending = false) {
      m_chain.push_back(Sort{Lane::P, {axis.x, axis.y, axis.z, 0}, descending});
      return *this;
    }
    /** ...or by any attribute's component: `.orderBy("energy")`. */
    Builder& orderBy(AttrRef by, int component = 0, bool descending = false) {
      m_chain.push_back(
          Sort{std::move(by), componentWeight(component), descending});
      return *this;
    }
    /** Carry a point attribute onto the PRIMITIVES the sink forms —
     *  the prim class, addressed by the same names. "Id" promotes the
     *  owning point's index. An empty @p to keeps the source's name. */
    Builder& promote(AttrRef from, std::string to = {}) {
      if (to.empty()) to = from.name;
      m_chain.push_back(Promote{std::move(from), std::move(to)});
      return *this;
    }
    /** Heal kinks: neighborhood smoothing on P (the ribbon-saver). */
    Builder& smooth(float strength = 0.5f, int iterations = 2) {
      m_chain.push_back(Relax{Lane::P, strength, iterations});
      return *this;
    }
    /** Escape hatch: any raw op joins the chain. */
    Builder& op(Op o) {
      m_chain.push_back(std::move(o));
      return *this;
    }

    operator Chain() const { return m_chain; }
    const Chain& chain() const { return m_chain; }

    // The sinks (defined after popops below): pick the former.
    Cloud cloud() const;
    Mesh stamps(const Mesh& stamp) const;
    Mesh tube(float radius, int sides = 12, bool closed = false,
              int segments = 160) const;
    Mesh ribbon(float width, bool closed = false, int segments = 160) const;
    Mesh sweep(const SkPath& profile, bool closed = false,
               int segments = 160) const;

   private:
    static glm::vec4 componentWeight(int component) {
      glm::vec4 w{0, 0, 0, 0};
      w[component < 0 ? 0 : (component > 3 ? 3 : component)] = 1;
      return w;
    }
    uint32_t nextSeed() { return m_seed++; }
    Chain m_chain;
    uint32_t m_seed = 101;
  };
  /** The entry verb: points on (a window of) this closed loop. */
  static Builder on(std::vector<glm::vec3> loop) {
    return Builder(std::move(loop));
  }
  /** The composing entry: points on another chain's cooked result. */
  static Builder on(const Chain& upstream) { return Builder(upstream); }
  /** The surface entry: points on a formed model's faces. */
  static Builder on(Mesh surface, int count = 10000) {
    MeshScatter scatter;
    scatter.mesh = std::move(surface);
    scatter.count = count;
    return Builder(std::move(scatter));
  }
};

namespace popops {

/** The CPU reference cook: evaluates @p chain into a Cloud with the
 *  conventional lanes — "t" (scalar), "dir" (vector), "tint" (color),
 *  "size" (scalar) — bit-faithful to the GPU executor's formulas. */
Cloud cook(const pop::Chain& chain);

/** The mesh-forming sink: cook @p chain and stamp @p stamp at every
 *  point into ONE Mesh (dir orients, size scales, tint colors) — a
 *  pop-DESCRIBED 3D model, drawable by space::drawMesh on the Skia
 *  painter and addSurface in SigilWorld alike. */
Mesh cookMesh(const pop::Chain& chain, const Mesh& stamp);

/** Swept sinks: the chain's cooked points become the PATH — a
 *  Catmull-Rom through P in chain order, so Jitter/Noise/Math edits
 *  BEND the sweep — and the curve generators form the model. The
 *  same nondestructive description, a different former. */
struct SweepSinkOptions {
  bool closed = false;  ///< join the last cooked point to the first
  int segments = 160;   ///< resampled cross-sections along the path
};
Mesh cookTube(const pop::Chain& chain, float radius, int sides = 12,
              const SweepSinkOptions& options = {});
Mesh cookRibbon(const pop::Chain& chain, float width,
                const SweepSinkOptions& options = {});
/** The general former: ANY closed 2D outline (a star, a squircle, an
 *  Ops.h recipe's result) becomes the cross-section, swept along the
 *  chain's cooked path — tube generalized to the whole shape
 *  vocabulary. */
Mesh cookSweep(const pop::Chain& chain, const SkPath& profile,
               const SweepSinkOptions& options = {});

}  // namespace popops

inline Cloud pop::Builder::cloud() const { return popops::cook(m_chain); }
inline Mesh pop::Builder::stamps(const Mesh& stamp) const {
  return popops::cookMesh(m_chain, stamp);
}
inline Mesh pop::Builder::tube(float radius, int sides, bool closed,
                               int segments) const {
  return popops::cookTube(m_chain, radius, sides,
                          {.closed = closed, .segments = segments});
}
inline Mesh pop::Builder::ribbon(float width, bool closed, int segments) const {
  return popops::cookRibbon(m_chain, width,
                            {.closed = closed, .segments = segments});
}
inline pop::Builder::Builder(const Chain& upstream)
    : Builder(popops::cook(upstream).positions) {}

inline Mesh pop::Builder::sweep(const SkPath& profile, bool closed,
                                int segments) const {
  return popops::cookSweep(m_chain, profile,
                           {.closed = closed, .segments = segments});
}

}  // namespace sigil::shape
