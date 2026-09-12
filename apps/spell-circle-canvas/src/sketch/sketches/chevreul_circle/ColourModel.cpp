#include "ChevreulCircle.h"

auto ChevreulCircle::wallLift(SkColor4f c) -> SkColor4f {
  const float y = 0.2126f * c.fR + 0.7152f * c.fG + 0.0722f * c.fB;
  const auto lift = [y](float v) {
    return std::clamp(y + (v - y) * kWallLift, 0.0f, 1.0f);
  };
  return {lift(c.fR), lift(c.fG), lift(c.fB), c.fA};
}

auto ChevreulCircle::computeColours() -> void {
  for (int n = 0; n < 72; ++n) {
    corrected[(size_t)n] = wallLift(hexColor(kCorrectedHex[(size_t)n]));
    scanned[(size_t)n] = hexColor(kScannedHex[(size_t)n]);
    lab[(size_t)n] = toLab(corrected[(size_t)n]);
  }
  // §164 read as equal REFLECTANCE, against the modern equal-code-value
  // ramp the illusion demonstrations use.
  for (int t = 1; t <= 20; ++t) {
    const float Y = (float)(20 - t) / 19.0f;
    const float s = linearToSrgb(Y);
    gamme[(size_t)(t - 1)] = {s, s, s, 1};
    const float c = (float)(20 - t) / 19.0f;
    gammeCode[(size_t)(t - 1)] = {c, c, c, 1};
  }
}

auto ChevreulCircle::verify(sketch::SketchContext& ctx) -> void {
  // --- 1. the circle closes, two ways -----------------------------
  v.named = 12;
  v.perNamed = 6;  // one named + five numbered intermediates
  v.closes1 = v.named * v.perNamed;
  v.closes2 = 3 + 3 * 23;  // colorants.hypotheses.org's framing
  derivation1 = kit::formatted(
      "120/2 = 60  ->  60/2 = 30  ->  30/5 = 6 deg;  "
      "360/6 = %d sectors of %.1f deg",
      360 / 6 * 1, kSectorDeg);
  // (the arithmetic, spelled the way §161 builds it)
  derivation1 = kit::formatted(
      "3 arcs of 120 -> 6 of 60 -> 12 of 30, each divided "
      "in 5: 12 + 60 = %d",
      v.closes1);

  // --- 2. the system total (§163-§165) ----------------------------
  const long plane = 72L * 20L;        // the circle's own plane
  const long broken = 9L * 72L * 20L;  // nine radii broken by tenths
  const long grey = 20L;               // the tenth radius, normal grey
  v.total = plane + broken + grey;
  derivation2 =
      kit::formatted("72 x 20 = %ld  +  9 x 72 x 20 = %ld  +  20 grey  =  %ld",
                     plane, broken, v.total);

  // --- 3. the plate's own diameter --------------------------------
  v.plateDelta = kScanVert - kScanRouge;

  // --- 4. §6's four statements against §161's +36 rule ------------
  struct Claim {
    int a, b;
    const char* text;
  };
  const std::array<Claim, 4> claims = {{
      {kNRed, kNGreen, "Red / Green"},
      {kNOrange, kNBlue, "Orange / Blue"},
      {18 /*ORANGE-JAUNE*/, 54 /*BLEU-VIOLET*/, "Orange-Yellow / Indigo"},
      {30 /*JAUNE-VERT*/, 60 /*VIOLET*/, "Greenish-Yellow / Violet"},
  }};
  for (const Claim& c : claims) {
    const int want = complementOf(c.a);
    const int off = sepSectors(want, c.b);
    if (off == 0)
      ++v.compExact;
    else {
      ++v.compOff;
      v.compWorstDeg = std::max(v.compWorstDeg, (float)off * kSectorDeg);
    }
  }

  // --- 5. the seventeen are all of them ---------------------------
  std::array<std::array<bool, 7>, 7> inTable{};
  for (const Observation& o : kObs) {
    inTable[(size_t)o.a][(size_t)o.b] = true;
    inTable[(size_t)o.b][(size_t)o.a] = true;
  }
  // the four pairs Chevreul's PROSE names complementary
  std::array<std::array<bool, 7>, 7> byProse{};
  auto mark = [&](int i, int j) {
    byProse[(size_t)i][(size_t)j] = byProse[(size_t)j][(size_t)i] = true;
  };
  mark(0, 3);  // §6  Red / Green
  mark(1, 4);  // §6  Orange / Blue
  mark(2, 6);  // §6  Greenish-Yellow / Violet
  mark(2, 5);  // §29-§36 "the complementary of Yellow (Indigo inclining
               //          to violet)"
  v.pairs21 = 0;
  v.byName = v.byStrict = v.byLoose = 0;
  v.nameSetMatches = true;
  for (int i = 0; i < 7; ++i)
    for (int j = i + 1; j < 7; ++j) {
      ++v.pairs21;
      const int sep = sepSectors(kNewton[(size_t)i], kNewton[(size_t)j]);
      if (!byProse[(size_t)i][(size_t)j]) ++v.byName;
      if (sep != 36) ++v.byStrict;
      if (sep < 30) ++v.byLoose;
      // the sharp version: the prose set must be EXACTLY the absent set
      if (byProse[(size_t)i][(size_t)j] == inTable[(size_t)i][(size_t)j])
        v.nameSetMatches = false;
    }

  // --- 6/7. the measured hues wind once, and not evenly -----------
  std::array<float, 72> hab{}, steps{};
  for (int n = 0; n < 72; ++n) {
    float h =
        std::atan2(lab[(size_t)n].b, lab[(size_t)n].a) * 180.0f / 3.14159265f;
    if (h < 0) h += 360.0f;
    hab[(size_t)n] = h;
  }
  v.huePositive = 0;
  v.hueWorst = 1e9f;
  for (int n = 0; n < 72; ++n) {
    float d = hab[(size_t)((n + 1) % 72)] - hab[(size_t)n];
    d = std::fmod(std::fmod(d, 360.0f) + 360.0f, 360.0f);
    if (d > 180.0f) d -= 360.0f;
    steps[(size_t)n] = d;
    if (d > 0) ++v.huePositive;
    if (d < v.hueWorst) {
      v.hueWorst = d;
      v.hueWorstAt = n;
    }
  }
  v.hueSum = 0;
  v.hueMin = 1e9f;
  v.hueMax = -1e9f;
  for (float d : steps) {
    v.hueSum += d;
    v.hueMin = std::min(v.hueMin, d);
    v.hueMax = std::max(v.hueMax, d);
  }
  v.hueMean = v.hueSum / 72.0f;
  float acc = 0;
  for (float d : steps) acc += (d - v.hueMean) * (d - v.hueMean);
  v.hueSd = std::sqrt(acc / 72.0f);

  // --- 8. the diameters, against two candidate centres ------------
  v.centA = v.centB = 0;
  for (const Lab& l : lab) {
    v.centA += l.a;
    v.centB += l.b;
  }
  v.centA /= 72.0f;
  v.centB /= 72.0f;
  v.meanChroma = 0;
  for (const Lab& l : lab) v.meanChroma += std::hypot(l.a, l.b);
  v.meanChroma /= 72.0f;
  auto chordMiss = [&](float px, float py, float& mean, float& worst) {
    mean = 0;
    worst = 0;
    for (int n = 0; n < 36; ++n) {
      const Lab &A = lab[(size_t)n], &B = lab[(size_t)n + 36];
      const float dx = B.a - A.a, dy = B.b - A.b;
      const float len = std::hypot(dx, dy);
      const float d = len > 1e-6f
                          ? std::fabs(dx * (A.b - py) - dy * (A.a - px)) / len
                          : 0.0f;
      mean += d;
      worst = std::max(worst, d);
    }
    mean /= 36.0f;
  };
  chordMiss(0.0f, 0.0f, v.missOrigin, v.missOriginMax);
  chordMiss(v.centA, v.centB, v.missCentroid, v.missCentroidMax);
  v.missPercent = 100.0f * v.missCentroid / v.meanChroma;

  // --- 9. §160's luminosity claim ---------------------------------
  v.yJaune = luminance(corrected[24]);
  v.yBleu = luminance(corrected[48]);
  v.yRouge = luminance(corrected[0]);
  v.jauneHighest = true;
  for (int i = 0; i < 12; ++i)
    if (i != 4 && luminance(corrected[(size_t)i * 6]) > v.yJaune)
      v.jauneHighest = false;
  v.bleuDarker = v.yBleu < v.yRouge;  // §160 says it should be

  // --- 10. the staircase, read back off a raster surface ----------
  //     Done FIRST of the drawing checks and before anything else is
  //     described: if a described #8C8C8C does not come back as
  //     0x8C8C8C, everything downstream is measuring a transform
  //     nobody declared.
  v.bands = kBandN;
  if (ctx.fonts) {
    Element strip = box().row();
    for (int b = 0; b < kBandN; ++b)
      strip.child(box()
                      .width(Dimension(kBandW))
                      .height(Dimension(32))
                      .shrink(0)
                      .fill(Fill::color(gamme[(size_t)b])));
    // test::rasterize is the read-back: it wraps the tree in the shell
    // snapshot() needs, draws it at an explicit canvas size and hands the
    // pixels over. N32 rather than the float default, because the claim
    // is about the 8-bit value a viewer's screen is handed.
    const test::Raster r =
        test::rasterize(std::move(strip), *ctx.fonts,
                        {(int)(kBandW * kBandN), 32}, kN32_SkColorType);
    if (r.valid()) {
      for (int b = 0; b < kBandN; ++b) {
        const int x0 = (int)(b * kBandW);
        const SkColor4f want = gamme[(size_t)b];
        int dev = 0;
        // interior only: the band edges are antialiased and a 1 px
        // blend seam there is correct behaviour, not a defect.
        double sum = 0, sum2 = 0;
        int cnt = 0;
        for (int x = x0 + 6; x < x0 + (int)kBandW - 6; x += 2)
          for (int y = 6; y < 26; y += 2) {
            const SkColor4f got = r.at(x, y);
            dev = std::max({dev, std::abs(code(got.fR) - code(want.fR)),
                            std::abs(code(got.fG) - code(want.fG)),
                            std::abs(code(got.fB) - code(want.fB))});
            const double lum = code(got.fG);
            sum += lum;
            sum2 += lum * lum;
            ++cnt;
          }
        const double mean = cnt ? sum / cnt : 0.0;
        const double var = cnt ? std::max(0.0, sum2 / cnt - mean * mean) : 0.0;
        v.bandSigmaMax = std::max(v.bandSigmaMax, (float)std::sqrt(var));
        v.bandMaxDev = std::max(v.bandMaxDev, dev);
        if (dev == 0) ++v.bandsExact;
      }
      // The same readback rule: a capture that is diffed prints the
      // staircase's claim — every hex exact, flat within its band.
      v.bandsExact = (int)ctx.measured(v.bandsExact, v.bands);
      v.bandSigmaMax = (float)ctx.measured(v.bandSigmaMax, 0.0);
      v.bandMaxDev = (int)ctx.measured(v.bandMaxDev, 0);
    }
  }

  // --- 11. the 72 sectors exactly tile the annulus ----------------
  //     Run against the UN-OVERLAPPED geometry; the drawn wheel uses a
  //     0.25 deg overlap so the antialiased seams do not double the
  //     radialHatch separators. The SkPath-region overload is what makes
  //     a radial tiling testable at all: with the SkRect form every
  //     corner of the bounding square reports as an uncovered gap.
  {
    const SkSize sz{2 * kRColour, 2 * kRColour};
    std::vector<SkPath> pieces;
    pieces.reserve(73);
    for (int n = 0; n < 72; ++n)
      pieces.push_back(shapes::sector(sectorStart(n), kSectorDeg, kInner)(sz));
    SkPathBuilder hub;
    hub.addOval(SkRect::MakeXYWH(kRColour * (1 - kInner),
                                 kRColour * (1 - kInner), 2 * kRColour * kInner,
                                 2 * kRColour * kInner));
    pieces.push_back(hub.detach());
    SkPathBuilder region;
    region.addOval(SkRect::MakeWH(sz.width(), sz.height()));
    const SkPath regionPath = region.detach();
    const test::Coverage cov = test::coverage(pieces, regionPath, 256);
    v.covSamples = cov.samples;
    // Read back off this run's own raster, so pinned for a capture that
    // is diffed — to the exact cover the construction claims.
    v.covUncovered = (int)ctx.measured(cov.uncovered, 0);
    v.covDoubled = (int)ctx.measured(cov.doubled, 0);

    // endpointDegrees on the same 72 pieces. It reports how many contours
    // were CLOSED rather than counting their endpoints, so a ring of
    // closed sectors reads as "this test does not apply" rather than as 72
    // points of degree 1.
    std::vector<SkPath> sectorsOnly(pieces.begin(), pieces.end() - 1);
    const test::VertexDegrees deg = test::endpointDegrees(sectorsOnly);
    v.closedContours = deg.closedContours;
    v.endpointPoints = deg.points.size();
  }

  // --- 12. instance tints, read back ------------------------------
  buildQuadrantPool();
  if (ctx.fonts && quadAtlas && quadPool) {
    const float gw = 10 * (kQCellW + kQGapX) - kQGapX;
    const float gh = 20 * (kQCellH + kQGapY) - kQGapY;
    Element probe = box()
                        .width(Dimension(gw))
                        .height(Dimension(gh))
                        .child(instancing::instances(quadAtlas, quadPool,
                                                     instancing::Mode::Data));
    const test::Raster r = test::rasterize(
        std::move(probe), *ctx.fonts, {(int)std::ceil(gw), (int)std::ceil(gh)},
        kN32_SkColorType);
    if (r.valid()) {
      const auto pos = quadPool->positions();
      const auto tints = quadPool->tints();
      for (size_t i = 0; i < pos.size(); ++i) {
        const SkColor4f got = r.at((int)pos[i].fX, (int)pos[i].fY);
        const SkColor4f want = tints[i];
        const int dev = std::max({std::abs(code(got.fR) - code(want.fR)),
                                  std::abs(code(got.fG) - code(want.fG)),
                                  std::abs(code(got.fB) - code(want.fB))});
        ++v.tintCells;
        v.tintMaxDev = std::max(v.tintMaxDev, dev);
        if (dev == 0) ++v.tintExact;
      }
      // Pixel readback is a measurement of this run, so a capture that
      // is diffed prints the claim instead: every cell exact.
      v.tintExact = (int)ctx.measured(v.tintExact, v.tintCells);
      v.tintMaxDev = (int)ctx.measured(v.tintMaxDev, 0);
    }
  }

  // --- the OCIO seam, MEASURED rather than asserted -----------------
  //     Nothing in the API states what a value comes out as, so this
  //     measures it. Push tone 10 of the
  //     §164 gamme (#C0C0C0) through ocio::exponent(2.2) and read it back
  //     off a raster surface, exactly the way check 10 reads the
  //     staircase — so "the seam works" is a number on the plate.
  v.ocioAvailable = ocio::available();
  if (v.ocioAvailable && ctx.fonts) {
    // test::rasterize draws a described tree and hands back the PIXELS,
    // shell and canvas size included — the read-back this probe needs,
    // and the same one check 10 makes against the staircase. N32 rather
    // than the float default: the claim is about the 8-bit value a
    // viewer's screen is handed.
    const test::Raster r =
        test::rasterize(box()
                            .width(Dimension(32))
                            .height(Dimension(32))
                            .fill(Fill::color(gamme[9]))
                            .effect(Effect::recipe(ocio::exponent(2.2f))),
                        *ctx.fonts, {32, 32}, kN32_SkColorType);
    if (r.valid()) {
      const SkColor4f got = r.at(16, 16);
      v.ocioSample = kit::formatted(
          "#%02X%02X%02X", (int)std::lround(got.fR * 255.0f),
          (int)std::lround(got.fG * 255.0f), (int)std::lround(got.fB * 255.0f));
    }
  }
}

auto ChevreulCircle::buildVerifyTable() -> void {
  verdict = {};
  verdict
      .add(measure::check("CIRCLE CLOSES     12 named \xc3\x97 6", 72,
                          v.closes1))
      .add(measure::check("                  3 + 3\xc3\x97"
                          "23",
                          72, v.closes2))
      .add(measure::check("SYSTEM TOTAL      72\xc3\x97"
                          "20\xc3\x97"
                          "10 + 20 grey",
                          14420L, v.total))
      .add(measure::check("PLATE DIAMETER    ROUGE\xe2\x86\x92VERT, deg", 180.0,
                          (double)v.plateDelta, 0.005))
      .add(measure::check("SEVENTEEN         C(7,2)=21 \xe2\x88\x92 4 named",
                          17, v.byName))
      .add(measure::check("                  and they are HIS seventeen",
                          v.nameSetMatches))
      .add(measure::reading("  by geometry     sep==36 / sep>=30",
                            kit::formatted("%d / %d", v.byStrict, v.byLoose)))
      // A statement about the 161-year-old PRINT, not about this
      // reconstruction: whether the measured hue really advances at every
      // one of the seventy-two steps.
      .add(measure::finding(measure::check("HUE WINDS ONCE    steps > 0, of 72",
                                           72, v.huePositive)))
      .add(measure::reading(
          "EQUAL SECTORS     step mean / sd",
          kit::formatted("%.2f / %.2f", (double)v.hueMean, (double)v.hueSd)))
      .add(measure::reading("DIAMETERS         miss: origin / centroid",
                            kit::formatted("%.2f / %.2f", (double)v.missOrigin,
                                           (double)v.missCentroid)))
      // \xc2\xa7" "6's four complementary statements against \xc2\xa7" "161's
      // own construction. Three land on the nose; greenish-yellow/violet
      // does not, and that is Chevreul's, not the reconstruction's.
      .add(measure::finding(measure::check("COMPLEMENTARIES   \xc2\xa7"
                                           "6 pairs exact, of 4",
                                           4, v.compExact)))
      // \xc2\xa7" "160: yellow lighter and blue darker than red, measured
      // off the plate's own medians.
      .add(measure::finding(measure::check("LUMINOSITY \xc2\xa7"
                                           "160  jaune is the lightest",
                                           v.jauneHighest)))
      .add(measure::finding(measure::check(
          "                  bleu darker than rouge", v.bleuDarker)))
      .add(measure::check("STAIRCASE         hexes exact, of 20", v.bands,
                          v.bandsExact))
      .add(measure::check("                  max within-band \xcf\x83", 0.0,
                          (double)v.bandSigmaMax, 0.0))
      .add(measure::check("EXACT COVER       uncovered", 0, v.covUncovered))
      .add(measure::check("                  doubled", 0, v.covDoubled))
      .add(measure::check("INSTANCE TINTS    cells exact", v.tintCells,
                          v.tintExact))
      .add(measure::check("                  max channel deviation", 0,
                          v.tintMaxDev));
}

auto ChevreulCircle::buildLaw() -> void {
  weave::TextStyle body = sheet().sans(13, kInk);
  body.shaping.languageTag = "en-GB";
  weave::ParagraphBuilder b(body);
  // SigilWeave breaks at SOFT HYPHENS only, so the discretionaries are
  // typed in the way a compositor would set them.
  b.addText(
      u8"“In the case where the eye sees at the same time two "
      u8"con­tigu­ous col­ours, they will appear as "
      u8"dis­sim­i­lar as pos­si­ble, both "
      u8"in their op­ti­cal com­po­si­tion "
      u8"and in the height of their tone.”  ");
  b.pushStyle(
      weave::textStyle({.face = serifIt(), .size = 11, .color = kInk2}));
  b.addText(
      u8"— M. E. Chevreul, §16, De la loi du contraste simultané des "
      u8"couleurs, 1839; trans. Charles Martel.");
  lawPara = std::make_shared<weave::Paragraph>(b.build());
}
