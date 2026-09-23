/** @file
 * slang_portable — a whole module compiled while the library runs, and
 * the layout the compiler reports for it.
 *
 * A material's body exists only as a value in memory, so its program is
 * assembled and compiled at RUN time: a renderer's scaffold text, the
 * recipe's generated declarations, the recipe's body, and one entry
 * point that calls it. What the build compiles on its own is the
 * scaffold, which is what makes a mistake there a build failure and a
 * mistake in a body a diagnostic.
 *
 * NOTHING GUESSES A LAYOUT. Every offset below is the one the compiler
 * reported for the program it just built, so a body that declares one
 * more parameter moves nothing a renderer has to be told about. A matrix
 * is rows at a STRIDE and an array is elements at one, which is why
 * writing either is one copy per row rather than one copy of the whole.
 *
 * Every session carries two modules by name, so a shader's `import`
 * resolves in memory and nothing is looked for on disk. `Portable` is
 * the subset whose transcendentals a host and a device answer alike — a
 * kernel compiled for both cannot afford two spellings of a square root
 * — and `Shading` is the material kit's own terms, so a renderer's
 * shading and every body compiled beside it call one definition.
 *
 * EDIT THESE FIRST
 *   kModule — the Slang source. Change a uniform and watch the offsets
 *             below move without anything here being told about it.
 *   kLit    — the one axis a session specialises on (defines SIGIL_LIT).
 */

// TAGS: Materials/Shaders

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cstring>
#include <glm/mat4x4.hpp>
#include <string>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace slang = sigil::material::slang;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1360};
constexpr float kCell = 500;
constexpr float kPicture = 260;
constexpr bool kLit = false;  // defines SIGIL_LIT in the session

constexpr material::Color kFigure{0.60f, 0.88f, 0.72f, 1};
constexpr material::Color kFault{0.96f, 0.52f, 0.46f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.10f, 0.105f, 0.125f, 1};
  return look;
}

/** THE MODULE: imports resolved in memory, one uniform buffer holding a
 *  matrix, a vector and an array, one sampled slot, and the two stages
 *  that read them. */
constexpr const char* kModule = R"SLANG(
import Portable;
import Shading;

struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

uniform float4x4 uModel;
uniform float4 uTint;
uniform float4 uStops[3];
uniform Texture2D uMap;
uniform SamplerState uMapSampler;

[shader("vertex")]
VSOut vsCover(uint id : SV_VertexID) {
  VSOut out;
  out.position = mul(uModel, float4(sqrtP(float(id)), 0, 0, 1));
  out.uv = float2(0, 0);
  return out;
}

[shader("fragment")]
float4 fsCover(VSOut input) : SV_Target {
  float k = lambert(float3(0, 0, 1), float3(0, 0, 1));
  return uTint * uStops[1] * k * uMap.Sample(uMapSampler, input.uv);
}
)SLANG";

/** The scaffold a renderer hands the compiler when it compiles a MATERIAL
 *  body: the two stages, and a fragment that calls the body's own
 *  `surface(uv)`. */
constexpr const char* kScaffold = R"SLANG(
struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VSOut vsCover(uint id : SV_VertexID) {
  VSOut out;
  out.position = float4(float(id), 0, 0, 1);
  out.uv = float2(0.5, 0.5);
  return out;
}

[shader("fragment")]
float4 fsCover(VSOut input) : SV_Target { return surface(input.uv); }
)SLANG";

weave::TextStyle mono(float size, material::Color color) {
  const sk_sp<SkTypeface> face =
      weave::ports::face({"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** A readout cell: a block of monospaced text in the plate, which is
 *  what a layout and a diagnostic ARE. */
sketch::kit::ComparisonCase readout(const char* caseTitle, const char* call,
                                    const std::string& note,
                                    const std::string& body,
                                    material::Color colour = kFigure,
                                    float height = kPicture) {
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::cell(
              {.plate = {.width = kCell,
                         .height = height,
                         .padding = 16,
                         .paddingY = 10}},
              "", "", text(body, mono(10.5f, colour)).width(kCell - 32)),
          .note = note};
}

}  // namespace

struct SlangPortable {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    slang::Compiled built;
    std::string error;
    const bool ok = slang::compileModule(kModule, "vsCover", "fsCover", kLit,
                                         &built, &error);

    // THE LAYOUT, as the compiler reported it. Nothing here computes an
    // offset; every number is read back.
    std::string layout = kit::formatted(
        "stages   vertex %zu words · fragment %zu words\n"
        "buffer   %zu bytes\n"
        "textures %s\n\n"
        "name          offset  bytes  count  stride\n",
        built.vertex.size(), built.fragment.size(), built.uniformBytes,
        built.textures.empty() ? "none" : built.textures.front().c_str());
    for (const auto& [name, slot] : built.uniforms)
      layout +=
          kit::formatted("%-13s %6zu %6zu %6zu %7zu\n", name.c_str(),
                         slot.offset, slot.bytes, slot.count, slot.stride);

    // ONE DRAW'S BYTES, written at those offsets and read straight back
    // out of the buffer.
    slang::Uniforms values(built);
    glm::mat4 model(1.0f);
    model[3][0] = 5.0f;  // the translation in x, column-major as glm holds it
    values.set("uModel", model);
    values.set("uTint", 0.25f, 0.5f, 0.75f, 1.0f);
    const float stop[4] = {9, 8, 7, 6};
    values.setElement("uStops", 1, stop, 4);
    values.set("uNeverDeclared", 1, 2, 3, 4);  // a no-op, not a fault

    std::string bytes;
    const auto read4 = [&](const char* name, size_t extra) {
      const slang::UniformSlot* slot = built.uniform(name);
      if (!slot) return std::string("(not in the layout)");
      float v[4] = {0, 0, 0, 0};
      std::memcpy(v, values.bytes().data() + slot->offset + extra, sizeof v);
      return kit::formatted("%.2f %.2f %.2f %.2f", (double)v[0], (double)v[1],
                            (double)v[2], (double)v[3]);
    };
    const slang::UniformSlot* stops = built.uniform("uStops");
    bytes = kit::formatted(
        "uModel  row 0     %s\n"
        "  the shader reads ROWS, so what was\n"
        "  written is the transpose\n\n"
        "uTint             %s\n\n"
        "uStops  element 1 %s\n"
        "  at offset + stride (%zu), never at\n"
        "  offset + four floats\n\n"
        "uNeverDeclared    skipped, and the buffer\n"
        "  is still %zu bytes",
        read4("uModel", 0).c_str(), read4("uTint", 0).c_str(),
        read4("uStops", stops ? stops->stride : 0).c_str(),
        stops ? stops->stride : 0, values.bytes().size());

    // THE KIT'S OWN BODIES, compiled through the scaffold a renderer
    // hands them.
    std::string surfaces = "recipe    slang bytes  uniforms  result\n";
    for (const material::Recipe* recipe :
         {material::kit::stoneRecipe().get(),
          material::kit::timberRecipe().get(),
          material::kit::lattenRecipe().get(),
          material::kit::boardRecipe().get()}) {
      slang::Compiled surface;
      std::string why;
      const std::string source =
          recipe->source(material::Target::Slang) + kScaffold;
      const bool made = slang::compileModule(source, "vsCover", "fsCover", kLit,
                                             &surface, &why);
      surfaces += kit::formatted(
          "%-9s %11zu %9zu  %s\n", recipe->name().c_str(), surface.uniformBytes,
          surface.uniforms.size(), made ? "compiled" : "FAILED");
    }

    // THE TWO WAYS A COMPILE SAYS NO, asked the same way: what came back
    // is the compiler's own diagnostic and nothing of ours.
    const auto whyNot = [](const char* source, const char* fragment) {
      slang::Compiled refused;
      std::string why;
      slang::compileModule(source, "vsCover", fragment, kLit, &refused, &why);
      return why.empty() ? std::string("(no message)") : why;
    };
    const std::string missingWhy = whyNot(kModule, "fsNoSuchStage");
    const std::string garbageWhy = whyNot("this is not Slang", "fsCover");

    ctx.composer.render(sketch::kit::page(
        {.title = "The compiler defines the layout",
         .subtitle = "Inspect a linked program, then inspect the bytes written "
                     "for one draw",
         .footer = "Both stages link as one program. Their shared layout is "
                   "reflected, never inferred by the renderer."},
        box().column().gap(26).children(
            {sketch::kit::sectionHeader(
                 {.label = "SOURCE → LAYOUT → BYTES",
                  .note = kit::formatted("Module compiled: %s · SIGIL_LIT: %s",
                                         ok ? "yes" : "no",
                                         kLit ? "true" : "false")}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children({sketch::kit::comparison(
                                {.cases = {readout(
                                     "01  DECLARE THE PROGRAM", "the module",
                                     "The source names its uniforms, sampled "
                                     "slot and two stages.",
                                     std::string(kModule).substr(1),
                                     sketch::kit::theme().palette.ink, 614)},
                                 .measure = 500,
                                 .gap = 20}),
                            box().column().gap(24).children(
                                {sketch::kit::comparison(
                                     {.cases = {readout(
                                          "02  REFLECT ITS LAYOUT",
                                          "Compiled::uniforms",
                                          "Offsets, byte counts and strides "
                                          "are reported by the compiler.",
                                          layout)},
                                      .measure = 500,
                                      .gap = 20}),
                                 sketch::kit::comparison(
                                     {.cases = {readout(
                                          "03  WRITE A DRAW",
                                          "slang::Uniforms · one draw",
                                          "Writes use the reported layout; the "
                                          "readback below verifies the bytes.",
                                          bytes)},
                                      .measure = 500,
                                      .gap = 20})})}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "MATERIAL BODIES", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {readout(
                                    "COMPILE THROUGH A SCAFFOLD",
                                    "the kit's bodies through a "
                                    "scaffold",
                                    "The material recipes compile through the "
                                    "renderer scaffold.",
                                    surfaces, kFigure, 100)},
                                .measure = 500,
                                .gap = 20})}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "DIAGNOSTICS", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {readout(
                                    "MISSING ENTRY POINT",
                                    "a missing entry point",
                                    "The requested entry point is absent.",
                                    missingWhy, kFault, 112)},
                                .measure = 500,
                                .gap = 20}),
                           sketch::kit::comparison(
                               {.cases = {readout(
                                    "INVALID SOURCE",
                                    "source that is not Slang",
                                    "The first compiler diagnostic identifies "
                                    "where parsing fails.",
                                    garbageWhy.substr(
                                        0, garbageWhy.find("\nerror[")),
                                    kFault, 112)},
                                .measure = 500,
                                .gap = 20})})})})));
  }
};

SIGIL_SKETCH(SlangPortable, "Kit · API",
             "one module compiled at run time, the layout its compiler "
             "reported, a draw's bytes landing at those offsets, and the "
             "two ways a compile says no")
