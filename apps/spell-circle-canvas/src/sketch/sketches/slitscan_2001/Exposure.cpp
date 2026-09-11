#include "SlitScan2001.h"

auto SlitScan2001::shotAt(int i) -> const Shot& {
  using namespace slit;
  // THE TWO PLANES STAND ABOVE AND BELOW. Trumbull's "two seemingly
  // infinite planes" are the walls of a CORRIDOR the camera flies down:
  // on the Star Gate frame they fill the top and the bottom of a 2.20:1
  // image and converge on a lit core between them. A pair set near the
  // diagonal instead reads as two beams crossing an otherwise black
  // frame — the same arithmetic, the wrong picture. Each shot leans its
  // own few degrees off the vertical, which is the plate turning between
  // takes and not a second idea.
  static const Shot k[4] = {
      {"SEQ 29 · SH 04", kGelStraw, kGelCyan, 86.0f, 0.35f, 0,
       "BACKLIT GRAPHIC SHAPES, FAST SCROLL [GE]"},
      {"SEQ 29 · SH 08", kGelMag, kGelGreen, 76.0f, -0.50f, 1,
       "ABSTRACT SHAPES AND SPIRALS [GE]"},
      {"SEQ 29 · SH 27", kGelRed, kGelAmber, 97.0f, 0.20f, 2,
       "POSTERISED CORAL / FLOWERS [GE]"},
      {"SEQ 29 · SH 29", kGelViolet, kGelCyan, 92.0f, -0.35f, 2,
       "MICROSCOPIC / BOTANICAL — SAME STRIP AS SH 27 [GE]"},
  };
  return k[(unsigned)i & 3u];
}

auto SlitScan2001::rebuildWalls() -> void {
  using namespace slit;
  const Shot& s = shotAt(shot);
  const SkPoint c = vp();
  coreX = c.fX;
  coreY = c.fY;
  WallSpec A;
  A.vp = c;
  A.phiDeg = s.phi0 + plateDeg;
  A.gel = s.gelA;
  A.gain = displayGain();
  A.artLeft = artOffset;
  // [NO]: "the artwork slid slowly left or right DURING the exposure".
  // Eighteen slit widths across one sweep, so the artwork a streak reads
  // changes along its length -- which is what gives the corridor its
  // rungs rather than a clean radial fan.
  A.artDrift = 18.0f * kWinFrac;
  A.cell = s.cell;
  WallSpec B = A;
  B.phiDeg = A.phiDeg + 180.0f;  // "two seemingly infinite planes" [T68]
  B.gel = s.gelB;
  B.artLeft = std::fmod(artOffset + 0.37f, 1.0f);
  B.artDrift = -18.0f * kWinFrac;
  buildWall(*wallA, A);
  buildWall(*wallB, B);

  // The monitor: the SAME two exposures at the demonstration rate, with
  // stamps beyond the carriage's current z suppressed. It is the only
  // place in the plate where you see a frame BEING MADE rather than made.
  const float mScale = 108.0f / kUNear;
  WallSpec MA = A;
  MA.vp = {124.0f, 60.0f};
  MA.uFar = kUFar * mScale;
  MA.upTo = (float)tau;
  MA.K = kK;
  WallSpec MB = MA;
  MB.phiDeg = B.phiDeg;
  MB.gel = B.gel;
  MB.artLeft = B.artLeft;
  MB.artDrift = B.artDrift;
  buildWall(*monA, MA);
  buildWall(*monB, MB);
}

auto SlitScan2001::fitAtK(sigil::weave::FontContext& fonts, int K) -> Fit {
  using namespace slit;
  Fit out;
  auto pa = std::make_shared<instancing::Pool>();
  auto pb = std::make_shared<instancing::Pool>();
  const SkPoint c{kFilmW * 0.5f, kFilmH * 0.5f};
  WallSpec A;
  A.vp = c;
  A.gel = kWhite;
  A.gain = 1.0f;
  A.artwork = false;
  A.K = K;
  WallSpec B = A;
  B.phiDeg = 180.0f;
  buildWall(*pa, A);
  buildWall(*pb, B);

  Element accum =
      box()
          .width(Dim(kFilmW))
          .height(Dim(kFilmH))
          .child(instancing::instances(flatAtlas, pa, instancing::Mode::Data,
                                       SkBlendMode::kPlus))
          .child(instancing::instances(flatAtlas, pb, instancing::Mode::Data,
                                       SkBlendMode::kPlus));
  // snapshot() sizes the picture by the root's CHILDREN, not by the
  // root's own width/height -- hence the shell box. Passed directly, the
  // accumulation's children are instancing leaves, which measure zero on
  // both axes, and the read-back would be a silently empty raster.
  sk_sp<SkPicture> pic = snapshot(box().child(std::move(accum)), fonts);
  sk_sp<SkSurface> surf = SkSurfaces::Raster(SkImageInfo::Make(
      (int)kFilmW, (int)kFilmH, kRGBA_F16_SkColorType, kPremul_SkAlphaType));
  if (!pic || !surf) return out;
  surf->getCanvas()->clear(SK_ColorTRANSPARENT);
  surf->getCanvas()->drawPicture(pic);

  const int W = (int)kFilmW, H = (int)kFilmH;
  const SkImageInfo dst =
      SkImageInfo::Make(W, H, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType);
  std::vector<float> px((size_t)W * (size_t)H * 4);
  if (!surf->readPixels(dst, px.data(), (size_t)W * 16, 0, 0)) return out;
  auto lumAt = [&](float x, float y) -> float {
    const int xi = (int)std::lround(x), yi = (int)std::lround(y);
    if (xi < 0 || yi < 0 || xi >= W || yi >= H) return -1;
    const float* p = &px[((size_t)yi * (size_t)W + (size_t)xi) * 4];
    return (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) * p[3];
  };

  // 32 rays inside the wedge (half-angle 26 deg; sampled to 20 deg so the
  // stamp rectangles' corners never truncate a ray), 120 log-spaced radii.
  const float u0 = 8.0f, u1 = 520.0f;
  double sp = 0, sr2 = 0;
  std::vector<float> resid;
  for (int w = 0; w < 2; ++w)
    for (int r = 0; r < 16; ++r) {
      const float dd = (-20.0f + 40.0f * (float)r / 15.0f) * 0.017453292f;
      const float ang = (w ? 3.14159265f : 0.0f) + dd;
      std::vector<double> lx, ly;
      std::array<int, 120> bin{};
      for (int i = 0; i < 120; ++i) {
        const float u = u0 * std::pow(u1 / u0, (float)i / 119.0f);
        const SkPoint at = arrange::onEllipse(c, {u, u}, ang);
        const float v = lumAt(at.fX, at.fY);
        bin[(size_t)i] = -1;
        if (v > 1e-6f) {
          bin[(size_t)i] = (int)lx.size();
          lx.push_back(std::log((double)u));
          ly.push_back(std::log((double)v));
        }
      }
      if (lx.size() < 40) continue;
      const size_t n = lx.size();
      // log v against log u: the exponent this ray falls off with is the
      // slope, and the fit reports the residuals that say whether the
      // exponent is worth quoting. Fitted in DOUBLE because the exponent
      // IS the finding here rather than a number a drawing rides on.
      const measure::LineFit<double> fit = measure::lineFit<double>(lx, ly);
      // A residual in the exponent is unreadable; the same distance as a
      // FRACTION of the measured luminance is the reading the card wants.
      for (size_t i = 0; i < n; ++i)
        resid.push_back(
            (float)std::fabs(std::exp(fit.residual(lx[i], ly[i])) - 1.0));
      // Keep the profile itself, each ray levelled by its own intercept,
      // so the plot shows MEASURED samples rather than a replay of the fit.
      for (int i = 0; i < 120; ++i)
        if (bin[(size_t)i] >= 0) {
          out.sum[(size_t)i] += ly[(size_t)bin[(size_t)i]] - fit.intercept;
          out.cnt[(size_t)i] += 1;
        }
      sp += -fit.slope;
      sr2 += fit.r2;
      ++out.rays;
      out.pts += (int)n;
    }
  if (out.rays) {
    out.p = (float)(sp / (double)out.rays);
    out.r2 = (float)(sr2 / (double)out.rays);
    std::sort(resid.begin(), resid.end());
    out.worst = resid.empty() ? 0 : resid.back();
    out.p95 = resid.empty() ? 0 : resid[(size_t)(0.95 * (double)resid.size())];
  }
  return out;
}

auto SlitScan2001::measureExposure(sigil::weave::FontContext& fonts) -> void {
  const Fit big = fitAtK(fonts, slit::kKDisplay);
  const Fit small = fitAtK(fonts, slit::kK);
  fitP = big.p;
  fitR2 = big.r2;
  fitResid = big.worst;
  fitP95 = big.p95;
  fitRays = big.rays;
  fitPts = big.pts;
  fitPMin = small.p;
  fitResidMin = small.worst;
  // Normalise the measured profile onto the same axes the analytic C/u
  // uses: an exact 1/u law is then the box diagonal, and every departure
  // -- including the +-1-stamp quantisation ripple -- is a visible wiggle.
  const double span = std::log(520.0 / 8.0);
  profN = 0;
  double anchor = 0;
  int anchorN = 0;
  for (int i = 0; i < 120; ++i)
    if (big.cnt[(size_t)i]) {
      const double le = big.sum[(size_t)i] / (double)big.cnt[(size_t)i];
      anchor += le + span * (double)i / 119.0;
      ++anchorN;
    }
  if (anchorN)
    anchor /= (double)anchorN;  // zero-mean against the analytic diagonal
  for (int i = 0; i < 120; ++i) {
    if (!big.cnt[(size_t)i]) continue;
    const double le = big.sum[(size_t)i] / (double)big.cnt[(size_t)i];
    profX[(size_t)profN] = (float)((double)i / 119.0);
    profY[(size_t)profN] = (float)((anchor - le) / span);
    ++profN;
  }
}

auto SlitScan2001::roundTrip(sigil::weave::FontContext& fonts) -> void {
  using namespace slit;
  const Strip& S = strips[0];
  if (!S.image || S.lum.empty()) return;
  constexpr int kFrames = 96;
  const int cw = (int)std::lround(kSlitWIn * kCellPxPerIn);  // 6
  const int chh = (int)std::lround(kCellH);                  // 492
  const int boxW = cw + 16, boxH = chh + 8;
  const float cx = (float)boxW * 0.5f, cy = (float)boxH * 0.5f;

  auto one = std::make_shared<instancing::Atlas>(1.0f);
  one->filter(SkFilterMode::kNearest);
  one->cell(box().fill(Paint::image(
                S.image, SkTileMode::kClamp, SkTileMode::kClamp,
                SkMatrix::Scale(kCellW / (float)S.w, kCellH / (float)S.h),
                SkSamplingOptions())),
            {kCellW, kCellH});
  auto pool = std::make_shared<instancing::Pool>();
  pool->resize(1);

  double sg = 0, sw = 0, sgg = 0, sww = 0, sgw = 0;
  size_t n = 0, bad = 0;
  float worst = 0;
  std::vector<float> got, want;
  const float aStart = 0.11f;
  for (int f = 0; f < kFrames; ++f) {
    const float left = aStart + (float)f * kWinFrac;
    pool->positions()[0] = {cx, cy};
    pool->rotations()[0] = 0;
    pool->scales()[0] = 1.0f;  // u* = X0*ppi  =>  s = 1
    pool->tints()[0] = {1, 1, 1, 1};
    pool->frames()[0] = 0;
    pool->sizes()[0] = {1, 1};
    pool->texWindows()[0] = SkRect::MakeXYWH(left, 0, kWinFrac, 1.0f);
    pool->commit();

    Element acc =
        box()
            .width(Dim((float)boxW))
            .height(Dim((float)boxH))
            .child(instancing::instances(one, pool, instancing::Mode::Data));
    sk_sp<SkPicture> pic = snapshot(box().child(std::move(acc)), fonts);
    SkBitmap bm;
    if (!pic || !bm.tryAllocN32Pixels(boxW, boxH)) return;
    {
      SkCanvas c(bm);
      c.clear(SK_ColorBLACK);
      c.drawPicture(pic);
    }
    const int x0 = (int)std::lround(cx - (float)cw * 0.5f);
    const int y0 = (int)std::lround(cy - (float)chh * 0.5f);
    for (int y = 0; y < chh; ++y)
      for (int x = 0; x < cw; ++x) {
        const SkColor s = bm.getColor(x0 + x, y0 + y);
        const float g = (float)SkColorGetG(s) / 255.0f;
        const float fx = left + ((float)x + 0.5f) / (float)cw * kWinFrac;
        const int sx = std::clamp((int)(fx * (float)S.w), 0, S.w - 1);
        const int sy = std::clamp(
            (int)(((float)y + 0.5f) / (float)chh * (float)S.h), 0, S.h - 1);
        const float wv =
            (float)S.lum[(size_t)sy * (size_t)S.w + (size_t)sx] / 255.0f;
        sg += g;
        sw += wv;
        sgg += g * g;
        sww += wv * wv;
        sgw += g * wv;
        worst = std::max(worst, std::fabs(g - wv));
        if (std::fabs(g - wv) > 0.5f) ++bad;
        ++n;
      }
  }
  if (!n) return;
  const double mg = sg / (double)n, mw = sw / (double)n;
  const double cgg = sgg / (double)n - mg * mg;
  const double cww = sww / (double)n - mw * mw;
  const double cgw = sgw / (double)n - mg * mw;
  rtMaxErr = worst;
  rtCorr = (cgg > 0 && cww > 0) ? (float)(cgw / std::sqrt(cgg * cww)) : 0.0f;
  rtMismatch = (float)bad / (float)n;
  rtCols = kFrames * cw;
}
