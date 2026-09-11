#include "HitmanVerlet.h"

auto HitmanVerlet::verlet(Body& b, SkPoint gravityStep) -> void {
  for (size_t i = 0; i < b.count(); ++i) {
    if (b.invm(i) <= 0.0f) {
      b.wasAt(i, b.at(i));
      continue;
    }
    const SkPoint prev = b.at(i);
    // VERIFIED 2: the paper's "2 -> 1.99" lowers ONLY the first
    // coefficient, which subtracts 1% of the POSITION. Both drop here.
    b.moveTo(i, b.at(i) + (b.at(i) - b.was(i)) * kDrag + gravityStep);
    b.wasAt(i, prev);
  }
}

auto HitmanVerlet::projectWorld(SkPoint p, float r, SkPoint* q) -> bool {
  SkPoint out = p;
  bool hit = false;
  if (out.fY < r) {
    out.fY = r;
    hit = true;
  }
  if (out.fX < r) {
    out.fX = r;
    hit = true;
  }
  if (out.fX > 1000.0f - r) {
    out.fX = 1000.0f - r;
    hit = true;
  }
  if (out.fY > 1000.0f - r) {
    out.fY = 1000.0f - r;
    hit = true;
  }
  // The bump, inflated by r: nearest point on the triangle.
  SkPoint onTri = nearestOnTriangle(out, kBumpA, kBumpB, kBumpC);
  SkPoint away = out - onTri;
  const float dist = len(away);
  const bool inside = pointInTriangle(out, kBumpA, kBumpB, kBumpC);
  if (inside || dist < r) {
    SkPoint n = dist > 1e-4f ? away * (1.0f / dist) : SkPoint{0, 1};
    if (inside) n = n * -1.0f;
    out = onTri + n * r;
    hit = true;
  }
  *q = out;
  return hit;
}

auto HitmanVerlet::nearestOnSegment(SkPoint p, SkPoint a, SkPoint b)
    -> SkPoint {
  const SkPoint ab = b - a;
  const float dd = dot(ab, ab);
  if (dd < 1e-6f) return a;
  const float t = std::clamp(dot(p - a, ab) / dd, 0.0f, 1.0f);
  return a + ab * t;
}

auto HitmanVerlet::nearestOnTriangle(SkPoint p, SkPoint a, SkPoint b, SkPoint c)
    -> SkPoint {
  const SkPoint p0 = nearestOnSegment(p, a, b);
  const SkPoint p1 = nearestOnSegment(p, b, c);
  const SkPoint p2 = nearestOnSegment(p, c, a);
  const float d0 = len(p - p0), d1 = len(p - p1), d2 = len(p - p2);
  if (d0 <= d1 && d0 <= d2) return p0;
  return d1 <= d2 ? p1 : p2;
}

auto HitmanVerlet::pointInTriangle(SkPoint p, SkPoint a, SkPoint b, SkPoint c)
    -> bool {
  const float s1 = cross2(b - a, p - a), s2 = cross2(c - b, p - b),
              s3 = cross2(a - c, p - c);
  const bool neg = s1 < 0 || s2 < 0 || s3 < 0;
  const bool pos = s1 > 0 || s2 > 0 || s3 > 0;
  return !(neg && pos);
}

auto HitmanVerlet::collideWorld(Body& b, bool record) -> void {
  for (size_t i = 0; i < b.count(); ++i) {
    if (b.invm(i) <= 0.0f) continue;
    SkPoint q;
    if (!projectWorld(b.at(i), b.radius, &q)) continue;
    const SkPoint delta = q - b.at(i);
    const float dp = len(delta);
    if (dp < 1e-5f) continue;
    const SkPoint n = delta * (1.0f / dp);
    b.moveTo(i, q);  // restitution ZERO: clamp, never reflect
    // §7 friction: reduce the TANGENTIAL velocity by k*dp, by moving x*.
    const SkPoint v = b.at(i) - b.was(i);
    const SkPoint vt = v - n * dot(v, n);
    const float m = len(vt);
    if (m > 1e-6f) {
      const float reduce = std::min(m, kFriction * dp);
      b.wasAt(i, b.was(i) + vt * (reduce / m));  // never reverses: clamped
    }
    if (record && contacts.size() < 40) contacts.push_back({q, n});
  }
}

auto HitmanVerlet::collideSticks(Body& b, bool record) -> void {
  for (const physics::Constraint& s : b.sticks) {
    const float c1 = 0.5f, c2 = 0.5f;
    const SkPoint p = b.at(s.a) * c1 + b.at(s.b) * c2;
    SkPoint q;
    if (!projectWorld(p, b.radius, &q)) continue;
    const SkPoint D = q - p;
    const float dd = dot(D, D);
    if (dd < 1e-8f) continue;
    const float lambda = dot(q - p, D) / ((c1 * c1 + c2 * c2) * dd);
    const float w1 = b.invm(s.a), w2 = b.invm(s.b);
    if (w1 > 0) b.moveTo(s.a, b.at(s.a) + D * (c1 * lambda));
    if (w2 > 0) b.moveTo(s.b, b.at(s.b) + D * (c2 * lambda));
    if (record && contacts.size() < 40)
      contacts.push_back({q, D * (1.0f / std::sqrt(dd))});
  }
}

auto HitmanVerlet::satisfy(Body& b, bool record) -> void {
  for (int it = 0; it < b.iterations; ++it) {
    const bool rec = record && it == b.iterations - 1;
    if (b.worldCollide) {
      collideWorld(b, rec);
      if (b.stickContact) collideSticks(b, rec);
    }
    for (const physics::Constraint& s : b.sticks) s.project(b.pts);
    for (const physics::Constraint& q : b.ineqs) q.project(b.pts);
    // §7 IK: keep setting the position INSIDE the loop.
    if (&b == &rig && dragging) b.moveTo(LHA, dragTarget);
  }
}

auto HitmanVerlet::applyBlast(Body& b, SkPoint c) -> void {
  for (size_t i = 0; i < b.count(); ++i) {
    if (b.invm(i) <= 0.0f) continue;
    SkPoint d = b.at(i) - c;
    const float r = std::max(12.0f, len(d));
    // |dx| = K / r^2  ->  dx = K (x-c) / r^3
    SkPoint push = d * (kBlastK / (r * r * r));
    const float m = len(push);
    if (m > 45.0f) push = push * (45.0f / m);
    // a POSITION displacement; verlet does the rest
    b.moveTo(i, b.at(i) + push);
  }
}

auto HitmanVerlet::maxError(const Body& b) const -> float {
  float e = 0;
  for (const physics::Constraint& s : b.sticks)
    e = std::max(e, std::abs(len(b.at(s.b) - b.at(s.a)) - s.rest) / s.rest);
  return e;
}

auto HitmanVerlet::chainStats(const Body& b, float* mean, float* mx) const
    -> void {
  float sum = 0, m = 0;
  for (const physics::Constraint& s : b.sticks) {
    const float e = std::abs(len(b.at(s.b) - b.at(s.a)) - s.rest) / s.rest;
    sum += e;
    m = std::max(m, e);
  }
  *mean = b.sticks.empty() ? 0 : sum / (float)b.sticks.size();
  *mx = m;
}

auto HitmanVerlet::stepPhysics() -> void {
  loopT += 1.0 / kSimHz;
  if (loopT >= kLoop) {
    loopT -= kLoop;
    buildRig({300.0f, kCapsule});
    didHit = didBomb = false;
    dragging = false;
  }
  contacts.clear();

  // Phase machine: the §7 motion-control events, on this study's schedule.
  if (!didHit && loopT >= 1.10) {
    // documented: displace ONE particle
    rig.moveTo(RSH, rig.at(RSH) - SkPoint{kHitPush, 0});
    didHit = true;
  }
  if (!didBomb && loopT >= 2.60) {
    const SkPoint c = kBlast;
    applyBlast(rig, c);
    applyBlast(cloth, c);
    for (Body& p : plants) applyBlast(p, c);
    didBomb = true;
    blastPhase = 1.0f;
  }
  const bool wantDrag = loopT >= 6.20 && loopT < 9.40;
  if (wantDrag && !dragging) {
    dragFrom = rig.at(LHA);
    dragging = true;
  }
  if (!wantDrag) dragging = false;
  if (dragging) {
    const float u = std::clamp((float)((loopT - 6.20) / 3.20), 0.0f, 1.0f);
    const float e = ch::easeInOutCubic(u);
    const SkPoint to{120.0f, 30.0f};
    dragTarget = dragFrom + (to - dragFrom) * e;
  }
  phase = loopT < 1.10   ? 0
          : loopT < 2.60 ? 1
          : loopT < 3.10 ? 2
          : loopT < 6.20 ? 3
          : loopT < 9.40 ? 4
                         : 3;

  const SkPoint g{0.0f, -kGravityStep};
  verlet(rig, g);
  satisfy(rig, true);
  verlet(cloth, g);
  satisfy(cloth, false);
  for (Body& p : plants) {
    verlet(p, g);
    satisfy(p, false);
  }
  // The A/B: same gravity, same drag, same initial condition, three
  // iteration counts. Drive the pins together so they never settle.
  chainPhase += (float)(1.0 / kSimHz);
  const float px = 26.0f * std::sin(chainPhase * 6.2831853f / 1.8f);
  const SkPoint cg{0.0f, kChainG};
  const float xs[3] = {60.0f, 162.0f, 264.0f};
  for (int k = 0; k < 3; ++k) {
    chains[(size_t)k].moveTo(0, {xs[k] + px, 16.0f});
    verlet(chains[(size_t)k], cg);
    satisfy(chains[(size_t)k], false);
    float mn = 0, mx = 0;
    chainStats(chains[(size_t)k], &mn, &mx);
    // Half-second exponential mean: the claim is about the solver, not
    // about which frame the capture happens to land on.
    chainMean[(size_t)k] = chainMean[(size_t)k] * 0.967f + mn * 0.033f;
    chainMax[(size_t)k] = std::max(chainMax[(size_t)k] * 0.967f, mx);
  }

  stageMaxErr = maxError(rig);
  contactCount = contacts.size();
  ++simSteps;
  blastPhase =
      std::max(0.0f, blastPhase.value() - (float)(1.0 / kSimHz) / 0.34f);
  const float fadeU = (float)((loopT - 10.40) / 0.60);
  bodyFade = loopT >= 10.40 ? std::clamp(1.0f - fadeU, 0.0f, 1.0f) : 1.0f;
}
