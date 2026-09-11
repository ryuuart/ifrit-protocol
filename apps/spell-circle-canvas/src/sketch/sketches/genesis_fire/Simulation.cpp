// Particle emission, fixed steps and streak geometry.

#include "GenesisFire.h"

physics::Emitter GenesisFire::mouthFor(const Site& s, int index,
                                       float speedScale, float sizeScale,
                                       float genScale) {
  return physics::Emitter{
      .from = physics::EmitFrom::Segment,
      .at = s.p,
      .along = s.u,
      .size = {kRGen * genScale, 0.0f},
      .aim = s.n,
      .cone = kPsiMax,
      .speed = {.mean = kMeanSpeed,
                .variation = kVarSpeed,
                .scale = speedScale * s.vScale},
      .attributes =
          {{kSize,
            {.mean = kMeanSize, .variation = kVarSize, .scale = sizeScale}},
           {std::string(physics::kLife),
            {.mean = kMeanLife, .variation = kVarLife}},
           {kRed, {.mean = 1.0f, .variation = kColorVar, .scale = kE0r}},
           {kGreen, {.mean = 1.0f, .variation = kColorVar, .scale = kE0g}},
           {kBlue, {.mean = 1.0f, .variation = kColorVar, .scale = kE0b}}},
      .fixed = {{kSite, (float)index}}};
}

void GenesisFire::setRates(physics::Particles& cloud, float sizeScale) {
  cloud.attribute(kSize).rate = kSizeRate * sizeScale;
  for (const auto& [name, decay] :
       {std::pair{kRed, kDecR}, std::pair{kGreen, kDecG},
        std::pair{kBlue, kDecB}}) {
    physics::Attribute& channel = cloud.attribute(name);
    channel.rate = -decay;
    channel.least = 0.0f;
  }
  // The site stamp is made here too, so the reads below find every
  // attribute rather than adding one.
  cloud.attribute(kSite);
}

void GenesisFire::advance(physics::Particles& cloud,
                          const std::vector<Site>& ss, float gravity,
                          bool killBelowSurface) {
  // Every one of these exists from setup: asking for an attribute a
  // cloud does not carry ADDS it, which moves the others.
  const std::vector<float>& which = cloud.attribute(kSite).values;
  const std::vector<float>& red = cloud.attribute(kRed).values;
  const std::vector<float>& green = cloud.attribute(kGreen).values;
  const std::vector<float>& blue = cloud.attribute(kBlue).values;

  for (size_t i = 0; i < cloud.size(); ++i) {
    const Site& s = ss[(size_t)which[i]];
    cloud.points.velocity[i] -= physics::Vec2(s.n) * gravity;
    cloud.points.position[i] += cloud.points.velocity[i];
  }
  // One film frame older, and every attribute moved by its own rate.
  cloud.live(1.0f);

  cloud.reap([&](size_t i) {
    const physics::Vec2 at = cloud.points.position[i];
    return cloud.expired(i) || (red[i] + green[i] + blue[i]) < kMinIntensity ||
           (killBelowSurface ? at.y > limbY(at.x) + 1.5f
                             : at.y > ss[(size_t)which[i]].p.fY + 2.0f);
  });
}

void GenesisFire::stepSim() {
  loopT += kSimStep;
  if (loopT >= kLoopSeconds) {  // the sequence had a cut; a hard reset reads
    loopT -= kLoopSeconds;      // as one
    parts.clear();
  }
  // Generation. THE WALL IS A STAGGER: site i ignites at
  // t_i = (x_i - X0) / SPREAD — distance from the point of impact.
  for (uint16_t i = 0; i < (uint16_t)sites.size(); ++i) {
    const float f = (float)(loopT - sites[i].t0) * (float)kSimHz;
    if (f < 0.0f || f >= kGenWindow) continue;
    const float rate = kInitialMeanParts + kDeltaMeanParts * f;
    const int n =
        (int)std::lround(std::max(0.0f, rate + rng.signedUnit() * kVarParts));
    mouths[i].burst(parts, rng, (size_t)n);
  }
  advance(parts, sites, kGravity, true);

  // The A/B bench: one steady-state explosion on a fixed recipe, births
  // stopping at the pool's kAbCount slots.
  const int nb =
      (int)std::lround(std::max(0.0f, 25.0f + rng.signedUnit() * 5.0f));
  abMouth.burst(abParts, rng,
                std::min((size_t)std::max(0, nb), kAbCount - abParts.size()));
  advance(abParts, abSites, 1.06f, false);

  liveCount = parts.size();
  ++simSteps;
}

void GenesisFire::buildStreaks(physics::Particles& cloud,
                               std::vector<sk_sp<SkVertices>>& out) {
  out.clear();
  if (cloud.empty()) return;
  const std::vector<float>& sizes = cloud.attribute(kSize).values;
  const std::vector<float>& red = cloud.attribute(kRed).values;
  const std::vector<float>& green = cloud.attribute(kGreen).values;
  const std::vector<float>& blue = cloud.attribute(kBlue).values;
  // SkVertices indices are uint16, so one list holds 65,535 vertices ->
  // 8,191 streaks at eight each. Chunk the way drawSpriteAtlas does.
  constexpr size_t kChunk = 8000;
  static thread_local std::vector<SkPoint> pos;
  static thread_local std::vector<SkColor> col;
  static thread_local std::vector<uint16_t> idx;
  // Six triangles: a FLAT core with an antialiased shoulder either side,
  // which is what an antialiased line actually is. (A three-band strip
  // whose core collapses to a single line reads dimmer for the same
  // coverage, because the flat interior is where the light lives.)
  static constexpr uint16_t kTri[18] = {0, 1, 2, 0, 2, 3, 3, 2, 4,
                                        3, 4, 5, 5, 4, 6, 5, 6, 7};
  for (size_t base = 0; base < cloud.size(); base += kChunk) {
    const size_t cnt = std::min(kChunk, cloud.size() - base);
    pos.clear();
    col.clear();
    idx.clear();
    pos.reserve(cnt * 8);
    col.reserve(cnt * 8);
    idx.reserve(cnt * 18);
    for (size_t i = 0; i < cnt; ++i) {
      const size_t at = base + i;
      const physics::Vec2 velocity = cloud.points.velocity[at];
      const float speed = velocity.length();
      const SkVector d = speed > 1e-4f
                             ? SkVector{velocity.x / speed, velocity.y / speed}
                             : SkVector{0.0f, -1.0f};
      // streaked spherical: length 0.5*|v| but never less than half the
      // diameter, width `size`. So the quad is elongated at ejection and
      // ends up wider than it is long once the particle slows at apogee.
      // The instancing path expresses that too, through Pool::sizes() —
      // the sidebar's bench shows it — and the field is built by hand
      // here for the other reason: Reeves' own renderer was "merely
      // antialiased lines", and one SkVertices list per 8,000 streaks is
      // that, with a per-vertex alpha ramp no atlas cell carries.
      const float len = std::max(sizes[at] * 0.5f, speed * 0.5f);
      const SkPoint head{cloud.points.position[at].x,
                         cloud.points.position[at].y};
      const SkPoint tail{head.fX - d.fX * len, head.fY - d.fY * len};
      const float hw = sizes[at] * 0.5f;
      const SkVector nn{-d.fY * hw, d.fX * hw};
      const SkVector ni{nn.fX * 0.42f, nn.fY * 0.42f};
      const SkColor c = SkColorSetARGB(
          255, (uint8_t)std::lround(std::min(1.0f, red[at]) * 255.0f),
          (uint8_t)std::lround(std::min(1.0f, green[at]) * 255.0f),
          (uint8_t)std::lround(std::min(1.0f, blue[at]) * 255.0f));
      constexpr SkColor kEdge = 0x00000000;
      const uint16_t v0 = (uint16_t)pos.size();
      pos.push_back({tail.fX + nn.fX, tail.fY + nn.fY});
      col.push_back(kEdge);
      pos.push_back({head.fX + nn.fX, head.fY + nn.fY});
      col.push_back(kEdge);
      pos.push_back({head.fX + ni.fX, head.fY + ni.fY});
      col.push_back(c);
      pos.push_back({tail.fX + ni.fX, tail.fY + ni.fY});
      col.push_back(c);
      pos.push_back({head.fX - ni.fX, head.fY - ni.fY});
      col.push_back(c);
      pos.push_back({tail.fX - ni.fX, tail.fY - ni.fY});
      col.push_back(c);
      pos.push_back({head.fX - nn.fX, head.fY - nn.fY});
      col.push_back(kEdge);
      pos.push_back({tail.fX - nn.fX, tail.fY - nn.fY});
      col.push_back(kEdge);
      for (uint16_t tri : kTri) idx.push_back((uint16_t)(v0 + tri));
    }
    out.push_back(SkVertices::MakeCopy(
        SkVertices::kTriangles_VertexMode, (int)pos.size(), pos.data(), nullptr,
        col.data(), (int)idx.size(), idx.data()));
  }
}

void GenesisFire::paintField(Pen& pen,
                             const std::vector<sk_sp<SkVertices>>& chunks,
                             float alpha) {
  if (chunks.empty() || alpha <= 0.001f) return;
  pen.push();
  pen.fill(255, 255, 255, alpha * 255.0f);
  for (const sk_sp<SkVertices>& v : chunks) pen.vertices(v);
  pen.pop();
}

void GenesisFire::writeBenchPool() {
  abPool->resize(kAbCount);
  auto p = abPool->positions();
  auto r = abPool->rotations();
  auto s = abPool->scales();
  auto sz = abPool->sizes();
  auto tn = abPool->tints();
  auto fr = abPool->frames();
  const std::vector<float>& sizes = abParts.attribute(kSize).values;
  const std::vector<float>& red = abParts.attribute(kRed).values;
  const std::vector<float>& green = abParts.attribute(kGreen).values;
  const std::vector<float>& blue = abParts.attribute(kBlue).values;
  for (size_t i = 0; i < kAbCount; ++i) {
    if (i < abParts.size()) {
      const physics::Vec2 velocity = abParts.points.velocity[i];
      const float speed = velocity.length();
      // The same two numbers the field's own quads are built from, so
      // the bench is a picture of the field's shape rather than of a
      // second one.
      const float len = std::max(sizes[i] * 0.5f, speed * 0.5f);
      p[i] = {abParts.points.position[i].x, abParts.points.position[i].y};
      r[i] = std::atan2(velocity.y, velocity.x);
      s[i] = 1.0f;
      // The cell's own ink is 4.4 x 2.3, and the lane is a multiplier on
      // it: the long axis takes the streak's length, the short one its
      // diameter.
      sz[i] = {std::max(0.12f, len / 4.4f), std::max(0.12f, sizes[i] / 2.3f)};
      tn[i] = {std::min(1.0f, red[i]), std::min(1.0f, green[i]),
               std::min(1.0f, blue[i]), 1.0f};
      fr[i] = 0;
    } else {
      p[i] = {-999, -999};
      sz[i] = {0.0f, 0.0f};
      tn[i] = {0, 0, 0, 0};
    }
  }
  abPool->commit();
}

void GenesisFire::writePlanPool() {
  const float front = (float)(loopT / kFrontCrossSeconds) * 124.0f;
  auto tn = planPool->tints();
  auto fr = planPool->frames();
  auto sc = planPool->scales();
  for (size_t i = 0; i < planMarks.size(); ++i) {
    const float d = planMarks[i].dist;
    const float since = front - d;
    if (since < 0.0f) {
      fr[i] = 0;
      tn[i] = {1, 1, 1, 0.55f};
      sc[i] = 1.0f;
    } else {
      const float k = std::clamp(1.0f - since / 96.0f, 0.0f, 1.0f);
      fr[i] = 1;
      const SkColor4f hot = overlap(12);
      tn[i] = {hot.fR, hot.fG, hot.fB, 0.35f + 0.65f * k};
      sc[i] = 1.0f + 0.5f * k;
    }
  }
  planPool->commit();
}

void GenesisFire::seedPlan() {
  planAtlas = std::make_shared<instancing::Atlas>(3.0f);
  // cell 0: unlit open ring; cell 1: lit dot.
  planAtlas->cell(box()
                      .width(3.2f)
                      .height(3.2f)
                      .shape(shapes::circle())
                      .stroke(stroke(0.7f, Fill::color(hexColor(0x2E3A46)),
                                     PathFormat::Align::Inner)),
                  {4, 4});
  planAtlas->cell(box().width(4.0f).height(4.0f).fill(
                      Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                        {{0.0f, {1, 1, 1, 1}},
                                         {0.45f, {1, 1, 1, 0.8f}},
                                         {1.0f, {1, 1, 1, 0}}})),
                  {4, 4});
  // Seven rings from the impact point; marks per ring =
  // round(0.055 * 2*pi*r) — [R83 §3]'s circumference x density rule.
  // Random angular placement, also documented.
  planPool = std::make_shared<instancing::Pool>();
  planMarks.clear();
  const SkPoint impact{34.0f, 106.0f};  // disc-local (disc centre 92,92)
  rng.reseed(0x51A7C0DEu);
  const float radii[7] = {14, 30, 46, 62, 78, 94, 110};
  for (float r : radii) {
    const int n = (int)std::lround(0.055f * 2.0f * 3.14159265f * r);
    for (int i = 0; i < n; ++i) {
      const float a = rng.unit() * 6.2831853f;
      const SkPoint p = arrange::onEllipse(impact, {r, r}, a);
      planMarks.push_back({p, r});
      planPool->add(p, 0, 0.0f, 1.0f, {1, 1, 1, 0.55f});
    }
  }
}

void GenesisFire::seedBench() {
  abAtlas = std::make_shared<instancing::Atlas>(4.0f);
  // ONE baked aspect. The paper's shape goes from elongated at ejection to
  // stubby at apogee; an atlas cell is one size and a Pool scale is one
  // float, so this cell is the compromise the middle two panels show.
  abAtlas->cell(box().width(4.4f).height(2.3f).corners({1.0f}).fill(
                    Paint::radialUnit({0.5f, 0.5f}, 1.05f,
                                      {{0.0f, {1, 1, 1, 1}},
                                       {0.42f, {1, 1, 1, 0.9f}},
                                       {1.0f, {1, 1, 1, 0}}})),
                {4.8f, 2.6f});
  abPool = std::make_shared<instancing::Pool>();
  abPool->resize(kAbCount);
}
