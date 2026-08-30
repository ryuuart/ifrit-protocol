/** @file
 * The stage's lifetime: /World and /World/Materials defined with the
 * up axis and unit scale, unique prim paths handed out, and the file
 * exported in the format its extension asks for.
 */

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>

#include <cctype>

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

bool Writer::save(std::string* error) {
  Impl& impl = *m_impl;
  if (!impl.stage) {
    if (error) *error = "no stage";
    return false;
  }
  std::error_code ec;
  if (!impl.file.parent_path().empty())
    std::filesystem::create_directories(impl.file.parent_path(), ec);
  // Export writes the file the extension asks for: .usdc (crate), .usda
  // (ascii), .usd (crate), .usdz (a package).
  if (!impl.stage->GetRootLayer()->Export(impl.file.string())) {
    if (error) *error = "USD refused to write " + impl.file.string();
    return false;
  }
  return true;
}

}  // namespace sigil::usd
