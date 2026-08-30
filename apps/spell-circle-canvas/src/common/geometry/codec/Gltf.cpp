/** @file
 * The glTF 2.0 reader, .gltf and .glb alike: cgltf parses the JSON and
 * the binary container, buffers and images are reached wherever they
 * live (a GLB chunk, a data: URI, an external file through the
 * resolver), node transforms are baked into the vertices, and the whole
 * metallic-roughness material rides onto the Part with its custom
 * per-vertex attributes as named lanes.
 */

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "Internal.h"

namespace sigil::geometry::decode::detail {

namespace {

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
}  // namespace

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
  // cgltf_free released everything cgltf_parse allocated.
  // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
  if (out.parts.empty()) return std::nullopt;
  return out;
}

}  // namespace sigil::geometry::decode::detail
