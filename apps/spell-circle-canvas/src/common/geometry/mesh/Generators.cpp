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
 *  direction of their own. When @p differenced, the normals are the
 *  formula's own differences rather than its word, and a vertex standing
 *  on its row's or its column's neighbour is a pole as well, whatever the
 *  difference there answered. */
Mesh sheet(
    int nu, int nv,
    const core::Callable<std::pair<glm::vec3, glm::vec3>(float, float)>& sample,
    const GridOptions& options, bool differenced) {
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
  std::vector<bool> pole;
  pole.reserve((size_t)nu * nv);
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
      pole.push_back(glm::dot(raw, raw) < 1e-16f);
      out.normals.push_back(normalized(raw));
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
  // A pole's normal is the sheet's limit there: the direction square to
  // every way the surface leaves the point. The pole's vertices are
  // grouped with the grid neighbours they coincide with — a row collapsed
  // onto an apex is one group, seam duplicates included — and each group
  // takes the sum of the triangles that meet it, weighted by their area.
  // Around a revolved apex those triangles fan evenly, so their sideways
  // leans cancel and the sum stands on the axis. A triangle with no area
  // adds nothing, so a seam column counted twice weighs nothing twice. A
  // group whose triangles all lack area borrows the nearest normal that
  // has a direction of its own, nearest in vertex order; a sheet with no
  // valid normal anywhere keeps the unit fallback.
  const size_t count = out.normals.size();
  glm::vec3 low = out.positions.front(), high = low;
  for (const glm::vec3& position : out.positions) {
    low = glm::min(low, position);
    high = glm::max(high, position);
  }
  const float coincident = std::max(glm::length(high - low) * 1e-5f, 1e-12f);
  // A differenced pole can keep a sliver of length: a formula that closes
  // its row only to within rounding answers a difference across that
  // sliver, which may even face backwards. The row or column standing on
  // one point says it is a pole all the same.
  if (differenced)
    for (int j = 0; j < nv; ++j)
      for (int i = 0; i < nu; ++i) {
        const size_t at = (size_t)j * nu + i;
        const glm::vec3 here = out.positions[at];
        const auto standsOn = [&](int column, int row) {
          return column >= 0 && column < nu && row >= 0 && row < nv &&
                 glm::length(out.positions[(size_t)row * nu + column] - here) <=
                     coincident;
        };
        if (standsOn(i - 1, j) || standsOn(i + 1, j) || standsOn(i, j - 1) ||
            standsOn(i, j + 1))
          pole[at] = true;
      }
  std::vector<size_t> degenerate;
  for (size_t index = 0; index < count; ++index)
    if (pole[index]) degenerate.push_back(index);
  if (!degenerate.empty() && degenerate.size() < count) {
    // Each pole vertex's group, found by walking grid neighbours that are
    // poles too and stand on the same point.
    std::vector<size_t> group(count, count);
    std::vector<size_t> members, stack;
    std::vector<std::vector<size_t>> groups;
    for (const size_t seed : degenerate) {
      if (group[seed] != count) continue;
      const size_t id = groups.size();
      members.clear();
      stack.assign(1, seed);
      group[seed] = id;
      while (!stack.empty()) {
        const size_t at = stack.back();
        stack.pop_back();
        members.push_back(at);
        const int column = (int)(at % (size_t)nu), row = (int)(at / (size_t)nu);
        const int steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& step : steps) {
          const int nextColumn = column + step[0], nextRow = row + step[1];
          if (nextColumn < 0 || nextColumn >= nu || nextRow < 0 ||
              nextRow >= nv)
            continue;
          const size_t next = (size_t)nextRow * nu + nextColumn;
          if (!pole[next] || group[next] != count) continue;
          if (glm::length(out.positions[next] - out.positions[at]) > coincident)
            continue;
          group[next] = id;
          stack.push_back(next);
        }
      }
      groups.push_back(members);
    }
    std::vector<glm::vec3> limit(groups.size(), glm::vec3(0.0f));
    for (size_t corner = 0; corner + 2 < out.indices.size(); corner += 3) {
      const uint32_t a = out.indices[corner], b = out.indices[corner + 1],
                     c = out.indices[corner + 2];
      if (!pole[a] && !pole[b] && !pole[c]) continue;
      const glm::vec3 face = cross(out.positions[b] - out.positions[a],
                                   out.positions[c] - out.positions[a]);
      // A triangle touching one group through two corners counts once.
      size_t counted[3] = {count, count, count};
      int touched = 0;
      for (const uint32_t vertex : {a, b, c}) {
        if (!pole[vertex]) continue;
        const size_t id = group[vertex];
        if (std::find(counted, counted + touched, id) != counted + touched)
          continue;
        counted[touched++] = id;
        limit[id] += face;
      }
    }
    std::vector<size_t> before(count, count), after(count, count);
    size_t last = count;
    for (size_t index = 0; index < count; ++index) {
      if (!pole[index]) last = index;
      before[index] = last;
    }
    last = count;
    for (size_t index = count; index-- > 0;) {
      if (!pole[index]) last = index;
      after[index] = last;
    }
    for (size_t id = 0; id < groups.size(); ++id) {
      const glm::vec3 sum = limit[id];
      const float length = glm::length(sum);
      for (const size_t index : groups[id]) {
        if (length > 0.0f && std::isfinite(length)) {
          out.normals[index] = sum / length;
          continue;
        }
        const size_t below = before[index], above = after[index];
        const size_t take =
            below == count
                ? above
                : (above == count
                       ? below
                       : (index - below <= above - index ? below : above));
        out.normals[index] = out.normals[take];
      }
    }
  }
  // THE TANGENTS, where the uvs are the sheet's own parameters: u runs
  // along the difference between a vertex's neighbours across the row,
  // squared to the normal, and v — which runs opposite the parameter —
  // stands beside it on the side the column's neighbours say. A pole,
  // where a row collapses onto one point, takes its direction from the
  // row beside it, which is the way u runs as the pole is approached.
  // They are squared to the normals once the poles have borrowed theirs.
  // A sheet whose texture is placed some other way reads them off the
  // placed uvs once they are written.
  const bool placed = options.textureFromPosition || options.textureRectangle;
  if (!placed) {
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
  if (placed) out.computeTangents();
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
      options, true);
}

Mesh grid(
    int nu, int nv,
    const core::Callable<std::pair<glm::vec3, glm::vec3>(float, float)>& fn,
    const GridOptions& options) {
  return sheet(nu, nv, fn, options, false);
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
  const bool tangents = mesh.tangents.size() == mesh.positions.size();
  out.positions.reserve(tris * 3);
  out.indices.reserve(tris * 3);
  if (normals) out.normals.reserve(tris * 3);
  if (uvs) out.uvs.reserve(tris * 3);
  if (tangents) out.tangents.reserve(tris * 3);
  out.colors.reserve(tris * 3);
  for (size_t t = 0; t < tris; ++t) {
    const glm::vec4 flat = (*primitive)[t];
    for (size_t k = 0; k < 3; ++k) {
      const uint32_t idx = mesh.indices[t * 3 + k];
      out.indices.push_back((uint32_t)out.positions.size());
      out.positions.push_back(mesh.positions[idx]);
      if (normals) out.normals.push_back(mesh.normals[idx]);
      if (uvs) out.uvs.push_back(mesh.uvs[idx]);
      if (tangents) out.tangents.push_back(mesh.tangents[idx]);
      out.colors.push_back(colors ? mesh.colors[idx] * flat : flat);
    }
  }
  return out;
}

}  // namespace sigil::geometry::mesh
