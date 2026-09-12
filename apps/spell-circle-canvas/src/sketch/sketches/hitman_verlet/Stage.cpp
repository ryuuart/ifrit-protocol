#include "HitmanVerlet.h"

auto HitmanVerlet::stageChrome(Pen& pen, double ms) -> void {
  const float a = cue(ms, 520, 400);
  // The floor: a band from world y = 0 down out of frame is the bottom
  // edge of the cube, so the solid reads as a plinth under it plus the
  // surface line the paper's projection actually clamps to. The grain is
  // a LAYER OF THE PAINT — Paint::blend soft-lights it onto the solid in
  // one fill, rather than costing a second pass over the same band.
  const float floorTop = kStage - kCapsule * kUnit;
  const Paint floor =
      Paint::blend({{Paint::solid(fadeTo(kSolid, a)), SkBlendMode::kSrc},
                    {Paint::recipe(field::grain(0.035f, 3, 11.0f, 0.5f)),
                     SkBlendMode::kSoftLight}});
  pen.noStroke();
  pen.fill(floor);
  pen.rect(0, floorTop, kStage, kStage - floorTop);
  pen.fill(hexColor(0x6FA8DC, 0.5f * a));
  pen.rect(0, floorTop, kStage, 1);

  // The bump, drawn to the paper's own Fig. 4 scale, with a drafting
  // section hatched inside it — a generator, not a texture.
  const SkPoint a1 = toStage(kBumpA), b1 = toStage(kBumpB),
                c1 = toStage(kBumpC);
  pen.fill(fadeTo(kSolid, a));
  pen.stroke(hexColor(0x6FA8DC, 0.40f * a));
  pen.strokeWeight(1.0f);
  pen.triangle(a1.fX, a1.fY, b1.fX, b1.fY, c1.fX, c1.fY);
  {
    // The hatch: 45° lines inside the section, masked by the section
    // itself — the same three points, drawn once as the shape and once
    // as the clip, so the hatch cannot outrun the bump it belongs to.
    pen.push();
    pen.clip([&] { pen.triangle(a1.fX, a1.fY, b1.fX, b1.fY, c1.fX, c1.fY); });
    pen.stroke(hexColor(0x6FA8DC, 0.20f * a));
    pen.strokeWeight(1.0f);
    const float span = c1.fX - a1.fX + (a1.fY - b1.fY);
    for (float s = 0; s < span; s += 6.0f)
      pen.line(a1.fX + s, a1.fY, a1.fX + s - span, a1.fY - span);
    pen.pop();
  }
  pen.noStroke();
}

auto HitmanVerlet::worldBox(Pen& pen, double ms) -> void {
  // The cube's bezel, and its top wall — real, just not interesting.
  const float g = cue(ms, 240, 620, &ch::easeOutCubic);
  pen.noFill();
  pen.strokeCap(draw::SQUARE);
  pen.stroke(hexColor(0x6FA8DC, 0.45f));
  pen.strokeWeight(1.5f);
  // the bezel drawn UP TO a fraction of its perimeter, as a trim window
  // on a rect is
  const float per = 4.0f * kStage, run = per * g;
  const SkPoint corner[5] = {{0.75f, 0.75f},
                             {kStage - 0.75f, 0.75f},
                             {kStage - 0.75f, kStage - 0.75f},
                             {0.75f, kStage - 0.75f},
                             {0.75f, 0.75f}};
  float walked = 0;
  for (int i = 0; i < 4 && walked < run; ++i) {
    const SkPoint p0 = corner[i], p1 = corner[i + 1];
    const float side = len(p1 - p0);
    const float take = std::min(side, run - walked);
    const SkPoint u = (p1 - p0) * (1.0f / side);
    pen.line(p0.fX, p0.fY, p0.fX + u.fX * take, p0.fY + u.fY * take);
    walked += side;
  }
  pen.stroke(hexColor(0x6FA8DC, 0.22f));
  pen.strokeDash({4.0f, 5.0f});
  pen.line(0, 1, kStage, 1);
  pen.noDash();
  pen.strokeCap(draw::ROUND);
  pen.noStroke();
}

auto HitmanVerlet::simulation(Pen& pen) -> void {
  const float f = bodyFade.value();
  // The cloth: one shape for the quad fill, then its edges.
  pen.noStroke();
  pen.fill(hexColor(0x6FA8DC, 0.07f * f));
  constexpr int C = 5, R = 5;
  pen.beginShape(draw::QUADS);
  for (int r = 0; r + 1 < R; ++r)
    for (int c = 0; c + 1 < C; ++c) {
      const int a = r * C + c;
      const SkPoint p0 = drawn(cloth, (size_t)a),
                    p1 = drawn(cloth, (size_t)a + 1),
                    p2 = drawn(cloth, (size_t)a + (size_t)C + 1),
                    p3 = drawn(cloth, (size_t)a + (size_t)C);
      pen.vertex(p0.fX, p0.fY);
      pen.vertex(p1.fX, p1.fY);
      pen.vertex(p2.fX, p2.fY);
      pen.vertex(p3.fX, p3.fY);
    }
  pen.endShape();

  pen.noFill();
  pen.strokeCap(draw::ROUND);
  pen.strokeWeight(1.0f);
  pen.stroke(hexColor(0x8A8F9C, 0.45f * f));
  for (const physics::Constraint& s : cloth.sticks) {
    const SkPoint a = drawn(cloth, s.a), b = drawn(cloth, s.b);
    pen.line(a.fX, a.fY, b.fX, b.fY);
  }
  pen.strokeWeight(2.0f);
  pen.stroke(hexColor(0x8A8F9C, 0.70f * f));
  for (const Body& p : plants)
    for (const physics::Constraint& st : p.sticks) {
      const SkPoint a = drawn(p, st.a), b = drawn(p, st.b);
      pen.line(a.fX, a.fY, b.fX, b.fY);
    }

  paintRig(pen, 1.0f, {0, 0}, f, true);

  // The contact markers: an open ring and a normal tick at every
  // projection this frame. These are the frames where the ramp fires.
  pen.noFill();
  pen.stroke(hexColor(0xC8402F, 0.95f * f));
  pen.strokeWeight(1.3f);
  for (const Contact& k : contacts) {
    const SkPoint p = toStage(k.p);
    pen.circle(p.fX, p.fY, 7.0f);
    pen.line(p.fX, p.fY, p.fX + k.n.fX * 14.0f, p.fY - k.n.fY * 14.0f);
  }

  // §7 IK: the target the hand is pinned to, "the hand of the player" —
  // a CASED leader (a wide casing under a narrow core) whose last eighth
  // is drawn in the accent, which is what a trim window on a path is
  // when the path is two points. The core MARCHES: a dash whose phase
  // runs backwards with the clock, so the leader reads as a pull toward
  // the target rather than a rod between two points. The casing under it
  // and the accent head stay solid, which is what makes the marching
  // legible.
  // FROM THE GRAB POINT, not from the hand. The pin puts the hand on the
  // target as the last operation of every constraint pass, so the hand
  // is never further from the target than one pass's stick residual — a
  // pixel or two — and a leader drawn from it is a point under the
  // target ring. The pull the leader shows is the drag's own: from where
  // the hand was taken hold of to where the target has moved it.
  if (dragging) {
    const SkPoint h = toStage(dragFrom), tgt = toStage(dragTarget);
    pen.strokeWeight(2.0f + 2.0f * 4.0f);
    pen.stroke(kInk);
    pen.line(h.fX, h.fY, tgt.fX, tgt.fY);
    pen.strokeWeight(2.0f);
    pen.stroke(hexColor(0x6FA8DC, 0.55f));
    pen.strokeDash({4.0f, 3.0f}, -(float)pen.millis() * 0.02f);
    pen.line(h.fX, h.fY, tgt.fX, tgt.fY);
    pen.noDash();
    const SkPoint head = h + (tgt - h) * 0.88f;
    pen.strokeWeight(2.6f);
    pen.stroke(kRed);
    pen.line(head.fX, head.fY, tgt.fX, tgt.fY);
    pen.strokeWeight(1.6f);
    pen.circle(tgt.fX, tgt.fY, 18.0f);
  }
  // The blast site, marked permanently: the law is documented, the
  // constant is not.
  {
    const SkPoint b = toStage(kBlast);
    pen.strokeWeight(1.0f);
    pen.stroke(hexColor(0xC8402F, 0.75f));
    pen.line(b.fX - 7, b.fY, b.fX + 7, b.fY);
    pen.line(b.fX, b.fY - 7, b.fX, b.fY + 7);
    pen.circle(b.fX, b.fY, 9.0f);
  }
  // The HIT chevron, 0.4 s on the displaced particle.
  if (loopT >= 1.10 && loopT < 1.50) {
    const SkPoint p = drawn(rig, RSH);
    pen.strokeWeight(2.0f);
    pen.stroke(kRed);
    pen.beginShape();
    pen.vertex(p.fX + 20, p.fY - 9);
    pen.vertex(p.fX + 9, p.fY);
    pen.vertex(p.fX + 20, p.fY + 9);
    pen.endShape();
  }
  pen.noStroke();
}

auto HitmanVerlet::stageOverlay() -> Element {
  return stack()
      .width(Dimension(kStage))
      .height(Dimension(kStage))
      .child(box().inset(0).child(
          instancing::instances(dotAtlas, dotPool, instancing::Mode::Live)));
}

auto HitmanVerlet::blastGlow(Pen& pen) -> void {
  const float a = std::clamp(ch::easeOutQuad(blastPhase.value()), 0.0f, 1.0f);
  if (a <= 0.0f) return;
  const SkPoint c = toStage(kBlast);
  pen.push();
  pen.translate(kStageX, kBodyY);
  pen.noStroke();
  pen.blendMode(sigil::draw::ADD);
  pen.rectMode(sigil::draw::CENTER);
  pen.fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                           {{0.0f, hexColor(0xFFF3E2, 1.0f * a)},
                            {0.35f, hexColor(0xFFC98A, 0.55f * a)},
                            {1.0f, hexColor(0xC8402F, 0.0f)}}),
           sigil::draw::SHAPE);
  pen.rect(c.fX, c.fY, 240.0f, 240.0f);
  pen.pop();
}

auto HitmanVerlet::stageLabels(Pen& pen) -> void {
  penMono(pen, 7.5f, kTick);
  pen.textAlign(draw::LEFT, draw::TOP);
  pen.text("(0, 0)", 7, kStage - 13);
  pen.text("(1000, 1000)", kStage - 62, 5);
  penUi(pen, 7.5f, kBone, 0.5f);
  pen.text(
      "\xc2\xa7"
      "9 \xc2\xb7 THE CORPSE \xc2\xb7 16 PARTICLES, 24 STICKS, "
      "4 ITERATIONS \xc2\xb7 EVERY STICK COLOURED BY ITS LIVE "
      "CONSTRAINT ERROR",
      16, 548, 214, 60);
  penUi(pen, 7.0f, kTick, 0.5f);
  pen.textAlign(draw::RIGHT, draw::TOP);
  pen.text(
      "\xc2\xa7"
      "4 \xc2\xb7 TRIANGULAR MESH \xc2\xb7 ONE PARTICLE PINNED "
      "\xc2\xb7 ONE ITERATION \xc2\xb7 THE SAG IS THE ITERATION "
      "COUNT",
      452, 412, 268, 40);
  pen.text(
      "\xc2\xa7"
      "4 \xc2\xb7 PLANTS = CLOTH + SUPPORT STICKS \xc2\xb7 ONE "
      "ITERATION \xc2\xb7 BASE ROW PINNED",
      452, 596, 268, 40);
  pen.textAlign(draw::LEFT, draw::TOP);
  pen.text(
      "736 \xc3\x97 736 px = THE PAPER'S CUBE "
      "(0,0,0)\xe2\x80\x93(1000,1000,1000) IN THE PLANE.",
      240, 18, 262, 24);
  pen.text(
      "1 UNIT = 0.736 px = 3.60 mm \xc2\xb7 THE STAGE IS SQUARE "
      "BECAUSE THE WORLD IS A CUBE.",
      240, 40, 262, 24);
  penUi(pen, 7.0f, hexColor(0xC8402F, 0.8f), 0.4f);
  pen.text(
      "\xc2\xa7"
      "7 BOMB \xe2\x8a\x95 \xc2\xb7 |\xce\x94x| = K / |x\xe2\x88\x92"
      "c|\xc2\xb2 \xc2\xb7 EVERY PARTICLE, ONCE \xc2\xb7 THE "
      "INTEGRATOR MAKES IT VELOCITY",
      240, 68, 262, 24);

  // The phase strip, and the live stage readout under the error legend.
  const char* names[5] = {"SPAWN", "HIT", "BOMB", "SETTLE", "DRAG"};
  float x = 16;
  for (int i = 0; i < 5; ++i) {
    penUi(pen, 8.0f, i == phase ? kRed : hexColor(0x8A8F9C, 0.45f), 1.7f);
    pen.text(names[i], x, 690);
    x += pen.textWidth(names[i]) + 9;
  }
}

auto HitmanVerlet::figPenetration(Pen& pen, float x0, float y0, float a)
    -> void {
  inset(pen, x0, y0, 208, 148, a);
  penUi(pen, 7.5f, fadeTo(kSteel, a), 1.2f);
  pen.text(
      "FIG. 4b/5b \xc2\xb7 \xc2\xa7"
      "5 PENETRATION",
      x0 + 8, y0 + 8);
  pen.push();
  pen.translate(x0 + 8, y0 + 21);
  // the obstacle
  pen.noStroke();
  pen.fill(fadeTo(kSolid, a));
  pen.beginShape();
  pen.vertex(10, 56);
  pen.vertex(96, 14);
  pen.vertex(180, 56);
  pen.vertex(180, 62);
  pen.vertex(10, 62);
  pen.endShape(draw::CLOSE);
  // the stick, penetrating a quarter along
  const SkPoint x1{22, 44}, x2{162, 26};
  const SkPoint pp{x1.fX * 0.75f + x2.fX * 0.25f,
                   x1.fY * 0.75f + x2.fY * 0.25f};
  const SkPoint q{pp.fX + 2, pp.fY - 17};
  pen.noFill();
  pen.strokeWeight(2.0f);
  pen.stroke(fadeTo(kBlue, a));
  pen.line(x1.fX, x1.fY, x2.fX, x2.fY);
  pen.strokeWeight(1.4f);
  pen.stroke(fadeTo(kRed, a));
  pen.line(pp.fX, pp.fY, q.fX, q.fY);
  pen.noStroke();
  pen.fill(fadeTo(kBone, a));
  pen.circle(x1.fX, x1.fY, 5.2f);
  pen.circle(x2.fX, x2.fY, 5.2f);
  pen.fill(fadeTo(kRed, a));
  pen.circle(pp.fX, pp.fY, 4.4f);
  pen.circle(q.fX, q.fY, 4.4f);
  pen.pop();
  float y = y0 + 21 + 64 + 3;
  penMono(pen, 7.0f, fadeTo(kBlue, a), 0.1f);
  pen.text("p = c1\xc2\xb7x1 + c2\xc2\xb7x2,  c1 = 0.75, c2 = 0.25", x0 + 8, y);
  y += 11;
  pen.text(
      "\xce\xbb = (q\xe2\x88\x92p)\xc2\xb7\xce\x94 / "
      "((c1\xc2\xb2+c2\xc2\xb2)\xc2\xb7\xce\x94\xc2\xb2)",
      x0 + 8, y);
  y += 11;
  pen.text("x1' = x1 + c1\xce\xbb\xce\x94    x2' = x2 + c2\xce\xbb\xce\x94",
           x0 + 8, y);
  y += 12;
  penUi(pen, 6.5f, fadeTo(kTick, a), 0.6f);
  pen.text("THE FIX-UP VIOLATES THE STICK. RELAX AGAIN.", x0 + 8, y);
}

auto HitmanVerlet::figFriction(Pen& pen, float x0, float y0, float a) -> void {
  inset(pen, x0, y0, 208, 148, a);
  penUi(pen, 7.5f, fadeTo(kSteel, a), 1.2f);
  pen.text(
      "FIG. 10 \xc2\xb7 \xc2\xa7"
      "7 FRICTION",
      x0 + 8, y0 + 8);
  pen.push();
  pen.translate(x0 + 8, y0 + 21);
  pen.noStroke();
  pen.fill(fadeTo(kSolid, a));
  pen.beginShape();
  pen.vertex(8, 58);
  pen.quadraticVertex(96, 34, 184, 58);
  pen.vertex(184, 70);
  pen.vertex(8, 70);
  pen.endShape(draw::CLOSE);
  pen.noFill();
  pen.strokeWeight(1.2f);
  // the box, sunk by d_p
  pen.stroke(hexColor(0x6FA8DC, 0.55f * a));
  pen.rect(58, 30, 34, 22);
  pen.stroke(fadeTo(kRed, a));
  pen.line(75, 52, 75, 44);  // d_p
  pen.stroke(fadeTo(kBlue, a));
  pen.line(96, 40, 146, 40);  // v_t before
  pen.line(146, 40, 140, 36);
  pen.line(146, 40, 140, 44);
  pen.stroke(fadeTo(kBone, a));
  pen.line(96, 52, 124, 52);  // v_t after
  pen.line(124, 52, 118, 48);
  pen.line(124, 52, 118, 56);
  pen.noStroke();
  pen.pop();
  float y = y0 + 21 + 64 + 3;
  penMono(pen, 7.0f, fadeTo(kBlue, a), 0.1f);
  pen.text("d_p MEASURED BEFORE THE PROJECTION,", x0 + 8, y);
  y += 11;
  pen.text(
      "v_t REDUCED BY k\xc2\xb7"
      "d_p BY MOVING x*.",
      x0 + 8, y);
  y += 11;
  pen.text("NEVER LET v_t REVERSE \xe2\x80\x94 CLAMP TO ZERO.", x0 + 8, y);
  y += 12;
  penUi(pen, 6.5f, fadeTo(kTick, a), 0.6f);
  pen.text("RESTITUTION IS ZERO: PARTICLES DO NOT BOUNCE.", x0 + 8, y);
}

auto HitmanVerlet::inset(Pen& pen, float x, float y, float w, float h, float a)
    -> void {
  pen.noStroke();
  pen.fill(hexColor(0x101116, 0.88f * a));
  pen.rect(x, y, w, h, 5);
  pen.noFill();
  pen.stroke(fadeTo(kKeyline, a));
  pen.strokeWeight(1.0f);
  pen.rect(x + 0.5f, y + 0.5f, w - 1, h - 1, 5);
  pen.noStroke();
}

auto HitmanVerlet::instancingStrip(Pen& pen, float x0, float y0, float a)
    -> void {
  constexpr float w = 330, h = 108;
  inset(pen, x0, y0, w, h, a);
  penUi(pen, 7.5f, fadeTo(kSteel, a), 0.9f);
  pen.text("SAME 24 STICKS \xc2\xb7 instances()+sizes() vs the pen", x0 + 7,
           y0 + 7);
  pen.element(barsEl, SkRect::MakeXYWH(x0 + 7, y0 + 19, 150, 76));
  {
    float sc = 1;
    SkPoint off{0, 0};
    rigFit(150.0f, 74.0f, &sc, &off);
    paintRig(pen, sc, {x0 + 7 + off.fX + 166.0f, y0 + 19 + off.fY + 2.0f},
             bodyFade.value(), false);
  }
  pen.noStroke();
  pen.fill(hexColor(0x191B22, 0.9f * a));
  pen.rect(x0 + 7 + 158, y0 + 19, 1, 76);
  penMono(pen, 7.0f, fadeTo(kTick, a), 0.2f);
  pen.text("ONE POOL WITH sizes() \xc2\xb7 ONE PEN PROGRAM", x0 + 7,
           y0 + h - 17);
}

auto HitmanVerlet::errorLegend(Pen& pen, float x0, float y0, float a) -> void {
  constexpr float w = 336, h = 48;
  pen.noStroke();
  pen.fill(hexColor(0x101116, 0.86f * a));
  pen.rect(x0, y0, w, h, 4);
  pen.noFill();
  pen.stroke(fadeTo(kKeyline, a));
  pen.strokeWeight(1);
  pen.rect(x0 + 0.5f, y0 + 0.5f, w - 1, h - 1, 4);
  pen.noStroke();
  const char* labels[5] = {"0.000", "0.004", "0.010", "0.020",
                           "\xe2\x89\xa5.035"};
  for (int i = 0; i < 5; ++i) {
    const float x = x0 + 7 + (float)i * 50;
    pen.fill(fadeTo(kRampCol[i], a));
    pen.rect(x, y0 + 7, 48, 7);
    penMono(pen, 6.5f, fadeTo(kTick, a), 0.2f);
    pen.text(labels[i], x, y0 + 17);
  }
  penUi(pen, 7.0f, fadeTo(kSteel, a), 0.3f);
  pen.textAlign(draw::RIGHT, draw::TOP);
  pen.text("e = ||x2\xe2\x88\x92x1|\xe2\x88\x92r| / r", x0 + w - 7, y0 + 7);
  pen.textAlign(draw::LEFT, draw::TOP);
  // the live readout: max error, contacts, step and the interpolant
  const std::string buf = kit::formatted(
      "MAX e %5.2f%%  \xc2\xb7  CONTACTS %2zu  \xc2\xb7  STEP "
      "%llu  \xc2\xb7  \xce\xb1 %.2f",
      stageMaxErr * 100, contactCount, (unsigned long long)simSteps,
      (double)alpha.value());
  penMonoB(pen, 8.0f, errColor(stageMaxErr, a), 0.1f);
  pen.text(buf, x0 + 7, y0 + 30);
}
