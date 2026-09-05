/** @file
 * over_under — local variation is a composition of materials, never a
 * bespoke recipe per pair.
 *
 * `over(base, top, mask, blend)` returns a MATERIAL. The three operands
 * become its children, so the stack compares, animates and resolves as
 * one value, and applying `over` again builds a taller one. `under(m)`
 * steps one down and `stackDepth(m)` counts the steps, which is what a
 * consumer that can only express one material writes down.
 *
 * A MASK is an ordinary material whose red channel, clamped, says how
 * much of the top shows. Two shapes cover every source: a CONSTANT mask
 * is a number, and a SAMPLED one reads its `source` slot as a channel,
 * as a SLOPE (a tangent normal dotted with an axis — moss on the faces
 * that point up), or as a HEIGHT (the value dotted with an axis, no
 * decode — a tide line, dust on the top shelf). Both then FIT: `low` and
 * `high` remap the raw value onto 0..1 and clamp, and `invert` flips it.
 * A slope or a height mask means nothing without a fit, which is why the
 * factories take the range.
 *
 * The base is stone and the top is sheet brass, both generated rather
 * than photographed, so the whole sheet is a function of the numbers in
 * this file.
 *
 * EDIT THESE FIRST
 *   kLow, kHigh — the fit both sampled masks are read through.
 *   kBevel — the shoulder the slope mask's normals are derived from.
 */

#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 622};
constexpr float kCell = 200;
constexpr float kPicture = 176;

constexpr float kLow = 0.32f;  // the fit both sampled masks are read through
constexpr float kHigh = 0.70f;
constexpr float kBevel = 26;  // the shoulder the slope normals come from

/** The plate every cell paints: one rounded octagon, so the slope mask's
 *  bevel has real corners to shade and the brass has an edge to catch. */
SkPath plate() {
  static const SkPath path = shapes::rounded(shapes::chamfered(30), 8)
                                 .path({kCell - 36, kPicture - 36})
                                 .makeTransform(SkMatrix::Translate(18, 18));
  return path;
}

/** A diagonal black-to-white ramp, baked once — the painted map every
 *  sampled mask on this sheet reads, so the four readings differ only in
 *  how they read it and never in what they were given. */
material::Texture ramp() {
  return material::Texture::produce("over_under.ramp", [] {
    constexpr int kSide = 128;
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
    const SkPoint ends[2] = {{0, 0}, {kSide, kSide}};
    const SkColor4f stops[2] = {{0, 0, 0, 1}, {1, 1, 1, 1}};
    SkPaint paint;
    paint.setShader(SkShaders::LinearGradient(
        ends, SkGradient({{stops, 2}, {}, SkTileMode::kClamp}, {})));
    surface->getCanvas()->drawPaint(paint);
    return surface->makeImageSnapshot();
  });
}

/** The map placed over the plate's own box, so a mask reading it lines
 *  up with the shape it is masking. */
material::Texture placedRamp() {
  material::Texture map = ramp();
  map.uv(SkMatrix::Scale((kCell - 36) / 128.0f, (kPicture - 36) / 128.0f)
             .postTranslate(18, 18));
  return map;
}

material::Material stone() {
  return material::kit::stone({.hi = {0.62f, 0.63f, 0.66f, 1},
                               .lo = {0.34f, 0.35f, 0.39f, 1},
                               .bedAngle = 18,
                               .bedLength = 44,
                               .speckle = 0.4f,
                               .speckleAlpha = 0.3f});
}

material::Material brass() {
  return material::kit::latten({.from = {18, 18},
                                .to = {kCell - 18, kPicture - 18},
                                .level = 0.55f,
                                .sheen = 0.45f,
                                .patina = 0.12f});
}

Element cell(const char* call, const std::string& note,
             material::Material paint) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture},
          custom(call, [paint = std::move(paint)](SkCanvas& canvas,
                                                  const PaintContext& pc) {
            material::skia::fill(
                canvas, plate(), paint,
                {.resolution = {pc.size.width(), pc.size.height()}});
          })));
}

}  // namespace

struct OverUnder final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    material::skia::install();  // the SkSL compiler, once per process

    const material::Material mixed =
        material::over(stone(), brass(), material::kit::maskMap(placedRamp()));
    const material::Material twice = material::over(
        mixed, material::kit::board({.paint = {0.10f, 0.11f, 0.13f, 1}}),
        material::kit::maskConstant(0.35f), material::Blend::Multiply);

    ctx.composer
        .render(
            sketch::kit::page({.title = toU8("OVER AND UNDER \xc2\xb7 "
                                             "over(base, top, mask, "
                                             "blend) and the mask family"),
                               .subtitle = toU8(
                                   "dials \xc2\xb7 the mask kind \xc2\xb7 the "
                                   "fit "
                                   "(0.32 to 0.70) \xc2\xb7 the blend \xc2\xb7 "
                                   "the bevel the slope normals come from (26 "
                                   "px)"),
                               .footer = toU8("kit::maskVertexColor reads a "
                                              "painted colour "
                                              "lane the same way maskMap reads "
                                              "an image "
                                              "\xe2\x80\x94 the renderer "
                                              "supplies the texture, "
                                              "and everything after it on this "
                                              "sheet is the "
                                              "same fit and the same invert")},
                              kit::cells({.cells =
                                              {
                                                  kit::cells({.cells = {cell("k"
                                                                             "i"
                                                                             "t"
                                                                             ":"
                                                                             ":"
                                                                             "s"
                                                                             "t"
                                                                             "o"
                                                                             "n"
                                                                             "e"
                                                                             "("
                                                                             "\xe2\x80\xa6)",
                                                                             "t"
                                                                             "h"
                                                                             "e"
                                                                             " "
                                                                             "B"
                                                                             "A"
                                                                             "S"
                                                                             "E"
                                                                             ":"
                                                                             " "
                                                                             "a"
                                                                             " "
                                                                             "g"
                                                                             "e"
                                                                             "n"
                                                                             "e"
                                                                             "r"
                                                                             "a"
                                                                             "t"
                                                                             "e"
                                                                             "d"
                                                                             " "
                                                                             "b"
                                                                             "e"
                                                                             "d"
                                                                             ","
                                                                             " "
                                                                             "g"
                                                                             "r"
                                                                             "a"
                                                                             "i"
                                                                             "n"
                                                                             " "
                                                                             "a"
                                                                             "n"
                                                                             "d"
                                                                             " "
                                                                             "s"
                                                                             "p"
                                                                             "e"
                                                                             "c"
                                                                             "k"
                                                                             "l"
                                                                             "e"
                                                                             ","
                                                                             " "
                                                                             "n"
                                                                             "o"
                                                                             " "
                                                                             "t"
                                                                             "e"
                                                                             "x"
                                                                             "t"
                                                                             "u"
                                                                             "r"
                                                                             "e"
                                                                             " "
                                                                             "a"
                                                                             "t"
                                                                             " "
                                                                             "a"
                                                                             "l"
                                                                             "l",
                                                                             stone()),
                                                                        cell("k"
                                                                             "i"
                                                                             "t"
                                                                             ":"
                                                                             ":"
                                                                             "l"
                                                                             "a"
                                                                             "t"
                                                                             "t"
                                                                             "e"
                                                                             "n"
                                                                             "("
                                                                             "\xe2\x80\xa6)",
                                                                             "t"
                                                                             "h"
                                                                             "e"
                                                                             " "
                                                                             "T"
                                                                             "O"
                                                                             "P"
                                                                             ":"
                                                                             " "
                                                                             "s"
                                                                             "h"
                                                                             "e"
                                                                             "e"
                                                                             "t"
                                                                             " "
                                                                             "b"
                                                                             "r"
                                                                             "a"
                                                                             "s"
                                                                             "s"
                                                                             ","
                                                                             " "
                                                                             "o"
                                                                             "n"
                                                                             "e"
                                                                             " "
                                                                             "c"
                                                                             "o"
                                                                             "l"
                                                                             "o"
                                                                             "u"
                                                                             "r"
                                                                             " "
                                                                             "a"
                                                                             "n"
                                                                             "d"
                                                                             " "
                                                                             "a"
                                                                             " "
                                                                             "l"
                                                                             "a"
                                                                             "d"
                                                                             "d"
                                                                             "e"
                                                                             "r"
                                                                             " "
                                                                             "o"
                                                                             "f"
                                                                             " "
                                                                             "l"
                                                                             "i"
                                                                             "g"
                                                                             "h"
                                                                             "t"
                                                                             "s"
                                                                             " "
                                                                             "u"
                                                                             "n"
                                                                             "d"
                                                                             "e"
                                                                             "r"
                                                                             " "
                                                                             "a"
                                                                             " "
                                                                             "s"
                                                                             "h"
                                                                             "e"
                                                                             "e"
                                                                             "n",
                                                                             brass()),
                                                                        cell(
                                                                            "ov"
                                                                            "er"
                                                                            "("
                                                                            "\xe2\x80\xa6, maskConstant(0.35))",
                                                                            "a "
                                                                            "ma"
                                                                            "sk"
                                                                            " t"
                                                                            "ha"
                                                                            "t "
                                                                            "is"
                                                                            " t"
                                                                            "he"
                                                                            " s"
                                                                            "am"
                                                                            "e "
                                                                            "ev"
                                                                            "er"
                                                                            "yw"
                                                                            "he"
                                                                            "re"
                                                                            " "
                                                                            "\xc2\xb7 the whole plate at 35% brass",
                                                                            material::over(
                                                                                stone(),
                                                                                brass(),
                                                                                material::
                                                                                    kit::maskConstant(
                                                                                        0.35f))),
                                                                        cell(
                                                                            "ov"
                                                                            "er"
                                                                            "("
                                                                            "\xe2\x80\xa6, maskMap(ramp))",
                                                                            "on"
                                                                            "e "
                                                                            "CH"
                                                                            "AN"
                                                                            "NE"
                                                                            "L "
                                                                            "of"
                                                                            " a"
                                                                            " p"
                                                                            "ai"
                                                                            "nt"
                                                                            "ed"
                                                                            " m"
                                                                            "ap"
                                                                            " "
                                                                            "\xc2\xb7 "
                                                                            "it"
                                                                            "s "
                                                                            "ow"
                                                                            "n "
                                                                            "uv"
                                                                            " p"
                                                                            "la"
                                                                            "ce"
                                                                            "me"
                                                                            "nt"
                                                                            " d"
                                                                            "ec"
                                                                            "id"
                                                                            "es"
                                                                            " w"
                                                                            "he"
                                                                            "re"
                                                                            " "
                                                                            "ea"
                                                                            "ch"
                                                                            " t"
                                                                            "ex"
                                                                            "el"
                                                                            " l"
                                                                            "an"
                                                                            "d"
                                                                            "s",
                                                                            mixed),
                                                                        cell(
                                                                            "in"
                                                                            "ve"
                                                                            "rt"
                                                                            "(m"
                                                                            "as"
                                                                            "kM"
                                                                            "ap"
                                                                            "(r"
                                                                            "am"
                                                                            "p)"
                                                                            ")",
                                                                            "th"
                                                                            "e "
                                                                            "sa"
                                                                            "me"
                                                                            " m"
                                                                            "ap"
                                                                            " w"
                                                                            "it"
                                                                            "h "
                                                                            "it"
                                                                            "s "
                                                                            "an"
                                                                            "sw"
                                                                            "er"
                                                                            " f"
                                                                            "li"
                                                                            "pp"
                                                                            "ed"
                                                                            " "
                                                                            "\xc2\xb7 brass where the ramp is dark",
                                                                            material::over(
                                                                                stone(),
                                                                                brass(),
                                                                                material::kit::invert(
                                                                                    material::kit::maskMap(
                                                                                        placedRamp()))))},
                                                              .gap = 12}),
                                                  kit::cells({.cells = {cell("f"
                                                                             "i"
                                                                             "t"
                                                                             "("
                                                                             "m"
                                                                             "a"
                                                                             "s"
                                                                             "k"
                                                                             "M"
                                                                             "a"
                                                                             "p"
                                                                             "("
                                                                             "r"
                                                                             "a"
                                                                             "m"
                                                                             "p"
                                                                             ")"
                                                                             ","
                                                                             " "
                                                                             "0"
                                                                             "."
                                                                             "3"
                                                                             "2"
                                                                             ","
                                                                             " "
                                                                             "0"
                                                                             "."
                                                                             "7"
                                                                             "0"
                                                                             ")",
                                                                             "t"
                                                                             "h"
                                                                             "e"
                                                                             " "
                                                                             "r"
                                                                             "a"
                                                                             "w"
                                                                             " "
                                                                             "r"
                                                                             "a"
                                                                             "n"
                                                                             "g"
                                                                             "e"
                                                                             " "
                                                                             "t"
                                                                             "h"
                                                                             "a"
                                                                             "t"
                                                                             " "
                                                                             "m"
                                                                             "a"
                                                                             "p"
                                                                             "s"
                                                                             " "
                                                                             "o"
                                                                             "n"
                                                                             "t"
                                                                             "o"
                                                                             " "
                                                                             "0"
                                                                             "."
                                                                             "."
                                                                             "1"
                                                                             " "
                                                                             "m"
                                                                             "o"
                                                                             "v"
                                                                             "e"
                                                                             "d"
                                                                             " "
                                                                             "\xc2\xb7 the transition "
                                                                             "n"
                                                                             "a"
                                                                             "r"
                                                                             "r"
                                                                             "o"
                                                                             "w"
                                                                             "s"
                                                                             " "
                                                                             "t"
                                                                             "o"
                                                                             " "
                                                                             "t"
                                                                             "h"
                                                                             "a"
                                                                             "t"
                                                                             " "
                                                                             "b"
                                                                             "a"
                                                                             "n"
                                                                             "d",
                                                                             material::over(
                                                                                 stone(),
                                                                                 brass(),
                                                                                 material::kit::fit(
                                                                                     material::kit::maskMap(placedRamp()), kLow,
                                                                                     kHigh))),
                                                                        cell("m"
                                                                             "a"
                                                                             "s"
                                                                             "k"
                                                                             "S"
                                                                             "l"
                                                                             "o"
                                                                             "p"
                                                                             "e"
                                                                             "("
                                                                             "b"
                                                                             "e"
                                                                             "v"
                                                                             "e"
                                                                             "l"
                                                                             "N"
                                                                             "o"
                                                                             "r"
                                                                             "m"
                                                                             "a"
                                                                             "l"
                                                                             "s"
                                                                             "("
                                                                             "p"
                                                                             "l"
                                                                             "a"
                                                                             "t"
                                                                             "e"
                                                                             ","
                                                                             " "
                                                                             "2"
                                                                             "6"
                                                                             ")"
                                                                             ")",
                                                                             "d"
                                                                             "o"
                                                                             "t"
                                                                             "("
                                                                             "N"
                                                                             ","
                                                                             " "
                                                                             "u"
                                                                             "p"
                                                                             ")"
                                                                             " "
                                                                             "f"
                                                                             "i"
                                                                             "t"
                                                                             "t"
                                                                             "e"
                                                                             "d"
                                                                             " "
                                                                             "\xc2\xb7 brass on "
                                                                             "t"
                                                                             "h"
                                                                             "e"
                                                                             " "
                                                                             "s"
                                                                             "h"
                                                                             "o"
                                                                             "u"
                                                                             "l"
                                                                             "d"
                                                                             "e"
                                                                             "r"
                                                                             " "
                                                                             "t"
                                                                             "h"
                                                                             "a"
                                                                             "t"
                                                                             " "
                                                                             "f"
                                                                             "a"
                                                                             "c"
                                                                             "e"
                                                                             "s"
                                                                             " "
                                                                             "t"
                                                                             "h"
                                                                             "e"
                                                                             " "
                                                                             "l"
                                                                             "i"
                                                                             "g"
                                                                             "h"
                                                                             "t"
                                                                             ","
                                                                             " "
                                                                             "s"
                                                                             "t"
                                                                             "o"
                                                                             "n"
                                                                             "e"
                                                                             " "
                                                                             "o"
                                                                             "n"
                                                                             " "
                                                                             "t"
                                                                             "h"
                                                                             "e"
                                                                             " "
                                                                             "o"
                                                                             "n"
                                                                             "e"
                                                                             " "
                                                                             "t"
                                                                             "h"
                                                                             "a"
                                                                             "t"
                                                                             " "
                                                                             "t"
                                                                             "u"
                                                                             "r"
                                                                             "n"
                                                                             "s"
                                                                             " "
                                                                             "a"
                                                                             "w"
                                                                             "a"
                                                                             "y",
                                                                             material::over(
                                                                                 stone(),
                                                                                 brass(),
                                                                                 material::
                                                                                     kit::maskSlope(
                                                                                         material::
                                                                                             bevelNormals(
                                                                                                 plate(),
                                                                                                 kBevel),
                                                                                         {0, -1, 0}, 0.05f, 0.55f))),
                                                                        cell("m"
                                                                             "a"
                                                                             "s"
                                                                             "k"
                                                                             "H"
                                                                             "e"
                                                                             "i"
                                                                             "g"
                                                                             "h"
                                                                             "t"
                                                                             "("
                                                                             "r"
                                                                             "a"
                                                                             "m"
                                                                             "p"
                                                                             ","
                                                                             " "
                                                                             "0"
                                                                             "."
                                                                             "3"
                                                                             "2"
                                                                             ","
                                                                             " "
                                                                             "0"
                                                                             "."
                                                                             "7"
                                                                             "0"
                                                                             ")",
                                                                             "t"
                                                                             "h"
                                                                             "e"
                                                                             " "
                                                                             "s"
                                                                             "a"
                                                                             "m"
                                                                             "e"
                                                                             " "
                                                                             "m"
                                                                             "a"
                                                                             "p"
                                                                             " "
                                                                             "r"
                                                                             "e"
                                                                             "a"
                                                                             "d"
                                                                             " "
                                                                             "w"
                                                                             "i"
                                                                             "t"
                                                                             "h"
                                                                             " "
                                                                             "N"
                                                                             "O"
                                                                             " "
                                                                             "t"
                                                                             "a"
                                                                             "n"
                                                                             "g"
                                                                             "e"
                                                                             "n"
                                                                             "t"
                                                                             " "
                                                                             "d"
                                                                             "e"
                                                                             "c"
                                                                             "o"
                                                                             "d"
                                                                             "e"
                                                                             " "
                                                                             "\xc2\xb7 a value dotted with an "
                                                                             "a"
                                                                             "x"
                                                                             "i"
                                                                             "s"
                                                                             ","
                                                                             " "
                                                                             "w"
                                                                             "h"
                                                                             "i"
                                                                             "c"
                                                                             "h"
                                                                             " "
                                                                             "i"
                                                                             "s"
                                                                             " "
                                                                             "w"
                                                                             "h"
                                                                             "a"
                                                                             "t"
                                                                             " "
                                                                             "a"
                                                                             " "
                                                                             "t"
                                                                             "i"
                                                                             "d"
                                                                             "e"
                                                                             " "
                                                                             "l"
                                                                             "i"
                                                                             "n"
                                                                             "e"
                                                                             " "
                                                                             "i"
                                                                             "s",
                                                                             material::over(
                                                                                 stone(),
                                                                                 brass(),
                                                                                 material::
                                                                                     kit::maskHeight(
                                                                                         placedRamp(),
                                                                                         kLow,
                                                                                         kHigh,
                                                                                         {0,
                                                                                          1,
                                                                                          0}))),
                                                                        cell(
                                                                            "ov"
                                                                            "er"
                                                                            "("
                                                                            "\xe2\x80\xa6, Blend::Add)",
                                                                            "th"
                                                                            "e "
                                                                            "to"
                                                                            "p "
                                                                            "AD"
                                                                            "DS"
                                                                            ", "
                                                                            "sc"
                                                                            "al"
                                                                            "ed"
                                                                            " b"
                                                                            "y "
                                                                            "th"
                                                                            "e "
                                                                            "ma"
                                                                            "sk"
                                                                            " "
                                                                            "\xc2\xb7 one recipe per blend, so no "
                                                                            "bo"
                                                                            "dy"
                                                                            " c"
                                                                            "ar"
                                                                            "ri"
                                                                            "es"
                                                                            " a"
                                                                            " b"
                                                                            "ra"
                                                                            "nc"
                                                                            "h",
                                                                            material::over(
                                                                                stone(),
                                                                                brass(),
                                                                                material::kit::maskMap(placedRamp()), material::Blend::Add)),
                                                                        cell(
                                                                            "ov"
                                                                            "er"
                                                                            "(o"
                                                                            "ve"
                                                                            "r("
                                                                            "\xe2\x80\xa6), \xe2\x80\xa6"
                                                                            ", "
                                                                            "Mu"
                                                                            "lt"
                                                                            "ip"
                                                                            "ly"
                                                                            ")",
                                                                            kit::format(
                                                                                "a stack over a stack \xc2\xb7 "
                                                                                "stackDepth %d, and under() walks "
                                                                                "back down to the stone",
                                                                                material::stackDepth(
                                                                                    twice)),
                                                                            twice)},
                                                              .gap = 12})},
                                          .column = true,
                                          .gap = 16})));
  }
};

SIGIL_SKETCH(OverUnder, "Kit \xc2\xb7 API",
             "brass stacked over stone through every kind of mask the kit "
             "ships, then through each blend, and finally a stack over a "
             "stack")
