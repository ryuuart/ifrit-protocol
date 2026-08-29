#pragma once

/** @file
 * DECLARED MOTION for world entities — a second way to move things,
 * alongside (never instead of) the imperative setters on World.
 *
 * The imperative door computes a value per frame and calls a setter:
 * `setTransform`, `setSweepWindow`, a field on a MaterialComponent. It
 * is still the right tool for a one-off poke. What it cannot do is
 * DECLARE — say once that this prop's uv window rides that phase
 * through that curve, and then stop thinking about it. The `Animated*`
 * components here are that declaration, built on
 * `motion::Animatable<float>`.
 *
 * The shape of it:
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
 * Three properties to know before using it:
 *
 * 1. **Every lane is a float.** A position is three lanes, not an
 *    `Animatable<glm::vec3>`. `bind()`'s normalise → curve → affine
 *    chain is float-only, and that chain is most of what a lane is worth;
 *    a vector-typed slot could hold only a plain constant or a raw
 *    binding. `Animatable<float>` converts implicitly from float, so a
 *    three-lane position still reads like a position at the call site.
 *
 * 2. **World owns no clock.** There is no `world.tick()` and no
 *    `render(dt)`. The caller steps a `motion::Ticker` with whatever
 *    delta it chooses, and `render()` is a pure function of what the
 *    Outputs hold at that instant — which is what lets a headless render
 *    of frame N be byte-identical every run. A clock read inside
 *    `render()` would make every such frame a function of wall time.
 *
 * 3. **The `animate(...)` form lands on its SETTLED value.** These lanes
 *    accept `animate(to(v))` and `animate(from(a).to(b))` because they
 *    are the ordinary motion slot type, but ramp-on-change needs a
 *    CHANGE event and world has no describe-and-diff over components —
 *    they are mutated in place. So a transitioned value resolves
 *    straight to its target with no ramp. To actually ramp, put the ramp
 *    on the timeline and bind it.
 *
 * The caller owns the `choreograph::Output`s a lane points at. A bound
 * lane that outlives its Output dangles.
 *
 * WITH THE SCENE RECONCILER (Scene.h): `Scene::find(keyPath)` publishes
 * the entity behind a declared node, and that is the supported way to
 * attach an `Animated*` to a reconciled leaf. Two rules then apply. A
 * kept leaf's lane OUTRANKS the placement or material its node
 * re-declares, and the reconciler warns once per node when it sees that.
 * A leaf whose mesh pointer or material changes is remove-and-add: a new
 * entity, with your lanes destroyed alongside the old one, so call
 * find() again and re-attach. Camera lanes are the exception that
 * composes freely, because a camera is not a scene node.
 */

#include <sigilmotion/Animation.h>
#include <sigilshape/Curves.h>
#include <sigilshape/Pop.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <vector>

#include "sigilworld/Components.h"

namespace sigil::world {

/** The motion value vocabulary, re-exported so world call sites can
 *  spell it without a prefix. `sigil::motion` remains where these are
 *  defined. */
using motion::Animatable;
using motion::animate;
using motion::bind;
using motion::Bound;
using motion::from;
using motion::through;
using motion::to;
using motion::Transition;
using motion::Transitioned;
using motion::wiggle;
namespace ease = motion::ease;

/** A COMPLETE placement, composed on every resolve as
 *  `model = base * translate * yaw * pitch * roll * scale` — the same
 *  convention, in the same order, as `scene::Node::localMatrix()`, so a
 *  declared node and an animated entity agree about what "turned 30"
 *  means.
 *
 *  Unlike the material and light components below, the lanes here are
 *  NOT optional, because this component describes the whole transform
 *  rather than overriding part of one: an unmentioned lane genuinely
 *  means "no translation" or "unit scale". A prop that already has a
 *  static placement puts it in @ref base and animates around it, so the
 *  TRS lanes read in that prop's own parent frame.
 *
 *  This component OWNS its entity's TransformComponent. Do not also
 *  drive that component by hand — with `setTransform()` or otherwise —
 *  because the next resolve overwrites whatever you wrote. */
struct AnimatedTransform {
  /** The static placement the animated TRS rides on top of. */
  glm::mat4 base{1.0f};
  Animatable<float> x = 0.0f, y = 0.0f, z = 0.0f;
  Animatable<float> yawDeg = 0.0f, pitchDeg = 0.0f, rollDeg = 0.0f;
  Animatable<float> scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
};

/** PARTIAL overrides of a MaterialComponent's live scalars: only the
 *  lanes you engage are written, and everything else on the material is
 *  left exactly as authored. That is what the `optional` buys, and it is
 *  not decoration — with plain defaults, engaging `uvOffsetX` would also
 *  slam a pane authored at alpha 0.4 back to 1.
 *
 *  `opacity` writes `Material::baseColor.w`, so it also routes the
 *  prop between the opaque and the blended pass, live, exactly as a
 *  hand-written alpha does.
 *
 *  Colour is deliberately absent. Three independent linear-RGB float
 *  lanes are the wrong way to ramp a colour — a perceptual space gives
 *  usable intermediate colours and linear RGB does not — so a colour
 *  lane wants a colour type rather than four loose floats. */
struct AnimatedMaterial {
  std::optional<Animatable<float>> opacity;
  std::optional<Animatable<float>> emissiveStrength;
  std::optional<Animatable<float>> uvOffsetX, uvOffsetY;
  std::optional<Animatable<float>> uvScaleX, uvScaleY;
};

/** PARTIAL overrides of a LightComponent, under the same optional rule
 *  as AnimatedMaterial. `x/y/z` drive `LightComponent::position`, so
 *  they are a point light's lanes; a directional light can animate only
 *  `intensity` here, because its `direction` must stay a unit vector and
 *  three free float lanes cannot promise that. */
struct AnimatedLight {
  std::optional<Animatable<float>> intensity;
  std::optional<Animatable<float>> x, y, z;
};

/** A FLIGHT PATH for the eye: a `shape::Spline3` plus the ONE float lane
 *  that walks it. Independent float lanes can describe a POINT but not a
 *  TRAJECTORY; driving `eyeX/Y/Z` from a spline by hand would mean the
 *  caller computing three numbers per frame, which is the imperative
 *  door wearing the declarative one's clothes.
 *
 *  The float-only rule is not bent to do it. The lane is @ref t, the
 *  position ALONG the curve, so the whole normalise → curve → affine
 *  chain still applies — to the parameter rather than to the geometry.
 *  `bind(&phase).map(&choreograph::easeInOutQuad)` eases the flight in
 *  and out, `.target(0, 3)` flies three laps, `.window(...)` makes the
 *  flight a slice of some larger phase. The curve supplies the shape,
 *  the lane supplies the schedule.
 *
 *  Four rules:
 *
 *  - **PRECEDENCE: whatever the path drives, it drives outright.** It
 *    always drives the eye, so `eyeX/Y/Z` are IGNORED while a path is
 *    engaged — not blended and not offset, because a lane that
 *    half-contradicts a curve can only place the camera off it. It
 *    drives the target if and only if @ref lookAhead is non-zero, which
 *    is the caller's own way of saying "aim it for me"; `lookAhead = 0`
 *    means "I aim it myself" and leaves `targetX/Y/Z` and any authored
 *    target untouched.
 *  - **WRAP: a closed spline wraps, an open one clamps.** On a loop, `t`
 *    past 1 comes round to `t - 1` and negative `t` runs backwards,
 *    because 0 and 1 are the same point there and a hard stop mid-loop
 *    is never what "closed" meant. An open curve parks at its ends. The
 *    wrap applies to the look-ahead point too, so the aim reads ACROSS
 *    the seam instead of staring at the end of the loop while flying
 *    past it.
 *  - **ARC LENGTH is the default.** A camera move wants constant SPEED,
 *    and parameter-uniform motion on a Catmull-Rom loop sprints through
 *    tight knots and crawls through loose ones. Set @ref arcLength to
 *    false for parameter-uniform motion, which is what you want when the
 *    knots ARE the schedule.
 *  - **ROLL still composes.** `AnimatedCamera::rollDeg` resolves after
 *    the path, from the eye and target the path just produced, so a
 *    dutch tilt turns about the FLIGHT axis and follows the curve round.
 *
 *  A path with no control points is not engaged at all: the authored
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
   *  length at `samples + 1` uniform parameter steps, plus a copy of the
   *  spline it was built from.
   *
   *  Rebuilt when — and only when — that copy no longer matches the live
   *  spline. Comparing against the INPUT rather than against the table
   *  is deliberate: comparing tables would mean building one, which is
   *  the cost being avoided. There is no dirty flag, so editing
   *  `path.points` in place cannot leave a stale table behind. */
  std::vector<float> arcTable;
  std::vector<glm::vec3> tablePoints;
  shape::Spline3::Type tableType = shape::Spline3::Type::CatmullRom;
  bool tableClosed = false;
  int tableSamples = 0;
};

/** PARTIAL overrides of a `CameraComponent`'s placement and lens, under
 *  the same optional rule as AnimatedMaterial and for the same reason:
 *  the caller authors the camera and these lanes drive part of it.
 *
 *  A camera needs no special home here, because a camera is already a
 *  registry entity — `CameraComponent{camera, active}`, whose rule is
 *  that an ACTIVE one overrides `World::setCamera` while it exists. An
 *  animated camera is therefore just a camera entity whose fields a
 *  system writes; the resolve stays device-free, and the precedence
 *  question answers itself.
 *
 *  Two absences are deliberate:
 *
 *  - `up` gets NO lanes. It must stay a unit vector roughly out of the
 *    view axis, which three free floats cannot promise — the same
 *    refusal AnimatedLight makes for a directional light's `direction`.
 *    `rollDeg` is the safe single-float parameterisation, and the
 *    only way to declare a dutch tilt: it turns @ref rollReference
 *    right-handed about the eye→target axis, so the camera rolls
 *    clockwise seen from behind it and the scene tips counter-clockwise
 *    in frame. It is recomputed from the fixed reference on every
 *    resolve, so it neither drifts nor accumulates.
 *  - `zNear`/`zFar` get no lanes. A moving near plane buys nothing and
 *    spends depth precision, so these are scene-scale constants — set
 *    them on the component or through `setCamera()`.
 *
 *  @ref path is not a lane but a CURVE plus one: it flies the eye along
 *  a `shape::Spline3` and outranks `eyeX/Y/Z` while engaged (see
 *  @ref CameraPath).
 *
 *  Lanes resolve in order, so `rollDeg` sees the eye and target this
 *  frame's own lanes, or path, just produced. `active` is NOT consulted
 *  here: it gates the RENDERER's choice of camera, not this system, so
 *  toggling a camera on never replays a backlog of missed frames.
 *
 *  This component OWNS the CameraComponent fields it engages. Do not
 *  also write those by hand; the next resolve overwrites them. */
struct AnimatedCamera {
  std::optional<Animatable<float>> eyeX, eyeY, eyeZ;
  std::optional<Animatable<float>> targetX, targetY, targetZ;
  std::optional<Animatable<float>> fovYDeg;
  std::optional<Animatable<float>> rollDeg;
  /** The un-rolled up vector `rollDeg` turns about the view axis.
   *  Not a lane — the fixed reference that keeps roll idempotent. */
  glm::vec3 rollReference{0, 1, 0};
  /** A curve for the eye to fly, instead of `eyeX/Y/Z` — see
   *  @ref CameraPath for the precedence, wrap and arc-length rules. It
   *  sits here rather than in a component of its own so that the
   *  precedence rule can be stated in one place, beside the lanes it
   *  outranks. As a separate component the outcome would depend on which
   *  system ran first, which is invisible at the call site. */
  std::optional<CameraPath> path;
};

/** The GPU generator window — the `(head, span)` pair taken by
 *  `placeSweep` and `placeChain`.
 *
 *  This is the one lane in this header sitting in front of a GPU
 *  RE-COOK rather than in front of a live shader parameter, which makes
 *  it the one lane that could be a trap: writing unconditionally would
 *  mark the prop dirty every frame and re-dispatch the compute pass
 *  forever, even while the bound Output sits still. So this lane — and,
 *  for one rule everywhere, every other lane too — is CHANGE-DETECTED.
 *  A resolve writes only what actually moved and reports it in
 *  AnimationStats, so a constant lane costs exactly one re-cook, ever.
 *
 *  Resolution routes through all three public window setters, each of
 *  which is a no-op on a prop of the wrong kind. That lets one
 *  component cover sweeps and point chains without World having
 *  to publish which kind an entity is. */
struct AnimatedWindow {
  Animatable<float> head = 1.0f;
  Animatable<float> span = 1.0f;

  /** Written by resolveAnimation(); the last pair actually pushed. */
  float appliedHead = 0, appliedSpan = 0;
  bool applied = false;
};

/** Declared motion for a point chain's operator DIALS — the twist's
 *  amount, the noise's seed, a selector's centre, a mix factor: any
 *  numeric field `shape::popops::setField` addresses, each a float lane
 *  bound to (operator index, field name). The component holds its own
 *  copy of the chain (the prop's value lives inside World); a resolve
 *  writes every lane into that copy and, when any lane MOVED, pushes it
 *  through `World::setChain`, which is a parameter re-cook — buffers
 *  and bindings stay — for everything but a lookup-table or loop edit.
 *  Change-detected like AnimatedWindow: a still lane costs nothing per
 *  frame. The chain is also the door for edits that are not lanes —
 *  change a field on it by hand and it goes with the next moved lane.
 *
 *  Use AnimatedWindow for head and span: it costs a two-float parameter
 *  push rather than a whole-chain re-describe. A lane naming a field the
 *  operator lacks is ignored (setField says no; nothing is written). */
struct AnimatedChain {
  /** One driven field: which operator in the chain it belongs to,
   *  which of that operator's fields it writes, and the value doing
   *  the driving. The last applied value is kept so a lane that has
   *  not moved can be skipped. */
  struct Lane {
    size_t op = 0;      ///< index into `chain`
    std::string field;  ///< "amount", "center.x", "seed"...
    Animatable<float> value = 0.0f;
    float applied = 0;
    bool wasApplied = false;
  };
  shape::pop::Chain chain;
  std::vector<Lane> lanes;
};

/** What the last resolve actually MOVED. Zero across the board means the
 *  resolve wrote nothing, which is what a settled scene reports and what
 *  makes the change detection observable from outside. */
struct AnimationStats {
  int transforms = 0;
  int materials = 0;
  int lights = 0;
  int cameras = 0;
  int windows = 0;
  int chains = 0;
};

/** One lane's current number, reading the forms in the order the slot
 *  discriminates them. A transitioned value yields its settled target,
 *  with no ramp — see the third point at the top of this file. */
inline float resolveValue(const Animatable<float>& v) {
  if (const choreograph::Output<float>* bound = v.binding()) {
    if (const motion::BoundFloat* shape = v.boundMap())
      return shape->apply(bound->value());
    return bound->value();
  }
  if (const float* plain = v.plain()) return *plain;
  return v.transitioned()->value;
}

namespace detail {

/** The wrap rule: a closed curve comes round, an open one parks. */
inline float wrapPathParameter(float t, bool closed) {
  if (!closed) return std::clamp(t, 0.0f, 1.0f);
  const float wrapped = std::fmod(t, 1.0f);
  return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
}

/** Rebuild the cumulative-length table if — and only if — the spline it
 *  was built from has changed. Compared against the spline rather than
 *  guarded by a dirty flag: an equal spline has an equal table, so a
 *  caller editing `path.points` in place cannot end up flying a stale
 *  curve, and there is no flag anyone can forget to set. */
inline void refreshArcTable(CameraPath& p) {
  const int samples = std::max(p.samples, 2);
  if (p.tableSamples == samples && p.tableClosed == p.path.closed &&
      p.tableType == p.path.type && p.tablePoints == p.path.points)
    return;
  p.arcTable.assign((size_t)samples + 1, 0.0f);
  glm::vec3 prev = p.path.position(0.0f);
  for (int i = 1; i <= samples; ++i) {
    const glm::vec3 q = p.path.position((float)i / (float)samples);
    p.arcTable[(size_t)i] = p.arcTable[(size_t)i - 1] + glm::length(q - prev);
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
inline float parameterAtArcFraction(const CameraPath& p, float s) {
  if (p.arcTable.size() < 2 || p.tableSamples < 1) return s;
  const float total = p.arcTable.back();
  if (!(total > 0.0f)) return s;
  const float target = s * total;
  size_t hi = (size_t)std::distance(
      p.arcTable.begin(),
      std::upper_bound(p.arcTable.begin(), p.arcTable.end(), target));
  hi = std::clamp<size_t>(hi, 1, p.arcTable.size() - 1);
  const size_t lo = hi - 1;
  const float span = p.arcTable[hi] - p.arcTable[lo];
  const float local = span < 1e-9f ? 0.0f : (target - p.arcTable[lo]) / span;
  return ((float)lo + local) / (float)p.tableSamples;
}

/** Where a path puts the eye this frame, and the chord it aims along
 *  (zero when the path is not aiming — `lookAhead == 0`, or a curve with
 *  no extent at all). */
struct CameraPathSample {
  glm::vec3 eye{0, 0, 0};
  glm::vec3 aim{0, 0, 0};
};

inline CameraPathSample samplePath(CameraPath& p) {
  if (p.arcLength) refreshArcTable(p);
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

}  // namespace detail

/** THE SYSTEM, in its device-free half: resolve every animated
 *  component that writes nothing but registry state.
 *
 *  A free function over a bare registry, so this half needs no GPU and
 *  the animation semantics can be exercised on a machine with no Vulkan
 *  runtime at all. `World::render()` calls the overload below, so
 *  nothing has to remember to call either by hand. */
inline AnimationStats resolveAnimation(entt::registry& registry) {
  constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
  AnimationStats stats;

  for (auto [e, animated, transform] :
       registry.view<AnimatedTransform, TransformComponent>().each()) {
    glm::mat4 m =
        glm::translate(animated.base, glm::vec3{resolveValue(animated.x),
                                                resolveValue(animated.y),
                                                resolveValue(animated.z)});
    // Zero-angle and unit-scale steps are skipped rather than multiplied
    // through, so that an unengaged lane composes bit-identically to the
    // same placement built by scene::Node::localMatrix().
    if (const float yaw = resolveValue(animated.yawDeg); yaw != 0)
      m = glm::rotate(m, yaw * kDegToRad, glm::vec3{0, 1, 0});
    if (const float pitch = resolveValue(animated.pitchDeg); pitch != 0)
      m = glm::rotate(m, pitch * kDegToRad, glm::vec3{1, 0, 0});
    if (const float roll = resolveValue(animated.rollDeg); roll != 0)
      m = glm::rotate(m, roll * kDegToRad, glm::vec3{0, 0, 1});
    const glm::vec3 scale{resolveValue(animated.scaleX),
                          resolveValue(animated.scaleY),
                          resolveValue(animated.scaleZ)};
    if (scale != glm::vec3{1, 1, 1}) m = glm::scale(m, scale);
    if (!(transform.model == m)) {
      transform.model = m;
      ++stats.transforms;
    }
  }

  const auto put = [](float& dst, const std::optional<Animatable<float>>& lane,
                      bool& changed) {
    if (!lane) return;
    const float v = resolveValue(*lane);
    if (dst != v) {
      dst = v;
      changed = true;
    }
  };

  for (auto [e, animated, material] :
       registry.view<AnimatedMaterial, MaterialComponent>().each()) {
    bool changed = false;
    Material& m = material.material;
    put(m.baseColor.w, animated.opacity, changed);
    put(m.emissiveStrength, animated.emissiveStrength, changed);
    put(m.uvOffset.x, animated.uvOffsetX, changed);
    put(m.uvOffset.y, animated.uvOffsetY, changed);
    put(m.uvScale.x, animated.uvScaleX, changed);
    put(m.uvScale.y, animated.uvScaleY, changed);
    if (changed) ++stats.materials;
  }

  for (auto [e, animated, light] :
       registry.view<AnimatedLight, LightComponent>().each()) {
    bool changed = false;
    put(light.intensity, animated.intensity, changed);
    put(light.position.x, animated.x, changed);
    put(light.position.y, animated.y, changed);
    put(light.position.z, animated.z, changed);
    if (changed) ++stats.lights;
  }

  for (auto [e, animated, cameraComponent] :
       registry.view<AnimatedCamera, CameraComponent>().each()) {
    bool changed = false;
    shape::space::Camera& c = cameraComponent.camera;
    // PRECEDENCE: an engaged path drives the eye outright, and the
    // target too if and only if it was asked to aim (lookAhead != 0).
    // Whatever the path drives, the corresponding lanes do not.
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
    // Roll last, so it turns about the view axis the eye and target
    // lanes just produced. `up` is derived from the fixed rollReference
    // and never from its own previous value, so resolving twice at the
    // same angle lands in the same place rather than rolling twice.
    if (animated.rollDeg) {
      const glm::vec3 forward = c.target - c.eye;
      if (const float length = glm::length(forward); length > 0) {
        const float roll = resolveValue(*animated.rollDeg) * kDegToRad;
        const glm::vec3 up =
            glm::vec3(glm::rotate(glm::mat4(1.0f), roll, forward / length) *
                      glm::vec4(animated.rollReference, 0.0f));
        if (c.up != up) {
          c.up = up;
          changed = true;
        }
      }
    }
    if (changed) ++stats.cameras;
  }

  return stats;
}

/** THE SYSTEM, whole: the registry half above plus AnimatedWindow, which
 *  needs the World because a generator window is a device parameter
 *  rather than a component field.
 *
 *  `World::render()` calls this before anything else it does, so the
 *  cook and draw passes see this frame's values. Call it yourself when
 *  you want the resolved state WITHOUT a frame — before a `readChain()`
 *  query, say — or when you want the Stats. Idempotent: a second call
 *  with unmoved Outputs writes nothing and reports zeros. */
inline AnimationStats resolveAnimation(World& world) {
  entt::registry& registry = world.registry();
  AnimationStats stats = resolveAnimation(registry);
  for (auto [e, window] : registry.view<AnimatedWindow>().each()) {
    const float head = resolveValue(window.head);
    const float span = resolveValue(window.span);
    if (window.applied && head == window.appliedHead &&
        span == window.appliedSpan)
      continue;
    const uint32_t id = (uint32_t)e;
    // At most one of these two does anything; the other is the
    // documented no-op on a prop of another kind.
    world.setSweepWindow(id, head, span);
    world.setChainWindow(id, head, span);
    window.appliedHead = head;
    window.appliedSpan = span;
    window.applied = true;
    ++stats.windows;
  }
  for (auto [e, animated] : registry.view<AnimatedChain>().each()) {
    bool moved = false;
    for (AnimatedChain::Lane& lane : animated.lanes) {
      const float value = resolveValue(lane.value);
      if (lane.wasApplied && value == lane.applied) continue;
      if (lane.op < animated.chain.size() &&
          shape::popops::setField(animated.chain[lane.op], lane.field, value))
        moved = true;
      lane.applied = value;
      lane.wasApplied = true;
    }
    if (!moved) continue;
    world.setChain((uint32_t)e, animated.chain);
    ++stats.chains;
  }
  return stats;
}

}  // namespace sigil::world
