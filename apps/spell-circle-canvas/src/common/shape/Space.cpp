#include "sigilshape/Space.h"

#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <include/core/SkVertices.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace sigil::shape::space {

namespace {

constexpr float kDegToRad = (float)M_PI / 180.0f;

SkV3 normalized(SkV3 v, SkV3 fallback = {0, 0, 1}) {
  const float len = v.length();
  return len < 1e-12f ? fallback : v * (1.0f / len);
}

/** Upper-left 3x3 of @p m, inverse-transposed — the normal matrix. */
struct Mat3 {
  float m[9];

  SkV3 apply(SkV3 v) const {
    return {m[0] * v.x + m[1] * v.y + m[2] * v.z,
            m[3] * v.x + m[4] * v.y + m[5] * v.z,
            m[6] * v.x + m[7] * v.y + m[8] * v.z};
  }
};

Mat3 normalMatrix(const SkM44 &src) {
  const float a00 = src.rc(0, 0), a01 = src.rc(0, 1), a02 = src.rc(0, 2);
  const float a10 = src.rc(1, 0), a11 = src.rc(1, 1), a12 = src.rc(1, 2);
  const float a20 = src.rc(2, 0), a21 = src.rc(2, 1), a22 = src.rc(2, 2);
  const float det = a00 * (a11 * a22 - a12 * a21) -
                    a01 * (a10 * a22 - a12 * a20) +
                    a02 * (a10 * a21 - a11 * a20);
  Mat3 out{{1, 0, 0, 0, 1, 0, 0, 0, 1}};
  if (std::abs(det) < 1e-12f)
    return out;
  const float inv = 1.0f / det;
  // rows of the inverse transpose = columns of the inverse
  out.m[0] = (a11 * a22 - a12 * a21) * inv;
  out.m[1] = (a12 * a20 - a10 * a22) * inv;
  out.m[2] = (a10 * a21 - a11 * a20) * inv;
  out.m[3] = (a02 * a21 - a01 * a22) * inv;
  out.m[4] = (a00 * a22 - a02 * a20) * inv;
  out.m[5] = (a01 * a20 - a00 * a21) * inv;
  out.m[6] = (a01 * a12 - a02 * a11) * inv;
  out.m[7] = (a02 * a10 - a00 * a12) * inv;
  out.m[8] = (a00 * a11 - a01 * a10) * inv;
  return out;
}

SkColor toColor(SkV3 rgb, float a) {
  const SkColor4f c = {std::clamp(rgb.x, 0.0f, 1.0f),
                       std::clamp(rgb.y, 0.0f, 1.0f),
                       std::clamp(rgb.z, 0.0f, 1.0f),
                       std::clamp(a, 0.0f, 1.0f)};
  return c.toSkColor();
}

} // namespace

SkM44 Camera::view() const {
  return SkM44::LookAt(eye, target, up);
}

SkM44 Camera::projection(float aspect) const {
  SkM44 m = SkM44::Perspective(zNear, zFar, fovYDeg * kDegToRad);
  if (aspect > 0)
    m.setRC(0, 0, m.rc(0, 0) / aspect);
  return m;
}

SkM44 Camera::viewProjection(SkSize viewport) const {
  const float w = viewport.width(), h = viewport.height();
  const float aspect = h > 0 ? w / h : 1;
  // NDC -> pixels, y flipped back to Skia's y-down.
  SkM44 vp = SkM44::Translate(w * 0.5f, h * 0.5f, 0);
  vp.preScale(w * 0.5f, -h * 0.5f, 1);
  SkM44 out = vp;
  out.preConcat(projection(aspect));
  out.preConcat(view());
  return out;
}

void drawMesh(SkCanvas &canvas, const Mesh &mesh, const SkM44 &model,
              const Camera &camera, SkSize viewport,
              const MeshStyle &style) {
  const size_t n = mesh.vertexCount();
  if (n == 0 || mesh.indices.size() < 3)
    return;

  SkM44 viewModel = camera.view();
  viewModel.preConcat(model);
  SkM44 full = camera.viewProjection(viewport);
  full.preConcat(model);
  const Mat3 normalM = normalMatrix(viewModel);
  const Mat3 lightM = normalMatrix(camera.view());

  // Project + shade every vertex once.
  std::vector<SkPoint> screen(n);
  std::vector<float> viewZ(n);
  std::vector<SkColor> shaded(n);
  std::vector<bool> valid(n, true);
  const bool hasNormals = mesh.normals.size() == n;
  const bool hasUvs = mesh.uvs.size() == n;

  for (size_t i = 0; i < n; ++i) {
    const SkV3 &p = mesh.positions[i];
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
      const SkV3 nrm = hasNormals
                           ? normalized(normalM.apply(mesh.normals[i]))
                           : SkV3{0, 0, 1};
      // Materials.h G-buffer convention: DEVICE-space normals, +y down.
      shaded[i] = toColor(
          {nrm.x * 0.5f + 0.5f, -nrm.y * 0.5f + 0.5f, nrm.z * 0.5f + 0.5f},
          1);
      break;
    }
    case MeshStyle::Mode::Uv: {
      const SkPoint uv = hasUvs ? mesh.uvs[i] : SkPoint{0, 0};
      shaded[i] = toColor({uv.fX, uv.fY, 0.5f}, 1);
      break;
    }
    case MeshStyle::Mode::Lit:
    default: {
      const SkV4 vp4 = viewModel * SkV4{p.x, p.y, p.z, 1};
      const SkV3 posView = {vp4.x, vp4.y, vp4.z};
      const SkV3 N = hasNormals
                         ? normalized(normalM.apply(mesh.normals[i]))
                         : SkV3{0, 0, 1};
      const SkV3 V = normalized(posView * -1.0f);
      SkV3 base = {style.baseColor.fR, style.baseColor.fG,
                   style.baseColor.fB};
      float alpha = style.baseColor.fA;
      if (i < mesh.colors.size()) { // per-vertex tint lane (instancing)
        base = {base.x * mesh.colors[i].fR, base.y * mesh.colors[i].fG,
                base.z * mesh.colors[i].fB};
        alpha *= mesh.colors[i].fA;
      }
      SkV3 accum = {style.ambient.fR * base.x, style.ambient.fG * base.y,
                    style.ambient.fB * base.z};
      for (const Light &light : style.lights) {
        const SkV3 L = normalized(lightM.apply(light.direction * -1.0f));
        const float diff = std::max(N.dot(L), 0.0f);
        const SkV3 lc = {light.color.fR * light.intensity,
                         light.color.fG * light.intensity,
                         light.color.fB * light.intensity};
        accum += {base.x * lc.x * diff, base.y * lc.y * diff,
                  base.z * lc.z * diff};
        if (style.specular > 0 && diff > 0) {
          const SkV3 H = normalized(L + V);
          const float spec =
              std::pow(std::max(N.dot(H), 0.0f), style.shininess) *
              style.specular;
          accum += lc * spec;
        }
      }
      if (style.rim > 0) {
        const float rim =
            std::pow(1.0f - std::max(N.dot(V), 0.0f), 3.0f) * style.rim;
        accum += {rim, rim, rim};
      }
      shaded[i] = toColor(accum, alpha);
      break;
    }
    }
  }

  // Assemble triangles: near-plane reject, backface cull, depth sort.
  struct Tri {
    uint32_t i0, i1, i2;
    float depth;
  };
  std::vector<Tri> tris;
  tris.reserve(mesh.indices.size() / 3);
  for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
    const uint32_t i0 = mesh.indices[t], i1 = mesh.indices[t + 1],
                   i2 = mesh.indices[t + 2];
    if (!valid[i0] || !valid[i1] || !valid[i2])
      continue;
    if (style.backfaceCull) {
      const SkPoint a = screen[i0], b = screen[i1], c = screen[i2];
      const float area2 = (b.fX - a.fX) * (c.fY - a.fY) -
                          (b.fY - a.fY) * (c.fX - a.fX);
      // Front faces wind CCW in y-up space, so they arrive CW (negative
      // area) after the viewport's y flip.
      if (area2 >= 0)
        continue;
    }
    tris.push_back({i0, i1, i2,
                    (viewZ[i0] + viewZ[i1] + viewZ[i2]) / 3.0f});
  }
  if (style.depthSort)
    std::sort(tris.begin(), tris.end(),
              [](const Tri &a, const Tri &b) { return a.depth < b.depth; });

  // Emit in chunks under the 16-bit SkVertices limit: three unshared
  // vertices per triangle keeps the chunking trivial.
  const SkSamplingOptions sampling(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);
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
    if (textured)
      tex.reserve(count * 3);
    for (size_t k = 0; k < count; ++k) {
      const Tri &tri = tris[start + k];
      for (uint32_t idx : {tri.i0, tri.i1, tri.i2}) {
        pos.push_back(screen[idx]);
        col.push_back(shaded[idx]);
        if (textured) {
          const SkPoint uv = style.uvTransform.mapPoint(mesh.uvs[idx]);
          tex.push_back({uv.fX * texW, uv.fY * texH});
        }
      }
    }
    sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
        SkVertices::kTriangles_VertexMode, (int)pos.size(), pos.data(),
        textured ? tex.data() : nullptr, col.data(), 0, nullptr);
    canvas.drawVertices(vertices,
                        textured ? SkBlendMode::kModulate
                                 : SkBlendMode::kDst,
                        paint);
  }
}

void drawPanel(SkCanvas &canvas, const SkM44 &model, const Camera &camera,
               SkSize viewport,
               const std::function<void(SkCanvas &)> &draw) {
  canvas.save();
  SkM44 full = camera.viewProjection(viewport);
  full.preConcat(model);
  // Panel-local drawing keeps Skia's y-down convention; the flip makes
  // local content upright in the y-up world.
  full.preConcat(SkM44::Scale(1, -1, 1));
  canvas.concat(full);
  draw(canvas);
  canvas.restore();
}

void drawImagePanel(SkCanvas &canvas, sk_sp<SkImage> image, float width,
                    float height, const SkM44 &model, const Camera &camera,
                    SkSize viewport, float opacity) {
  if (!image)
    return;
  drawPanel(canvas, model, camera, viewport, [&](SkCanvas &local) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setAlphaf(opacity);
    local.drawImageRect(
        image, SkRect::MakeXYWH(-width * 0.5f, -height * 0.5f, width, height),
        SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear),
        &paint);
  });
}

SkM44 place(SkV3 position, float yawDeg, float pitchDeg, float rollDeg,
            float scale) {
  SkM44 m = SkM44::Translate(position.x, position.y, position.z);
  if (yawDeg != 0)
    m.preConcat(SkM44::Rotate({0, 1, 0}, yawDeg * kDegToRad));
  if (pitchDeg != 0)
    m.preConcat(SkM44::Rotate({1, 0, 0}, pitchDeg * kDegToRad));
  if (rollDeg != 0)
    m.preConcat(SkM44::Rotate({0, 0, 1}, rollDeg * kDegToRad));
  if (scale != 1)
    m.preScale(scale, scale, scale);
  return m;
}

} // namespace sigil::shape::space
