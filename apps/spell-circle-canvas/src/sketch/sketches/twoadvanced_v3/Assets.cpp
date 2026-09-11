// Production images, cloud grading and embedded Rive resources.

#include "TwoAdvancedV3.h"

bool TwoAdvancedV3::available(std::string* why) {
  return sketch::requireCached(
      {"https://v3.2advanced.com/V3ExpansionsReboot/assets/background.gif",
       "https://v3.2advanced.com/V3ExpansionsReboot/assets/images/"
       "social-icons@2x.png",
       "https://v3.2advanced.com/V3ExpansionsReboot/assets/images/"
       "2a-logo@2x.png",
       "https://v3.2advanced.com/v3expansionsreboot/mainstage.riv"},
      why);
}

mskia::Paint TwoAdvancedV3::stretchFill(const ImagePtr& asset, float w, float h,
                                        SkTileMode tx, SkTileMode ty) {
  const sk_sp<SkImage>& img = asset->frames()[0].image;
  return mskia::Paint::image(
      img, tx, ty,
      SkMatrix::Scale(w / (float)img->width(), h / (float)img->height()),
      SkSamplingOptions(SkFilterMode::kLinear));
}

sk_sp<SkImageFilter> TwoAdvancedV3::buildCloudLook() {
  const float sat = 0.60f, gain = 0.90f;
  const float lr = 0.2126f * (1 - sat), lg = 0.7152f * (1 - sat),
              lb = 0.0722f * (1 - sat);
  const float m[20] = {gain * (lr + sat),
                       gain * lg,
                       gain * lb,
                       0,
                       0,
                       gain * lr,
                       gain * (lg + sat),
                       gain * lb,
                       0,
                       0,
                       gain * lr,
                       gain * lg,
                       gain * (lb + sat),
                       0,
                       0,
                       0,
                       0,
                       0,
                       1,
                       0};
  return SkImageFilters::ColorFilter(SkColorFilters::Matrix(m), nullptr);
}

void TwoAdvancedV3::buildGapMask() {
  static const SkPoint kGap[] = {
      {448, 62},  {700, 92},  {688, 125}, {658, 172}, {628, 215}, {588, 252},
      {548, 258}, {512, 238}, {490, 196}, {468, 145}, {450, 95},
  };
  SkBitmap bm;
  if (!bm.tryAllocPixels(SkImageInfo::MakeN32Premul(1277, 385))) return;
  bm.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(bm);
  SkPathBuilder b;
  b.moveTo(kGap[0].fX, kGap[0].fY);
  for (size_t i = 1; i < std::size(kGap); ++i) b.lineTo(kGap[i].fX, kGap[i].fY);
  b.close();
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorWHITE);
  paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 4.5f));
  canvas.drawPath(b.detach(), paint);
  bm.setImmutable();
  gapMask = bm.asImage();
}

void TwoAdvancedV3::extractRivImages(const std::vector<std::byte>& bytes) {
  clouds.assign(62, nullptr);
  discordSeq.assign(102, nullptr);

  for (const sigil::image::EmbeddedImage& found :
       sigil::image::embeddedPngs(bytes)) {
    const std::string& name = found.name;
    auto want = [&](const char* w) { return name == w; };
    ImagePtr* dest = nullptr;
    if (want("home-background"))
      dest = &homeBg;
    else if (want("ui-top-header"))
      dest = &topHeader;
    else if (want("ui-navbar-background"))
      dest = &navbarBg;
    else if (want("ui-lower-panel-bg"))
      dest = &lowerPanelBg;
    else if (want("rive-R-logo"))
      dest = &riveLogo;
    else if (want("ddd-logo"))
      dest = &dddLogo;
    else if (sectionFor(name) >= 0)
      dest = &sectionBg[(size_t)sectionFor(name)];
    else if (name.rfind("discord icon ", 0) == 0) {
      int idx = 0;
      for (size_t p = 13; p < name.size() && idx >= 0; ++p)
        idx = (name[p] >= '0' && name[p] <= '9') ? idx * 10 + (name[p] - '0')
                                                 : -1;
      if (idx >= 1 && idx <= 102) dest = &discordSeq[(size_t)(idx - 1)];
    } else if (name.rfind("Cloud Seq", 0) == 0 && name.size() == 11) {
      const int idx = (name[9] - '0') * 10 + (name[10] - '0');
      if (idx >= 0 && idx < 62) dest = &clouds[(size_t)idx];
    }
    if (dest && !*dest) {
      if (auto img = sigil::image::ImageAsset::decode(
              SkData::MakeWithCopy(bytes.data() + found.offset, found.length)))
        *dest = std::make_shared<sigil::image::ImageAsset>(std::move(*img));
    }
  }
  // Sequences are used dense-or-not-at-all: one absent frame would
  // strobe, so a partial extraction drops the whole loop and the use
  // site falls back to its static form.
  for (const ImagePtr& f : clouds)
    if (!f) {
      clouds.clear();
      break;
    }
  for (const ImagePtr& f : discordSeq)
    if (!f) {
      discordSeq.clear();
      break;
    }
}
