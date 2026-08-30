/** @file
 * The Wavefront OBJ reader: tinyobjloader parses the text, the .mtl
 * libraries it names are gathered through the caller's resolver, and
 * every shape becomes a Part wearing its material's diffuse colour and
 * texture reference.
 */

#include <tiny_obj_loader.h>

#include <sstream>
#include <string>

#include "Internal.h"

namespace sigil::geometry::mesh::codec::decode::detail {

namespace {

/** The .mtl texts an OBJ names, concatenated — newmtl blocks are
 *  independent, so one string feeds tinyobj for any number of
 *  mtllib lines. */
std::string gatherMtl(std::string_view text, const Resolver& resolve) {
  std::string mtl;
  if (!resolve) return mtl;
  std::istringstream lines{std::string(text)};
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream words(line);
    std::string keyword;
    words >> keyword;
    if (keyword != "mtllib") continue;
    std::string name;
    while (words >> name)
      if (auto bytes = resolve(name)) {
        mtl.append(asText(bytes->data(), bytes->size()));
        mtl.push_back('\n');
      }
  }
  return mtl;
}

}  // namespace

std::optional<Model> importObj(std::string_view text, const Resolver& resolve) {
  tinyobj::ObjReaderConfig config;
  config.triangulate = true;
  tinyobj::ObjReader reader;
  if (!reader.ParseFromString(std::string(text), gatherMtl(text, resolve),
                              config))
    return std::nullopt;

  const tinyobj::attrib_t& attrib = reader.GetAttrib();
  const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();
  const bool tinted = std::any_of(attrib.colors.begin(), attrib.colors.end(),
                                  [](tinyobj::real_t c) { return c != 1; });

  Model out;
  for (const tinyobj::shape_t& shape : reader.GetShapes()) {
    // One Part per material used inside the shape; -1 = no material.
    struct Building {
      Part part;
      std::map<std::array<int, 3>, uint32_t> seen;
      bool hasNormals = true;
    };
    std::map<int, Building> byMaterial;

    size_t cursor = 0;
    for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
      const size_t corners = shape.mesh.num_face_vertices[face];
      const int materialId = face < shape.mesh.material_ids.size()
                                 ? shape.mesh.material_ids[face]
                                 : -1;
      Building& building = byMaterial[materialId];
      Mesh& mesh = building.part.mesh;
      for (size_t corner = 0; corner < corners; ++corner) {
        const tinyobj::index_t index = shape.mesh.indices[cursor + corner];
        const std::array<int, 3> key = {
            index.vertex_index, index.texcoord_index, index.normal_index};
        auto [it, inserted] =
            building.seen.emplace(key, (uint32_t)mesh.positions.size());
        if (inserted) {
          const size_t v = (size_t)index.vertex_index * 3;
          mesh.positions.emplace_back(attrib.vertices[v],
                                      attrib.vertices[v + 1],
                                      attrib.vertices[v + 2]);
          if (index.normal_index >= 0) {
            const size_t n = (size_t)index.normal_index * 3;
            mesh.normals.emplace_back(attrib.normals[n], attrib.normals[n + 1],
                                      attrib.normals[n + 2]);
          } else {
            building.hasNormals = false;
          }
          if (index.texcoord_index >= 0) {
            const size_t t = (size_t)index.texcoord_index * 2;
            // OBJ vt origin is bottom-left; Mesh uvs are IMAGE
            // convention (top-left), so v flips.
            mesh.uvs.emplace_back(attrib.texcoords[t],
                                  1 - attrib.texcoords[t + 1]);
          } else {
            mesh.uvs.emplace_back(0, 0);
          }
          if (tinted)
            mesh.colors.emplace_back(attrib.colors[v], attrib.colors[v + 1],
                                     attrib.colors[v + 2], 1);
        }
        mesh.indices.push_back(it->second);
      }
      cursor += corners;
    }

    const bool split = byMaterial.size() > 1;
    for (auto& [materialId, building] : byMaterial) {
      Part& part = building.part;
      part.name = shape.name;
      if (materialId >= 0 && (size_t)materialId < materials.size()) {
        const tinyobj::material_t& material = materials[(size_t)materialId];
        if (split && !material.name.empty())
          part.name += (part.name.empty() ? "" : "/") + material.name;
        part.baseColor = {material.diffuse[0], material.diffuse[1],
                          material.diffuse[2], 1};
        part.textureUri = material.diffuse_texname;
        if (!part.textureUri.empty() && resolve)
          if (auto bytes = resolve(part.textureUri))
            part.textureBytes = std::move(*bytes);
      }
      finishPart(part, building.hasNormals);
      if (!part.mesh.indices.empty()) out.parts.push_back(std::move(part));
    }
  }
  if (out.parts.empty()) return std::nullopt;
  return out;
}
}  // namespace sigil::geometry::mesh::codec::decode::detail
