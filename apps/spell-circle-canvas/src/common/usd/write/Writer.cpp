/** @file
 * The stage's lifetime: /World and /World/Materials defined with the
 * up axis and unit scale, unique prim paths handed out, and the file
 * written in the form its extension asks for — a layer exported, or a
 * package built around one.
 */

#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdUtils/usdzPackage.h>

#include <cctype>
#include <string>
#include <system_error>

#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

std::string identifier(std::string_view name) {
  std::string out;
  for (char c : name)
    out += (std::isalnum((unsigned char)c) || c == '_') ? c : '_';
  if (out.empty() || std::isdigit((unsigned char)out[0])) out = "_" + out;
  return out;
}

std::string Writer::Impl::uniquePath(std::string_view parent,
                                     std::string_view name) {
  std::string base = std::string(parent) + "/" + identifier(name);
  std::string path = base;
  for (int n = 2; usedPaths.count(path); ++n)
    path = base + "_" + std::to_string(n);
  usedPaths.insert(path);
  return path;
}

Writer::Writer(const std::filesystem::path& file, WriteOptions options)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->file = file;
  m_impl->options = std::move(options);
  m_impl->stage = UsdStage::CreateInMemory();
  if (m_impl->stage) {
    UsdGeomXform world = UsdGeomXform::Define(m_impl->stage, SdfPath("/World"));
    m_impl->stage->SetDefaultPrim(world.GetPrim());
    UsdGeomSetStageUpAxis(m_impl->stage, UsdGeomTokens->y);
    UsdGeomSetStageMetersPerUnit(m_impl->stage, m_impl->options.metersPerUnit);
    UsdGeomScope::Define(m_impl->stage, SdfPath("/World/Materials"));
    m_impl->usedPaths.insert("/World");
    m_impl->usedPaths.insert("/World/Materials");
  }
}

Writer::~Writer() = default;

void Writer::Impl::removeWrittenImages() {
  std::error_code ec;
  const std::filesystem::path root = file.parent_path();
  for (const auto& [image, asset] : writtenImages)
    std::filesystem::remove(root / asset, ec);
  writtenImages.clear();
  const std::filesystem::path dir = root / textureDir();
  if (texturesDirReady && std::filesystem::is_empty(dir, ec))
    std::filesystem::remove(dir, ec);
}

namespace {

/** @p file's extension, lower-cased, with its dot — so a name written
 *  `.USDZ` asks for the same thing `.usdz` does. */
std::string extensionOf(const std::filesystem::path& file) {
  std::string extension = file.extension().string();
  for (char& c : extension) c = (char)std::tolower((unsigned char)c);
  return extension;
}

/** A crate path beside @p package that nothing is standing on. The
 *  packager reads an asset off disk, and this is where that asset is put
 *  — beside the package rather than in a directory of its own, because
 *  the images the stage names are already beside the package and their
 *  paths are relative to the layer that names them. */
std::filesystem::path stagingPath(const std::filesystem::path& package) {
  const std::filesystem::path root = package.parent_path();
  const std::string stem = package.stem().string();
  std::filesystem::path candidate = root / (stem + ".usdc");
  for (int n = 2; std::filesystem::exists(candidate); ++n)
    candidate = root / (stem + "_" + std::to_string(n) + ".usdc");
  return candidate;
}

}  // namespace

bool Writer::save(std::string* error) {
  Impl& impl = *m_impl;
  if (!impl.stage) {
    if (error) *error = "no stage";
    return false;
  }
  std::error_code ec;
  if (!impl.file.parent_path().empty())
    std::filesystem::create_directories(impl.file.parent_path(), ec);

  // A PACKAGE IS NOT A LAYER. `.usdc`, `.usda` and `.usd` are layers and
  // a layer exports itself; a `.usdz` is an archive OF a layer and
  // everything that layer refers to, and exporting a root layer onto one
  // is refused. So the layer goes out as a crate first, beside where the
  // package will stand, which is where the images this writer already
  // wrote are and therefore where the stage's relative asset paths
  // resolve. The packager then localizes the layer and pulls the images
  // in, the staged crate goes, and what is left is the one file.
  if (extensionOf(impl.file) == ".usdz") {
    const std::filesystem::path staged = stagingPath(impl.file);
    if (!impl.stage->GetRootLayer()->Export(staged.string())) {
      if (error) *error = "USD refused to write " + staged.string();
      return false;
    }
    // What a consumer opening the package sees, whatever the staged file
    // had to be called to stand on nothing.
    const std::string firstLayer = impl.file.stem().string() + ".usdc";
    const bool packaged = UsdUtilsCreateNewUsdzPackage(
        SdfAssetPath(staged.string()), impl.file.string(), firstLayer);
    std::filesystem::remove(staged, ec);
    if (!packaged) {
      if (error) *error = "USD refused to package " + impl.file.string();
      return false;
    }
    // The images are inside the archive now, and a package that left
    // copies of them beside it would not be the one file it exists to
    // be.
    impl.removeWrittenImages();
    return true;
  }

  // Export writes the file the extension asks for: .usdc (crate), .usda
  // (ascii), .usd (crate).
  if (!impl.stage->GetRootLayer()->Export(impl.file.string())) {
    if (error) *error = "USD refused to write " + impl.file.string();
    return false;
  }
  return true;
}

}  // namespace sigil::usd
