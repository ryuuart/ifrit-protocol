/** @file
 * The parametric sheet every stock surface is evaluated through, the flat
 * quad, and a primitive colour lane baked into unwelded vertex colours.
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/Vec.h"

namespace sigil::geometry::mesh {

using glm::cross;

namespace {

/** Overwrite a sheet's parameter coordinates where @p options places the
 *  texture another way. The rectangle's offset is divided out once so a
 *  vertex reads `projection / size + offset`, the same arithmetic as a
 *  coordinate written by hand as a position over a size plus a half. */
void placeTexture(Mesh& out, const GridOptions& options) {
  if (options.textureFromPosition) {
    for (size_t index = 0; index < out.positions.size(); ++index)
      out.uvs[index] = options.textureFromPosition(out.positions[index]);
    return;
  }
  if (!options.textureRectangle) return;
  const TextureRectangle& rectangle = *options.textureRectangle;
  const glm::vec2 offset{
      -glm::dot(rectangle.origin, rectangle.across) / rectangle.size.x,
      -glm::dot(rectangle.origin, rectangle.down) / rectangle.size.y};
  for (size_t index = 0; index < out.positions.size(); ++index) {
    const glm::vec3 position = out.positions[index];
    out.uvs[index] = {
        glm::dot(position, rectangle.across) / rectangle.size.x + offset.x,
        glm::dot(position, rectangle.down) / rectangle.size.y + offset.y};
  }
}

/** The sheet every grid is built by: one sample per vertex answering the
 *  position and the RAW normal — raw because a pole is only recognisable
 *  before the normalize, and the repair below reads which entries had no
 *  direction of their own. */
Mesh sheet(
    int nu, int nv,
    const core::Callable<std::pair<glm::vec3, glm::vec3>(float, float)>& sample,
    const GridOptions& options) {
  Mesh out;
  nu = std::max(nu, 2);
  nv = std::max(nv, 2);
  out.positions.reserve((size_t)nu * nv);
  out.uvs.reserve((size_t)nu * nv);
  out.normals.reserve((size_t)nu * nv);
  // A pole is where the sheet's two parameter directions collapse onto
  // one point: the cross product there has no length and so no direction
  // to normalize. Which entries those are has to be recorded while the
  // raw product is still in hand — once normalized, a pole is
  // indistinguishable from the unit fallback that replaced it.
  std::vector<size_t> degenerate;
  for (int j = 0; j < nv; ++j) {
    const float v = (float)j / (float)(nv - 1);
    for (int i = 0; i < nu; ++i) {
      const float u = (float)i / (float)(nu - 1);
      const auto [position, raw] = sample(u, v);
      out.positions.push_back(position);
      // Image-convention UVs: (0,0) samples the texture's TOP-left, so
      // v runs opposite the parameter (v param 0 is the sheet's bottom
      // in y-up space). Every renderer that samples one of these sheets
      // assumes it.
      out.uvs.emplace_back(u, 1.0f - v);
      if (glm::dot(raw, raw) < 1e-16f) degenerate.push_back(out.normals.size());
      out.normals.push_back(normalized(raw));
    }
  }
  // THE TANGENTS, where the uvs are the sheet's own parameters: u runs
  // along the difference between a vertex's neighbours across the row,
  // squared to the normal, and v — which runs opposite the parameter —
  // stands beside it on the side the column's neighbours say. A pole,
  // where a row collapses onto one point, takes its direction from the
  // row beside it, which is the way u runs as the pole is approached.
  // A sheet whose texture is placed some other way carries none.
  if (!options.textureFromPosition && !options.textureRectangle) {
    out.tangents.assign(out.positions.size(), glm::vec4(0.0f));
    const auto at = [&](int i, int j) {
      return out.positions[(size_t)j * nu + i];
    };
    for (int j = 0; j < nv; ++j)
      for (int i = 0; i < nu; ++i) {
        const glm::vec3 n = out.normals[(size_t)j * nu + i];
        glm::vec3 across{0.0f};
        // The row itself, or the nearest row that has a length.
        for (int reach = 0; reach < nv; ++reach) {
          for (const int row : {j - reach, j + reach}) {
            if (row < 0 || row >= nv) continue;
            across =
                at(std::min(i + 1, nu - 1), row) - at(std::max(i - 1, 0), row);
            across -= n * glm::dot(across, n);
            if (glm::dot(across, across) > 1e-12f) break;
          }
          if (glm::dot(across, across) > 1e-12f) break;
        }
        if (glm::dot(across, across) <= 1e-12f) continue;
        across = glm::normalize(across);
        // v in the uvs grows as the parameter falls.
        const glm::vec3 up =
            at(i, std::max(j - 1, 0)) - at(i, std::min(j + 1, nv - 1));
        const float side =
            glm::dot(glm::cross(n, across), up) < 0.0f ? -1.0f : 1.0f;
        out.tangents[(size_t)j * nu + i] = glm::vec4(across, side);
      }
  }
  placeTexture(out, options);
  for (int j = 0; j + 1 < nv; ++j) {
    for (int i = 0; i + 1 < nu; ++i) {
      const uint32_t a = (uint32_t)(j * nu + i);
      const uint32_t b = a + 1;
      const uint32_t c = a + (uint32_t)nu;
      const uint32_t d = c + 1;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }
  }
  // The poles borrow the nearest normal that had a direction of its own,
  // nearest measured in vertex order. One sweep each way carries the last
  // such normal, and every pole takes whichever of the two is closer, so
  // the borrow costs one pass however many poles a sheet has. A sheet
  // with no valid normal anywhere keeps the unit fallback.
  if (!degenerate.empty() && degenerate.size() < out.normals.size()) {
    const size_t n = out.normals.size();
    std::vector<bool> pole(n, false);
    for (const size_t i : degenerate) pole[i] = true;
    std::vector<size_t> before(n, n), after(n, n);
    size_t last = n;
    for (size_t i = 0; i < n; ++i) {
      if (!pole[i]) last = i;
      before[i] = last;
    }
    last = n;
    for (size_t i = n; i-- > 0;) {
      if (!pole[i]) last = i;
      after[i] = last;
    }
    for (const size_t i : degenerate) {
      const size_t b = before[i], a = after[i];
      const size_t take = b == n ? a : (a == n ? b : (i - b <= a - i ? b : a));
      out.normals[i] = out.normals[take];
    }
  }
  return out;
}

}  // namespace

Mesh grid(int nu, int nv, const core::Callable<glm::vec3(float, float)>& fn,
          const GridOptions& options) {
  const float eps = 1e-3f;
  return sheet(
      nu, nv,
      [&fn, eps](float u, float v) {
        const glm::vec3 du =
            fn(std::min(u + eps, 1.0f), v) - fn(std::max(u - eps, 0.0f), v);
        const glm::vec3 dv =
            fn(u, std::min(v + eps, 1.0f)) - fn(u, std::max(v - eps, 0.0f));
        return std::pair<glm::vec3, glm::vec3>{fn(u, v), cross(du, dv)};
      },
      options);
}

Mesh grid(
    int nu, int nv,
    const core::Callable<std::pair<glm::vec3, glm::vec3>(float, float)>& fn,
    const GridOptions& options) {
  return sheet(nu, nv, fn, options);
}

Mesh quad(float width, float height) {
  return grid(2, 2, [=](float u, float v) -> glm::vec3 {
    return {(u - 0.5f) * width, (v - 0.5f) * height, 0};
  });
}

Mesh bakePrimitiveColor(const Mesh& mesh, std::string_view lane) {
  const std::vector<glm::vec4>* primitive = mesh.primitiveIf(lane);
  const size_t tris = mesh.triangleCount();
  if (!primitive || primitive->size() != tris || tris == 0) return mesh;

  Mesh out;
  out.primitives = mesh.primitives;  // triangle order survives the unweld
  const bool normals = mesh.normals.size() == mesh.positions.size();
  const bool uvs = mesh.uvs.size() == mesh.positions.size();
  const bool colors = mesh.colors.size() == mesh.positions.size();
  out.positions.reserve(tris * 3);
  out.indices.reserve(tris * 3);
  if (normals) out.normals.reserve(tris * 3);
  if (uvs) out.uvs.reserve(tris * 3);
  out.colors.reserve(tris * 3);
  for (size_t t = 0; t < tris; ++t) {
    const glm::vec4 flat = (*primitive)[t];
    for (size_t k = 0; k < 3; ++k) {
      const uint32_t idx = mesh.indices[t * 3 + k];
      out.indices.push_back((uint32_t)out.positions.size());
      out.positions.push_back(mesh.positions[idx]);
      if (normals) out.normals.push_back(mesh.normals[idx]);
      if (uvs) out.uvs.push_back(mesh.uvs[idx]);
      out.colors.push_back(colors ? mesh.colors[idx] * flat : flat);
    }
  }
  return out;
}

}  // namespace sigil::geometry::mesh
