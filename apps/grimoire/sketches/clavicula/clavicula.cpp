/** A drawn grimoire plate: a cross within two interlocking squares,
 *  concentric divine names, a zodiac limb and light moving over gilding. */
// TAGS: Studies/Manuscripts, Geometry/Diagrams, Typography/Lettering,
// Materials/Metal, Patterns/Ornament

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr float kWidth = 1300, kHeight = 1600;
constexpr glm::vec2 kCentre{650, 802};
constexpr float kPi = 3.14159265359f;
const auto kInk = material::hexColor(0x332819);
const auto kRed = material::hexColor(0x963D29);
const auto kGold = material::hexColor(0xAE8232);
const auto kFaint = material::hexColor(0x8F7954, 0.48f);

glm::vec2 polar(float radius, float degrees, glm::vec2 centre = kCentre) {
  const float angle = degrees * kPi / 180.0f;
  return centre + radius * glm::vec2(std::cos(angle), std::sin(angle));
}

Text roman(std::string words, float size, material::Color ink = kInk) {
  return text(std::move(words))
      .font({.face = weave::ports::face({"Baskerville", "Georgia"}),
             .size = size})
      .ink(ink);
}

Text italic(std::string words, float size, material::Color ink = kInk) {
  return text(std::move(words))
      .font({.face = weave::ports::face({"Baskerville", "Georgia"}, 400,
                                        weave::FaceSlant::Italic),
             .size = size})
      .ink(ink);
}

Text script(std::string words, float size, material::Color ink = kInk) {
  return text(std::move(words))
      .font({.face = weave::ports::face({"Snell Roundhand", "Baskerville"}),
             .size = size})
      .ink(ink);
}

Element centered(Text node, float x, float y, float width, float height) {
  return kit::at(std::move(node).textAlign(weave::TextAlignment::kCenter),
                 x - width * 0.5f, y, width, height);
}

Element ring(glm::vec2 c, float radius, float weight,
             material::Material color) {
  return kit::at(c.x - radius, c.y - radius, 2 * radius, 2 * radius)
      .shape(geometry::shapes::circle())
      .fill(Fill::none())
      .stroke(stroke(weight, Fill::fromMaterial(color)));
}

Element inscription(std::string words, float radius, float angle, float size,
                    material::Color color = kInk) {
  return kit::at(roman(std::move(words), size, color)
                     .font({.track = 1.7f})
                     .textOnPath({.path = geometry::shapes::circle(),
                                  .at = angle / 360.0f,
                                  .align = TextPath::Align::Center,
                                  .exactTangent = true}),
                 kCentre.x - radius, kCentre.y - radius, 2 * radius,
                 2 * radius);
}

// One retained outline holds the fine rules, rather than one leaf per mark.
struct Rules {
  std::ostringstream path;

  void line(glm::vec2 a, glm::vec2 b) {
    path << 'M' << a.x << ' ' << a.y << 'L' << b.x << ' ' << b.y;
  }

  void cross(glm::vec2 c, float size, float angle = 0) {
    line(polar(size, angle, c), polar(size, angle + 180, c));
    line(polar(size, angle + 90, c), polar(size, angle + 270, c));
  }

  void polygon(float radius, int count, float phase, glm::vec2 c = kCentre) {
    auto first = polar(radius, phase, c);
    path << 'M' << first.x << ' ' << first.y;
    for (int i = 1; i < count; ++i) {
      auto p = polar(radius, phase + i * 360.0f / count, c);
      path << 'L' << p.x << ' ' << p.y;
    }
    path << 'Z';
  }

  Element draw(float weight, material::Material color) const {
    return kit::at(0, 0, kWidth, kHeight)
        .shape(heldPath(geometry::path::Outline::svg(path.str())))
        .fill(Fill::none())
        .stroke(stroke(weight, Fill::fromMaterial(color)));
  }
};

// The stepped cross is drawn as one closed contour, including all six ends.
const char* kCross =
    "M-130 -54H-100V-16H-28V-78H28V-16H100V-54H130V54"
    "H100V16H28V78H-28V16H-100V54H-130Z";

Element centralCross(material::Color color, bool outline = false) {
  auto node = kit::at(520, 724, 260, 156).shape(geometry::shapes::svg(kCross));
  return outline
             ? node.fill(Fill::none()).stroke(stroke(2.4f, Fill::color(color)))
             : node.fill(color);
}

void smallCircle(std::vector<Element>& page, glm::vec2 c) {
  Rules figure;
  figure.polygon(60, 4, 45, c);
  figure.cross(c, 13);
  page.push_back(figure.draw(1.2f, kInk));
  page.push_back(ring(c, 42, 1.1f, kInk));
  page.push_back(ring(c, 32, 0.8f, kRed));
  page.push_back(centered(italic("agla", 12), c.x, c.y - 39, 75, 20));
  page.push_back(centered(italic("adonay", 12), c.x, c.y + 23, 75, 20));
}
}  // namespace

struct Clavicula {
  std::shared_ptr<TextureScene> lightScene;
  Element lightField, fixedPage, trace;
  geometry::path::Outline gildedRules;
  motion::Animatable<float> direction = motion::animatable(125.0f);
  motion::Animatable<float> elevation = motion::animatable(47.0f);
  motion::Animatable<float> traceStart = motion::animatable(0.0f);
  motion::Animatable<float> traceEnd = motion::animatable(0.06f);

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(material::hexColor(0x161C1C));
    ctx.captureAt(5.0);
    const auto paper =
        material::shader(ctx.assets.hub(), ctx.local("vellum.sksl"))
            .effects(material::Filter::shadow({0, 0, 0, 0.45f},
                                              {.blur = 14, .offset = {6, 8}}));
    std::vector<Element> page{
        kit::at(32, 24, 1236, 1552)
            .shape(geometry::shapes::svg(
                "M9 0L342 4L631 0L932 6L1228 2L1236 284L1232 741"
                "L1236 1224L1225 1550L846 1547L489 1552L6 1545L0 1086"
                "L5 622L0 233Z"))
            .fill(paper),
        centered(script("La Clavicule", 87), 650, 77, 950, 115),
        centered(
            roman("SALOMONIS · PENTACVLVM MAGNVM", 22).font({.track = 3.8f}),
            650, 180, 1010, 42),
        centered(italic("Les figures, les noms & les caractères", 23, kRed),
                 650, 225, 750, 39),
        kit::at(italic("Fig. I", 19), 89, 73, 100, 30),
        kit::at(italic("126", 20), 1170, 73, 65, 30),
    };

    Rules border, fine, heavy, gilt;
    border.path << "M67 282V67H239M1061 67H1233V282"
                   "M67 1315V1533H239M1061 1533H1233V1315";
    border.path << "M76 271V76H224M1076 76H1224V271"
                   "M76 1326V1524H224M1076 1524H1224V1326";
    page.push_back(border.draw(0.8f, kGold));
    for (int i = 0; i < 7; ++i) {
      const float y = 135 + i * 213.0f;
      page.push_back(kit::at(47, y, 3.5f, 6.5f)
                         .shape(geometry::shapes::circle())
                         .fill(material::hexColor(0x5B492C, 0.5f)));
    }
    smallCircle(page, {161, 239});
    smallCircle(page, {1139, 239});
    page.push_back(centered(italic("le cercle", 17), 161, 299, 140, 28));
    page.push_back(centered(italic("les noms", 17), 1139, 299, 140, 28));

    // The zodiac title ring surrounds the independent grand-pentacle figure.
    for (const float r : {532.0f, 524.0f, 503.0f, 452.0f, 439.0f, 396.0f})
      page.push_back(ring(kCentre, r, r == 503 ? 2.2f : 0.85f, kInk));
    page.push_back(ring(kCentre, 516, 2.0f, kGold));
    page.push_back(ring(kCentre, 443, 1.2f, kRed));
    for (int i = 0; i < 144; ++i) {
      const float angle = i * 2.5f - 90;
      fine.line(polar(i % 12 == 0 ? 503 : 510, angle), polar(524, angle));
    }
    const std::array<const char*, 12> zodiac{
        "♈", "♉", "♊", "♋", "♌", "♍", "♎", "♏", "♐", "♑", "♒", "♓"};
    const std::array<const char*, 12> signs{
        "ARIES", "TAVRVS",  "GEMINI",      "CANCER",      "LEO",      "VIRGO",
        "LIBRA", "SCORPIO", "SAGITTARIVS", "CAPRICORNVS", "AQVARIVS", "PISCES"};
    for (int i = 0; i < 12; ++i) {
      const float angle = i * 30 - 90.0f;
      fine.line(polar(452, angle + 15), polar(503, angle + 15));
      page.push_back(kit::at(
          text(zodiac[i])
              .font({.face = weave::ports::face({"Apple Symbols", "Symbol"}),
                     .size = 39})
              .ink(i % 3 == 0 ? kRed : kInk)
              .textOnPath({.path = geometry::shapes::circle(),
                           .at = angle / 360.0f,
                           .align = TextPath::Align::Center,
                           .exactTangent = true}),
          177, 329, 946, 946));
      page.push_back(inscription(signs[i], 458, angle, 9.5f, kRed));
    }
    const std::array<const char*, 4> names{"TETRAGRAMMATON", "ADONAY", "AGLA",
                                           "ELOHIM"};
    for (int i = 0; i < 4; ++i) {
      const float angle = i * 90 - 90.0f;
      page.push_back(inscription(names[i], 412, angle, 25));
      gilt.cross(polar(419, angle + 45), 7, angle);
    }

    // Each square ends at four of the eight cross-tipped gates.
    heavy.polygon(347, 4, -90);
    heavy.polygon(347, 4, -45);
    fine.polygon(335, 4, -90);
    fine.polygon(335, 4, -45);
    for (int i = 0; i < 8; ++i) {
      const float angle = i * 45 - 90.0f;
      heavy.line(polar(347, angle), polar(374, angle));
      heavy.cross(polar(367, angle), 8.0f, angle);
      gilt.cross(polar(367, angle), 5.0f, angle);
      const auto point = polar(385, angle);
      page.push_back(centered(italic(i % 2 == 0 ? "adonay" : "agla", 15, kRed),
                              point.x, point.y - 9, 86, 23)
                         .rotate(angle + 90));
      page.push_back(
          inscription(i % 2 == 0 ? "ADONAY" : "ORIEL", 284, angle, 20, kInk));
      fine.line(polar(272, angle + 22.5f), polar(308, angle + 22.5f));
      gilt.cross(polar(291, angle + 22.5f), 3.5f, angle);
    }
    page.push_back(ring(kCentre, 245, 2.2f, kInk));
    page.push_back(ring(kCentre, 237, 0.8f, kGold));
    page.push_back(ring(kCentre, 198, 1.8f, kInk));
    page.push_back(ring(kCentre, 188, 0.7f, kRed));
    for (int i = 0; i < 4; ++i) {
      const float angle = i * 90 - 90.0f;
      page.push_back(
          inscription(i % 2 == 0 ? "ALPHA" : "OMEGA", 211, angle, 24, kInk));
      gilt.cross(polar(215, angle + 45), 5.5f, angle);
    }

    page.push_back(centralCross(kGold).fill(
        material::from(kGold)
            .layer(material::noise(0.7f, {.seed = 126, .grain = true}),
                   {.blend = material::BlendMode::SoftLight, .opacity = 0.18f})
            .effects(material::Filter::bevel(
                {.depth = 1.8f,
                 .size = 1.6f,
                 .highlight = material::hexColor(0xFAE5A4, 0.8f),
                 .shadow = material::hexColor(0x49301A, 0.65f)}))));
    page.push_back(centralCross(kInk, true));
    page.push_back(centered(italic("Principium", 18, kRed), 650, 678, 270, 28));
    page.push_back(centered(italic("et finis", 18, kRed), 650, 900, 240, 28));
    fine.line({530, 717}, {770, 717});
    fine.line({530, 893}, {770, 893});
    for (int i = 0; i < 8; ++i) {
      const float angle = -90 + i * 45.0f;
      fine.line(polar(542, angle), polar(559, angle));
      fine.cross(polar(555, angle), 4.0f, angle);
    }

    page.push_back(fine.draw(0.75f, kFaint));
    page.push_back(heavy.draw(2.5f, kInk));
    page.push_back(gilt.draw(1.8f, kGold));
    page.push_back(centered(italic("Oriens", 21, kRed), 650, 251, 160, 34));
    page.push_back(centered(italic("Occidens", 21, kRed), 650, 1350, 180, 34));
    page.push_back(
        centered(italic("Septentrio", 21, kRed), 71, 785, 190, 36).rotate(-90));
    page.push_back(
        centered(italic("Meridies", 21, kRed), 1227, 785, 190, 36).rotate(90));

    // Printed glosses are readable type; the larger hand supplies the folio's
    // voice.
    page.push_back(
        kit::at(script("Du Pentacle", 46, kRed), 107, 1386, 362, 65));
    page.push_back(kit::at(
        italic("Deux quarrés, huit pointes,\nle cercle & les noms divins.", 22),
        111, 1452, 420, 68));
    page.push_back(kit::at(italic("Les caractères demeurent inscrits ;\nla "
                                  "lumière seule parcourt la figure.",
                                  22),
                           745, 1423, 445, 77));
    page.push_back(
        centered(roman("LA CLEF DE SALOMON  ·  ÉTUDE D’APRÈS MS. 4659", 11)
                     .font({.track = 1.5f}),
                 650, 1543, 990, 23));

    // A compass-like marginal figure and pen flourishes complete the new plate.
    Rules ornament;
    const glm::vec2 rose{650, 1455};
    for (int i = 0; i < 8; ++i) {
      const float angle = i * 45.0f;
      const float radius = i % 2 == 0 ? 45.0f : 32.0f;
      ornament.line(polar(radius, angle, rose), polar(10, angle + 35, rose));
      ornament.line(polar(radius, angle, rose), polar(10, angle - 35, rose));
    }
    page.push_back(ornament.draw(1.0f, kRed));
    page.push_back(ring(rose, 50, 0.6f, kGold));
    page.push_back(ring(rose, 7, 1.2f, kInk));
    Rules curls;
    curls.path
        << "M475 1455C513 1422 524 1469 549 1455C565 1445 551 1434 540 1446"
           "M470 1462C505 1495 521 1454 550 1466";
    page.push_back(curls.draw(0.8f, kGold));

    fixedPage = positioned()
                    .width(kWidth)
                    .height(kHeight)
                    .children(std::move(page))
                    .cache(Cache::Texture);
    auto normal = ctx.textureScene({130, 160});
    normal->render(box().width(130).height(160).fill(
        material::shader(ctx.assets.hub(), ctx.local("gold_normal.sksl"))));
    const auto gold =
        material::from(material::hexColor(0xC59B44))
            .surface({.metallic = 1.0f,
                      .roughness = 0.4f,
                      .normal = material::image(normal->texture().source())});
    lightScene = ctx.textureScene({130, 160});
    lightField = box().width(130).height(160).fill(gold).lighting(
        material::studio({.direction = direction,
                          .elevation = elevation,
                          .color = material::Color{1, 0.96f, 0.83f, 1},
                          .intensity = 0.9f,
                          .ambient = 0.75f}));
    lightScene->render(lightField);
    gildedRules = geometry::path::Outline::svg(gilt.path.str());
    Rules traced;
    traced.polygon(347, 4, -90);
    traced.polygon(347, 4, -45);
    trace = traced.draw(2.2f, material::hexColor(0xD4AA4B))
                .mask(by::spans(spans::wrap(traceStart, traceEnd)));
    ctx.composer.render(describe());
  }

  Element describe() {
    // Taking the scene's texture after rendering carries its current revision.
    // Its UV matrix places the small light field in page coordinates, directly
    // on the native contours without a page-sized coverage layer.
    glm::mat3 pageScale(1.0f);
    pageScale[0][0] = 10.0f;
    pageScale[1][1] = 10.0f;
    auto light = lightScene->texture();
    auto reflection = material::image(light.source());
    reflection.slot("image", light.uv(pageScale));
    reflection = material::from(material::Color{0, 0, 0, 0})
                     .layer(reflection, {.opacity = 0.66f})
                     .worldSpace();
    auto liveGold = positioned().width(kWidth).height(kHeight).children(
        {ring(kCentre, 516, 2.0f, reflection),
         ring(kCentre, 237, 0.8f, reflection),
         centralCross(kGold).fill(reflection),
         kit::at(0, 0, kWidth, kHeight)
             .shape(heldPath(gildedRules))
             .fill(Fill::none())
             .stroke(stroke(1.8f, Fill::fromMaterial(reflection)))});
    return positioned().width(kWidth).height(kHeight).children({
        fixedPage,
        liveGold,
        trace,
    });
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const float t = float(elapsed);
    direction = 115.0f + 80.0f * std::sin(t * 0.42f);
    elevation = 38.0f + 20.0f * std::sin(t * 0.42f + 0.8f);
    traceStart = t / 18.0f;
    traceEnd = t / 18.0f + 0.065f;
    lightScene->render(lightField, elapsed);
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(Clavicula, "Study · Esoteric",
             "A grand pentacle after Wellcome MS 4659: interlocking squares, "
             "eight gates, zodiac lettering and moving gold-leaf illumination")
