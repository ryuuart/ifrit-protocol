#include "HitmanVerlet.h"

auto HitmanVerlet::buildRig(SkPoint feet) -> void {
  rig = Body{};
  rig.iterations = 4;  // documented range 1-10; 3-4 for rigid bodies
  rig.worldCollide = true;
  rig.stickContact = true;
  rig.radius = kCapsule;
  auto P = [&](Norm n, float side) { return world(n, side, feet); };
  // x* = x at spawn, which `add` does: zero velocity.
  for (SkPoint p :
       {P(kHead, 0), P(kNeck, 0), P(kSh, -1), P(kSh, +1), P(kEl, -1),
        P(kEl, +1), P(kHa, -1), P(kHa, +1), P(kWa, -1), P(kWa, +1), P(kHi, -1),
        P(kHi, +1), P(kKn, -1), P(kKn, +1), P(kFo, -1), P(kFo, +1)})
    rig.add(p);
  // The 24 sticks, in the paper's five groups (re-counted at 600 dpi).
  addStick(rig, HEAD, NECK);  // 1
  addStick(rig, NECK, LSH);
  addStick(rig, NECK, RSH);  // 2
  addStick(rig, LSH, RSH);   // 1  shoulder bar
  addStick(rig, NECK, LWA);
  addStick(rig, NECK, RWA);  // 2  long braces
  addStick(rig, LSH, LWA);
  addStick(rig, RSH, RWA);  // 2  side
  addStick(rig, LSH, RWA);
  addStick(rig, RSH, LWA);  // 2  crossed
  addStick(rig, LWA, RWA);  // 1  waist bar
  addStick(rig, LWA, LHI);
  addStick(rig, RWA, RHI);  // 2  side
  addStick(rig, LWA, RHI);
  addStick(rig, RWA, LHI);  // 2  crossed
  addStick(rig, LHI, RHI);  // 1  hip bar
  addStick(rig, LSH, LEL);
  addStick(rig, RSH, REL);  // 2
  addStick(rig, LEL, LHA);
  addStick(rig, REL, RHA);  // 2
  addStick(rig, LHI, LKN);
  addStick(rig, RHI, RKN);  // 2  THE ANCHOR
  addStick(rig, LKN, LFO);
  addStick(rig, RKN, RFO);  // 2
  // The documented inequality: "between the two knees - making sure that
  // the legs never cross".
  // A band with no upper end: enforced only when the knees are closer
  // than the minimum, which is what an inequality constraint says.
  physics::Constraint knees = physics::range(
      LKN, RKN, kKneeMin, std::numeric_limits<float>::infinity());
  knees.approximate = true;
  rig.ineqs.push_back(knees);
}

auto HitmanVerlet::buildCloth() -> void {
  cloth = Body{};
  cloth.iterations = 1;  // documented
  // MEASURED, on ONE pin at ONE iteration (both documented): a 7x7 sheet
  // of 26-unit cells hangs 255 units on a 221-unit diagonal - 15% over -
  // with 61% peak constraint error. A 5x5 of 32-unit cells hangs 198 on
  // 181, 9% over, 28% peak. The load per stick is what one iteration
  // cannot carry, so the patch is sized to the solver, not the reverse.
  constexpr int C = 5, R = 5;
  constexpr float sp = 32.0f;
  const float x0 = 776.0f, y0 = 706.0f;
  cloth.worldCollide = true;
  cloth.radius = 3.0f;
  auto idx = [&](int c, int r) { return r * C + c; };
  for (int r = 0; r < R; ++r)
    for (int c = 0; c < C; ++c)
      cloth.add({x0 + (float)c * sp + (((unsigned)r & 1u) ? sp * 0.5f : 0.0f),
                 y0 - (float)r * sp},
                r == 0 && c == 0);  // "Constrain one particle of the cloth to
                                    // origo"
  for (int r = 0; r < R; ++r) {
    for (int c = 0; c < C; ++c) {
      if (c + 1 < C) addStick(cloth, idx(c, r), idx(c + 1, r));
      if (r + 1 < R) {
        addStick(cloth, idx(c, r), idx(c, r + 1));
        const int dc = ((unsigned)r & 1u) ? c + 1 : c - 1;
        if (dc >= 0 && dc < C) addStick(cloth, idx(c, r), idx(dc, r + 1));
      }
    }
  }
}

auto HitmanVerlet::buildPlants() -> void {
  const float roots[4] = {664.0f, 736.0f, 808.0f, 880.0f};
  for (int p = 0; p < 4; ++p) {
    Body& b = plants[(size_t)p];
    b = Body{};
    b.iterations = 1;  // documented — "exactly the right amount of bending"
    b.worldCollide = true;
    b.radius = 3.0f;
    // A patch of cloth three wide and five tall, fully braced, base row
    // pinned. Slenderness is the whole design question here, because ONE
    // relaxation iteration has a slenderness limit: at this study's
    // gravity a 3-wide braced truss of 24-unit cells stands at five rows
    // (near its nominal height, and holding) and FOLDS FLAT at six. A
    // 2-wide, 7-tall strip is a wet noodle from the first second. That is
    // a fact about aspect ratio and iteration count, not about the paper.
    constexpr int R = 5, C = 3;
    constexpr float sp = 24.0f, w = 28.0f;
    const float lean = (p % 2 == 0) ? 2.2f : -2.8f;
    auto id = [](int r, int c) { return r * C + c; };
    for (int r = 0; r < R; ++r)
      for (int c = 0; c < C; ++c)
        b.add({roots[p] + lean * (float)r + (float)c * w, (float)r * sp},
              r == 0);  // the base row is the root
    for (int r = 0; r < R; ++r)
      for (int c = 0; c < C; ++c) {
        if (c + 1 < C) addStick(b, id(r, c), id(r, c + 1));
        if (r + 1 < R) {
          addStick(b, id(r, c), id(r + 1, c));
          // the documented support sticks, "between strategically chosen
          // couples of vertices sharing a neighbor" — both diagonals, so
          // every cell is a braced truss and the plant stands up
          if (c + 1 < C) addStick(b, id(r, c), id(r + 1, c + 1));
          if (c > 0) addStick(b, id(r, c), id(r + 1, c - 1));
        }
      }
  }
}

auto HitmanVerlet::buildChains() -> void {
  const int iters[3] = {1, 4, 10};
  const float xs[3] = {60.0f, 162.0f, 264.0f};
  for (int k = 0; k < 3; ++k) {
    Body& b = chains[(size_t)k];
    b = Body{};
    b.iterations = iters[k];
    constexpr int N = 12;
    for (int i = 0; i < N; ++i)
      b.add({xs[k], 16.0f + (float)i * kChainRest}, i == 0);
    // ORDER MATTERS, and it is the whole reason the paper talks about
    // iteration counts at all: listed FROM THE PIN a chain converges in a
    // single Gauss-Seidel sweep and 1, 4 and 10 are indistinguishable.
    // A real constraint list has no reason to be dependency-sorted -
    // the paper's own cloth listing is row-major over a 2D mesh, which
    // is not one - so these are listed from the FREE END.
    for (int i = N - 2; i >= 0; --i)
      b.sticks.push_back(physics::stick((size_t)i, (size_t)i + 1, kChainRest));
  }
}

auto HitmanVerlet::rigBounds() const -> SkRect {
  SkRect r = SkRect::MakeLTRB(1e9f, 1e9f, -1e9f, -1e9f);
  for (size_t i = 0; i < rig.count(); ++i) {
    const SkPoint p = drawn(rig, i);
    r.fLeft = std::min(r.fLeft, p.fX);
    r.fTop = std::min(r.fTop, p.fY);
    r.fRight = std::max(r.fRight, p.fX);
    r.fBottom = std::max(r.fBottom, p.fY);
  }
  return r.makeOutset(12, 12);
}

auto HitmanVerlet::rigFit(float w, float h, float* scale, SkPoint* offset) const
    -> void {
  const SkRect b = rigBounds();
  const float s =
      std::min(w / std::max(1.0f, b.width()), h / std::max(1.0f, b.height()));
  *scale = s;
  *offset = {(w - b.width() * s) * 0.5f - b.fLeft * s,
             (h - b.height() * s) * 0.5f - b.fTop * s};
}

auto HitmanVerlet::paintRig(Pen& pen, float scale, SkPoint offset, float fade,
                            bool proxies) const -> void {
  auto at = [&](size_t i) {
    const SkPoint p = drawn(rig, i);
    return SkPoint{offset.fX + p.fX * scale, offset.fY + p.fY * scale};
  };
  pen.noFill();
  pen.strokeCap(draw::ROUND);
  if (proxies) {
    // 1. The capped-cylinder proxies — the collision geometry the paper
    //    describes, at the width the solver actually uses.
    pen.strokeWeight(2.0f * kCapsule * kUnit * scale);
    pen.stroke(hexColor(0x6FA8DC, 0.10f * fade));
    for (const physics::Constraint& s : rig.sticks) {
      const SkPoint a = at(s.a), b = at(s.b);
      pen.line(a.fX, a.fY, b.fX, b.fY);
    }
  }
  // 2. The centrelines, coloured by LIVE constraint error.
  pen.strokeWeight(std::max(4.6f, 2.5f * scale));
  for (const physics::Constraint& s : rig.sticks) {
    const float e = std::abs(len(rig.at(s.b) - rig.at(s.a)) - s.rest) / s.rest;
    pen.stroke(errColor(e, fade));
    const SkPoint a = at(s.a), b = at(s.b);
    pen.line(a.fX, a.fY, b.fX, b.fY);
  }
  // 3. The inequality constraint — dotted, as Figure 8 draws it.
  pen.strokeWeight(1.4f * scale);
  pen.stroke(hexColor(0xC8402F, 0.75f * fade));
  pen.strokeDash({2.5f, 3.5f});
  const SkPoint lk = at(LKN), rk = at(RKN);
  pen.line(lk.fX, lk.fY, rk.fX, rk.fY);
  pen.noDash();
  pen.noStroke();
}

auto HitmanVerlet::writeDotPool() -> void {
  dotPool->clear();
  auto push = [&](const Body& b, int pinFrame) {
    for (size_t i = 0; i < b.count(); ++i)
      dotPool->add(drawn(b, i), b.invm(i) <= 0.0f ? pinFrame : cellDot);
  };
  push(rig, cellPin);
  push(cloth, cellPin);
  for (const Body& p : plants) push(p, cellPin);
  // Fade the whole field with the body.
  const float f = bodyFade.value();
  if (f < 1.0f) {
    auto tints = dotPool->tints();
    for (SkColor4f& t : tints) t.fA = f;
  }
}

auto HitmanVerlet::writeBarPool(SkPoint origin, float scale) -> void {
  barPool->resize(rig.sticks.size());
  auto pos = barPool->positions();
  auto rot = barPool->rotations();
  auto tint = barPool->tints();
  auto frame = barPool->frames();
  auto size = barPool->sizes();
  const float f = bodyFade.value();
  for (size_t i = 0; i < rig.sticks.size(); ++i) {
    const physics::Constraint& s = rig.sticks[i];
    SkPoint a = drawn(rig, s.a), b = drawn(rig, s.b);
    a = {origin.fX + a.fX * scale, origin.fY + a.fY * scale};
    b = {origin.fX + b.fX * scale, origin.fY + b.fY * scale};
    const SkPoint d = b - a;
    const float L = std::max(0.5f, len(d));
    pos[i] = a + d * 0.5f;
    rot[i] = std::atan2(d.fY, d.fX);
    frame[i] = cellBar;
    // cell is 32 x 8 logical, so the x multiplier is L/32 while y is
    // fixed: ONE cell length serving 24 different stick lengths remaps the
    // cell's aspect by itself.
    size[i] = {L / 32.0f, 4.6f / 8.0f};
    const float e = std::abs(len(rig.at(s.b) - rig.at(s.a)) - s.rest) / s.rest;
    tint[i] = errColor(e, f);
  }
  barPool->commit();
}
