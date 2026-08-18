#include "sigilshape/Import.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define CGLTF_IMPLEMENTATION
#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
#include <cgltf.h>
#include <tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <istream>
#include <map>
#include <sstream>
#include <streambuf>
#include <variant>

namespace sigil::shape::import {

namespace {

constexpr glm::vec4 kWhite = {1, 1, 1, 1};

std::string_view asText(const void* bytes, size_t size) {
  return {static_cast<const char*>(bytes), size};
}

std::string lowerExtension(std::string_view pathHint) {
  const size_t slash = pathHint.find_last_of("/\\");
  const size_t dot = pathHint.rfind('.');
  if (dot == std::string_view::npos ||
      (slash != std::string_view::npos && dot < slash))
    return {};
  std::string ext(pathHint.substr(dot + 1));
  for (char& c : ext) c = (char)std::tolower((unsigned char)c);
  return ext;
}

/** Every importer's closing chores: lanes sized to positions (the Mesh
 *  contract), normals derived from the triangles when the file carried
 *  none. */
void finishPart(Part& part, bool hasNormals) {
  Mesh& mesh = part.mesh;
  if (!hasNormals || mesh.normals.size() != mesh.positions.size())
    mesh.computeNormals();
  if (mesh.uvs.size() != mesh.positions.size())
    mesh.uvs.resize(mesh.positions.size(), {0, 0});
  if (!mesh.colors.empty() && mesh.colors.size() != mesh.positions.size())
    mesh.colors.resize(mesh.positions.size(), kWhite);
}

// --- Wavefront OBJ (tinyobjloader) ---------------------------------------

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
          mesh.positions.push_back({attrib.vertices[v], attrib.vertices[v + 1],
                                    attrib.vertices[v + 2]});
          if (index.normal_index >= 0) {
            const size_t n = (size_t)index.normal_index * 3;
            mesh.normals.push_back({attrib.normals[n], attrib.normals[n + 1],
                                    attrib.normals[n + 2]});
          } else {
            building.hasNormals = false;
          }
          if (index.texcoord_index >= 0) {
            const size_t t = (size_t)index.texcoord_index * 2;
            // OBJ vt origin is bottom-left; Mesh uvs are IMAGE
            // convention (top-left), so v flips.
            mesh.uvs.push_back(
                {attrib.texcoords[t], 1 - attrib.texcoords[t + 1]});
          } else {
            mesh.uvs.push_back({0, 0});
          }
          if (tinted)
            mesh.colors.push_back({attrib.colors[v], attrib.colors[v + 1],
                                   attrib.colors[v + 2], 1});
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

// --- glTF 2.0 (cgltf) -----------------------------------------------------

cgltf_result cgltfRead(const cgltf_memory_options*,
                       const cgltf_file_options* file, const char* path,
                       cgltf_size* size, void** data) {
  const auto* resolve = static_cast<const Resolver*>(file->user_data);
  if (!resolve || !*resolve) return cgltf_result_file_not_found;
  std::optional<std::vector<std::byte>> bytes = (*resolve)(path);
  if (!bytes) return cgltf_result_file_not_found;
  void* out = std::malloc(bytes->empty() ? 1 : bytes->size());
  if (!out) return cgltf_result_out_of_memory;
  std::memcpy(out, bytes->data(), bytes->size());
  *size = bytes->size();
  *data = out;
  return cgltf_result_success;
}

void cgltfRelease(const cgltf_memory_options*, const cgltf_file_options*,
                  void* data) {
  std::free(data);
}

/** The encoded bytes of a glTF image, wherever they live: a buffer
 *  view (GLB), a data: URI, or an external file via the resolver.
 *  Also records the external URI (percent-decoded). */
void fetchGltfImage(const cgltf_options& options, const cgltf_image& image,
                    const Resolver& resolve, std::string& uriOut,
                    std::vector<std::byte>& bytesOut) {
  if (image.buffer_view && image.buffer_view->buffer &&
      image.buffer_view->buffer->data) {
    const auto* begin =
        static_cast<const std::byte*>(image.buffer_view->buffer->data) +
        image.buffer_view->offset;
    bytesOut.assign(begin, begin + image.buffer_view->size);
    return;
  }
  if (!image.uri) return;
  const std::string_view uri = image.uri;
  if (uri.starts_with("data:")) {
    const size_t comma = uri.find(";base64,");
    if (comma == std::string_view::npos) return;
    const std::string encoded(uri.substr(comma + 8));
    size_t padding = 0;
    while (padding < 2 && encoded.size() > padding &&
           encoded[encoded.size() - 1 - padding] == '=')
      ++padding;
    const size_t decodedSize = encoded.size() / 4 * 3 - padding;
    void* decoded = nullptr;
    if (cgltf_load_buffer_base64(&options, decodedSize, encoded.c_str(),
                                 &decoded) == cgltf_result_success) {
      const auto* begin = static_cast<const std::byte*>(decoded);
      bytesOut.assign(begin, begin + decodedSize);
      std::free(decoded);
    }
    return;
  }
  std::string decoded(uri);
  cgltf_decode_uri(decoded.data());
  decoded.resize(std::strlen(decoded.c_str()));
  uriOut = decoded;
  if (resolve)
    if (auto bytes = resolve(decoded)) bytesOut = std::move(*bytes);
}

void importGltfMesh(const cgltf_options& options, const cgltf_data& data,
                    const cgltf_mesh& gltfMesh, const glm::mat4& world,
                    const char* nodeName, const Resolver& resolve, Model& out) {
  for (size_t p = 0; p < gltfMesh.primitives_count; ++p) {
    const cgltf_primitive& primitive = gltfMesh.primitives[p];
    if (primitive.type != cgltf_primitive_type_triangles ||
        primitive.has_draco_mesh_compression)
      continue;

    const cgltf_accessor* position = nullptr;
    const cgltf_accessor* normal = nullptr;
    const cgltf_accessor* texcoord = nullptr;
    const cgltf_accessor* color = nullptr;
    for (size_t a = 0; a < primitive.attributes_count; ++a) {
      const cgltf_attribute& attribute = primitive.attributes[a];
      if (attribute.type == cgltf_attribute_type_position)
        position = attribute.data;
      else if (attribute.type == cgltf_attribute_type_normal)
        normal = attribute.data;
      else if (attribute.type == cgltf_attribute_type_texcoord &&
               attribute.index == 0)
        texcoord = attribute.data;
      else if (attribute.type == cgltf_attribute_type_color &&
               attribute.index == 0)
        color = attribute.data;
    }
    if (!position) continue;

    Part part;
    part.name = nodeName ? nodeName : gltfMesh.name ? gltfMesh.name : "";
    Mesh& mesh = part.mesh;
    const size_t count = position->count;
    mesh.positions.resize(count);
    for (size_t i = 0; i < count; ++i) {
      float v[3] = {};
      cgltf_accessor_read_float(position, i, v, 3);
      mesh.positions[i] = {v[0], v[1], v[2]};
    }
    if (normal && normal->count == count) {
      mesh.normals.resize(count);
      for (size_t i = 0; i < count; ++i) {
        float v[3] = {};
        cgltf_accessor_read_float(normal, i, v, 3);
        mesh.normals[i] = {v[0], v[1], v[2]};
      }
    }
    if (texcoord && texcoord->count == count) {
      // glTF texcoords are already image convention (0,0 = top-left).
      mesh.uvs.resize(count);
      for (size_t i = 0; i < count; ++i) {
        float v[2] = {};
        cgltf_accessor_read_float(texcoord, i, v, 2);
        mesh.uvs[i] = {v[0], v[1]};
      }
    }
    if (color && color->count == count) {
      mesh.colors.resize(count);
      for (size_t i = 0; i < count; ++i) {
        float v[4] = {1, 1, 1, 1};
        cgltf_accessor_read_float(color, i, v,
                                  cgltf_num_components(color->type));
        mesh.colors[i] = {v[0], v[1], v[2], v[3]};
      }
    }
    // Custom attributes — the _NAME accessors Blender and Houdini
    // exporters write — become named lanes, routed by width the same
    // way Cloud's maps split (1 scalar / 3 vector / 2+4 color). Lanes
    // import VERBATIM: node transforms bake into positions/normals
    // only, never into attribute data.
    for (size_t a = 0; a < primitive.attributes_count; ++a) {
      const cgltf_attribute& attribute = primitive.attributes[a];
      if (!attribute.name || attribute.name[0] != '_' || !attribute.data ||
          attribute.data->count != count || attribute.name[1] == '\0')
        continue;
      const std::string lane = attribute.name + 1;
      const cgltf_size components = cgltf_num_components(attribute.data->type);
      if (components == 1) {
        std::vector<float>& values = part.scalarLanes[lane];
        values.resize(count);
        for (size_t i = 0; i < count; ++i)
          cgltf_accessor_read_float(attribute.data, i, &values[i], 1);
      } else if (components == 3) {
        std::vector<glm::vec3>& values = part.vectorLanes[lane];
        values.resize(count);
        for (size_t i = 0; i < count; ++i) {
          float v[3] = {};
          cgltf_accessor_read_float(attribute.data, i, v, 3);
          values[i] = {v[0], v[1], v[2]};
        }
      } else {
        std::vector<glm::vec4>& values = part.colorLanes[lane];
        values.resize(count);
        for (size_t i = 0; i < count; ++i) {
          float v[4] = {0, 0, 0, 0};
          cgltf_accessor_read_float(attribute.data, i, v, components);
          values[i] = {v[0], v[1], v[2], v[3]};
        }
      }
    }

    if (primitive.indices) {
      mesh.indices.resize(primitive.indices->count);
      for (size_t i = 0; i < primitive.indices->count; ++i)
        mesh.indices[i] =
            (uint32_t)cgltf_accessor_read_index(primitive.indices, i);
    } else {
      mesh.indices.resize(count);
      for (size_t i = 0; i < count; ++i) mesh.indices[i] = (uint32_t)i;
    }

    if (primitive.material) {
      const cgltf_material& material = *primitive.material;
      const auto fetch = [&](const cgltf_texture_view& view,
                             const char* usage) {
        if (!view.texture || !view.texture->image) return;
        Part::TextureRef& ref = part.textures[usage];
        fetchGltfImage(options, *view.texture->image, resolve, ref.uri,
                       ref.bytes);
      };
      if (material.has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness& pbr =
            material.pbr_metallic_roughness;
        part.baseColor = {pbr.base_color_factor[0], pbr.base_color_factor[1],
                          pbr.base_color_factor[2], pbr.base_color_factor[3]};
        part.metallic = pbr.metallic_factor;
        part.roughness = pbr.roughness_factor;
        if (pbr.base_color_texture.texture &&
            pbr.base_color_texture.texture->image)
          fetchGltfImage(options, *pbr.base_color_texture.texture->image,
                         resolve, part.textureUri, part.textureBytes);
        // glTF packs roughness (G) and metallic (B) into one image;
        // occlusion conventionally rides its R, and the occlusion
        // texture below often names the same image.
        fetch(pbr.metallic_roughness_texture, "orm");
      }
      fetch(material.normal_texture, "normal");
      fetch(material.occlusion_texture, "occlusion");
      fetch(material.emissive_texture, "emissive");
      part.emissive = {material.emissive_factor[0], material.emissive_factor[1],
                       material.emissive_factor[2], 1};
      if (material.has_transmission)
        part.transmission = material.transmission.transmission_factor;
      if (material.has_ior) part.ior = material.ior.ior;
      part.opaque = material.alpha_mode == cgltf_alpha_mode_opaque;
      if (material.alpha_mode == cgltf_alpha_mode_mask)
        part.alphaCutoff = material.alpha_cutoff;
      if (part.opaque) part.baseColor.a = 1;  // OPAQUE ignores base alpha
      part.materialIndex = (int)(primitive.material - data.materials);
    }

    finishPart(part, normal && normal->count == count);
    part.mesh.transform(world);
    // The material slot every triangle of this part wears, on the
    // primitive class, so a merged model keeps per-face materials.
    if (part.materialIndex >= 0)
      part.mesh.prim("Material", {0, 0, 0, 0})
          .assign(part.mesh.triangleCount(),
                  {(float)part.materialIndex, 0, 0, 0});
    if (!part.mesh.indices.empty()) out.parts.push_back(std::move(part));
  }
}

std::optional<Model> importGltf(const void* bytes, size_t size,
                                std::string_view pathHint,
                                const Resolver& resolve) {
  cgltf_options options = {};
  options.file.read = &cgltfRead;
  options.file.release = &cgltfRelease;
  options.file.user_data = const_cast<Resolver*>(&resolve);

  cgltf_data* data = nullptr;
  if (cgltf_parse(&options, bytes, size, &data) != cgltf_result_success)
    return std::nullopt;
  const std::string base(pathHint);
  if (cgltf_load_buffers(&options, data, base.c_str()) !=
          cgltf_result_success ||
      cgltf_validate(data) != cgltf_result_success) {
    cgltf_free(data);
    return std::nullopt;
  }

  Model out;
  if (data->nodes_count > 0) {
    // The flat node array covers every node once; bake each mesh
    // instance at its world transform.
    for (size_t n = 0; n < data->nodes_count; ++n) {
      const cgltf_node& node = data->nodes[n];
      if (!node.mesh) continue;
      cgltf_float matrix[16];
      cgltf_node_transform_world(&node, matrix);
      importGltfMesh(options, *data, *node.mesh, glm::make_mat4(matrix),
                     node.name, resolve, out);
    }
  } else {
    for (size_t m = 0; m < data->meshes_count; ++m)
      importGltfMesh(options, *data, data->meshes[m], glm::mat4(1.0f), nullptr,
                     resolve, out);
  }
  cgltf_free(data);
  if (out.parts.empty()) return std::nullopt;
  return out;
}

// --- STL ------------------------------------------------------------------

void pushStlTriangle(Mesh& mesh, const glm::vec3 corners[3], glm::vec3 normal) {
  if (glm::dot(normal, normal) < 1e-12f) {
    const glm::vec3 a = corners[1] - corners[0];
    const glm::vec3 b = corners[2] - corners[0];
    normal = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x};
  }
  const float length = glm::length(normal);
  if (length > 1e-12f)
    normal = normal * (1.0f / length);
  else
    normal = {0, 0, 1};
  for (int i = 0; i < 3; ++i) {
    mesh.indices.push_back((uint32_t)mesh.positions.size());
    mesh.positions.push_back(corners[i]);
    mesh.normals.push_back(normal);
    mesh.uvs.push_back({0, 0});
  }
}

std::optional<Model> importStlBinary(const std::byte* bytes, size_t size) {
  uint32_t count = 0;
  std::memcpy(&count, bytes + 80, 4);
  Part part;
  const std::byte* cursor = bytes + 84;
  for (uint32_t t = 0; t < count; ++t, cursor += 50) {
    float f[12];
    std::memcpy(f, cursor, sizeof(f));
    const glm::vec3 corners[3] = {
        {f[3], f[4], f[5]}, {f[6], f[7], f[8]}, {f[9], f[10], f[11]}};
    pushStlTriangle(part.mesh, corners, {f[0], f[1], f[2]});
  }
  (void)size;
  if (part.mesh.indices.empty()) return std::nullopt;
  Model out;
  out.parts.push_back(std::move(part));
  return out;
}

std::optional<Model> importStlAscii(std::string_view text) {
  std::istringstream words{std::string(text)};
  std::string word;
  Part part;
  glm::vec3 normal = {0, 0, 0};
  glm::vec3 corners[3];
  int corner = 0;
  bool named = false;
  while (words >> word) {
    if (word == "solid" && !named) {
      std::string rest;
      std::getline(words, rest);
      const size_t start = rest.find_first_not_of(" \t\r");
      part.name = start == std::string::npos ? "" : rest.substr(start);
      named = true;
    } else if (word == "facet") {
      words >> word;  // "normal"
      words >> normal.x >> normal.y >> normal.z;
      corner = 0;
    } else if (word == "vertex" && corner < 3) {
      words >> corners[corner].x >> corners[corner].y >> corners[corner].z;
      if (++corner == 3) pushStlTriangle(part.mesh, corners, normal);
    }
  }
  if (part.mesh.indices.empty()) return std::nullopt;
  Model out;
  out.parts.push_back(std::move(part));
  return out;
}

/** 80-byte header, uint32 triangle count, 50 bytes a triangle: the
 *  arithmetic identifies binary STL beyond doubt (some binary files
 *  even start with "solid"). */
bool looksLikeBinaryStl(const std::byte* bytes, size_t size) {
  if (size < 84) return false;
  uint32_t count = 0;
  std::memcpy(&count, bytes + 80, 4);
  return 84 + 50ull * count == size;
}

std::optional<Model> importStl(const std::byte* bytes, size_t size) {
  if (looksLikeBinaryStl(bytes, size)) return importStlBinary(bytes, size);
  return importStlAscii(asText(bytes, size));
}

// --- PLY ------------------------------------------------------------------
// Hand-rolled like STL: the header names every element and property,
// the body is rows. ascii and binary_little_endian both supported.
// Conventional property names route into the Mesh (x/y/z, nx/ny/nz,
// s/t or u/v, red/green/blue/alpha — integer colors normalize to
// 0..1); EVERY other vertex property becomes a scalar lane under its
// own name. Files without a face element are honest point clouds
// (empty indices) — asCloud() is their natural consumer.
//
// FACE properties are the PRIMITIVE class and land in Mesh::prims, the
// read leg of save::ply's per-face write: one value per TRIANGLE,
// replicated when a polygon fans. Point lanes and prim lanes never
// share a container, so their cardinalities cannot be confused.

struct PlyScalarType {
  int size = 0;
  bool floating = false;
  bool signedInt = false;
  double intMax = 1;  ///< color normalization divisor for integers
};

std::optional<PlyScalarType> plyScalarType(std::string_view name) {
  if (name == "char" || name == "int8")
    return PlyScalarType{1, false, true, 127.0};
  if (name == "uchar" || name == "uint8")
    return PlyScalarType{1, false, false, 255.0};
  if (name == "short" || name == "int16")
    return PlyScalarType{2, false, true, 32767.0};
  if (name == "ushort" || name == "uint16")
    return PlyScalarType{2, false, false, 65535.0};
  if (name == "int" || name == "int32")
    return PlyScalarType{4, false, true, 2147483647.0};
  if (name == "uint" || name == "uint32")
    return PlyScalarType{4, false, false, 4294967295.0};
  if (name == "float" || name == "float32")
    return PlyScalarType{4, true, false, 1.0};
  if (name == "double" || name == "float64")
    return PlyScalarType{8, true, false, 1.0};
  return std::nullopt;
}

double plyLoadBinary(const std::byte*& cursor, const PlyScalarType& type) {
  uint64_t raw = 0;
  std::memcpy(&raw, cursor, (size_t)type.size);  // little-endian host
  cursor += type.size;
  if (type.floating) {
    if (type.size == 4) {
      float f;
      std::memcpy(&f, &raw, 4);
      return f;
    }
    double d;
    std::memcpy(&d, &raw, 8);
    return d;
  }
  if (type.signedInt) {
    // Sign-extend from the value's width.
    const int shift = 64 - type.size * 8;
    return (double)((int64_t)(raw << shift) >> shift);
  }
  return (double)raw;
}

struct PlyProperty {
  std::string name;
  PlyScalarType type;
  bool list = false;
  PlyScalarType countType;
};

struct PlyElement {
  std::string name;
  size_t count = 0;
  std::vector<PlyProperty> properties;
};

/** Fold suffixed scalar triples/quads back into wide lanes — the
 *  return leg of save::ply's spelling (name_x/_y/_z, name_r/_g/_b/_a)
 *  — so a round trip reconstitutes vectors and colors, not loose
 *  floats. Alpha is optional; it defaults to 1. Consumed components
 *  leave @p scalars; a partial group, or one whose members disagree on
 *  length, stays scalar.
 *
 *  BOTH attribute classes fold through here: the POINT lanes on the
 *  Part and the PRIMITIVE lanes bound for Mesh::prims. The suffix
 *  grammar is one grammar, so it gets one implementation — a second
 *  copy would be the thing that drifts. */
void foldSuffixedLanes(
    std::map<std::string, std::vector<float>, std::less<>>& scalars,
    std::map<std::string, std::vector<glm::vec3>, std::less<>>& vectors,
    std::map<std::string, std::vector<glm::vec4>, std::less<>>& colors) {
  std::vector<std::string> vectorBases, colorBases;
  for (const auto& [name, lane] : scalars) {
    if (name.size() > 2 && name.ends_with("_x"))
      vectorBases.push_back(name.substr(0, name.size() - 2));
    else if (name.size() > 2 && name.ends_with("_r"))
      colorBases.push_back(name.substr(0, name.size() - 2));
  }
  for (const std::string& base : vectorBases) {
    const auto x = scalars.find(base + "_x");
    const auto y = scalars.find(base + "_y");
    const auto z = scalars.find(base + "_z");
    if (x == scalars.end() || y == scalars.end() || z == scalars.end())
      continue;
    const size_t n = x->second.size();
    if (y->second.size() != n || z->second.size() != n) continue;
    std::vector<glm::vec3>& lane = vectors[base];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i)
      lane[i] = {x->second[i], y->second[i], z->second[i]};
    scalars.erase(x);
    scalars.erase(y);
    scalars.erase(z);
  }
  for (const std::string& base : colorBases) {
    const auto r = scalars.find(base + "_r");
    const auto g = scalars.find(base + "_g");
    const auto b = scalars.find(base + "_b");
    if (r == scalars.end() || g == scalars.end() || b == scalars.end())
      continue;
    const size_t n = r->second.size();
    if (g->second.size() != n || b->second.size() != n) continue;
    const auto alpha = scalars.find(base + "_a");
    const bool alphaMatched =
        alpha != scalars.end() && alpha->second.size() == n;
    std::vector<glm::vec4>& lane = colors[base];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i)
      lane[i] = {r->second[i], g->second[i], b->second[i],
                 alphaMatched ? alpha->second[i] : 1.0f};
    scalars.erase(r);
    scalars.erase(g);
    scalars.erase(b);
    // A size-mismatched "_a" was ignored above; only a consumed alpha
    // lane is erased.
    if (alphaMatched) scalars.erase(base + "_a");
  }
}

// ---------------------------------------------------------------------------
// Houdini .geo (JSON)

/** The little JSON reader the .geo importer needs: values as a tree of
 *  variants, numbers as doubles, no streaming. Houdini writes plain
 *  JSON with no extensions, so nothing beyond the grammar is handled. */
struct Json {
  using Array = std::vector<Json>;
  using Object = std::vector<std::pair<std::string, Json>>;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v;

  bool isArray() const { return std::holds_alternative<Array>(v); }
  bool isObject() const { return std::holds_alternative<Object>(v); }
  bool isString() const { return std::holds_alternative<std::string>(v); }
  bool isNumber() const { return std::holds_alternative<double>(v); }
  bool isBool() const { return std::holds_alternative<bool>(v); }
  const Array& array() const {
    static const Array empty;
    return isArray() ? std::get<Array>(v) : empty;
  }
  const Object& object() const {
    static const Object empty;
    return isObject() ? std::get<Object>(v) : empty;
  }
  const std::string& string() const {
    static const std::string empty;
    return isString() ? std::get<std::string>(v) : empty;
  }
  double number(double fallback = 0) const {
    return isNumber() ? std::get<double>(v) : fallback;
  }
  bool boolean() const { return isBool() && std::get<bool>(v); }
  /** Object member by key, or an array laid out as alternating
   *  key/value entries — the shape Houdini uses throughout the file. */
  const Json* get(std::string_view key) const {
    if (isObject()) {
      for (const auto& [k, value] : object())
        if (k == key) return &value;
      return nullptr;
    }
    if (isArray()) {
      const Array& a = array();
      for (size_t i = 0; i + 1 < a.size(); i += 2)
        if (a[i].isString() && a[i].string() == key) return &a[i + 1];
    }
    return nullptr;
  }
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view text) : m_text(text) {}
  std::optional<Json> read() {
    Json out;
    if (!value(out)) return std::nullopt;
    skipSpace();
    return out;
  }

 private:
  bool value(Json& out) {
    skipSpace();
    if (m_pos >= m_text.size()) return false;
    const char c = m_text[m_pos];
    if (c == '{') return object(out);
    if (c == '[') return array(out);
    if (c == '"') {
      std::string s;
      if (!string(s)) return false;
      out.v = std::move(s);
      return true;
    }
    if (m_text.substr(m_pos, 4) == "true") {
      m_pos += 4;
      out.v = true;
      return true;
    }
    if (m_text.substr(m_pos, 5) == "false") {
      m_pos += 5;
      out.v = false;
      return true;
    }
    if (m_text.substr(m_pos, 4) == "null") {
      m_pos += 4;
      out.v = nullptr;
      return true;
    }
    return number(out);
  }
  bool object(Json& out) {
    ++m_pos;  // {
    Json::Object members;
    skipSpace();
    if (peek() == '}') {
      ++m_pos;
      out.v = std::move(members);
      return true;
    }
    for (;;) {
      skipSpace();
      std::string key;
      if (!string(key)) return false;
      skipSpace();
      if (peek() != ':') return false;
      ++m_pos;
      Json member;
      if (!value(member)) return false;
      members.emplace_back(std::move(key), std::move(member));
      skipSpace();
      if (peek() == ',') {
        ++m_pos;
        continue;
      }
      if (peek() == '}') {
        ++m_pos;
        out.v = std::move(members);
        return true;
      }
      return false;
    }
  }
  bool array(Json& out) {
    ++m_pos;  // [
    Json::Array items;
    skipSpace();
    if (peek() == ']') {
      ++m_pos;
      out.v = std::move(items);
      return true;
    }
    for (;;) {
      Json item;
      if (!value(item)) return false;
      items.push_back(std::move(item));
      skipSpace();
      if (peek() == ',') {
        ++m_pos;
        continue;
      }
      if (peek() == ']') {
        ++m_pos;
        out.v = std::move(items);
        return true;
      }
      return false;
    }
  }
  bool string(std::string& out) {
    if (peek() != '"') return false;
    ++m_pos;
    while (m_pos < m_text.size()) {
      const char c = m_text[m_pos++];
      if (c == '"') return true;
      if (c != '\\') {
        out += c;
        continue;
      }
      if (m_pos >= m_text.size()) return false;
      const char e = m_text[m_pos++];
      switch (e) {
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        case '/':
          out += '/';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'u': {
          if (m_pos + 4 > m_text.size()) return false;
          unsigned code = 0;
          for (int i = 0; i < 4; ++i) {
            const char h = m_text[m_pos++];
            code <<= 4;
            if (h >= '0' && h <= '9')
              code |= (unsigned)(h - '0');
            else if (h >= 'a' && h <= 'f')
              code |= (unsigned)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
              code |= (unsigned)(h - 'A' + 10);
            else
              return false;
          }
          // Basic-plane code points to UTF-8; surrogates are rare in
          // attribute names and are passed through as replacement.
          if (code < 0x80) {
            out += (char)code;
          } else if (code < 0x800) {
            out += (char)(0xC0 | (code >> 6));
            out += (char)(0x80 | (code & 0x3F));
          } else {
            out += (char)(0xE0 | (code >> 12));
            out += (char)(0x80 | ((code >> 6) & 0x3F));
            out += (char)(0x80 | (code & 0x3F));
          }
          break;
        }
        default:
          return false;
      }
    }
    return false;
  }
  bool number(Json& out) {
    const size_t start = m_pos;
    if (peek() == '-' || peek() == '+') ++m_pos;
    while (m_pos < m_text.size() &&
           (std::isdigit((unsigned char)m_text[m_pos]) ||
            m_text[m_pos] == '.' || m_text[m_pos] == 'e' ||
            m_text[m_pos] == 'E' || m_text[m_pos] == '-' ||
            m_text[m_pos] == '+'))
      ++m_pos;
    if (m_pos == start) return false;
    const std::string token(m_text.substr(start, m_pos - start));
    char* end = nullptr;
    const double d = std::strtod(token.c_str(), &end);
    if (!end || *end != '\0') return false;
    out.v = d;
    return true;
  }
  char peek() const { return m_pos < m_text.size() ? m_text[m_pos] : '\0'; }
  void skipSpace() {
    while (m_pos < m_text.size() && std::isspace((unsigned char)m_text[m_pos]))
      ++m_pos;
  }
  std::string_view m_text;
  size_t m_pos = 0;
};

/** One decoded .geo attribute: a name, its tuple size, and every
 *  element's tuple flattened. Strings arrive as their table indices. */
struct GeoAttribute {
  std::string name;
  int size = 1;
  bool isString = false;
  std::vector<std::string> strings;
  std::vector<float> values;  ///< count * size
  size_t count() const { return size > 0 ? values.size() / (size_t)size : 0; }
  float at(size_t element, int component) const {
    return values[element * (size_t)size + (size_t)component];
  }
};

/** Decode a "values" (or "indices") block into a flat float array of
 *  @p count tuples of @p size. Three spellings exist: "tuples" (a list
 *  of tuples), "arrays" (a list of per-component arrays), and the paged
 *  form — "packing" splits the tuple into slices, "pagesize" elements
 *  per page, "constantpageflags" one bool per slice per page, and
 *  "rawpagedata" the concatenation, slice-major, page by page, a page
 *  being one tuple-slice when its flag is set and pagesize of them
 *  otherwise (the last page shorter). */
bool geoDecodeValues(const Json& block, size_t count, int size,
                     std::vector<float>& out) {
  out.assign(count * (size_t)size, 0.0f);
  if (count == 0) return true;
  if (const Json* tuples = block.get("tuples")) {
    const Json::Array& list = tuples->array();
    if (list.size() < count) return false;
    for (size_t i = 0; i < count; ++i) {
      const Json::Array& tuple = list[i].array();
      for (int c = 0; c < size && (size_t)c < tuple.size(); ++c)
        out[i * (size_t)size + (size_t)c] = (float)tuple[(size_t)c].number();
    }
    return true;
  }
  if (const Json* arrays = block.get("arrays")) {
    // One array per component (size 1: one array of the values).
    const Json::Array& list = arrays->array();
    if (list.size() < (size_t)size) return false;
    for (int c = 0; c < size; ++c) {
      const Json::Array& column = list[(size_t)c].array();
      if (column.size() < count) return false;
      for (size_t i = 0; i < count; ++i)
        out[i * (size_t)size + (size_t)c] = (float)column[i].number();
    }
    return true;
  }
  const Json* raw = block.get("rawpagedata");
  if (!raw) return false;
  const Json::Array& data = raw->array();
  std::vector<int> packing;
  if (const Json* p = block.get("packing"))
    for (const Json& n : p->array()) packing.push_back((int)n.number());
  if (packing.empty()) packing.push_back(size);
  int packed = 0;
  for (int p : packing) packed += p;
  if (packed != size) return false;
  const size_t pageSize =
      block.get("pagesize") ? (size_t)block.get("pagesize")->number() : count;
  if (pageSize == 0) return false;
  const size_t pages = (count + pageSize - 1) / pageSize;
  const Json* flags = block.get("constantpageflags");
  size_t cursor = 0;
  int componentBase = 0;
  for (size_t slice = 0; slice < packing.size(); ++slice) {
    const int width = packing[slice];
    const Json::Array* sliceFlags = nullptr;
    if (flags && slice < flags->array().size())
      sliceFlags = &flags->array()[slice].array();
    for (size_t page = 0; page < pages; ++page) {
      const size_t first = page * pageSize;
      const size_t n = std::min(pageSize, count - first);
      const bool constant = sliceFlags && page < sliceFlags->size() &&
                            (*sliceFlags)[page].boolean();
      if (constant) {
        if (cursor + (size_t)width > data.size()) return false;
        for (size_t i = 0; i < n; ++i)
          for (int c = 0; c < width; ++c)
            out[(first + i) * (size_t)size + (size_t)(componentBase + c)] =
                (float)data[cursor + (size_t)c].number();
        cursor += (size_t)width;
      } else {
        if (cursor + n * (size_t)width > data.size()) return false;
        for (size_t i = 0; i < n; ++i)
          for (int c = 0; c < width; ++c)
            out[(first + i) * (size_t)size + (size_t)(componentBase + c)] =
                (float)data[cursor + i * (size_t)width + (size_t)c].number();
        cursor += n * (size_t)width;
      }
    }
    componentBase += width;
  }
  return true;
}

/** Decode one attribute class list ("pointattributes" etc.). Each entry
 *  is a two-element array: [descriptor, payload]. Numeric attributes
 *  carry "size", "storage" and "values"; string attributes carry
 *  "strings" and "indices". Anything else (dicts, arrays-of-arrays) is
 *  skipped. */
std::vector<GeoAttribute> geoAttributes(const Json* list, size_t count) {
  std::vector<GeoAttribute> out;
  if (!list) return out;
  for (const Json& entry : list->array()) {
    const Json::Array& pair = entry.array();
    if (pair.size() < 2) continue;
    const Json& desc = pair[0];
    const Json& payload = pair[1];
    const Json* type = desc.get("type");
    const Json* name = desc.get("name");
    if (!type || !name) continue;
    GeoAttribute attr;
    attr.name = name->string();
    if (type->string() == "numeric") {
      attr.size = payload.get("size") ? (int)payload.get("size")->number() : 1;
      const Json* values = payload.get("values");
      if (!values || attr.size < 1 ||
          !geoDecodeValues(*values, count, attr.size, attr.values))
        continue;
    } else if (type->string() == "string") {
      attr.isString = true;
      attr.size = 1;
      if (const Json* strings = payload.get("strings"))
        for (const Json& str : strings->array())
          attr.strings.push_back(str.string());
      const Json* indices = payload.get("indices");
      if (!indices || !geoDecodeValues(*indices, count, 1, attr.values))
        continue;
    } else {
      continue;
    }
    out.push_back(std::move(attr));
  }
  return out;
}

const GeoAttribute* geoFind(const std::vector<GeoAttribute>& attrs,
                            std::string_view name) {
  for (const GeoAttribute& a : attrs)
    if (a.name == name) return &a;
  return nullptr;
}

/** A group's membership as a per-element flag list. Two spellings:
 *  "unordered" with a "boolRLE" run-length list ([count, flag, ...])
 *  or "i8" bytes; "ordered" with an explicit index list. */
std::vector<float> geoGroup(const Json& selection, size_t count) {
  std::vector<float> flags(count, 0.0f);
  if (const Json* unordered = selection.get("unordered")) {
    if (const Json* rle = unordered->get("boolRLE")) {
      const Json::Array& runs = rle->array();
      size_t at = 0;
      for (size_t i = 0; i + 1 < runs.size() && at < count; i += 2) {
        const size_t n = (size_t)runs[i].number();
        const float flag = runs[i + 1].boolean() ? 1.0f : 0.0f;
        for (size_t k = 0; k < n && at < count; ++k) flags[at++] = flag;
      }
    } else if (const Json* bytes = unordered->get("i8")) {
      const Json::Array& list = bytes->array();
      for (size_t i = 0; i < count && i < list.size(); ++i)
        flags[i] = list[i].number() != 0 ? 1.0f : 0.0f;
    }
  } else if (const Json* ordered = selection.get("ordered")) {
    for (const Json& index : ordered->array()) {
      const size_t i = (size_t)index.number();
      if (i < count) flags[i] = 1.0f;
    }
  }
  return flags;
}

/** The polygons: a list of (vertex index list, closed) pairs, from
 *  "Polygon" entries, "Polygon_run" runs (start vertex plus a
 *  run-length list of vertex counts) and generic "run" entries whose
 *  runtype is Polygon. Every other primitive type is skipped. */
struct GeoPolygon {
  std::vector<uint32_t> vertices;
  size_t primitiveIndex = 0;  ///< position among ALL primitives
  bool closed = true;
};
std::vector<GeoPolygon> geoPolygons(const Json* primitives) {
  std::vector<GeoPolygon> out;
  if (!primitives) return out;
  size_t primitiveIndex = 0;
  for (const Json& entry : primitives->array()) {
    const Json::Array& pair = entry.array();
    if (pair.empty()) continue;
    const Json& head = pair[0];
    const Json* type = head.get("type");
    if (!type) continue;
    const std::string& kind = type->string();
    if (kind == "Polygon" && pair.size() >= 2) {
      GeoPolygon poly;
      poly.primitiveIndex = primitiveIndex++;
      if (const Json* vertex = pair[1].get("vertex"))
        for (const Json& v : vertex->array())
          poly.vertices.push_back((uint32_t)v.number());
      if (const Json* closed = pair[1].get("closed"))
        poly.closed = closed->boolean();
      out.push_back(std::move(poly));
    } else if (kind == "Polygon_run" && pair.size() >= 2) {
      const Json& body = pair[1];
      uint32_t vertex = body.get("startvertex")
                            ? (uint32_t)body.get("startvertex")->number()
                            : 0;
      const size_t nprims = body.get("nprimitives")
                                ? (size_t)body.get("nprimitives")->number()
                                : 0;
      std::vector<size_t> counts;
      if (const Json* rle = body.get("nvertices_rle")) {
        const Json::Array& runs = rle->array();
        for (size_t i = 0; i + 1 < runs.size(); i += 2) {
          const size_t n = (size_t)runs[i].number();
          const size_t repeat = (size_t)runs[i + 1].number();
          for (size_t k = 0; k < repeat; ++k) counts.push_back(n);
        }
      } else if (const Json* list = body.get("nvertices")) {
        for (const Json& n : list->array())
          counts.push_back((size_t)n.number());
      }
      const bool closed = !body.get("closed") || body.get("closed")->boolean();
      for (size_t p = 0; p < nprims && p < counts.size(); ++p) {
        GeoPolygon poly;
        poly.primitiveIndex = primitiveIndex++;
        poly.closed = closed;
        for (size_t k = 0; k < counts[p]; ++k)
          poly.vertices.push_back(vertex++);
        out.push_back(std::move(poly));
      }
    } else if (kind == "run" && pair.size() >= 2) {
      const Json* runtype = head.get("runtype");
      const Json* varying = head.get("varyingfields");
      const Json* uniform = head.get("uniformfields");
      const bool polygonRun = runtype && runtype->string() == "Polygon";
      int vertexField = -1;
      if (varying)
        for (size_t i = 0; i < varying->array().size(); ++i)
          if (varying->array()[i].string() == "vertex") vertexField = (int)i;
      bool closed = true;
      if (uniform)
        if (const Json* c = uniform->get("closed")) closed = c->boolean();
      const Json::Array& items = pair[1].array();
      for (const Json& item : items) {
        if (!polygonRun || vertexField < 0) {
          ++primitiveIndex;
          continue;
        }
        GeoPolygon poly;
        poly.primitiveIndex = primitiveIndex++;
        poly.closed = closed;
        const Json::Array& fields = item.array();
        if ((size_t)vertexField < fields.size())
          for (const Json& v : fields[(size_t)vertexField].array())
            poly.vertices.push_back((uint32_t)v.number());
        out.push_back(std::move(poly));
      }
    } else {
      // Another primitive type, or a run of one: it still occupies
      // primitive indices, so primitive attributes stay aligned.
      size_t occupied = 1;
      if (kind == "run" && pair.size() >= 2) occupied = pair[1].array().size();
      if (kind.ends_with("_run") && pair.size() >= 2 &&
          pair[1].get("nprimitives"))
        occupied = (size_t)pair[1].get("nprimitives")->number();
      primitiveIndex += occupied;
    }
  }
  return out;
}

/** Houdini .geo: one Part. Polygons unweld into a triangle mesh whose
 *  vertices are the file's VERTICES (each carrying its point's position
 *  and the vertex- or point-class N, uv, Cd and Alpha it resolves to);
 *  a file with no polygons is a point cloud whose mesh vertices are its
 *  points. Point attributes beyond the conventional ones become named
 *  lanes on the Part; primitive attributes become Mesh::prims lanes,
 *  replicated across each polygon's fan triangles; point groups become
 *  0/1 scalar lanes under the group's name and primitive groups the
 *  same on the primitive class. Detail attributes have no home and are
 *  dropped. */
std::optional<Model> importHoudiniGeo(std::string_view text) {
  std::optional<Json> rootOpt = JsonReader(text).read();
  if (!rootOpt || !rootOpt->isArray()) return std::nullopt;
  const Json& root = *rootOpt;
  if (!root.get("fileversion") || !root.get("pointcount")) return std::nullopt;
  const size_t pointCount = (size_t)root.get("pointcount")->number();
  const size_t vertexCount =
      root.get("vertexcount") ? (size_t)root.get("vertexcount")->number() : 0;
  const size_t primitiveCount =
      root.get("primitivecount") ? (size_t)root.get("primitivecount")->number()
                                 : 0;

  // Topology: vertex -> point.
  std::vector<uint32_t> pointRef;
  if (const Json* topology = root.get("topology"))
    if (const Json* pointref = topology->get("pointref"))
      if (const Json* indices = pointref->get("indices"))
        for (const Json& i : indices->array())
          pointRef.push_back((uint32_t)i.number());
  if (pointRef.size() < vertexCount) pointRef.resize(vertexCount, 0);

  const Json* attributes = root.get("attributes");
  const std::vector<GeoAttribute> pointAttrs = geoAttributes(
      attributes ? attributes->get("pointattributes") : nullptr, pointCount);
  const std::vector<GeoAttribute> vertexAttrs = geoAttributes(
      attributes ? attributes->get("vertexattributes") : nullptr, vertexCount);
  const std::vector<GeoAttribute> primAttrs = geoAttributes(
      attributes ? attributes->get("primitiveattributes") : nullptr,
      primitiveCount);
  const GeoAttribute* P = geoFind(pointAttrs, "P");
  if (!P || P->size < 3 || P->count() < pointCount) return std::nullopt;

  const std::vector<GeoPolygon> polygons = geoPolygons(root.get("primitives"));

  Part part;
  part.name = "geo";
  Mesh& mesh = part.mesh;

  // Which class an attribute is read from, vertex first: a vertex uv
  // outranks a point uv, as in Houdini.
  const auto attr =
      [&](std::string_view name) -> std::pair<const GeoAttribute*, bool> {
    if (const GeoAttribute* v = geoFind(vertexAttrs, name)) return {v, true};
    if (const GeoAttribute* p = geoFind(pointAttrs, name)) return {p, false};
    return {nullptr, false};
  };
  const auto [N, nVertex] = attr("N");
  const auto [uv, uvVertex] = attr("uv");
  const auto [Cd, cdVertex] = attr("Cd");
  const auto [Alpha, alphaVertex] = attr("Alpha");

  const bool cloud = polygons.empty();
  // Emit one mesh vertex per (vertex, or point in the cloud case).
  const auto emit = [&](uint32_t vertex, uint32_t point) {
    mesh.positions.push_back(
        {P->at(point, 0), P->at(point, 1), P->at(point, 2)});
    const auto pick = [&](const GeoAttribute* a, bool fromVertex) -> size_t {
      return fromVertex ? (size_t)vertex : (size_t)point;
    };
    if (N && N->size >= 3) {
      const size_t i = pick(N, nVertex);
      mesh.normals.push_back({N->at(i, 0), N->at(i, 1), N->at(i, 2)});
    }
    if (uv && uv->size >= 2) {
      const size_t i = pick(uv, uvVertex);
      // Houdini's v runs up the image; the mesh currency's runs down.
      mesh.uvs.push_back({uv->at(i, 0), 1.0f - uv->at(i, 1)});
    }
    if (Cd && Cd->size >= 3) {
      const size_t i = pick(Cd, cdVertex);
      const float a = Alpha && Alpha->size >= 1
                          ? Alpha->at(pick(Alpha, alphaVertex), 0)
                          : (Cd->size >= 4 ? Cd->at(i, 3) : 1.0f);
      mesh.colors.push_back({Cd->at(i, 0), Cd->at(i, 1), Cd->at(i, 2), a});
    }
  };
  std::vector<uint32_t> ownerPoint;  // mesh vertex -> point index
  if (cloud) {
    for (uint32_t p = 0; p < pointCount; ++p) {
      emit(p, p);
      ownerPoint.push_back(p);
    }
  } else {
    // Unwelded: every polygon vertex is its own mesh vertex, so vertex
    // attributes (uv seams, hard normals) survive as authored.
    std::vector<size_t> triPrim;  // triangle -> primitive index
    for (const GeoPolygon& poly : polygons) {
      if (poly.vertices.size() < 3) continue;
      const uint32_t base = (uint32_t)mesh.positions.size();
      for (uint32_t v : poly.vertices) {
        if (v >= pointRef.size()) return std::nullopt;
        emit(v, pointRef[v]);
        ownerPoint.push_back(pointRef[v]);
      }
      for (uint32_t k = 1; k + 1 < (uint32_t)poly.vertices.size(); ++k) {
        mesh.indices.insert(mesh.indices.end(), {base, base + k, base + k + 1});
        triPrim.push_back(poly.primitiveIndex);
      }
    }
    // Primitive attributes and groups: one value per triangle, from
    // the polygon that produced it.
    const auto primLane = [&](const std::string& name, const GeoAttribute& a) {
      std::vector<glm::vec4>& lane = mesh.prim(name, {0, 0, 0, 0});
      for (size_t t = 0; t < triPrim.size(); ++t) {
        const size_t p = triPrim[t];
        if (p >= a.count()) continue;
        glm::vec4 v{0, 0, 0, 0};
        for (int c = 0; c < a.size && c < 4; ++c) v[c] = a.at(p, c);
        if (a.size == 1) v = {v.x, v.x, v.x, v.x};
        if (name == "Color" && a.size == 3) v.w = 1;
        lane[t] = v;
      }
    };
    for (const GeoAttribute& a : primAttrs) {
      if (a.isString) {
        // Houdini's material assignment is a string per primitive; the
        // string table's index is the material slot, and the table's
        // order is the slot order.
        if (a.name == "shop_materialpath") primLane("Material", a);
        continue;
      }
      primLane(a.name == "Cd" ? "Color" : a.name, a);
    }
    if (const Json* groups = root.get("primitivegroups"))
      for (const Json& entry : groups->array()) {
        const Json::Array& pair = entry.array();
        if (pair.size() < 2 || !pair[0].get("name") ||
            !pair[1].get("selection"))
          continue;
        GeoAttribute flags;
        flags.name = pair[0].get("name")->string();
        flags.size = 1;
        flags.values = geoGroup(*pair[1].get("selection"), primitiveCount);
        primLane(flags.name, flags);
      }
  }
  if (mesh.normals.size() != mesh.positions.size()) mesh.normals.clear();
  if (mesh.uvs.size() != mesh.positions.size()) mesh.uvs.clear();
  if (mesh.colors.size() != mesh.positions.size()) mesh.colors.clear();
  if (!cloud && mesh.normals.empty()) mesh.computeNormals();

  // Point-class lanes on the Part, one value per mesh vertex, read
  // through the owning point: every point attribute the conventional
  // slots did not take, and every point group as a 0/1 scalar.
  const size_t n = mesh.positions.size();
  const auto pointLane = [&](const GeoAttribute& a, const std::string& name) {
    if (a.isString) return;
    if (a.size == 1) {
      std::vector<float>& lane = part.scalarLanes[name];
      lane.resize(n);
      for (size_t i = 0; i < n; ++i) lane[i] = a.at(ownerPoint[i], 0);
    } else if (a.size == 3) {
      std::vector<glm::vec3>& lane = part.vectorLanes[name];
      lane.resize(n);
      for (size_t i = 0; i < n; ++i)
        lane[i] = {a.at(ownerPoint[i], 0), a.at(ownerPoint[i], 1),
                   a.at(ownerPoint[i], 2)};
    } else if (a.size == 2 || a.size == 4) {
      std::vector<glm::vec4>& lane = part.colorLanes[name];
      lane.resize(n);
      for (size_t i = 0; i < n; ++i) {
        glm::vec4 v{0, 0, 0, 0};
        for (int c = 0; c < a.size; ++c) v[c] = a.at(ownerPoint[i], c);
        lane[i] = v;
      }
    }
  };
  for (const GeoAttribute& a : pointAttrs) {
    if (a.name == "P") continue;
    if ((a.name == "N" && !nVertex && N) ||
        (a.name == "uv" && !uvVertex && uv) ||
        (a.name == "Cd" && !cdVertex && Cd) ||
        (a.name == "Alpha" && !alphaVertex && Alpha))
      continue;  // taken by the conventional slot
    pointLane(a, a.name);
  }
  if (const Json* groups = root.get("pointgroups"))
    for (const Json& entry : groups->array()) {
      const Json::Array& pair = entry.array();
      if (pair.size() < 2 || !pair[0].get("name") || !pair[1].get("selection"))
        continue;
      GeoAttribute flags;
      flags.name = pair[0].get("name")->string();
      flags.size = 1;
      flags.values = geoGroup(*pair[1].get("selection"), pointCount);
      pointLane(flags, flags.name);
    }

  Model model;
  model.parts.push_back(std::move(part));
  return model;
}

bool looksLikeHoudiniGeo(std::string_view text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos || text[start] != '[') return false;
  return text.substr(start, 256).find("\"fileversion\"") !=
         std::string_view::npos;
}

std::optional<Model> importPly(const std::byte* bytes, size_t size) {
  const std::string_view text = asText(bytes, size);
  const size_t headerEnd = text.find("end_header");
  if (!text.starts_with("ply") || headerEnd == std::string_view::npos)
    return std::nullopt;
  size_t bodyStart = text.find('\n', headerEnd);
  if (bodyStart == std::string_view::npos) return std::nullopt;
  ++bodyStart;

  // Header.
  bool binary = false;
  std::vector<PlyElement> elements;
  {
    std::istringstream header(std::string(text.substr(0, headerEnd)));
    std::string line;
    while (std::getline(header, line)) {
      std::istringstream words(line);
      std::string word;
      words >> word;
      if (word == "format") {
        std::string format;
        words >> format;
        if (format == "binary_little_endian")
          binary = true;
        else if (format != "ascii")
          return std::nullopt;  // big-endian: not this century
      } else if (word == "element") {
        // The count is file-supplied: a negative (or missing) value
        // must never reach the size_t — it would wrap huge.
        PlyElement element;
        long long declared = -1;
        words >> element.name >> declared;
        if (declared < 0) return std::nullopt;
        element.count = (size_t)declared;
        elements.push_back(std::move(element));
      } else if (word == "property" && !elements.empty()) {
        PlyProperty property;
        std::string type;
        words >> type;
        if (type == "list") {
          std::string countName, valueName;
          words >> countName >> valueName >> property.name;
          const auto count = plyScalarType(countName);
          const auto value = plyScalarType(valueName);
          if (!count || !value) return std::nullopt;
          property.list = true;
          property.countType = *count;
          property.type = *value;
        } else {
          const auto value = plyScalarType(type);
          if (!value) return std::nullopt;
          words >> property.name;
          property.type = *value;
        }
        elements.back().properties.push_back(std::move(property));
      }
    }
  }

  // Element counts are trusted nowhere yet: before any resize acts on
  // them, bound each against the bytes that actually follow the header
  // — a row costs at least one byte per property (two in ascii), so a
  // count beyond the remaining byte count cannot be backed by data.
  const size_t remaining = size - bodyStart;
  for (const PlyElement& element : elements)
    if (element.count > remaining) return std::nullopt;

  Part part;
  Mesh& mesh = part.mesh;
  bool hasNormals = false;
  /** PRIMITIVE-class values as they arrive: raw floats under the
   *  property's own name, one entry per TRIANGLE (not per face row —
   *  see the fan replication below). Accumulated across every face
   *  element, folded and widened into Mesh::prims once the body is
   *  read. */
  std::map<std::string, std::vector<float>, std::less<>> primScalars;

  // One reader per source; ascii tokenizes, binary walks a cursor.
  std::istringstream ascii(binary ? std::string()
                                  : std::string(text.substr(bodyStart)));
  const std::byte* cursor = bytes + bodyStart;
  const std::byte* end = bytes + size;
  const auto read = [&](const PlyScalarType& type, double* value) -> bool {
    if (binary) {
      if (cursor + type.size > end) return false;
      *value = plyLoadBinary(cursor, type);
      return true;
    }
    return (bool)(ascii >> *value);
  };

  for (const PlyElement& element : elements) {
    const bool isVertex = element.name == "vertex";
    const bool isFace = element.name == "face";

    // Per-property sinks, resolved once. index in [0, count).
    std::vector<std::function<void(size_t, double)>> sinks;
    if (isVertex) {
      mesh.positions.resize(element.count, glm::vec3{0});
      // "s"/"t" and "u"/"v" are texture coordinates only as PAIRS. A
      // lone "t" — the scalar every readChain/cook/asCloud cloud
      // carries — must stay a lane, not clobber uv.y.
      const auto hasProp = [&element](std::string_view want) {
        for (const PlyProperty& p : element.properties)
          if (!p.list && p.name == want) return true;
        return false;
      };
      const bool uvST = hasProp("s") && hasProp("t");
      const bool uvUV = hasProp("u") && hasProp("v");
      for (const PlyProperty& property : element.properties) {
        const std::string& n = property.name;
        if (property.list) {
          // A list on the vertex element has no lane shape here: keep
          // the sink walk aligned with a no-op and create no lane.
          sinks.push_back([](size_t, double) {});
          continue;
        }
        const double norm =
            property.type.floating ? 1.0 : 1.0 / property.type.intMax;
        auto axis = [&](auto member) {
          return [&mesh, member](size_t i, double v) {
            mesh.positions[i].*member = (float)v;
          };
        };
        if (n == "x")
          sinks.push_back(axis(&glm::vec3::x));
        else if (n == "y")
          sinks.push_back(axis(&glm::vec3::y));
        else if (n == "z")
          sinks.push_back(axis(&glm::vec3::z));
        else if (n == "nx" || n == "ny" || n == "nz") {
          hasNormals = true;
          mesh.normals.resize(element.count, glm::vec3{0});
          const int c = n == "nx" ? 0 : n == "ny" ? 1 : 2;
          sinks.push_back([&mesh, c](size_t i, double v) {
            mesh.normals[i][c] = (float)v;
          });
        } else if (((n == "s" || n == "t") && uvST) ||
                   ((n == "u" || n == "v") && uvUV)) {
          mesh.uvs.resize(element.count, glm::vec2{0});
          const int c = (n == "s" || n == "u") ? 0 : 1;
          sinks.push_back(
              [&mesh, c](size_t i, double v) { mesh.uvs[i][c] = (float)v; });
        } else if (n == "red" || n == "green" || n == "blue" || n == "alpha") {
          mesh.colors.resize(element.count, kWhite);
          const int c = n == "red" ? 0 : n == "green" ? 1 : n == "blue" ? 2 : 3;
          sinks.push_back([&mesh, c, norm](size_t i, double v) {
            mesh.colors[i][c] = (float)(v * norm);
          });
        } else {
          std::vector<float>& lane = part.scalarLanes[n];
          lane.resize(element.count, 0.0f);
          sinks.push_back([&lane](size_t i, double v) {
            lane[i] = (float)v;  // raw — ids stay ids
          });
        }
      }
    }

    // Per-face lane targets, resolved once and parallel to
    // element.properties (null for lists, for an unnamed property, and
    // for a name already claimed in THIS element — a duplicate
    // property would append twice and desync its lane). Nothing is
    // sized from element.count: a header that promises face rows it
    // never delivers allocates nothing here.
    struct FaceLane {
      std::vector<float>* lane = nullptr;
      double scale = 1;
    };
    std::vector<FaceLane> faceLanes;
    std::vector<double> faceRow;
    if (isFace) {
      faceLanes.resize(element.properties.size());
      faceRow.assign(element.properties.size(), 0.0);
      for (size_t p = 0; p < element.properties.size(); ++p) {
        const PlyProperty& property = element.properties[p];
        if (property.list || property.name.empty()) continue;
        // MeshLab-style conventional per-face color is spelled
        // red/green/blue/alpha; it is collected under the SUFFIXED
        // name so the one folder below reconstitutes it as the same
        // "Color" lane save::ply writes as Color_r/_g/_b/_a. Integer
        // channels normalize, exactly like the vertex leg; every other
        // property stays raw (ids stay ids).
        const std::string& n = property.name;
        const std::string laneName = n == "red"     ? "Color_r"
                                     : n == "green" ? "Color_g"
                                     : n == "blue"  ? "Color_b"
                                     : n == "alpha" ? "Color_a"
                                                    : n;
        std::vector<float>* target = &primScalars[laneName];
        const bool duplicate = std::any_of(
            faceLanes.begin(), faceLanes.begin() + (std::ptrdiff_t)p,
            [target](const FaceLane& existing) {
              return existing.lane == target;
            });
        if (duplicate) continue;
        const bool color = laneName != n;
        faceLanes[p] = {target, color && !property.type.floating
                                    ? 1.0 / property.type.intMax
                                    : 1.0};
      }
    }

    for (size_t i = 0; i < element.count; ++i) {
      size_t sink = 0;
      size_t faceTriangles = 0;
      for (const PlyProperty& property : element.properties) {
        if (property.list) {
          double countValue = 0;
          if (!read(property.countType, &countValue)) return std::nullopt;
          // Malformed list counts — negative, NaN, or beyond anything
          // the file's bytes could back — must never reach the size_t
          // cast or the reserve below.
          if (!(countValue >= 0) || countValue > (double)size)
            return std::nullopt;
          const size_t count = (size_t)countValue;
          std::vector<uint32_t> row;
          row.reserve(count);
          for (size_t k = 0; k < count; ++k) {
            double value = 0;
            if (!read(property.type, &value)) return std::nullopt;
            // Negative (or NaN) list values would wrap in the
            // uint32_t cast — malformed file.
            if (!(value >= 0)) return std::nullopt;
            row.push_back((uint32_t)value);
          }
          if (isFace && (property.name == "vertex_indices" ||
                         property.name == "vertex_index")) {
            // File-supplied indices feed every later positions[] and
            // normals[] access; a face naming a vertex that does not
            // exist is dropped whole, the rest still imports.
            const bool inRange =
                std::all_of(row.begin(), row.end(), [&mesh](uint32_t v) {
                  return (size_t)v < mesh.positions.size();
                });
            if (inRange) {
              for (size_t k = 1; k + 1 < row.size(); ++k)
                mesh.indices.insert(mesh.indices.end(),
                                    {row[0], row[k], row[k + 1]});
              faceTriangles += row.size() > 2 ? row.size() - 2 : 0;
            }
          }
        } else {
          double value = 0;
          if (!read(property.type, &value)) return std::nullopt;
          if (isVertex)
            sinks[sink](i, value);
          else if (isFace)
            faceRow[sink] = value;
        }
        ++sink;
      }
      // The row's per-face values are REPLICATED across exactly the
      // triangles the row produced: an n-gon fans into n-2 triangles,
      // and a face naming a vertex that does not exist produces NONE.
      // So the lanes stay in lockstep with triangleCount() by
      // construction, and every byte allocated here is backed by index
      // data that was actually read.
      for (size_t p = 0; faceTriangles > 0 && p < faceLanes.size(); ++p)
        if (faceLanes[p].lane)
          faceLanes[p].lane->insert(faceLanes[p].lane->end(), faceTriangles,
                                    (float)(faceRow[p] * faceLanes[p].scale));
    }
  }

  if (mesh.positions.empty()) return std::nullopt;

  foldSuffixedLanes(part.scalarLanes, part.vectorLanes, part.colorLanes);

  // The PRIMITIVE class comes home to Mesh::prims — its OWN container,
  // triangleCount()-sized BY DEFINITION, so a per-face lane can never
  // be mistaken for a per-vertex one: Part's scalar/vector/color lanes
  // and asCloud() stay strictly point-class. Same folder as the point
  // lanes, then widened to the vec4 currency prims speak: a folded
  // color IS the vec4 (this is save::ply's own Color_r/_g/_b/_a leg),
  // a folded vector takes w = 0 (Mesh::append's pad for non-"Color"
  // lanes), and a lone scalar lands in .x — the "Id" convention.
  {
    std::map<std::string, std::vector<glm::vec3>, std::less<>> primVectors;
    std::map<std::string, std::vector<glm::vec4>, std::less<>> primColors;
    foldSuffixedLanes(primScalars, primVectors, primColors);
    const size_t tris = mesh.triangleCount();
    // A lane the file under- or over-supplied is DROPPED whole rather
    // than published at a lying cardinality — the same posture the
    // dropped-face path takes, and the reason a header that promises
    // face properties it never delivers cannot desync mesh.prims.
    const auto publish = [tris](const std::string& name, size_t n) {
      return tris > 0 && n == tris && !name.empty();
    };
    for (const auto& [name, lane] : primColors)
      if (publish(name, lane.size())) mesh.prims[name] = lane;
    for (const auto& [name, lane] : primVectors)
      if (publish(name, lane.size())) {
        std::vector<glm::vec4>& out = mesh.prims[name];
        out.resize(lane.size());
        for (size_t i = 0; i < lane.size(); ++i)
          out[i] = glm::vec4(lane[i], 0.0f);
      }
    for (const auto& [name, lane] : primScalars)
      if (publish(name, lane.size())) {
        std::vector<glm::vec4>& out = mesh.prims[name];
        out.assign(lane.size(), glm::vec4{0});
        for (size_t i = 0; i < lane.size(); ++i) out[i].x = lane[i];
      }
  }

  finishPart(part, hasNormals);
  Model out;
  out.parts.push_back(std::move(part));
  return out;
}

bool looksLikePly(std::string_view text) {
  return text.starts_with("ply") &&
         text.find("end_header") != std::string_view::npos;
}

bool looksLikeAsciiStl(std::string_view text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  return start != std::string_view::npos &&
         text.substr(start).starts_with("solid") &&
         text.find("facet") != std::string_view::npos;
}

// --- Alembic (Ogawa) ------------------------------------------------------
// The vfx cache format, read through the Alembic library — Ogawa core
// only (the optional HDF5 backend stays out; legacy archives come back
// nullopt). Every IPolyMesh in the hierarchy becomes a Part with its
// xform stack baked (the glTF posture); every IPoints a faceless Part
// (the PLY point-cloud posture). arbGeomParams land as named lanes.
// Sampling takes the NEAREST stored sample — no interpolation.

namespace Abc = Alembic::Abc;
namespace AbcA = Alembic::AbcCoreAbstract;
namespace AbcGeom = Alembic::AbcGeom;

/** A read-only streambuf over bytes the CALLER owns — no copy. Ogawa
 *  reads its archive lazily and by SEEK (a header, then jumps to group
 *  offsets), so an istream is the only shape the factory accepts; what
 *  it does not need is a second copy of the whole cache, which
 *  istringstream forces and which doubles peak memory on the caches
 *  this format exists for.
 *
 *  The get area IS the caller's buffer, so the only override that has
 *  to do real work is the seek pair (std::streambuf's own seekoff
 *  fails); sgetc/sbumpc/sgetn already read straight out of it.
 *
 *  LIFETIME: the buffer, the stream over it, and the caller's bytes all
 *  have to outlive the IArchive, because the archive keeps reading
 *  after getArchive() returns. Enforced by declaration ORDER in the one
 *  function that builds them — buf, then stream, then archive, all in
 *  the same scope, so destruction unwinds archive first. Do not hoist
 *  the archive out of that scope or hand it back to a caller. */
class NonOwningStreamBuf final : public std::streambuf {
 public:
  NonOwningStreamBuf(const void* bytes, size_t size) {
    // setg's non-const signature is a formality: nothing here writes,
    // and no overflow/pbackfail path can reach the pointers.
    char* begin = const_cast<char*>(static_cast<const char*>(bytes));
    setg(begin, begin, begin + size);
  }

 protected:
  std::streamsize showmanyc() override { return egptr() - gptr(); }

  pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                   std::ios_base::openmode which) override {
    if (!(which & std::ios_base::in)) return pos_type(off_type(-1));
    const off_type size = egptr() - eback();
    off_type target = off;
    if (dir == std::ios_base::cur)
      target += gptr() - eback();
    else if (dir == std::ios_base::end)
      target += size;
    // A seek PAST the end is a failure, not a clamp: Ogawa uses a
    // short read to detect truncation, and clamping would hand it a
    // valid-looking position into a file that has no such bytes.
    if (target < 0 || target > size) return pos_type(off_type(-1));
    setg(eback(), eback() + target, egptr());
    return pos_type(target);
  }

  pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
    return seekoff(off_type(pos), std::ios_base::beg, which);
  }
};

/** XformSample matrices are row-vector M44d; glm's mat4 is column-
 *  vector. Reading the 16 doubles in memory order through make_mat4
 *  IS the row->column conversion — no explicit transpose — so the
 *  walk composes world * local like any glm stack. */
glm::mat4 toGlm(const Abc::M44d& m) {
  return glm::mat4(glm::make_mat4(&m[0][0]));
}

/** Where an output vertex came from — the indices the geom-param
 *  scopes select by. */
struct AbcVertexSource {
  int32_t point = 0;   ///< index into the sample's positions
  int32_t corner = 0;  ///< index into faceIndices, FILE order
  int32_t face = 0;    ///< the face the vertex was first seen on
};

/** One part's sampling domain: a source per output vertex plus the
 *  counts each scope validates against. corners == 0 marks a faceless
 *  cloud — only constant and per-point scopes apply there. */
struct AbcDomain {
  std::vector<AbcVertexSource> sources;
  size_t points = 0;
  size_t corners = 0;
  size_t faces = 0;
};

/** Expand one geom param and hand each output vertex its value; the
 *  scope picks the source index — 0 for constant, the face for
 *  uniform, the point for varying/vertex, the corner for facevarying.
 *  Deliberate simplification: uniform values broadcast per-face
 *  THROUGH the dedup, so a vertex shared across faces keeps its first
 *  face's value. False when the scope cannot apply or the array runs
 *  short — the caller drops the lane rather than half-fill it. */
template <typename Param, typename Sink>
bool unpackAbcParam(const Param& param, const Abc::ISampleSelector& at,
                    const AbcDomain& domain, Sink&& sink) {
  typename Param::Sample sample = param.getExpandedValue(at);
  const auto values = sample.getVals();
  if (!values) return false;
  const AbcGeom::GeometryScope scope = param.getScope();
  const bool faceless = domain.corners == 0;
  size_t need = 0;
  switch (scope) {
    case AbcGeom::kConstantScope:
      need = 1;
      break;
    case AbcGeom::kUniformScope:
      if (faceless) return false;
      need = domain.faces;
      break;
    case AbcGeom::kVaryingScope:
    case AbcGeom::kVertexScope:
      need = domain.points;
      break;
    case AbcGeom::kFacevaryingScope:
      if (faceless) return false;
      need = domain.corners;
      break;
    default:
      return false;
  }
  if (need == 0 || values->size() < need) return false;
  for (size_t i = 0; i < domain.sources.size(); ++i) {
    const AbcVertexSource& v = domain.sources[i];
    const size_t index = scope == AbcGeom::kConstantScope  ? 0
                         : scope == AbcGeom::kUniformScope ? (size_t)v.face
                         : scope == AbcGeom::kFacevaryingScope
                             ? (size_t)v.corner
                             : (size_t)v.point;
    sink(i, (*values)[index]);
  }
  return true;
}

/** arbGeomParams into named lanes, routed by width like glTF/PLY:
 *  1 -> scalars (ints cast raw — ids stay ids), 3 -> vectors, 2 and
 *  4 -> colors (vec4; V2f zero-pads, C3f gets alpha 1). Names arrive
 *  verbatim — no Houdini renaming. */
void importAbcLanes(const Abc::ICompoundProperty& params,
                    const Abc::ISampleSelector& at, const AbcDomain& domain,
                    Part& part) {
  if (!params.valid()) return;
  const size_t n = domain.sources.size();
  for (size_t i = 0; i < params.getNumProperties(); ++i) {
    const AbcA::PropertyHeader& header = params.getPropertyHeader(i);
    const std::string& name = header.getName();
    const auto scalars = [&](const auto& param) {
      std::vector<float> lane(n, 0.0f);
      if (unpackAbcParam(param, at, domain, [&lane](size_t v, auto value) {
            lane[v] = (float)value;  // raw — ids stay ids
          }))
        part.scalarLanes[name] = std::move(lane);
    };
    const auto vectors = [&](const auto& param) {
      std::vector<glm::vec3> lane(n, glm::vec3{0});
      if (unpackAbcParam(param, at, domain,
                         [&lane](size_t v, const Imath::V3f& value) {
                           lane[v] = {value.x, value.y, value.z};
                         }))
        part.vectorLanes[name] = std::move(lane);
    };
    if (AbcGeom::IFloatGeomParam::matches(header))
      scalars(AbcGeom::IFloatGeomParam(params, name));
    else if (AbcGeom::IDoubleGeomParam::matches(header))
      scalars(AbcGeom::IDoubleGeomParam(params, name));
    else if (AbcGeom::IInt32GeomParam::matches(header))
      scalars(AbcGeom::IInt32GeomParam(params, name));
    else if (AbcGeom::IUInt32GeomParam::matches(header))
      scalars(AbcGeom::IUInt32GeomParam(params, name));
    else if (AbcGeom::IV3fGeomParam::matches(header))
      vectors(AbcGeom::IV3fGeomParam(params, name));
    else if (AbcGeom::IP3fGeomParam::matches(header))
      vectors(AbcGeom::IP3fGeomParam(params, name));
    else if (AbcGeom::IN3fGeomParam::matches(header))
      vectors(AbcGeom::IN3fGeomParam(params, name));
    else if (AbcGeom::IC3fGeomParam::matches(header)) {
      std::vector<glm::vec4> lane(n, glm::vec4{0});
      if (unpackAbcParam(AbcGeom::IC3fGeomParam(params, name), at, domain,
                         [&lane](size_t v, const Imath::C3f& value) {
                           lane[v] = {value.x, value.y, value.z, 1};
                         }))
        part.colorLanes[name] = std::move(lane);
    } else if (AbcGeom::IC4fGeomParam::matches(header)) {
      std::vector<glm::vec4> lane(n, glm::vec4{0});
      if (unpackAbcParam(AbcGeom::IC4fGeomParam(params, name), at, domain,
                         [&lane](size_t v, const Imath::C4f& value) {
                           lane[v] = {value.r, value.g, value.b, value.a};
                         }))
        part.colorLanes[name] = std::move(lane);
    } else if (AbcGeom::IV2fGeomParam::matches(header)) {
      // Width 2 rides the color lane zero-padded — the routing rule.
      std::vector<glm::vec4> lane(n, glm::vec4{0});
      if (unpackAbcParam(AbcGeom::IV2fGeomParam(params, name), at, domain,
                         [&lane](size_t v, const Imath::V2f& value) {
                           lane[v] = {value.x, value.y, 0, 0};
                         }))
        part.colorLanes[name] = std::move(lane);
    }
  }
}

/** One IPolyMesh at one time into a Part: n-gons fan-triangulated
 *  after reversing each face's corners (Alembic winds CLOCKWISE, Mesh
 *  is CCW), vertices deduplicated OBJ-style on (point, uv, normal)
 *  sources. Deliberate simplification: custom facevarying lanes ride
 *  that same key, so corners agreeing on point+uv+normal share one
 *  lane slot. */
void importAbcMesh(AbcGeom::IPolyMesh object, const glm::mat4& world,
                   const Abc::ISampleSelector& at, Model& out) {
  AbcGeom::IPolyMeshSchema& schema = object.getSchema();
  AbcGeom::IPolyMeshSchema::Sample sample;
  schema.get(sample, at);
  const Abc::P3fArraySamplePtr positions = sample.getPositions();
  const Abc::Int32ArraySamplePtr faceIndices = sample.getFaceIndices();
  const Abc::Int32ArraySamplePtr faceCounts = sample.getFaceCounts();
  // The quiet policy hands back empty samples, never throws.
  if (!positions || !faceIndices || !faceCounts) return;
  const size_t numPoints = positions->size();
  const size_t numCorners = faceIndices->size();

  // UV/normal sources: -1 absent, else per-point or per-corner by the
  // declared scope (kVertex/kVarying carry one value per POINT here).
  const auto classify = [&](AbcGeom::GeometryScope scope, size_t count) {
    if ((scope == AbcGeom::kVertexScope || scope == AbcGeom::kVaryingScope) &&
        count >= numPoints)
      return 0;
    if (scope == AbcGeom::kFacevaryingScope && count >= numCorners) return 1;
    return -1;
  };
  Abc::V2fArraySamplePtr uvs;
  int uvMode = -1;
  if (AbcGeom::IV2fGeomParam param = schema.getUVsParam(); param.valid()) {
    uvs = param.getExpandedValue(at).getVals();
    if (uvs) uvMode = classify(param.getScope(), uvs->size());
  }
  Abc::N3fArraySamplePtr normals;
  int normalMode = -1;
  if (AbcGeom::IN3fGeomParam param = schema.getNormalsParam(); param.valid()) {
    normals = param.getExpandedValue(at).getVals();
    if (normals) normalMode = classify(param.getScope(), normals->size());
  }

  Part part;
  part.name = object.getName();
  Mesh& mesh = part.mesh;
  AbcDomain domain;
  domain.points = numPoints;
  domain.corners = numCorners;
  domain.faces = faceCounts->size();

  std::map<std::array<int32_t, 3>, uint32_t> seen;
  std::vector<uint32_t> ring;
  size_t cursor = 0;
  for (size_t face = 0; face < faceCounts->size(); ++face) {
    const int32_t count = (*faceCounts)[face];
    if (count < 0 || cursor + (size_t)count > numCorners)
      return;  // inconsistent topology — drop the whole part
    if (count < 3) {
      cursor += (size_t)count;
      continue;
    }
    ring.clear();
    for (int32_t j = 0; j < count; ++j) {
      // Reverse the face's corner list for the CW -> CCW flip;
      // `corner` keeps the FILE's index — facevarying arrays are
      // addressed by it.
      const size_t corner = cursor + (size_t)(count - 1 - j);
      const int32_t point = (*faceIndices)[corner];
      if (point < 0 || (size_t)point >= numPoints) return;
      const int32_t uvSource = uvMode < 0    ? -1
                               : uvMode == 0 ? point
                                             : (int32_t)corner;
      const int32_t normalSource = normalMode < 0    ? -1
                                   : normalMode == 0 ? point
                                                     : (int32_t)corner;
      const std::array<int32_t, 3> key = {point, uvSource, normalSource};
      auto [it, inserted] = seen.emplace(key, (uint32_t)mesh.positions.size());
      if (inserted) {
        const Imath::V3f& p = (*positions)[point];
        mesh.positions.push_back({p.x, p.y, p.z});
        if (normalSource >= 0) {
          const Imath::V3f& nrm = (*normals)[normalSource];
          mesh.normals.push_back({nrm.x, nrm.y, nrm.z});
        }
        if (uvSource >= 0) {
          const Imath::V2f& uv = (*uvs)[uvSource];
          // Alembic uv origin is bottom-left; Mesh is IMAGE
          // convention (top-left), so v flips — the OBJ rule.
          mesh.uvs.push_back({uv.x, 1 - uv.y});
        } else {
          mesh.uvs.push_back({0, 0});
        }
        domain.sources.push_back({point, (int32_t)corner, (int32_t)face});
      }
      ring.push_back(it->second);
    }
    for (size_t j = 1; j + 1 < ring.size(); ++j)
      mesh.indices.insert(mesh.indices.end(), {ring[0], ring[j], ring[j + 1]});
    cursor += (size_t)count;
  }

  // Conventional member: velocities land verbatim as a lane.
  if (const Abc::V3fArraySamplePtr velocities = sample.getVelocities();
      velocities && velocities->size() >= numPoints) {
    std::vector<glm::vec3>& lane = part.vectorLanes["velocity"];
    lane.resize(domain.sources.size());
    for (size_t i = 0; i < domain.sources.size(); ++i) {
      const Imath::V3f& v = (*velocities)[domain.sources[i].point];
      lane[i] = {v.x, v.y, v.z};
    }
  }
  importAbcLanes(schema.getArbGeomParams(), at, domain, part);

  finishPart(part, normalMode >= 0);
  part.mesh.transform(world);  // bake AFTER finishing — lanes verbatim
  if (!part.mesh.indices.empty()) out.parts.push_back(std::move(part));
}

/** One IPoints at one time into a faceless Part — the PLY point-cloud
 *  posture: empty indices, asCloud() the natural consumer. ids and
 *  widths keep their file names ("id" casts uint64 to float, exact
 *  only to 2^24 — a stated trade); velocities land as "velocity". */
void importAbcPoints(AbcGeom::IPoints object, const glm::mat4& world,
                     const Abc::ISampleSelector& at, Model& out) {
  AbcGeom::IPointsSchema& schema = object.getSchema();
  AbcGeom::IPointsSchema::Sample sample;
  schema.get(sample, at);
  const Abc::P3fArraySamplePtr positions = sample.getPositions();
  if (!positions || positions->size() == 0) return;
  const size_t n = positions->size();

  Part part;
  part.name = object.getName();
  Mesh& mesh = part.mesh;
  mesh.positions.resize(n);
  AbcDomain domain;
  domain.points = n;  // corners/faces stay 0: faceless
  domain.sources.resize(n);
  for (size_t i = 0; i < n; ++i) {
    const Imath::V3f& p = (*positions)[i];
    mesh.positions[i] = {p.x, p.y, p.z};
    domain.sources[i].point = (int32_t)i;
  }

  if (const Abc::UInt64ArraySamplePtr ids = sample.getIds();
      ids && ids->size() >= n) {
    std::vector<float>& lane = part.scalarLanes["id"];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i) lane[i] = (float)(*ids)[i];
  }
  if (const Abc::V3fArraySamplePtr velocities = sample.getVelocities();
      velocities && velocities->size() >= n) {
    std::vector<glm::vec3>& lane = part.vectorLanes["velocity"];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i) {
      const Imath::V3f& v = (*velocities)[i];
      lane[i] = {v.x, v.y, v.z};
    }
  }
  if (AbcGeom::IFloatGeomParam widths = schema.getWidthsParam();
      widths.valid()) {
    std::vector<float> lane(n, 0.0f);
    if (unpackAbcParam(widths, at, domain,
                       [&lane](size_t v, float value) { lane[v] = value; }))
      part.scalarLanes["width"] = std::move(lane);
  }
  importAbcLanes(schema.getArbGeomParams(), at, domain, part);

  finishPart(part, false);  // faceless: same posture as importPly
  part.mesh.transform(world);
  out.parts.push_back(std::move(part));
}

/** Depth-first over the hierarchy, xform stack carried as glm and
 *  baked at the leaves — the glTF posture. Honors each xform's
 *  inherits flag; unknown object types recurse transparently. */
void walkAlembic(const Abc::IObject& parent, const glm::mat4& world,
                 const Abc::ISampleSelector& at, Model& out) {
  for (size_t i = 0; i < parent.getNumChildren(); ++i) {
    const AbcA::ObjectHeader& header = parent.getChildHeader(i);
    if (AbcGeom::IXform::matches(header)) {
      AbcGeom::IXform xform(parent, header.getName());
      AbcGeom::XformSample xs = xform.getSchema().getValue(at);
      const glm::mat4 local = toGlm(xs.getMatrix());
      walkAlembic(xform, xs.getInheritsXforms() ? world * local : local, at,
                  out);
    } else if (AbcGeom::IPolyMesh::matches(header)) {
      importAbcMesh(AbcGeom::IPolyMesh(parent, header.getName()), world, at,
                    out);
    } else if (AbcGeom::IPoints::matches(header)) {
      importAbcPoints(AbcGeom::IPoints(parent, header.getName()), world, at,
                      out);
    } else {
      walkAlembic(Abc::IObject(parent, header.getName()), world, at, out);
    }
  }
}

}  // namespace

// --- Model ----------------------------------------------------------------

Cloud Part::asCloud() const {
  Cloud out;
  out.positions = mesh.positions;
  const size_t n = out.positions.size();
  std::vector<float>& t = out.scalar("t");
  for (size_t i = 0; i < n; ++i)
    t[i] = n > 1 ? (float)i / (float)(n - 1) : 0.0f;
  if (mesh.normals.size() == n) out.vectors["normal"] = mesh.normals;
  if (mesh.uvs.size() == n) {
    std::vector<glm::vec4>& uv = out.color("uv", {0, 0, 0, 0});
    for (size_t i = 0; i < n; ++i) uv[i] = {mesh.uvs[i].x, mesh.uvs[i].y, 0, 0};
  }
  if (mesh.colors.size() == n) out.colors["tint"] = mesh.colors;
  for (const auto& [name, lane] : scalarLanes)
    if (lane.size() == n) out.scalars[name] = lane;
  for (const auto& [name, lane] : vectorLanes)
    if (lane.size() == n) out.vectors[name] = lane;
  for (const auto& [name, lane] : colorLanes)
    if (lane.size() == n) out.colors[name] = lane;
  return out;
}

int Model::materialSlotCount() const {
  int slots = 0;
  for (const Part& part : parts)
    slots = std::max(slots, part.materialIndex + 1);
  return slots;
}

Cloud Model::mergedCloud() const {
  Cloud out;
  for (const Part& part : parts) out.append(part.asCloud());
  return out;
}

size_t Model::vertexCount() const {
  size_t count = 0;
  for (const Part& part : parts) count += part.mesh.vertexCount();
  return count;
}

size_t Model::triangleCount() const {
  size_t count = 0;
  for (const Part& part : parts) count += part.mesh.triangleCount();
  return count;
}

void Model::bounds(glm::vec3* lo, glm::vec3* hi) const {
  glm::vec3 mn = {std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
  glm::vec3 mx = {-mn.x, -mn.y, -mn.z};
  bool any = false;
  for (const Part& part : parts) {
    if (part.mesh.positions.empty()) continue;
    glm::vec3 partLo, partHi;
    part.mesh.bounds(&partLo, &partHi);
    mn = {std::min(mn.x, partLo.x), std::min(mn.y, partLo.y),
          std::min(mn.z, partLo.z)};
    mx = {std::max(mx.x, partHi.x), std::max(mx.y, partHi.y),
          std::max(mx.z, partHi.z)};
    any = true;
  }
  if (!any) mn = mx = {0, 0, 0};
  *lo = mn;
  *hi = mx;
}

Mesh Model::merged() const {
  Mesh out;
  for (const Part& part : parts) {
    Mesh mesh = part.mesh;
    if (!(part.baseColor == kWhite)) {
      if (mesh.colors.empty())
        mesh.colors.assign(mesh.positions.size(), part.baseColor);
      else
        for (glm::vec4& c : mesh.colors) c *= part.baseColor;
    }
    out.append(mesh);
  }
  return out;
}

glm::mat4 Model::fitTransform(float size) const {
  glm::vec3 lo, hi;
  bounds(&lo, &hi);
  const glm::vec3 center = (lo + hi) * 0.5f;
  const glm::vec3 extent = hi - lo;
  const float largest = std::max(extent.x, std::max(extent.y, extent.z));
  const float scale = largest > 1e-12f ? size / largest : 1;
  return glm::scale(glm::mat4(1.0f), {scale, scale, scale}) *
         glm::translate(glm::mat4(1.0f), -center);
}

// --- entry points ---------------------------------------------------------

std::optional<Model> alembic(const void* bytes, size_t size,
                             const AlembicOptions& options) {
  if (!bytes || size == 0) return std::nullopt;
  // The library can still throw on malformed input despite the quiet
  // policy below — one net turns every failure into nullopt.
  try {
    // The buffer and the stream must outlive the archive; all three
    // stay stack-local IN THIS ORDER, so unwinding destroys the archive
    // first, and the archive is never returned or stored. The buffer is
    // non-owning — the caller's bytes are read in place, never copied.
    NonOwningStreamBuf buf(bytes, size);
    std::istream stream(&buf);
    Alembic::AbcCoreFactory::IFactory factory;
    factory.setPolicy(Alembic::Abc::ErrorHandler::kQuietNoopPolicy);
    Alembic::AbcCoreFactory::IFactory::CoreType core =
        Alembic::AbcCoreFactory::IFactory::kUnknown;
    std::vector<std::istream*> streams = {&stream};
    Alembic::Abc::IArchive archive = factory.getArchive(streams, core);
    // HDF5-cored archives need the backend we deliberately don't
    // build — they land here as kUnknown/invalid, honest nullopt.
    if (!archive.valid()) return std::nullopt;
    const Abc::ISampleSelector at(options.time,
                                  Abc::ISampleSelector::kNearIndex);
    Model out;
    walkAlembic(archive.getTop(), glm::mat4(1.0f), at, out);
    if (out.parts.empty()) return std::nullopt;
    return out;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<Model> model(const void* bytes, size_t size,
                           std::string_view pathHint, const Resolver& resolve) {
  if (!bytes || size == 0) return std::nullopt;
  const auto* data = static_cast<const std::byte*>(bytes);
  const std::string ext = lowerExtension(pathHint);
  if (ext == "obj") return importObj(asText(bytes, size), resolve);
  if (ext == "gltf" || ext == "glb")
    return importGltf(bytes, size, pathHint, resolve);
  if (ext == "stl") return importStl(data, size);
  if (ext == "ply") return importPly(data, size);
  if (ext == "abc") return alembic(bytes, size);
  if (ext == "geo") return importHoudiniGeo(asText(bytes, size));

  // No useful extension: sniff. GLB and Ogawa magics and JSON are
  // unambiguous; binary STL is identified by its size arithmetic.
  if (size >= 4 && std::memcmp(bytes, "glTF", 4) == 0)
    return importGltf(bytes, size, pathHint, resolve);
  if (size >= 5 && std::memcmp(bytes, "Ogawa", 5) == 0)
    return alembic(bytes, size);
  const std::string_view text = asText(bytes, size);
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start != std::string_view::npos && text[start] == '{')
    return importGltf(bytes, size, pathHint, resolve);
  if (looksLikeHoudiniGeo(text)) return importHoudiniGeo(text);
  if (looksLikePly(text)) return importPly(data, size);
  if (looksLikeBinaryStl(data, size) || looksLikeAsciiStl(text))
    return importStl(data, size);
  return std::nullopt;
}

std::optional<Model> model(const std::filesystem::path& file) {
  std::ifstream stream(file, std::ios::binary | std::ios::ate);
  if (!stream) return std::nullopt;
  const std::streamsize size = stream.tellg();
  stream.seekg(0);
  std::vector<std::byte> bytes((size_t)size);
  if (!stream.read(reinterpret_cast<char*>(bytes.data()), size))
    return std::nullopt;
  const std::filesystem::path dir = file.parent_path();
  const Resolver siblings =
      [dir](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    std::ifstream ref(dir / std::filesystem::path(std::string(uri)),
                      std::ios::binary | std::ios::ate);
    if (!ref) return std::nullopt;
    const std::streamsize refSize = ref.tellg();
    ref.seekg(0);
    std::vector<std::byte> refBytes((size_t)refSize);
    if (!ref.read(reinterpret_cast<char*>(refBytes.data()), refSize))
      return std::nullopt;
    return refBytes;
  };
  return model(bytes.data(), bytes.size(), file.filename().string(), siblings);
}

}  // namespace sigil::shape::import
