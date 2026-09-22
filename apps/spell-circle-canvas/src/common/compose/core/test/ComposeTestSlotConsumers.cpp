// THE FOUR CONSUMERS OF THE SLOT TABLE, ONE LANE AT A TIME.
//
// ComposeTestFieldPins pins the table's SHAPE: one row per slot, each row
// aiming at its own field of the computed style, each standing at that
// field's default. None of that says the four functions that READ the table
// move the property. A row aimed perfectly and dropped from
// `applyTransitions` ramps nothing, and the four absences SlotSpecs.h names
// are each silent — no error, and a still frame of the affected node looks
// exactly right.
//
// So: one scene per slot, spelling that lane as the only thing in the tree
// that moves, under an ancestor that would hold a bake across it. Three of
// the four consumers answer in ONE PAIR OF FRAMES, because all three
// failures freeze the same pixels —
//
//   `applyMountTransitions`  the entrance never plays, so the node stands
//                            at its settled value from the first frame
//   `computeVolatile`        the property is not volatility, so the
//                            ancestor caches across it and replays
//   `collectGroupScalars`    the group holds its bake while the lane moves
//
// — and the fourth, `applyTransitions`, is a second pair: a re-described
// endpoint on a node already mounted must EASE to its new value rather than
// arrive at it.
//
// Every slot is accounted for. A row is either driven by a scene here or
// carries a written reason, and a slot that is neither fails the last case
// in this file rather than passing unnoticed.
//
// This file reaches the library's own source directory, as the field walk
// does and for the same reason: the accounting is against `kSlotSpecs`
// itself, so a slot appended to the enum is a case that fails here.

#include <vector>

#include "../ComposeRuntime.h"
#include "support/CoreTestSupport.h"

namespace cd = sigil::compose::detail;

namespace {

using Lane = sigil::motion::Animatable<float>;

/** The thing whose pixels move: one red square, big enough that any of the
 *  lanes below shifts a great many of them. */
Element square() { return box().width(40).height(40).fill(red()); }

/** The viewer a depth lane needs to project through. */
Element viewed(Element plane, Lane distance = 400.0f) {
  Element view = box();
  view.perspective(std::move(distance));
  view.children({std::move(plane)});
  return view;
}

/** ONE SCENE DRIVING ONE SLOT. The lane handed to `build` is the only
 *  animatable anywhere in what it returns; `rest` and `moved` are the two
 *  ends the scene is asked to travel between, chosen so the pixels between
 *  them are nowhere near equal. */
struct SlotScene {
  cd::Instance::Slot slot;
  const char* name;
  float rest;
  float moved;
  Element (*build)(Lane lane);
};

const SlotScene kSlotScenes[] = {
    {cd::Instance::kOpacity, "opacity", 1.0f, 0.05f,
     [](Lane lane) {
       Element e = square();
       e.opacity(std::move(lane));
       return e;
     }},
    {cd::Instance::kTx, "translateX", 0.0f, 90.0f,
     [](Lane lane) {
       Element e = square();
       e.translateX(std::move(lane));
       return e;
     }},
    {cd::Instance::kTy, "translateY", 0.0f, 90.0f,
     [](Lane lane) {
       Element e = square();
       e.translateY(std::move(lane));
       return e;
     }},
    {cd::Instance::kRotate, "rotate", 0.0f, 45.0f,
     [](Lane lane) {
       Element e = square();
       e.rotate(std::move(lane));
       return e;
     }},
    {cd::Instance::kScale, "scale", 1.0f, 3.0f,
     [](Lane lane) {
       Element e = square();
       e.scale(std::move(lane));
       return e;
     }},
    {cd::Instance::kSkewX, "skewX", 0.0f, 35.0f,
     [](Lane lane) {
       Element e = square();
       e.skewX(std::move(lane));
       return e;
     }},
    {cd::Instance::kSkewY, "skewY", 0.0f, 35.0f,
     [](Lane lane) {
       Element e = square();
       e.skewY(std::move(lane));
       return e;
     }},
    {cd::Instance::kScaleX, "scaleX", 1.0f, 3.0f,
     [](Lane lane) {
       Element e = square();
       e.scaleX(std::move(lane));
       return e;
     }},
    {cd::Instance::kScaleY, "scaleY", 1.0f, 3.0f,
     [](Lane lane) {
       Element e = square();
       e.scaleY(std::move(lane));
       return e;
     }},
    // travel(): the square rides a diagonal resolved against its parent's
    // box, so `t` carries it clear across the canvas.
    {cd::Instance::kMotionT, "travel t", 0.0f, 1.0f,
     [](Lane lane) {
       Element e = box();
       e.absolute();
       e.rect(SkRect::MakeXYWH(0, 0, 24, 24));
       e.fill(red());
       e.travel({.path =
                     [](SkSize) {
                       SkPathBuilder b;
                       b.moveTo(20, 20);
                       b.lineTo(170, 170);
                       return b.detach();
                     },
                 .t = std::move(lane)});
       return e;
     }},
    // The depth lanes, each seen through a viewer: without one the
    // projection is orthographic and three of the four move nothing.
    {cd::Instance::kRotateX, "rotateX", 0.0f, 70.0f,
     [](Lane lane) {
       Element plane = square();
       plane.rotateX(std::move(lane));
       return viewed(std::move(plane));
     }},
    {cd::Instance::kRotateY, "rotateY", 0.0f, 70.0f,
     [](Lane lane) {
       Element plane = square();
       plane.rotateY(std::move(lane));
       return viewed(std::move(plane));
     }},
    {cd::Instance::kTranslateZ, "translateZ", 0.0f, -300.0f,
     [](Lane lane) {
       Element plane = square();
       plane.translateZ(std::move(lane));
       return viewed(std::move(plane));
     }},
    // scaleZ scales the space about the pivot, so a FLAT plane needs a
    // pivot standing off the page for the lane to reach it: at sz the
    // plane sits at z = pivotZ·(1 − sz), which the viewer then projects.
    {cd::Instance::kScaleZ, "scaleZ", 1.0f, 4.0f,
     [](Lane lane) {
       Element plane = square();
       plane.transformOrigin(pct(50), pct(50), Dimension(80.0f));
       plane.scaleZ(std::move(lane));
       return viewed(std::move(plane));
     }},
    // perspective(): the one lane whose motion lands on ANOTHER node's
    // device rect, so what moves here is the turned plane below it.
    {cd::Instance::kPerspective, "perspective", 250.0f, 4000.0f,
     [](Lane lane) {
       Element plane = square();
       plane.rotateY(55.0f);
       return viewed(std::move(plane), std::move(lane));
     }},
};

/** The scene under an ancestor that WOULD hold a bake across everything
 *  below it. A lane missing from the volatility walk or from the group's
 *  scalars freezes here and nowhere else. */
Element grouped(Element inner) {
  Element group = box();
  group.cache(Cache::Group);
  group.children({std::move(inner)});
  return box().children({std::move(group)});
}

/** Every pixel of the host's surface — the comparison a frozen frame
 *  fails. */
std::vector<uint32_t> pixels(Host& host, int w = 200, int h = 200) {
  std::vector<uint32_t> out((size_t)w * (size_t)h);
  host.surface->readPixels(SkImageInfo::MakeN32Premul(w, h), out.data(),
                           (size_t)w * 4, 0, 0);
  return out;
}

const sigil::motion::Transition kSecondFlat{1000ms, &choreograph::easeNone};

}  // namespace

TEST(ComposeSlotConsumers, EveryLanePlaysItsEntranceThroughACachingAncestor) {
  for (const SlotScene& scene : kSlotScenes) {
    Host host(200, 200);
    host.composer.render(grouped(scene.build(animate(
        sigil::motion::from(scene.rest).to(scene.moved), kSecondFlat))));
    host.frame();  // the entrance stands at `rest`
    const std::vector<uint32_t> entering = pixels(host);
    host.frame(1.0);  // …and has arrived at `moved`
    const std::vector<uint32_t> arrived = pixels(host);
    EXPECT_NE(entering, arrived)
        << scene.name
        << ": nothing moved between the entrance and its destination. "
           "Either the lane is absent from applyMountTransitions and the "
           "node simply appeared settled, or it is absent from "
           "computeVolatile / collectGroupScalars and the group above it "
           "replayed one bake across the whole motion.";
  }
}

TEST(ComposeSlotConsumers, EveryLaneEasesToARedescribedEndpoint) {
  for (const SlotScene& scene : kSlotScenes) {
    Host host(200, 200);
    host.composer.render(grouped(scene.build(Lane{scene.rest})));
    host.frame();
    host.frame(1.0);  // settled at `rest`, with no motion running
    const std::vector<uint32_t> atRest = pixels(host);

    host.composer.render(grouped(
        scene.build(animate(sigil::motion::to(scene.moved), kSecondFlat))));
    host.frame(0.02);  // a fiftieth of the way: still beside `rest`
    const std::vector<uint32_t> justAfter = pixels(host);
    host.frame(2.0);  // settled at `moved`
    const std::vector<uint32_t> atMoved = pixels(host);

    EXPECT_NE(atRest, atMoved) << scene.name
                               << ": the two ends of this scene draw the "
                                  "same pixels, so the case proves nothing";
    EXPECT_NE(justAfter, atMoved)
        << scene.name
        << ": the property was already at its new value one frame after the "
           "describe. It SNAPPED — the lane is absent from applyTransitions, "
           "which looks exactly like a missing transition spec.";
  }
}

TEST(ComposeSlotConsumers, EverySlotIsDrivenHereOrCarriesAWrittenReason) {
  // Why a slot carries no scene above. A slot appended to the enum has
  // neither a scene nor a line here, and fails this case.
  const char* excused[cd::Instance::kSlots] = {};
  excused[cd::Instance::kFillLerp] =
      "a progress the composer synthesizes for a colour-to-colour fill: "
      "there is no float lane in a description to hand a scene, and the "
      "ramp it drives is what the fill transition cases watch";
  excused[cd::Instance::kInkLerp] =
      "the same shape over a declared ink, whose one reader is the cascade "
      "pass; ComposeTestCascade watches the colour it produces";
  excused[cd::Instance::kTextPathAt] =
      "textOnPath() is compiled into the typography target and this binary "
      "links the kernel alone, so the lane cannot be SPELLED here; the "
      "field walk reaches its field directly instead";

  bool driven[cd::Instance::kSlots] = {};
  for (const SlotScene& scene : kSlotScenes) driven[(int)scene.slot] = true;
  for (int i = 0; i < (int)cd::Instance::kSlots; ++i)
    EXPECT_TRUE(driven[i] || excused[i] != nullptr)
        << "slot " << i
        << " is neither driven by a scene in this file nor excused with a "
           "reason. Add a scene that spells the lane, or say here why no "
           "scene in this binary can — an unaccounted slot is exactly the "
           "silent absence the table exists to prevent.";
}
