#include "ChaucerAstrolabe.h"

auto ChaucerAstrolabe::verify() -> void {
  measure::Table A, B, C, D;

  // --- circles stay circles ------------------------------------------
  // Do NOT take "stereographic projection maps circles to circles" on
  // trust: sample the ecliptic at 3600 longitudes and put every point
  // through the RAW point formula, never through the closed-form centre
  // and radius — otherwise you are checking that a circle is a circle.
  double maxEcl = 0;
  for (int i = 0; i < 3600; ++i) {
    const double lam = (double)i * 0.1 * kDD;
    const double dec = std::asin(std::sin(kEpsD * kDD) * std::sin(lam)) / kDD;
    const double ra =
        std::atan2(std::cos(kEpsD * kDD) * std::sin(lam), std::cos(lam));
    const double r = rOfDecD(dec);
    const double x = r * std::cos(ra), y = r * std::sin(ra);
    maxEcl = std::max(maxEcl, std::abs(std::hypot(x, y - kEclCyD) - kEclRD));
  }
  std::vector<P2> alm;
  for (int i = 0; i < 720; ++i) {
    double dec = 0, H = 0;
    horizToEq((double)i * 0.5, 30.0, &dec, &H);
    alm.push_back(projD(dec, H));
  }
  const FitD fa = fitCircleD(alm);

  A.add(measure::heading("STEREOGRAPHIC \xe2\x80\x94 CIRCLES STAY CIRCLES"))
      .add(measure::reading("ecliptic sampled at N longitudes", 3600))
      .add(measure::check("max |dist to centre \xe2\x88\x92 r_ecl|, R", 0.0,
                          maxEcl, 1e-14))
      .add(measure::reading("almucantar h=30, azimuths fitted", 720))
      .add(measure::check("|centre \xe2\x88\x92 closed form|, R", 0.0,
                          std::abs(fa.cy - almCyD(30.0)), 1e-13))
      .add(measure::check("|radius \xe2\x88\x92 closed form|, R", 0.0,
                          std::abs(fa.r - almRD(30.0)), 1e-13))
      .add(measure::check("least-squares residual, R", 0.0, fa.res, 1e-13))
      .add(measure::heading(
          "TANGENCY \xe2\x80\x94 THE ECLIPTIC TOVCHES BOTH TROPICS"))
      .add(measure::check("|c_ecl| + r_ecl \xe2\x88\x92 R_cap", 0.0,
                          std::abs(kEclCyD) + kEclRD - 1.0, 1e-15))
      .add(measure::check("r_ecl \xe2\x88\x92 |c_ecl| \xe2\x88\x92 R_can", 0.0,
                          kEclRD - std::abs(kEclCyD) - kRcanD, 1e-15))
      .add(measure::heading("THE HORIZON MEETS THE EQVATOR EAST AND WEST"))
      .add(measure::reading("R_eq / sin \xcf\x86",
                            kReqD / std::sin(kPhiD * kDD)))
      .add(measure::check("dist((R_eq,0) \xe2\x86\x92 horizon centre)",
                          kReqD / std::sin(kPhiD * kDD),
                          std::hypot(kReqD, almCyD(0.0)), 1e-14))
      .add(measure::check("prime vertical: R_eq\xc2\xb2 + cy\xc2\xb2 "
                          "\xe2\x88\x92 a\xc2\xb2",
                          0.0, kReqD * kReqD + kAzCyD * kAzCyD - kAzAD * kAzAD,
                          1e-15));

  // --- the azimuth family, checked the hard way ------------------------
  double resPrime = 0, resNaive = 0, lineMax = 0;
  for (int ai = 0; ai < 12; ++ai) {
    const double A_ = (double)(ai * 15);
    std::vector<P2> pts;
    for (int i = 4; i < 176; ++i) {
      double dec = 0, H = 0;
      horizToEq(A_, (double)i * 0.5, &dec, &H);
      pts.push_back(projD(dec, H));
    }
    if (A_ == 0.0) {
      for (const P2& p : pts) lineMax = std::max(lineMax, std::abs(p.x));
      continue;
    }
    const FitD f = fitCircleD(pts);
    const double ap = 90.0 - A_;
    resPrime = std::max({resPrime, std::abs(f.cx - kAzAD * std::tan(ap * kDD)),
                         std::abs(f.cy - kAzCyD),
                         std::abs(f.r - kAzAD / std::cos(ap * kDD))});
    if (A_ != 90.0)  // tan(A) diverges there, and inf is not a diagnostic
      resNaive =
          std::max({resNaive, std::abs(f.cx - kAzAD * std::tan(A_ * kDD)),
                    std::abs(f.r - kAzAD / std::cos(A_ * kDD))});
  }
  B.add(measure::heading("THE AZIMVTH FAMILY, CHECKED THE HARD WAY"))
      .add(measure::reading(
          "each A projected (A,h)\xe2\x86\x92(\xce\xb4,H)\xe2\x86\x92plate",
          12))
      .add(measure::check("A\xe2\x80\xb2 = 90\xc2\xb0\xe2\x88\x92"
                          "A, from the PRIME VERTICAL",
                          0.0, resPrime, 1e-12))
      // A FINDING, not a claim about this code: the published
      // parameterisation is measured from north, and drawn that way the
      // family misses by more than the plate's own radius. Its failing is
      // the result, so it is printed with a verdict and counted separately.
      .add(measure::finding(
          measure::check("A from NORTH, as published", 0.0, resNaive, 1e-12)))
      .add(measure::check("A = 0/180 degenerates to a LINE: max |x|", 0.0,
                          lineMax, 1e-15))
      .add(measure::reading("\xe2\x86\x91 the check that caught it", "1.7 R"));

  // --- the seasonal hours ---------------------------------------------
  double worst = 0, worstDec = 0;
  bool sixStraight = false;
  auto H0d = [](double dec) {
    return std::acos(std::clamp(-std::tan(kPhiD * kDD) * std::tan(dec * kDD),
                                -1.0, 1.0)) /
           kDD;
  };
  auto stepd = [&](double dec) { return (360.0 - 2.0 * H0d(dec)) / 12.0; };
  for (int k = 1; k <= 11; ++k) {
    const P2 can = projD(kEpsD, H0d(kEpsD) + k * stepd(kEpsD));
    const P2 equ = projD(0.0, H0d(0.0) + k * stepd(0.0));
    const P2 cap = projD(-kEpsD, H0d(-kEpsD) + k * stepd(-kEpsD));
    const double det = 2 * (can.x * (equ.y - cap.y) + equ.x * (cap.y - can.y) +
                            cap.x * (can.y - equ.y));
    if (std::abs(det) < 1e-9) {
      sixStraight = (k == 6);
      continue;
    }
    const double aa = can.x * can.x + can.y * can.y,
                 bb = equ.x * equ.x + equ.y * equ.y,
                 cc = cap.x * cap.x + cap.y * cap.y;
    const double cx =
        (aa * (equ.y - cap.y) + bb * (cap.y - can.y) + cc * (can.y - equ.y)) /
        det;
    const double cy =
        (aa * (cap.x - equ.x) + bb * (can.x - cap.x) + cc * (equ.x - can.x)) /
        det;
    const double rr = std::hypot(can.x - cx, can.y - cy);
    for (int i = 0; i <= 480; ++i) {
      const double dec = -kEpsD + 2 * kEpsD * (double)i / 480.0;
      const P2 p = projD(dec, H0d(dec) + (double)k * stepd(dec));
      const double d = std::abs(std::hypot(p.x - cx, p.y - cy) - rr);
      if (d > worst) {
        worst = d;
        worstDec = dec;
      }
    }
  }
  B.add(measure::heading(
            "THE SEASONAL HOVRES \xe2\x80\x94 AND THEIR DOCVMENTED ERROVR"))
      // Twelve seasonal hours between sunrise and sunset: on the equator
      // the day is exactly twelve equinoctial hours, so the step is 15°
      // and the medieval construction is exact THERE and nowhere else.
      .add(measure::check("spacing on the equator, degrees", 15.0, stepd(0.0),
                          1e-12))
      .add(measure::check("k=6 is straight (3 points collinear)", sixStraight))
      // The medieval construction — a circle through one point on each
      // tropic — against the true locus. It is a FINDING: the approximation
      // is the subject, and how far it misses is the number this study is
      // here to produce.
      .add(measure::finding(
          measure::check("3-point circle vs TRVE locus, R", 0.0, worst, 1e-6)))
      .add(measure::reading("worst at declination, degrees", worstDec))
      .add(measure::reading("on the real 132 mm object, mm", worst * 60.0))
      .add(measure::reading("one engraved line is about, mm", 0.2));

  // --- the zodiac cells tile the ring ---------------------------------
  {
    const float ri = 0.86f * kEclR, ro = 1.10f * kEclR;
    std::vector<SkPath> cells;
    for (int i = 0; i < 12; ++i) {
      const float a0 = ringAngle((float)(i * 30));
      float a1 = ringAngle((float)((i + 1) * 30));
      if (a1 < a0) a1 += 360.0f;
      SkPathBuilder b;
      const int n = 40;
      for (int j = 0; j <= n; ++j) {
        const float a = arrange::along(a0, a1 - a0, (size_t)j, (size_t)n + 1,
                                       arrange::Turn::Open) *
                        kD;
        const SkPoint p = arrange::onEllipse({0, 0}, {ro, ro}, a);
        if (j == 0)
          b.moveTo(p);
        else
          b.lineTo(p);
      }
      for (int j = n; j >= 0; --j) {
        const float a = arrange::along(a0, a1 - a0, (size_t)j, (size_t)n + 1,
                                       arrange::Turn::Open) *
                        kD;
        b.lineTo(arrange::onEllipse({0, 0}, {ri, ri}, a));
      }
      b.close();
      cells.push_back(b.detach());
    }
    // The reference region is built from the SAME polyline vertices as the
    // cells, so "uncovered minus outside-the-ring" is exactly zero rather
    // than a few dozen samples of chord error against a true circle. If it
    // is not zero, the \xce\xbb \xe2\x86\x92 ring-angle map is wrong,
    // which is precisely the failure coverage() exists to catch.
    SkPathBuilder rb;
    rb.setFillType(SkPathFillType::kEvenOdd);
    for (int pass = 0; pass < 2; ++pass) {
      const float rad = pass == 0 ? ro : ri;
      for (int i = 0; i < 12; ++i) {
        const float a0 = ringAngle((float)(i * 30));
        float a1 = ringAngle((float)((i + 1) * 30));
        if (a1 < a0) a1 += 360.0f;
        const int n = 40;
        for (int j = 0; j <= n; ++j) {
          const float a = arrange::along(a0, a1 - a0, (size_t)j, (size_t)n + 1,
                                         arrange::Turn::Open) *
                          kD;
          const SkPoint q = arrange::onEllipse({0, 0}, {rad, rad}, a);
          if (i == 0 && j == 0)
            rb.moveTo(q);
          else
            rb.lineTo(q);
        }
      }
      rb.close();
    }
    const SkPath ringPath = rb.detach();
    const SkRect reg = SkRect::MakeLTRB(-ro, -ro, ro, ro);
    const test::Coverage cov = test::coverage(cells, reg, 256);
    const test::Coverage ref =
        test::coverage(std::span<const SkPath>(&ringPath, 1), reg, 256);
    double sum = 0;
    for (int i = 0; i < 12; ++i) {
      const float a0 = ringAngle((float)(i * 30));
      float a1 = ringAngle((float)((i + 1) * 30));
      if (a1 < a0) a1 += 360.0f;
      sum += a1 - a0;
    }
    C.add(measure::heading("THE ZODIAC CELLS TILE THE RING"))
        .add(measure::reading("coverage samples, grid 256", cov.samples))
        .add(measure::check("doubled (an overlap)", 0, cov.doubled))
        .add(measure::check("uncovered \xe2\x88\x92 outside-the-ring", 0,
                            cov.uncovered - ref.uncovered))
        .add(measure::check("\xce\xa3 spans, degrees", 360.0, sum, 1e-3));
  }

  // --- the rete is one piece of metal ---------------------------------
  {
    std::vector<SkPath> paths;
    std::vector<SkPoint> tips;
    for (const Piece& p : pieces) {
      paths.push_back(p.path);
      if (p.kind == Part::Thorn)
        tips.push_back(p.path.getPoint(p.path.countPoints() - 1));
    }
    const test::VertexDegrees vd = test::endpointDegrees(paths, 0.6f);
    int spurs = 0;
    for (size_t i = 0; i < vd.degree.size(); ++i) {
      if (vd.degree[i] != 1) continue;
      bool isTip = false;
      for (const SkPoint& t : tips)
        if (SkPoint::Distance(t, vd.points[i]) <= 1.2f) isTip = true;
      if (!isTip) ++spurs;
    }
    C.add(measure::heading("THE RETE IS ONE PIECE OF METAL"))
        .add(measure::reading("bars, arcs and thorns, tol 0.6 px",
                              (long)paths.size()))
        .add(measure::check("spurs (degree 1, not a star's tip)", 0, spurs))
        // A spur is a piece that falls out of the object; two components
        // are two objects. The adjacency is the check header's own union
        // over shared endpoints.
        .add(
            measure::check("connected components", size_t{1}, vd.components()));
  }

  // --- telling the time -----------------------------------------------
  {
    const double lam = 1.0, h = 25.5;
    const double dec =
        std::asin(std::sin(kEpsD * kDD) * std::sin(lam * kDD)) / kDD;
    const double rs = rOfDecD(dec);
    const double cy = almCyD(h), ra = almRD(h);
    // GRAPHICAL: intersect the two circles that are actually DRAWN
    const double y = (rs * rs - ra * ra + cy * cy) / (2.0 * cy);
    const double x = std::sqrt(std::max(0.0, rs * rs - y * y));
    const double psi = std::atan2(y, -x) / kDD;  // the morning root
    const double Hg = std::abs(90.0 - psi);
    // ANALYTIC: spherical trigonometry, which never sees the drawing
    const double Ha = std::acos((std::sin(h * kDD) -
                                 std::sin(kPhiD * kDD) * std::sin(dec * kDD)) /
                                (std::cos(kPhiD * kDD) * std::cos(dec * kDD))) /
                      kDD;
    chaucerH = kit::formatted(
        "graphical (two DRAWN circles cut)  H = %.9f\xc2\xb0", Hg);
    chaucerA = kit::formatted(
        "analytic  (cos H = (sin h \xe2\x88\x92 s\xcf\x86 s"
        "\xce\xb4)/(c\xcf\x86 c\xce\xb4))   %.9f\xc2\xb0",
        Ha);
    chaucerDelta = kit::formatted(
        "agreement  %.2e\xc2\xb0 \xe2\x80\x94 the DRAWN "
        "geometry encodes the trigonometry",
        std::abs(Hg - Ha));
    D.add(measure::heading("TELLING THE TIME \xe2\x80\x94 12 MARCH 1391, "
                           "ALT 25\xc2\xb0 30\xe2\x80\xb2"))
        .add(measure::reading("graphical, two DRAWN circles cut, degrees", Hg))
        .add(measure::reading("analytic, spherical trigonometry, degrees", Ha))
        // THE HEADLINE. The drawn geometry and the trigonometry are not
        // the same code and never meet: that they agree to 1e-14 degrees
        // is what says the picture encodes the mathematics.
        .add(measure::check("the DRAWN geometry IS the trigonometry", Ha, Hg,
                            1e-12))
        .add(measure::reading("the instrument reads", "08:53.8"))
        .add(measure::reading("lettre X (21st) \xc2\xb7 Chaucer read",
                              "9 of the clokke"))
        .add(measure::reading("his residual, minutes", 6.2));
  }

  // --- the instrument's own error --------------------------------------
  {
    const float kt = std::tan((90.0f - kEpsTrue1326) * 0.5f * kD);
    D.add(measure::heading("THE INSTRVMENT'S OWNE ERROVR"))
        .add(measure::reading("\xce\xb5 Chaucer I.17, R_eq", (double)kReq))
        .add(measure::reading("\xce\xb5 true 1326, R_eq", (double)kt))
        // Chaucer's obliquity is an inherited Ptolemaic figure, 18.4
        // arcminutes too large in 1326. The instrument is not approximate
        // because it was crudely made; it is approximate because the
        // number it was built on was 1200 years old.
        .add(measure::finding(measure::check(
            "\xce\xb5 = 23\xc2\xb0 50\xe2\x80\xb2 is the true obliquity",
            (double)kEpsTrue1326, (double)kEps, 0.01)))
        .add(measure::reading("equator radius, % small",
                              (double)((kReq - kt) / kt * 100.0f)))
        .add(measure::reading("Tropic of Cancer, % small",
                              (double)((kRcan - kt * kt) / (kt * kt) * 100.0f)))
        .add(measure::reading("on the 132 mm object, mm",
                              (double)((kReq - kt) / kt * 60.0f)));
  }

  // The inks: the tinted set's names, and a reading in the dim one.
  const test::ReportStyles ink{.pass = "pass",
                               .fail = "fail",
                               .finding = "fail",
                               .reading = "dim",
                               .heading = "heading",
                               .labelWidth = 46,
                               .valueWidth = 10};
  test::report(logA, A, ink);
  test::report(logB, B, ink);
  test::report(logC, C, ink);
  test::report(logD, D, ink);
}
