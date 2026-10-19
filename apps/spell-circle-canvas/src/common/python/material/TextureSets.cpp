/** @file
 * `sigil.material.texture`'s texture sets: the folder a material
 * authoring tool exports, read back as textures by role. `Role`,
 * `classify` and `roleForUsage` read the role words; `TextureSet` and
 * `discover` group a directory's files into sets; `TextureMaps`,
 * `Volume`, `fromFiles` and `fromUsageMap` decode a set into one
 * repeating texture per role; and `material.surface.program` takes the
 * decoded maps as its C++ twin does.
 */

#include <include/core/SkImage.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilio/source/Source.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/TextureSet.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Decode.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/image/Decode.h>
#include <sigilmedia/video/Video.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sigil::python {
namespace py = pybind11;

namespace {

namespace texture = material::texture;

/** The pixels a Python decoder answered: a decoded image, a video, a
 *  picture, or None for a file it could not read. */
media::PixelSource pixelsAnswered(py::handle value) {
  if (value.is_none()) return {};
  if (py::isinstance<media::Image>(value))
    return media::PixelSource(std::shared_ptr<const media::Image>(
        value.cast<std::shared_ptr<media::Image>>()));
  if (py::isinstance<media::Video>(value))
    return media::PixelSource(std::shared_ptr<const media::Video>(
        value.cast<std::shared_ptr<media::Video>>()));
  return media::PixelSource(value.cast<sk_sp<SkImage>>());
}

/** The file at @p path read whole and decoded as an image, or an empty
 *  source when it cannot be read or is not one. */
media::PixelSource decodedFile(const std::filesystem::path& path) {
  const std::optional<io::Bytes> bytes = io::readBytes(path);
  if (!bytes) return {};
  auto image = media::decode<media::Image>(bytes->span(), {}, path);
  if (!image) return {};
  return media::PixelSource(std::move(image));
}

/** The decoder a Python author handed over, holding the interpreter lock
 *  while it runs; None decodes each file as an image from disk. */
texture::Decoder decoderOf(py::object decode) {
  if (decode.is_none()) return decodedFile;
  return [decode = std::move(decode)](const std::filesystem::path& path) {
    py::gil_scoped_acquire lock;
    return pixelsAnswered(decode(path));
  };
}

}  // namespace

void bindMaterialTextureSets(py::module_& module) {
  auto textures = submodule(module, "material.texture");
  auto surface = submodule(module, "material.surface");

  py::enum_<texture::Role>(textures, "Role")
      .value("Unknown", texture::Role::Unknown)
      .value("BaseColor", texture::Role::BaseColor)
      .value("Normal", texture::Role::Normal)
      .value("Roughness", texture::Role::Roughness)
      .value("Metallic", texture::Role::Metallic)
      .value("Occlusion", texture::Role::Occlusion)
      .value("Emissive", texture::Role::Emissive)
      .value("Packed", texture::Role::Packed)
      .value("Height", texture::Role::Height)
      .value("Opacity", texture::Role::Opacity)
      .value("Specular", texture::Role::Specular)
      .value("Clearcoat", texture::Role::Clearcoat)
      .value("Sheen", texture::Role::Sheen)
      .value("SheenSpecular", texture::Role::SheenSpecular)
      .value("SpecularColor", texture::Role::SpecularColor)
      .value("Anisotropy", texture::Role::Anisotropy)
      .value("Iridescence", texture::Role::Iridescence)
      .value("ClearcoatNormal", texture::Role::ClearcoatNormal)
      .value("Transmission", texture::Role::Transmission)
      .value("Thickness", texture::Role::Thickness)
      .value("Distortion", texture::Role::Distortion)
      .value("Volume", texture::Role::Volume)
      .value("Attenuation", texture::Role::Attenuation);
  textures.def(
      "name",
      [](texture::Role role) { return std::string(texture::name(role)); },
      py::arg("role"), "The role's name as messages and keys spell it.");

  bindRecord<texture::Classified>(textures, "Classified",
                                  "Unknown Classified field: ")
      .def_readwrite("role", &texture::Classified::role)
      .def_readwrite("set", &texture::Classified::set)
      .def_readwrite("directX", &texture::Classified::directX);
  textures.def(
      "classify",
      [](const std::string& filename) { return texture::classify(filename); },
      py::arg("filename"),
      "What one texture file's name says it is: its role, the set it "
      "belongs to and whether its normal map is DirectX's.");
  textures.def(
      "roleForUsage",
      [](const std::string& usage) { return texture::roleForUsage(usage); },
      py::arg("usage"),
      "The role a channel usage word names, or Unknown.");

  bindRecord<texture::TextureSet>(textures, "TextureSet",
                                  "Unknown TextureSet field: ")
      .def_readwrite("name", &texture::TextureSet::name)
      .def_readwrite("files", &texture::TextureSet::files)
      .def_readwrite("normalDirectX", &texture::TextureSet::normalDirectX);
  textures.def("discover", &texture::discover, py::arg("directory"),
               "Every set in a directory, not recursive: image files grouped "
               "by the stem their role word leaves behind, sorted by name.");

  bindRecord<texture::Volume>(textures, "Volume", "Unknown Volume field: ")
      .def_readwrite("ior", &texture::Volume::ior)
      .def_property(
          "attenuationColor",
          [](const texture::Volume& self) { return self.attenuationColor; },
          [](texture::Volume& self, py::handle value) {
            self.attenuationColor = material::Color(color(value));
          })
      .def_readwrite("attenuationDistance",
                     &texture::Volume::attenuationDistance)
      .def_readwrite("dispersion", &texture::Volume::dispersion)
      .def(py::self == py::self);

  bindRecord<texture::TextureMaps>(textures, "TextureMaps",
                                   "Unknown TextureMaps field: ")
      .def_readwrite("name", &texture::TextureMaps::name)
      .def_readwrite("maps", &texture::TextureMaps::maps)
      .def_readwrite("normalDirectX", &texture::TextureMaps::normalDirectX)
      .def_readwrite("emissiveStrength",
                     &texture::TextureMaps::emissiveStrength)
      .def_readwrite("thickness", &texture::TextureMaps::thickness)
      .def_readwrite("distortionStrength",
                     &texture::TextureMaps::distortionStrength)
      .def_readwrite("volume", &texture::TextureMaps::volume)
      .def(
          "map",
          [](const texture::TextureMaps& self,
             texture::Role role) -> std::optional<material::Texture> {
            if (const material::Texture* found = self.map(role)) return *found;
            return std::nullopt;
          },
          py::arg("role"),
          "The texture for a role, or None when the set has none.");

  textures.def(
      "fromFiles",
      [](const texture::TextureSet& set, py::object decode) {
        const texture::Decoder decoder = decoderOf(std::move(decode));
        return texture::fromFiles(set, decoder);
      },
      py::arg("set"), py::kw_only(), py::arg("decode") = py::none(),
      "Every file of a set decoded into a texture that repeats on both "
      "axes. `decode` takes a path and answers an image, a video, a picture "
      "or None; left out, each file is read from disk and decoded as an "
      "image. A file that does not decode leaves its role out.");
  textures.def(
      "fromUsageMap",
      [](const std::map<std::string, py::object>& byUsage, bool normalDirectX,
         const std::map<std::string, std::string>& statedColorSpaces) {
        std::map<std::string, media::PixelSource> pixels;
        for (const auto& [usage, value] : byUsage)
          pixels.emplace(usage, pixelsAnswered(value));
        return texture::fromUsageMap(pixels, normalDirectX, statedColorSpaces);
      },
      py::arg("byUsage"), py::kw_only(), py::arg("normalDirectX") = true,
      py::arg("statedColorSpaces") = std::map<std::string, std::string>{},
      "Images keyed by channel usage word as textures by role; the first "
      "word naming a role wins.");

  surface.def(
      "program",
      py::overload_cast<const texture::TextureMaps&,
                        material::surface::SurfaceParameters>(
          &material::surface::program),
      py::arg("maps"), py::kw_only(),
      py::arg("base") = material::surface::SurfaceParameters{},
      "The lit surface program over a decoded texture set: each map in its "
      "slot over the base parameters.");
}

}  // namespace sigil::python
