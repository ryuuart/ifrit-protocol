#pragma once

/** @file
 * DECLARED MOTION for world entities — the second animation door, next
 * to (never instead of) the imperative setters.
 *
 * Until 2026-07-29 SigilWorld's only way to move anything was "two
 * floats per frame": the caller computed a value and called a setter
 * (`setTransform`, `setSweepWindow`, a `MaterialComponent` field). That
 * door stays exactly as it is and is still the right tool for a one-off
 * poke. What it cannot do is DECLARE — say once that this surface's
 * uv window rides that phase through this curve, and then never think
 * about it again. `Animatable<float>` is that declaration, and it
 * became reachable here when it moved out of SigilCompose into
 * <sigilmotion/Animation.h> (SigilMotion links choreograph and nothing
 * else, so world can hold it without swallowing a drawing library).
 *
 * The shape of the door:
 *
 *     choreograph::Output<float> phase{0};
 *     ticker.timeline().apply(&phase).then<ch::RampTo>(1.0f, 8.0f);
 *
 *     auto &reg = world.registry();
 *     reg.emplace<AnimatedMaterial>(entity(bandId)).uvOffsetX =
 *         bind(&phase).target(0.0f, 1.0f);
 *     reg.emplace<AnimatedTransform>(entity(dartId), base).yawDeg =
 *         bind(&phase).map(ease::inOutBack()).target(0.0f, 360.0f);
 *
 *     for (int frame = 0; frame < 600; ++frame) {
 *       ticker.tick(1.0 / 60.0);   // the CALLER owns the clock
 *       world.render();            // resolves, then draws
 *     }
 *
 * Three rulings are worth knowing before you use it (the arguments are
 * in world/README.md, dated 2026-07-29):
 *
 * 1. **Every lane is a float.** Not `Animatable<glm::vec3>` — a
 *    position is three lanes. `bind()`'s normalise → curve → affine
 *    chain (`source`/`window`/`map`/`target`/`quantize`/`clamp`) is
 *    FLOAT-ONLY, and that chain is most of the value of the door; a
 *    vec3 slot could only hold a plain constant or a raw binding, which
 *    is a weaker lane wearing a fancier type. `Animatable<float>`
 *    converts implicitly from float, so `at(0, 60, 0)` still reads like
 *    a position.
 *
 * 2. **World owns no clock.** There is no `world.tick()`, no
 *    `render(dt)`, no FrameClock in here. `FrameClock::tick()` reads
 *    `steady_clock`; a world that ticked one inside `render()` would
 *    make every headless plate a function of wall time, and world_demo's
 *    13 artifacts must be byte-reproducible. The caller steps a
 *    `motion::Ticker` with the delta it chooses (`1.0/60.0` in a
 *    headless loop) and `render()` is a pure function of whatever the
 *    Outputs hold at that instant.
 *
 * 3. **The `animate(...)` form lands on its SETTLED value.** These
 *    lanes accept `animate(to(v))` / `animate(from(a).to(b))` because
 *    they are the same slot type compose uses — but ramp-on-change
 *    needs a CHANGE event, and world has no describe/diff over
 *    components (they are mutated in place). So a transitioned value
 *    resolves to its target with no ramp, exactly as compose's
 *    `snapshot()` bakes the settled value. To actually ramp, put the
 *    ramp on the timeline and bind it.
 *
 * The caller owns the `choreograph::Output`s a lane points at; a bound
 * lane outliving its Output dangles (the same contract compose has).
 *
 * The camera joined on 2026-07-29 as `AnimatedCamera` — a fifth
 * component on the same footing as the rest, because a camera is
 * already a registry entity (`CameraComponent`) and needed no new home.
 *
 * MEETING `scene::Scene` (the light door, 2026-08-04): a declared
 * node's entity is published by `Scene::find(keyPath)`, which is the
 * supported way to attach an `Animated*` to a scene-managed leaf. Two
 * rules still stand and are now LOUD rather than silent: a kept leaf's
 * lane outranks its re-described placement/material (the reconciler
 * warns, once per node), and a leaf whose mesh or material changes is
 * remove+add — a NEW entity, your lanes destroyed with the old one, so
 * find() again and re-attach. Camera lanes remain the exception that
 * composes freely — the camera is not a scene node. Pinned by the
 * `WorldSceneAnimation` tests and argued in the README.
 */

#include "sigilworld/Components.h"

#include <sigilmotion/Animation.h>
#include <sigilshape/Curves.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace sigil::world {

/** The value vocabulary, spelled without a namespace prefix at world
 *  call sites — the same re-export SigilCompose does into
 *  `sigil::compose`. `motion::` remains the home. */
using motion::Animatable;
using motion::Bound;
using motion::Transition;
using motion::Transitioned;
using motion::animate;
using motion::bind;
using motion::from;
using motion::through;
using motion::to;
using motion::wiggle;
namespace ease = motion::ease;

/** A COMPLETE placement, composed each resolve as
 *  `model = base * translate * yaw * pitch * roll * scale` — the same
 *  TRS convention (and the same order) as `scene::Node::localMatrix()`,
 *  so a node and an animated entity agree about what "turned 30" means.
 *
 *  Unlike the material and light components below, the lanes here are
 *  NOT optional, because this component describes the whole transform
 *  rather than overriding part of one: an unmentioned lane is genuinely
 *  "no translation" / "unit scale". A surface that already has a static
 *  placement puts it in @ref base and animates around it — TRS is then
 *  read in the surface's own parent frame.
 *
 *  This component OWNS its entity's TransformComponent. Do not also
 *  drive it with `setTransform()`; the next resolve would win. */
struct AnimatedTransform {
  /** The static placement the animated TRS rides on top of. */
  glm::mat4 base{1.0f};
  Animatable<float> x = 0.0f, y = 0.0f, z = 0.0f;
  Animatable<float> yawDeg = 0.0f, pitchDeg = 0.0f, rollDeg = 0.0f;
  Animatable<float> scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
};

/** PARTIAL overrides of a MaterialComponent's live scalars: only the
 *  lanes you engage are written, everything else on the material is
 *  left exactly as authored. That is what the `optional` buys and it is
 *  not decoration — a component with plain defaults would slam
 *  `opacity` to 1 on a pane authored at 0.4 the moment you engaged
 *  `uvOffsetX`.
 *
 *  `opacity` writes `Material::baseColor.w`, so it also routes the
 *  surface between the opaque and the blended pass, live, exactly as a
 *  hand-written alpha does.
 *
 *  Colour is deliberately absent — see the README ruling: three
 *  independent linear-RGB float lanes are the WRONG default for a
 *  colour ramp (this repo already runs OKLab in `shape::blend` for
 *  precisely that reason), so a colour lane wants a colour type, not
 *  four floats in a trench coat. */
struct AnimatedMaterial {
  std::optional<Animatable<float>> opacity;
  std::optional<Animatable<float>> emissiveStrength;
  std::optional<Animatable<float>> uvOffsetX, uvOffsetY;
  std::optional<Animatable<float>> uvScaleX, uvScaleY;
};

/** PARTIAL overrides of a LightComponent (same optional rule as
 *  AnimatedMaterial). `x/y/z` drive `LightComponent::position`, so they
 *  are a point light's lanes; a directional light animates only
 *  `intensity` here (its `direction` wants to stay normalised, which
 *  three free float lanes cannot promise). */
struct AnimatedLight {
  std::optional<Animatable<float>> intensity;
  std::optional<Animatable<float>> x, y, z;
};

/** A FLIGHT PATH for the eye: a `shape::Spline3` plus the ONE float
 *  lane that walks it. Added 2026-07-29 because eight independent float
 *  lanes could not spell "fly along this curve" — three of them can
 *  describe a point, but nothing could describe a TRAJECTORY, and
 *  hand-driving `eyeX/Y/Z` from a spline means the caller computing
 *  three numbers per frame, i.e. the imperative door wearing the
 *  declarative one's clothes.
 *
 *  The float-only ruling is not bent to do it. The lane here is @ref t —
 *  the position ALONG the curve — so the whole normalise → curve →
 *  affine chain still applies, to the parameter rather than to the
 *  geometry: `bind(&phase).map(&choreograph::easeInOutQuad)` eases the
 *  flight in and out, `.target(0, 3)` flies three laps, `.window(...)`
 *  makes the flight a slice of some larger phase. The curve supplies the
 *  shape; the lane supplies the schedule. That separation is the whole
 *  design.
 *
 *  Four rules, all argued in world/README.md (2026-07-29):
 *
 *  - **PRECEDENCE: whatever the path drives, it drives outright.** It
 *    always drives the eye — that is what a path IS — so `eyeX/Y/Z` are
 *    IGNORED while a path is engaged (not blended, not offset: a lane
 *    that half-contradicts a curve can only produce a point off it). It
 *    drives the target if and only if @ref lookAhead is non-zero, which
 *    is the caller's own spelling of "aim it for me"; `lookAhead = 0`
 *    means "I aim it myself" and leaves `targetX/Y/Z` (and an authored
 *    target) untouched. One rule with a switch you can see at the call
 *    site, not two rules.
 *  - **WRAP: a closed spline wraps, an open one clamps.** `t` past 1 on
 *    a loop comes round to `t - 1` (and negative `t` runs backwards),
 *    because on a closed curve 0 and 1 are the same point and a hard
 *    stop mid-loop is never what "closed" meant. An open curve parks at
 *    its ends, which is what `Spline3::position` already does. The wrap
 *    also applies to the look-ahead point, so the aim reads ACROSS the
 *    seam instead of staring at the end of the loop while flying past it.
 *  - **ARC LENGTH is the default.** A camera move wants constant SPEED,
 *    and parameter-uniform motion on a Catmull-Rom loop sprints through
 *    tight knots and crawls through loose ones. @ref arcLength = false
 *    opts back out to parameter-uniform (which is what you want when the
 *    knots ARE the schedule).
 *  - **ROLL still composes.** `AnimatedCamera::rollDeg` resolves after
 *    the path, from the eye/target the path just produced, so a dutch
 *    tilt turns about the FLIGHT axis and follows the curve round.
 *
 *  A path with no control points is not engaged at all — the authored
 *  camera and the eye lanes stand. */
struct CameraPath {
  /** The curve the eye rides. Held by value: edit `points` in place and
   *  the next resolve picks it up (the table below re-derives). */
  shape::Spline3 path;
  /** WHERE along it, in [0,1] — arc-length fraction unless @ref
   *  arcLength is false, in which case it is the curve parameter. One
   *  float, so every `bind()` shaping verb still applies. */
  Animatable<float> t = 0.0f;
  /** How far ahead the camera looks, in the same units as @ref t. The
   *  target becomes `eye + (position(t + lookAhead) - position(t))`, so
   *  a negative value looks BACK down the curve (a chase cam) and
   *  exactly 0 disengages the target entirely — see the precedence rule.
   *  At the end of an OPEN curve, where the forward chord collapses, the
   *  last good chord is held rather than aiming the camera at itself. */
  float lookAhead = 0.05f;
  /** Constant speed (true, the default) or constant parameter rate. */
  bool arcLength = true;
  /** Resolution of the arc-length table; ignored when @ref arcLength is
   *  false. More samples = a truer speed on a wildly uneven curve. */
  int samples = 256;

  /** THE CACHE, written by `resolveAnimation()`: the cumulative chord
   *  length at `samples + 1` uniform parameter steps, plus the spline it
   *  was built from. Rebuilt when — and only when — that spline no
   *  longer matches, which is the same "compare against the
   *  destination" rule the value lanes use, applied to the INPUT that
   *  determines this table (comparing the table itself would mean
   *  building it, which is the cost being avoided). No dirty flag, so
   *  poking `path.points` directly cannot leave a stale table behind. */
  std::vector<float> arcTable;
  std::vector<glm::vec3> tablePoints;
  shape::Spline3::Type tableType = shape::Spline3::Type::CatmullRom;
  bool tableClosed = false;
  int tableSamples = 0;
};

/** PARTIAL overrides of a `CameraComponent`'s placement and lens (the
 *  same optional rule as AnimatedMaterial and for the same reason: the
 *  caller authors the camera, these lanes drive part of it).
 *
 *  The camera needed NO new home. Every other lane in this header hangs
 *  off a registry entity, and since Components.h a camera is a registry
 *  entity too — `CameraComponent{camera, active}`, whose documented,
 *  already-pinned rule is that an ACTIVE one overrides `World::setCamera`
 *  while it exists. So an animated camera is just a camera entity whose
 *  fields a system writes, this resolve stays in the device-free half
 *  (a camera lane is pinnable on a machine with no Vulkan), and the
 *  precedence question answers itself — see the README, 2026-07-29.
 *
 *  Eight lanes, and the two absences are the argument:
 *
 *  - `up` gets NO lanes. It must stay a unit vector roughly out of the
 *    view axis, which three free floats cannot promise — the same
 *    refusal AnimatedLight makes for a directional light's `direction`.
 *    @ref rollDeg is the safe single-float parameterisation of it (and
 *    the only way to declare a dutch tilt at all): it turns
 *    @ref rollReference right-handed about the eye→target axis, so the
 *    camera rolls CLOCKWISE seen from behind it and the scene tips
 *    counter-clockwise in frame. Recomputed from the fixed reference
 *    every resolve, so it neither drifts nor accumulates.
 *  - `zNear`/`zFar` get no lanes. Nobody ramps a clip plane on purpose:
 *    a moving near plane buys nothing and spends depth precision
 *    (z-fighting that pops as it slides). They are scene-scale
 *    constants — set them on the component or through `setCamera()`.
 *
 *  A ninth slot, @ref path, is not a lane but a CURVE plus one: it flies
 *  the eye along a `shape::Spline3` and outranks `eyeX/Y/Z` while
 *  engaged (@ref CameraPath has the whole argument).
 *
 *  Lanes resolve in order, so @ref rollDeg sees the eye/target this
 *  frame's own lanes (or path) just produced. `active` is NOT consulted: it gates
 *  the RENDERER's choice of camera, not this system, so toggling a
 *  camera on never replays a backlog of missed frames.
 *
 *  This component OWNS the CameraComponent fields it engages. Do not
 *  also write them by hand — the next resolve would win. */
struct AnimatedCamera {
  std::optional<Animatable<float>> eyeX, eyeY, eyeZ;
  std::optional<Animatable<float>> targetX, targetY, targetZ;
  std::optional<Animatable<float>> fovYDeg;
  std::optional<Animatable<float>> rollDeg;
  /** The un-rolled up vector @ref rollDeg turns about the view axis.
   *  Not a lane — the fixed reference that keeps roll idempotent. */
  glm::vec3 rollReference{0, 1, 0};
  /** A curve for the eye to fly, instead of `eyeX/Y/Z` — see
   *  @ref CameraPath for the precedence, wrap and arc-length rules. It
   *  lives HERE rather than in a component of its own because a path is
   *  a way of driving this camera's eye, and putting it beside the lanes
   *  it outranks is what makes that precedence statable in one place
   *  (two components would leave the rule depending on which system ran
   *  first — invisible at the call site). */
  std::optional<CameraPath> path;
};

/** The GPU generator window — `addSweep`/`addFlock`/`addPoints`'s
 *  `(head, span)`, the single most-animated pair in the whole corpus
 *  (world_demo's entire flight is this).
 *
 *  It is the one lane in this header that sits in front of a GPU
 *  RE-COOK rather than in front of a live shader parameter, and that
 *  makes it the one lane that could be a trap: an unconditional write
 *  would mark the surface dirty EVERY frame and re-dispatch the compute
 *  pass forever, even while the bound Output sits still (a 300k-point
 *  flock re-scattering for nothing). So this lane — and, for one rule
 *  everywhere, every other lane too — is CHANGE-DETECTED: resolve
 *  writes only what moved and reports it in AnimationStats. A constant
 *  lane costs exactly one re-cook, ever.
 *
 *  Resolution routes through the three public window setters, each of
 *  which is a documented no-op on a surface of the wrong kind, so one
 *  component covers sweeps, flocks and pop chains without world having
 *  to publish which kind an entity is. */
struct AnimatedWindow {
  Animatable<float> head = 1.0f;
  Animatable<float> span = 1.0f;

  /** Written by resolveAnimation(); the last pair actually pushed. */
  float appliedHead = 0, appliedSpan = 0;
  bool applied = false;
};

/** What the last resolve MOVED — the same "pruning is observable"
 *  contract `scene::Scene::Stats` sets. Zero across the board means the
 *  resolve was a no-op, which is exactly what a settled scene should
 *  report and what the idempotence pin asserts. */
struct AnimationStats {
  int transforms = 0;
  int materials = 0;
  int lights = 0;
  int cameras = 0;
  int windows = 0;
};

/** One lane's current number. The three live forms, in the order the
 *  slot discriminates them; a transitioned value yields its settled
 *  target (ruling 3 at the top of this file). */
inline float resolveValue(const Animatable<float> &v) {
  if (const choreograph::Output<float> *bound = v.binding()) {
    if (const motion::BoundFloat *shape = v.boundMap())
      return shape->apply(bound->value());
    return bound->value();
  }
  if (const float *plain = v.plain())
    return *plain;
  return v.transitioned()->value;
}

namespace detail {

/** The wrap rule: a closed curve comes round, an open one parks. */
inline float wrapPathParameter(float t, bool closed) {
  if (!closed)
    return std::clamp(t, 0.0f, 1.0f);
  const float wrapped = std::fmod(t, 1.0f);
  return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
}

/** Rebuild the cumulative-length table if — and only if — the spline it
 *  was built from has changed. Comparison against the spline, not a
 *  dirty flag: an equal spline has an equal table, so a caller who edits
 *  `path.points` in place cannot end up flying a stale curve. */
inline void refreshArcTable(CameraPath &p) {
  const int samples = std::max(p.samples, 2);
  if (p.tableSamples == samples && p.tableClosed == p.path.closed &&
      p.tableType == p.path.type && p.tablePoints == p.path.points)
    return;
  p.arcTable.assign((size_t)samples + 1, 0.0f);
  glm::vec3 prev = p.path.position(0.0f);
  for (int i = 1; i <= samples; ++i) {
    const glm::vec3 q = p.path.position((float)i / (float)samples);
    p.arcTable[(size_t)i] =
        p.arcTable[(size_t)i - 1] + glm::length(q - prev);
    prev = q;
  }
  p.tablePoints = p.path.points;
  p.tableType = p.path.type;
  p.tableClosed = p.path.closed;
  p.tableSamples = samples;
}

/** The curve PARAMETER at arc-length fraction @p s, by inverting the
 *  table. A curve with no extent answers `s` — there is nothing to
 *  re-space, and it keeps the degenerate case off the NaN path. */
inline float parameterAtArcFraction(const CameraPath &p, float s) {
  if (p.arcTable.size() < 2 || p.tableSamples < 1)
    return s;
  const float total = p.arcTable.back();
  if (!(total > 0.0f))
    return s;
  const float target = s * total;
  size_t hi = (size_t)std::distance(
      p.arcTable.begin(),
      std::upper_bound(p.arcTable.begin(), p.arcTable.end(), target));
  hi = std::clamp<size_t>(hi, 1, p.arcTable.size() - 1);
  const size_t lo = hi - 1;
  const float span = p.arcTable[hi] - p.arcTable[lo];
  const float local =
      span < 1e-9f ? 0.0f : (target - p.arcTable[lo]) / span;
  return ((float)lo + local) / (float)p.tableSamples;
}

/** Where a path puts the eye this frame, and the chord it aims along
 *  (zero when the path is not aiming — `lookAhead == 0`, or a curve with
 *  no extent at all). */
struct CameraPathSample {
  glm::vec3 eye{0, 0, 0};
  glm::vec3 aim{0, 0, 0};
};

inline CameraPathSample samplePath(CameraPath &p) {
  if (p.arcLength)
    refreshArcTable(p);
  const auto at = [&](float u) {
    const float w = wrapPathParameter(u, p.path.closed);
    return p.path.position(p.arcLength ? parameterAtArcFraction(p, w) : w);
  };
  CameraPathSample out;
  const float s = resolveValue(p.t);
  out.eye = at(s);
  if (p.lookAhead != 0.0f) {
    out.aim = at(s + p.lookAhead) - out.eye;
    // At the end of an OPEN curve the forward chord collapses; hold the
    // last good one rather than aiming the camera at its own eye.
    if (glm::dot(out.aim, out.aim) <= 1e-12f)
      out.aim = out.eye - at(s - p.lookAhead);
  }
  return out;
}

} // namespace detail

/** THE SYSTEM, in its device-free half: resolve every animated
 *  component that writes nothing but registry state.
 *
 *  A free function over the registry is the entt-idiomatic answer and
 *  it is also the TESTABLE one — this half needs no Vulkan, so the
 *  animation semantics stay pinned on a machine where every
 *  device-backed world test skips. `World::render()` calls the overload
 *  below, so nothing has to remember to call either. */
inline AnimationStats resolveAnimation(entt::registry &registry) {
  constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
  AnimationStats stats;

  for (auto [e, animated, transform] :
       registry.view<AnimatedTransform, TransformComponent>().each()) {
    glm::mat4 m = glm::translate(
        animated.base,
        glm::vec3{resolveValue(animated.x), resolveValue(animated.y),
                  resolveValue(animated.z)});
    // Zero-angle / unit-scale steps are skipped rather than multiplied
    // through, matching scene::Node::localMatrix() exactly.
    if (const float yaw = resolveValue(animated.yawDeg); yaw != 0)
      m = glm::rotate(m, yaw * kDegToRad, glm::vec3{0, 1, 0});
    if (const float pitch = resolveValue(animated.pitchDeg); pitch != 0)
      m = glm::rotate(m, pitch * kDegToRad, glm::vec3{1, 0, 0});
    if (const float roll = resolveValue(animated.rollDeg); roll != 0)
      m = glm::rotate(m, roll * kDegToRad, glm::vec3{0, 0, 1});
    const glm::vec3 scale{resolveValue(animated.scaleX),
                          resolveValue(animated.scaleY),
                          resolveValue(animated.scaleZ)};
    if (scale != glm::vec3{1, 1, 1})
      m = glm::scale(m, scale);
    if (!(transform.model == m)) {
      transform.model = m;
      ++stats.transforms;
    }
  }

  const auto put = [](float &dst,
                      const std::optional<Animatable<float>> &lane,
                      bool &changed) {
    if (!lane)
      return;
    const float v = resolveValue(*lane);
    if (dst != v) {
      dst = v;
      changed = true;
    }
  };

  for (auto [e, animated, material] :
       registry.view<AnimatedMaterial, MaterialComponent>().each()) {
    bool changed = false;
    Material &m = material.material;
    put(m.baseColor.w, animated.opacity, changed);
    put(m.emissiveStrength, animated.emissiveStrength, changed);
    put(m.uvOffset.x, animated.uvOffsetX, changed);
    put(m.uvOffset.y, animated.uvOffsetY, changed);
    put(m.uvScale.x, animated.uvScaleX, changed);
    put(m.uvScale.y, animated.uvScaleY, changed);
    if (changed)
      ++stats.materials;
  }

  for (auto [e, animated, light] :
       registry.view<AnimatedLight, LightComponent>().each()) {
    bool changed = false;
    put(light.intensity, animated.intensity, changed);
    put(light.position.x, animated.x, changed);
    put(light.position.y, animated.y, changed);
    put(light.position.z, animated.z, changed);
    if (changed)
      ++stats.lights;
  }

  for (auto [e, animated, cameraComponent] :
       registry.view<AnimatedCamera, CameraComponent>().each()) {
    bool changed = false;
    shape::space::Camera &c = cameraComponent.camera;
    // PRECEDENCE: an engaged path drives the eye outright, and the
    // target too iff it was asked to aim (lookAhead != 0). What the path
    // drives, the corresponding lanes do not — see CameraPath.
    const bool pathDrivesEye =
        animated.path && !animated.path->path.points.empty();
    const bool pathDrivesTarget =
        pathDrivesEye && animated.path->lookAhead != 0.0f;
    if (pathDrivesEye) {
      const detail::CameraPathSample flight =
          detail::samplePath(*animated.path);
      if (c.eye != flight.eye) {
        c.eye = flight.eye;
        changed = true;
      }
      if (pathDrivesTarget && glm::dot(flight.aim, flight.aim) > 0.0f) {
        const glm::vec3 target = flight.eye + flight.aim;
        if (c.target != target) {
          c.target = target;
          changed = true;
        }
      }
    } else {
      put(c.eye.x, animated.eyeX, changed);
      put(c.eye.y, animated.eyeY, changed);
      put(c.eye.z, animated.eyeZ, changed);
    }
    if (!pathDrivesTarget) {
      put(c.target.x, animated.targetX, changed);
      put(c.target.y, animated.targetY, changed);
      put(c.target.z, animated.targetZ, changed);
    }
    put(c.fovYDeg, animated.fovYDeg, changed);
    // Roll last, so it turns around the view axis the eye/target lanes
    // just produced. `up` is derived from the fixed rollReference, never
    // from its own previous value, so resolving twice lands twice.
    if (animated.rollDeg) {
      const glm::vec3 forward = c.target - c.eye;
      if (const float length = glm::length(forward); length > 0) {
        const float roll = resolveValue(*animated.rollDeg) * kDegToRad;
        const glm::vec3 up = glm::vec3(
            glm::rotate(glm::mat4(1.0f), roll, forward / length) *
            glm::vec4(animated.rollReference, 0.0f));
        if (c.up != up) {
          c.up = up;
          changed = true;
        }
      }
    }
    if (changed)
      ++stats.cameras;
  }

  return stats;
}

/** THE SYSTEM, whole: the registry half above plus AnimatedWindow,
 *  which needs the World because a generator window is a device
 *  parameter rather than a component field.
 *
 *  `World::render()` calls this before anything else it does, so the
 *  cook/draw passes see the frame's values. Call it yourself when you
 *  want the resolved state WITHOUT a frame — before a `readPoints()`
 *  query, say — or when you want the Stats. It is idempotent: a second
 *  call with unmoved Outputs writes nothing and reports zeros. */
inline AnimationStats resolveAnimation(World &world) {
  entt::registry &registry = world.registry();
  AnimationStats stats = resolveAnimation(registry);
  for (auto [e, window] : registry.view<AnimatedWindow>().each()) {
    const float head = resolveValue(window.head);
    const float span = resolveValue(window.span);
    if (window.applied && head == window.appliedHead &&
        span == window.appliedSpan)
      continue;
    const uint32_t id = (uint32_t)e;
    // Exactly one of these three does anything; the other two are the
    // documented no-op on a surface of another kind.
    world.setSweepWindow(id, head, span);
    world.setFlockWindow(id, head, span);
    world.setPointsWindow(id, head, span);
    window.appliedHead = head;
    window.appliedSpan = span;
    window.applied = true;
    ++stats.windows;
  }
  return stats;
}

} // namespace sigil::world
