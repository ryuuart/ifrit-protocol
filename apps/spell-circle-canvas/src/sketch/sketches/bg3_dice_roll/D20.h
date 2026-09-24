#pragma once

// The d20: a regular icosahedron numbered like a real die, turned to an
// attitude and projected flat. A real die pairs opposite faces to sum to
// 21, and the face that lands square to the viewer carries the roll, so
// the numbering is derived from the solid rather than typed in.

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Faces.h>

#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <vector>

namespace d20 {

namespace mesh = sigil::geometry::mesh;

/** One face as seen: its three corners and its centre on the page, how
 *  squarely it faces the viewer (1 is square on, 0 edge on) and the
 *  number it carries. Faces turned away are not listed. */
struct Face {
  std::array<glm::vec2, 3> corners;
  glm::vec2 centre;
  float facing;
  int number;
};

/** One edge as seen. An edge between two visible faces is an interior
 *  crease; an edge with only one visible face is the silhouette. */
struct Edge {
  glm::vec2 from, to;
  bool silhouette;
};

struct View {
  std::vector<Face> faces;
  std::vector<Edge> edges;
};

class Die {
 public:
  /** The die whose face @p landing settles square to the viewer showing
   *  @p roll. */
  Die(size_t landing, int roll)
      : m_solid(mesh::platonic(mesh::Platonic::Icosahedron,
                               {.circumradius = 1, .sharedVertices = true})),
        m_edges(mesh::edges(m_solid)) {
    const size_t count = m_solid.indices.size() / 3;
    m_numbers.assign(count, 0);
    auto place = [&](size_t face, int number) {
      m_numbers[face] = number;
      if (const std::optional<size_t> across = mesh::opposedFace(m_solid, face))
        m_numbers[*across] = 21 - number;
    };
    place(landing, roll);
    int next = 1;
    for (size_t face = 0; face < count; ++face) {
      if (m_numbers[face] != 0) continue;
      while (next == roll || next == 21 - roll) ++next;
      place(face, next++);
    }
    // Square on, a regular solid projects to a symmetric badge; a small
    // tilt off that axis makes the surviving faces unequal, which is what
    // reads as a solid.
    m_landed = turn(0.175f, -0.125f, 0.085f) *
               glm::mat3(mesh::faceUp(m_solid, landing, {0, 0, 1}));
  }

  /** Three turns about x, y and z, in radians. */
  static glm::mat3 turn(float aboutX, float aboutY, float aboutZ) {
    glm::mat4 rotation(1.0f);
    rotation = glm::rotate(rotation, aboutZ, {0, 0, 1});
    rotation = glm::rotate(rotation, aboutY, {0, 1, 0});
    rotation = glm::rotate(rotation, aboutX, {1, 0, 0});
    return glm::mat3(rotation);
  }

  /** The die @p unsettled of the way back from its landing attitude —
   *  zero is landed — drawn at @p radius about @p centre, y down. */
  View seen(float unsettled, glm::vec2 centre, float radius) const {
    constexpr float kTurn = 6.2831853f;
    const glm::mat3 attitude =
        turn(unsettled * 2.25f * kTurn, unsettled * 1.5f * kTurn,
             unsettled * 0.75f * kTurn) *
        m_landed;
    auto onPage = [&](glm::vec3 point) {
      const glm::vec3 turned = attitude * point;
      return centre + glm::vec2(turned.x, -turned.y) * radius;
    };
    const size_t count = m_numbers.size();
    std::vector<float> facing(count);
    View view;
    for (size_t face = 0; face < count; ++face) {
      facing[face] = (attitude * mesh::faceNormal(m_solid, face)).z;
      if (facing[face] <= 0) continue;
      Face seen{.centre = onPage(mesh::faceCentroid(m_solid, face)),
                .facing = facing[face],
                .number = m_numbers[face]};
      for (size_t corner = 0; corner < 3; ++corner)
        seen.corners[corner] =
            onPage(m_solid.positions[m_solid.indices[face * 3 + corner]]);
      view.faces.push_back(seen);
    }
    for (const mesh::Edge& edge : m_edges) {
      const bool near = facing[edge.face] > 0;
      const bool far =
          edge.opposite != mesh::kNoFace && facing[edge.opposite] > 0;
      if (!near && !far) continue;
      view.edges.push_back({onPage(m_solid.positions[edge.from]),
                            onPage(m_solid.positions[edge.to]), near != far});
    }
    return view;
  }

 private:
  mesh::Mesh m_solid;
  std::vector<mesh::Edge> m_edges;
  std::vector<int> m_numbers;
  glm::mat3 m_landed{1.0f};
};

}  // namespace d20
