/** @file
 * The built-in executor — every step on the CPU, emitting onto the
 * canvas — and the Runtime value that carries it.
 */

#include "sigilgeometry/mesh/render/Runtime.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <include/core/SkVertices.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>
#include <numeric>

#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/mesh/render/Painter.h"

namespace sigil::geometry::mesh::render {

// The camera parameter every draw call carries shares the feature's
// name, so the seam is pulled in here rather than spelled through the
// namespace it hides.
using camera::toSkM44;

namespace {

/** Upper-left 3x3 of @p m, inverse-transposed — the normal matrix. A
 *  non-uniform scale tilts a surface and its normal the opposite way, so
 *  the plain matrix would leave every normal off the surface it
 *  describes.
 *
 *  The determinant is tested before the inverse is taken: glm's ends
 *  with an unguarded divide by it, so a placement that collapsed an axis
 *  would come back as infinities. A basis with no volume has no normal
 *  transform, and the identity is what leaves a normal alone. */
glm::mat3 normalMatrix(const glm::mat4& m) {
  const glm::mat3 basis(m);
  if (std::abs(glm::determinant(basis)) < 1e-12f) return glm::mat3(1.0f);
  return glm::inverseTranspose(basis);
}

/** A shaded colour as this executor carries it between shading and
 *  emission: STRAIGHT FLOAT, so a primitive lane multiplies into it in
 *  the same domain the device painter multiplies in and the mesh's own
 *  `bakePrimColor` folds in. Quantising first and multiplying bytes
 *  afterwards is a different answer, and one lane must not mean two
 *  colours. */
glm::vec4 rgba(glm::vec3 rgb, float a) { return {rgb.x, rgb.y, rgb.z, a}; }

SkColor toColor(glm::vec3 rgb, float a) {
  const SkColor4f c = {
      std::clamp(rgb.x, 0.0f, 1.0f), std::clamp(rgb.y, 0.0f, 1.0f),
      std::clamp(rgb.z, 0.0f, 1.0f), std::clamp(a, 0.0f, 1.0f)};
  return c.toSkColor();
}

/** The built-in executor. It holds nothing, so every instance is the
 *  same value — which is what lets two default MeshStyles compare
 *  equal. */
struct CpuExecutor : Executor {
  // Stateless: every instance is the same value, which is what makes two
  // default styles compare equal. (A defaulted comparison cannot say so —
  // the abstract base it derives from has none.)
  bool operator==(const CpuExecutor&) const { return true; }

  void drawMesh(SkCanvas& canvas, const Mesh& mesh, const glm::mat4& model,
                const camera::Camera& camera, SkSize viewport,
                const MeshStyle& style) const override {
    const size_t n = mesh.vertexCount();
    if (n == 0 || mesh.indices.size() < 3) return;

    SkM44 viewModel = toSkM44(camera.view());
    viewModel.preConcat(toSkM44(model));
    SkM44 full = toSkM44(camera.viewProjection(viewport));
    full.preConcat(toSkM44(model));
    const glm::mat3 normalM = normalMatrix(camera.view() * model);
    const glm::mat3 lightM = normalMatrix(camera.view());
    // The shading is written in view space and an environment map is a
    // panorama of the WORLD, so a direction goes back out through the
    // inverse of the rotation that brought it in — which, for the
    // rotation part of a rigid placement, is its transpose.
    const glm::mat3 worldM = glm::transpose(lightM);
    const Environment& environment = style.environment;
    const bool sky = environment.valid();
    const float metal = std::clamp(style.metallic, 0.0f, 1.0f);
    const float rough = std::clamp(style.roughness, 0.0f, 1.0f);

    // Project + shade every vertex once.
    std::vector<SkPoint> screen(n);
    std::vector<float> viewZ(n);
    std::vector<glm::vec4> shaded(n);
    std::vector<bool> valid(n, true);
    const bool hasNormals = mesh.normals.size() == n;
    const bool hasUvs = mesh.uvs.size() == n;

    for (size_t i = 0; i < n; ++i) {
      const glm::vec3& p = mesh.positions[i];
      const SkV4 clip = full * SkV4{p.x, p.y, p.z, 1};
      if (clip.w <= 1e-4f) {
        valid[i] = false;
        screen[i] = {0, 0};
        viewZ[i] = 0;
      } else {
        screen[i] = {clip.x / clip.w, clip.y / clip.w};
        const SkV4 vp4 = viewModel * SkV4{p.x, p.y, p.z, 1};
        viewZ[i] = vp4.z;
      }

      switch (style.mode) {
        case MeshStyle::Mode::Normals: {
          const glm::vec3 nrm = hasNormals
                                    ? normalized(normalM * mesh.normals[i])
                                    : glm::vec3{0, 0, 1};
          // Materials.h G-buffer convention: DEVICE-space normals, +y down.
          shaded[i] = rgba(
              {nrm.x * 0.5f + 0.5f, -nrm.y * 0.5f + 0.5f, nrm.z * 0.5f + 0.5f},
              1);
          break;
        }
        case MeshStyle::Mode::Uv: {
          const glm::vec2 uv = hasUvs ? mesh.uvs[i] : glm::vec2{0, 0};
          shaded[i] = rgba({uv.x, uv.y, 0.5f}, 1);
          break;
        }
        case MeshStyle::Mode::Lit:
        default: {
          const SkV4 vp4 = viewModel * SkV4{p.x, p.y, p.z, 1};
          const glm::vec3 posView = {vp4.x, vp4.y, vp4.z};
          const glm::vec3 N = hasNormals
                                  ? normalized(normalM * mesh.normals[i])
                                  : glm::vec3{0, 0, 1};
          const glm::vec3 V = normalized(posView * -1.0f);
          glm::vec3 base = {style.baseColor.fR, style.baseColor.fG,
                            style.baseColor.fB};
          float alpha = style.baseColor.fA;
          if (i < mesh.colors.size()) {  // per-vertex tint lane (instancing)
            base = {base.x * mesh.colors[i].r, base.y * mesh.colors[i].g,
                    base.z * mesh.colors[i].b};
            alpha *= mesh.colors[i].a;
          }
          // A SURFACE THAT IS ITS OWN LIGHT stops here: its colour is
          // what it shows, with no ambient under it and no emitter,
          // specular or rim over it.
          if (!style.lit) {
            shaded[i] = rgba(base, alpha);
            break;
          }
          // WHAT A METAL IS: the light stops reaching the diffuse and
          // the highlight takes the surface's own colour. At metal zero
          // — every surface that says nothing — this is the arithmetic
          // that was already here, term for term.
          const glm::vec3 albedo = base * (1.0f - metal);
          const glm::vec3 f0 = specularColor(base, metal);
          const glm::vec3 highlight = glm::vec3(1.0f) + (base - glm::vec3(1.0f)) * metal;
          // THE AMBIENT TERM IS THE ENVIRONMENT where there is one: what
          // actually falls on a surface facing this way from every
          // direction, rather than one constant for the whole set.
          const glm::vec3 ambient =
              sky ? environmentIrradiance(environment, worldM * N)
                  : glm::vec3{style.ambient.fR, style.ambient.fG,
                              style.ambient.fB};
          glm::vec3 accum = albedo * ambient;
          for (const Light& light : style.lights) {
            const glm::vec3 L =
                normalized(lightM * (light.direction * -1.0f));
            const float diff = std::max(glm::dot(N, L), 0.0f);
            const glm::vec3 lc = {light.color.fR * light.intensity,
                                  light.color.fG * light.intensity,
                                  light.color.fB * light.intensity};
            accum += albedo * lc * diff;
            if (style.specular > 0 && diff > 0) {
              const glm::vec3 H = normalized(L + V);
              const float spec =
                  std::pow(std::max(glm::dot(N, H), 0.0f), style.shininess) *
                  style.specular;
              accum += lc * spec * highlight;
            }
          }
          // …and what the surface MIRRORS, off the reflected view
          // vector, at the level its roughness picks.
          if (sky) {
            const float nDotV = std::max(glm::dot(N, V), 0.0f);
            const glm::vec3 R = N * (2.0f * nDotV) - V;
            accum += environmentSpecular(
                environmentRadiance(environment, worldM * R, rough), f0,
                rough, nDotV);
          }
          if (style.rim > 0) {
            const float rim =
                std::pow(1.0f - std::max(glm::dot(N, V), 0.0f), 3.0f) *
                style.rim;
            accum += glm::vec3{rim, rim, rim};
          }
          // WHERE THE LIT SUM ENDS: a radiance, read at the set's
          // exposure and compressed onto what a display can hold. Only
          // here — a surface that is its own light returned above with
          // an authored colour, and the two buffer modes are not
          // pictures at all.
          shaded[i] = rgba(toneMap(accum, style.environment.exposure), alpha);
          break;
        }
      }
    }

    // The PRIMITIVE lane: one float4 per triangle, multiplied into that
    // triangle's emitted vertex colours. Lit mode only — the Normals and
    // Uv buffers must stay unmodulated.
    const std::vector<glm::vec4>* primColor =
        style.primColorLane.empty() || style.mode != MeshStyle::Mode::Lit
            ? nullptr
            : mesh.primIf(style.primColorLane);
    if (primColor && primColor->size() != mesh.triangleCount())
      primColor = nullptr;

    // Assemble triangles: near-plane reject, backface cull, depth sort.
    struct Tri {
      uint32_t i0, i1, i2;
      float depth;
      uint32_t index;  ///< primitive index, for the prim lanes
    };
    std::vector<Tri> tris;
    tris.reserve(mesh.indices.size() / 3);
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
      const uint32_t i0 = mesh.indices[t], i1 = mesh.indices[t + 1],
                     i2 = mesh.indices[t + 2];
      if (!valid[i0] || !valid[i1] || !valid[i2]) continue;
      if (style.backfaceCull) {
        const SkPoint a = screen[i0], b = screen[i1], c = screen[i2];
        const float area2 =
            (b.fX - a.fX) * (c.fY - a.fY) - (b.fY - a.fY) * (c.fX - a.fX);
        // Front faces wind CCW in y-up space, so they arrive CW (negative
        // area) after the viewport's y flip.
        if (area2 >= 0) continue;
      }
      tris.push_back({i0, i1, i2, (viewZ[i0] + viewZ[i1] + viewZ[i2]) / 3.0f,
                      (uint32_t)(t / 3)});
    }
    if (style.depthSort)
      std::sort(tris.begin(), tris.end(),
                [](const Tri& a, const Tri& b) { return a.depth < b.depth; });

    // Emit in chunks under the 16-bit SkVertices limit: three unshared
    // vertices per triangle keeps the chunking trivial.
    // NEAREST TAKES NO MIP LEVEL WITH IT: a map asked for hard texel
    // edges would get them back softened if two levels were blended
    // under the lookup, so the level is the image itself.
    const SkSamplingOptions sampling(style.filter,
                                     style.filter == SkFilterMode::kNearest
                                         ? SkMipmapMode::kNone
                                         : SkMipmapMode::kLinear);
    SkPaint paint;
    paint.setAntiAlias(true);
    const bool textured = style.texture && hasUvs;
    if (textured) {
      const SkTileMode tile =
          style.tileTexture ? SkTileMode::kRepeat : SkTileMode::kClamp;
      paint.setShader(style.texture->makeShader(tile, tile, sampling));
    }
    const float texW = textured ? (float)style.texture->width() : 1;
    const float texH = textured ? (float)style.texture->height() : 1;

    const size_t maxTrisPerChunk = 65535 / 3;
    for (size_t start = 0; start < tris.size(); start += maxTrisPerChunk) {
      const size_t count = std::min(maxTrisPerChunk, tris.size() - start);
      std::vector<SkPoint> pos;
      std::vector<SkPoint> tex;
      std::vector<SkColor> col;
      pos.reserve(count * 3);
      col.reserve(count * 3);
      if (textured) tex.reserve(count * 3);
      for (size_t k = 0; k < count; ++k) {
        const Tri& tri = tris[start + k];
        const glm::vec4 flat =
            primColor ? (*primColor)[tri.index] : glm::vec4{1, 1, 1, 1};
        for (uint32_t idx : {tri.i0, tri.i1, tri.i2}) {
          pos.push_back(screen[idx]);
          const glm::vec4 c = primColor ? shaded[idx] * flat : shaded[idx];
          col.push_back(toColor({c.x, c.y, c.z}, c.w));
          if (textured) {
            const SkPoint uv =
                style.uvTransform.mapPoint({mesh.uvs[idx].x, mesh.uvs[idx].y});
            tex.push_back({uv.fX * texW, uv.fY * texH});
          }
        }
      }
      sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
          SkVertices::kTriangles_VertexMode, (int)pos.size(), pos.data(),
          textured ? tex.data() : nullptr, col.data(), 0, nullptr);
      canvas.drawVertices(vertices,
                          textured ? SkBlendMode::kModulate : SkBlendMode::kDst,
                          paint);
    }
  }

  void drawPanel(SkCanvas& canvas, const glm::mat4& model,
                 const camera::Camera& camera, SkSize viewport,
                 const std::function<void(SkCanvas&)>& draw) const override {
    canvas.save();
    SkM44 full = toSkM44(camera.viewProjection(viewport));
    full.preConcat(toSkM44(model));
    // Panel-local drawing keeps Skia's y-down convention; the flip makes
    // local content upright in the y-up world.
    full.preConcat(SkM44::Scale(1, -1, 1));
    canvas.concat(full);
    draw(canvas);
    canvas.restore();
  }
};

}  // namespace

Runtime Runtime::cpu() {
  static const Runtime kCpu{CpuExecutor{}};
  return kCpu;
}

}  // namespace sigil::geometry::mesh::render
