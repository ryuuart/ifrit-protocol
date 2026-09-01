/** @file
 * The mesh painter on the device: one pipeline built from the painter
 * program, the style's fields written into its uniforms, and the pixels
 * read back onto the canvas the caller handed over.
 *
 * A PAINTER'S DRAW IS NOT A FRAME. It has no pass, no named resources
 * and no material: what it is drawn with is entirely the `MeshStyle`
 * beside it, which is why the three shading modes are a uniform here
 * rather than three programs. It opens and closes a device frame of its
 * own around each draw, because the uniform heap a draw writes into is
 * refilled once a frame and a run of draws that never finished one would
 * exhaust it.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkM44.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/diligent/Runtime.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <memory>
#include <utility>
#include <vector>

#include "Compile.h"
#include "Gpu.h"
#include "sigilworld/diligent/Painter.h"

namespace sigil::world::diligent {

namespace {

namespace render = ::sigil::geometry::mesh::render;
namespace camera = ::sigil::geometry::mesh::camera;

/** How many emitters one draw carries. It is the array the program
 *  declares; a style naming more is drawn with the first four, which is
 *  the same budget a frame's lights are gathered under. */
constexpr size_t kLights = 4;

/** THE EXECUTOR. Its device state is shared rather than held, so the
 *  value is copyable and two styles carrying copies of one runtime
 *  compare equal while two separately made runtimes do not — they hold
 *  separate targets, separate pipelines and separate uploads. */
class PainterExecutor : public render::Executor {
 public:
  explicit PainterExecutor(std::shared_ptr<Gpu> gpu) : m_gpu(std::move(gpu)) {}

  bool operator==(const PainterExecutor& other) const {
    return m_gpu == other.m_gpu;
  }

  void drawMesh(SkCanvas& canvas, const Mesh& mesh, const glm::mat4& model,
                const camera::Camera& cam, SkSize viewport,
                const render::MeshStyle& style) const override {
    const SkISize extent{(int)std::ceil(viewport.width()),
                         (int)std::ceil(viewport.height())};
    if (extent.isEmpty()) return;
    if (mesh.positions.empty() || mesh.indices.size() < 3) return;
    const Compiled& program = painterProgram();
    if (program.empty()) return;

    Gpu& gpu = *m_gpu;
    gpu.resize(extent);
    dg::ITexture* colour = gpu.working(0);
    if (!colour) return;
    gpu.beginFrame();

    // THE PRIMITIVE LANE makes the vertices unshared, so it is read at
    // upload rather than at the draw. Only the lit mode carries one: the
    // normal and uv buffers would be corrupted by a tint.
    const bool tinted = !style.primColorLane.empty() &&
                        style.mode == render::MeshStyle::Mode::Lit;
    const MeshBuffers* buffers =
        gpu.stream(mesh, tinted ? style.primColorLane : std::string_view{});
    if (!buffers) {
      gpu.endFrame();
      return;
    }

    const PipelineKey key{&program,
                          SkBlendMode::kSrcOver,
                          /*depth=*/true,
                          /*depthWrite=*/true,
                          /*fullscreen=*/false,
                          /*prim=*/true,
                          /*cull=*/style.backfaceCull};
    const Pipeline* pipeline = gpu.pipeline(key);
    if (!pipeline) {
      gpu.endFrame();
      return;
    }

    const float clear[4] = {0, 0, 0, 0};
    openTarget(gpu, colour, clear, /*withDepth=*/true);

    Uniforms uniforms(program);
    writeUniforms(uniforms, program, model, cam, extent, style);

    std::vector<dg::ITexture*> textures(program.textures.size(), nullptr);
    if (style.texture) {
      const material::Texture map = material::Texture::of(style.texture);
      if (dg::ITexture* sampled = gpu.sample(map))
        for (size_t i = 0; i < textures.size(); ++i)
          if (program.textures[i] == "uTexture") textures[i] = sampled;
    }

    dg::IDeviceContext* context = gpu.device->context();
    context->SetPipelineState(pipeline->state);
    bindAndCommit(gpu, *pipeline, program, uniforms, textures, style.filter,
                  style.tileTexture);
    dg::IBuffer* vertices = buffers->vertices;
    const dg::Uint64 offset = 0;
    context->SetVertexBuffers(0, 1, &vertices, &offset,
                              dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                              dg::SET_VERTEX_BUFFERS_FLAG_RESET);
    context->SetIndexBuffer(buffers->indices, 0,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    dg::DrawIndexedAttribs draw;
    draw.NumIndices = buffers->indexCount;
    draw.IndexType = dg::VT_UINT32;
    draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
    context->DrawIndexed(draw);

    const sk_sp<SkImage> painted = gpu.readTexture(colour);
    gpu.endFrame();
    if (!painted) return;
    // Onto the canvas as it stands: the image is premultiplied and the
    // untouched pixels are transparent, so what the canvas already
    // carries shows through exactly where nothing was drawn — which is
    // what emitting triangles onto it does.
    canvas.drawImage(painted, 0, 0);
  }

  void drawPanel(SkCanvas& canvas, const glm::mat4& model,
                 const camera::Camera& cam, SkSize viewport,
                 const std::function<void(SkCanvas&)>& draw) const override {
    // THE CANVAS'S OWN DRAW. A panel is the caller's 2D content under a
    // perspective transform, and that content is Skia's to rasterise: a
    // panel on a GPU-backed canvas is already on the GPU, and a device
    // that took it away and gave it back would only cost a crossing.
    canvas.save();
    SkM44 full = camera::toSkM44(cam.viewProjection(viewport));
    full.preConcat(camera::toSkM44(model));
    // Panel-local drawing keeps Skia's y-down convention; the flip makes
    // local content upright in the y-up world.
    full.preConcat(SkM44::Scale(1, -1, 1));
    canvas.concat(full);
    draw(canvas);
    canvas.restore();
  }

 private:
  /** Every field of the style the program reads, at the offsets the
   *  compiler reported for them. */
  static void writeUniforms(Uniforms& uniforms, const Compiled& program,
                            const glm::mat4& model, const camera::Camera& cam,
                            SkISize extent, const render::MeshStyle& style) {
    const glm::mat4 view = cam.view();
    const glm::mat4 modelView = view * model;
    uniforms.set("uViewProj", clipFor(cam, extent));
    uniforms.set("uModel", model);
    uniforms.set("uModelView", modelView);
    uniforms.set("uNormalMatrix",
                 glm::mat4(glm::inverseTranspose(glm::mat3(modelView))));
    uniforms.set("uLightMatrix",
                 glm::mat4(glm::inverseTranspose(glm::mat3(view))));
    uniforms.set("uBaseColor", style.baseColor.fR, style.baseColor.fG,
                 style.baseColor.fB, style.baseColor.fA);
    uniforms.set("uAmbient", style.ambient.fR, style.ambient.fG,
                 style.ambient.fB, style.ambient.fA);
    const float mode = style.mode == render::MeshStyle::Mode::Uv        ? 2.0f
                       : style.mode == render::MeshStyle::Mode::Normals ? 1.0f
                                                                        : 0.0f;
    uniforms.set("uMode", mode, style.lit ? 1.0f : 0.0f, 0.0f, 0.0f);
    uniforms.set("uTextureUv", mapMatrix(style.uvTransform));

    const size_t count = std::min(style.lights.size(), kLights);
    for (size_t i = 0; i < count; ++i) {
      const render::Light& light = style.lights[i];
      const float direction[4] = {light.direction.x, light.direction.y,
                                  light.direction.z, 0.0f};
      const float colour[4] = {light.color.fR * light.intensity,
                               light.color.fG * light.intensity,
                               light.color.fB * light.intensity, 1.0f};
      uniforms.setElement("uLightDir", i, direction, 4);
      uniforms.setElement("uLightColor", i, colour, 4);
    }
    uniforms.set("uShading", style.specular, style.shininess, style.rim,
                 (float)count);
    (void)program;
  }

  std::shared_ptr<Gpu> m_gpu;
};

}  // namespace

render::Runtime painterRuntime(Device& device) {
  installSlangCompiler();
  return render::Runtime{PainterExecutor{makeGpu(device)}};
}

}  // namespace sigil::world::diligent
