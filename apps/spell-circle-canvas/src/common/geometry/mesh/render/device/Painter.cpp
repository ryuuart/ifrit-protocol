/** @file
 * The mesh painter on the device: one program compiled once for the
 * process, one pipeline built from it, the style's fields written into
 * its uniforms, and the pixels read back onto the canvas the caller
 * handed over.
 *
 * A PAINTER'S DRAW IS NOT A FRAME. It has no pass, no named resources
 * and no material: what it is drawn with is entirely the `MeshStyle`
 * beside it, which is why the three shading modes are a uniform here
 * rather than three programs. It opens and closes a device frame of its
 * own around each draw, because the uniform heap a draw writes into is
 * refilled once a frame and a run of draws that never finished one would
 * exhaust it.
 */

#include "sigilgeometry/mesh/render/device/Painter.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkM44.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilshaders/GeometryMeshRenderDevice.h>

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Meshes.h"
#include "Pipelines.h"
#include "Resources.h"
#include "Textures.h"

namespace sigil::geometry::mesh::render {

namespace {

namespace dg = Diligent;
namespace device = ::sigil::geometry::device;

/** How many emitters one draw carries: the array the program declares,
 *  which is the number this executor honours. A style naming more is
 *  drawn with the first eight. */
constexpr size_t kLights = 8;

/** THE PAINTER'S PROGRAM, compiled once for the process.
 *
 *  Unspecialised: the painter's modes and its "does light reach this"
 *  answer are fields of a style, which is a value a caller changes
 *  between two draws, so specialising on one would compile a program per
 *  draw rather than per process. Empty when it failed to compile, which
 *  is reported once. */
const material::slang::Compiled& painterProgram() {
  static const material::slang::Compiled built = [] {
    material::slang::Compiled program;
    std::string error;
    if (!material::slang::compileModule(shaderSource("Painter.slang"),
                                        "vsPaint", "fsPaint", /*lit=*/false,
                                        &program, &error))
      material::reportOnce("geometry.mesh.render.device.painter",
                           "the mesh painter did not compile: " + error);
    return program;
  }();
  return built;
}

/** A texture's placement as a shader reads it: the same matrix a style
 *  carries, in the four-by-four the uniform is. */
glm::mat4 mapMatrix(const SkMatrix& uv) {
  glm::mat4 out(1.0f);
  out[0] = {uv.getScaleX(), uv.getSkewY(), 0.0f, 0.0f};
  out[1] = {uv.getSkewX(), uv.getScaleY(), 0.0f, 0.0f};
  out[3] = {uv.getTranslateX(), uv.getTranslateY(), 0.0f, 1.0f};
  return out;
}

/** WHAT THE EXECUTOR HOLDS BETWEEN DRAWS, over the device's own shared
 *  resources: the one colour target and depth buffer a draw rasterises
 *  into, sized to whatever viewport it was last asked for, and the
 *  residencies and pipelines every executor on this device shares.
 *
 *  A painter names no resources, so there is nothing here to key by
 *  name: one target, remade when the viewport changes. */
struct PainterState {
  explicit PainterState(device::Device& d)
      : device(&d), shared(d), meshes(d), maps(d), pipelines(d) {}
  ~PainterState() {
    // Everything below borrows the device's queue; nothing may be
    // recording when the objects behind it go.
    if (device && device->context()) device->context()->Flush();
  }

  device::Device* device = nullptr;
  device::Resources shared;
  device::MeshResidency meshes;
  device::TextureResidency maps;
  device::PipelineCache pipelines;
  SkISize extent{0, 0};
  dg::RefCntAutoPtr<dg::ITexture> colour;
  dg::RefCntAutoPtr<dg::ITexture> depth;

  /** The colour target and depth buffer at @p size, made on the first
   *  ask and remade when the size changes. Null when the device refused
   *  either. */
  dg::ITexture* target(SkISize size) {
    if (size != extent) {
      extent = size;
      colour.Release();
      depth.Release();
    }
    if (extent.isEmpty() || !device->renderDevice()) return nullptr;
    if (!colour) {
      dg::TextureDesc desc;
      desc.Name = "painted mesh";
      desc.Type = dg::RESOURCE_DIM_TEX_2D;
      desc.Width = (dg::Uint32)extent.width();
      desc.Height = (dg::Uint32)extent.height();
      desc.Format = device::kColorFormat;
      desc.BindFlags = dg::BIND_RENDER_TARGET | dg::BIND_SHADER_RESOURCE;
      desc.Usage = dg::USAGE_DEFAULT;
      device->renderDevice()->CreateTexture(desc, nullptr, &colour);
    }
    if (!depth) {
      dg::TextureDesc desc;
      desc.Name = "painted mesh depth";
      desc.Type = dg::RESOURCE_DIM_TEX_2D;
      desc.Width = (dg::Uint32)extent.width();
      desc.Height = (dg::Uint32)extent.height();
      desc.Format = device::kDepthFormat;
      desc.BindFlags = dg::BIND_DEPTH_STENCIL;
      desc.Usage = dg::USAGE_DEFAULT;
      device->renderDevice()->CreateTexture(desc, nullptr, &depth);
    }
    return colour;
  }

  /** Closes the draw's own device frame: the uniform heap is refilled
   *  once a frame, and what the residencies hold is aged on the same
   *  beat. */
  void endFrame() {
    if (dg::IDeviceContext* context = device->context()) {
      context->Flush();
      context->FinishFrame();
    }
    meshes.endFrame();
    maps.endFrame();
  }
};

/** THE EXECUTOR. Its device state is shared rather than held, so the
 *  value is copyable and two styles carrying copies of one runtime
 *  compare equal while two separately made runtimes do not — they hold
 *  separate targets, separate pipelines and separate uploads. */
class PainterExecutor : public Executor {
 public:
  explicit PainterExecutor(std::shared_ptr<PainterState> state)
      : m_state(std::move(state)) {}

  bool operator==(const PainterExecutor& other) const {
    return m_state == other.m_state;
  }

  void drawMesh(SkCanvas& canvas, const Mesh& mesh, const glm::mat4& model,
                const camera::Camera& cam, SkSize viewport,
                const MeshStyle& style) const override {
    const SkISize extent{(int)std::ceil(viewport.width()),
                         (int)std::ceil(viewport.height())};
    if (extent.isEmpty()) return;
    if (mesh.positions.empty() || mesh.indices.size() < 3) return;
    const material::slang::Compiled& program = painterProgram();
    if (program.empty()) return;

    PainterState& state = *m_state;
    dg::ITexture* colour = state.target(extent);
    if (!colour || !state.depth) return;

    // THE PRIMITIVE LANE makes the vertices unshared, so it is read at
    // upload rather than at the draw. Only the lit mode carries one: the
    // normal and uv buffers would be corrupted by a tint.
    const bool tinted = !style.primColorLane.empty() &&
                        style.mode == MeshStyle::Mode::Lit;
    const device::MeshBuffers* buffers = state.meshes.stream(
        mesh, tinted ? style.primColorLane : std::string_view{});
    if (!buffers) {
      state.endFrame();
      return;
    }

    const device::PipelineKey key{&program,
                                  SkBlendMode::kSrcOver,
                                  /*depth=*/true,
                                  /*depthWrite=*/true,
                                  /*fullscreen=*/false,
                                  /*prim=*/true,
                                  /*cull=*/style.backfaceCull};
    const device::Pipeline* pipeline = state.pipelines.pipeline(key);
    if (!pipeline) {
      state.endFrame();
      return;
    }

    const float clear[4] = {0, 0, 0, 0};
    device::openTarget(*state.device, colour, state.depth, clear);

    material::slang::Uniforms uniforms(program);
    writeUniforms(uniforms, model, cam, extent, style);

    std::vector<dg::ITexture*> textures(program.textures.size(), nullptr);
    if (style.texture) {
      const material::Texture map = material::Texture::of(style.texture);
      if (dg::ITexture* sampled = state.maps.sample(map))
        for (size_t i = 0; i < textures.size(); ++i)
          if (program.textures[i] == "uTexture") textures[i] = sampled;
    }

    dg::IDeviceContext* context = state.device->context();
    context->SetPipelineState(pipeline->state);
    device::bindDraw(state.shared, *pipeline, program, uniforms, textures,
                     style.filter, style.tileTexture);
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

    const sk_sp<SkImage> painted = state.shared.read(colour);
    state.endFrame();
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
  static void writeUniforms(material::slang::Uniforms& uniforms,
                            const glm::mat4& model, const camera::Camera& cam,
                            SkISize extent, const MeshStyle& style) {
    const glm::mat4 view = cam.view();
    const glm::mat4 modelView = view * model;
    uniforms.set("uViewProj", cam.clipProjection(extent));
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
    const float mode = style.mode == MeshStyle::Mode::Uv        ? 2.0f
                       : style.mode == MeshStyle::Mode::Normals ? 1.0f
                                                                : 0.0f;
    uniforms.set("uMode", mode, style.lit ? 1.0f : 0.0f, 0.0f, 0.0f);
    uniforms.set("uTextureUv", mapMatrix(style.uvTransform));

    const size_t count = std::min(style.lights.size(), kLights);
    for (size_t i = 0; i < count; ++i) {
      const Light& light = style.lights[i];
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
    // The exposure the lit sum is read at, which stands whether or not
    // the style carries a panorama.
    uniforms.set("uTone", style.environment.exposure, 0.0f, 0.0f, 0.0f);
  }

  std::shared_ptr<PainterState> m_state;
};

}  // namespace

Runtime deviceRuntime(device::Device& device) {
  return Runtime{PainterExecutor{std::make_shared<PainterState>(device)}};
}

}  // namespace sigil::geometry::mesh::render
