/** @file
 * The registry's rows and the catalog's, put into the protocol's tables.
 */

#include "sigilsketch/core/agent/RegistryAgent.h"

#include <utility>

namespace sigil::sketch {

namespace {

namespace values = protocol::registry::values;

values::Entry entryOf(const RegistryRow& row) {
  values::Entry entry;
  entry.name = row.name;
  entry.stem = row.key;
  entry.kind = row.kind;
  entry.available = row.available;
  entry.reason = row.reason;
  return entry;
}

values::CatalogRow catalogRowOf(const CatalogRow& row) {
  values::CatalogRow out;
  out.name = row.name;
  out.key = row.key;
  out.folder = row.category;
  out.blurb = row.blurb;
  out.path = row.path.string();
  out.kind = row.kind;
  out.available = row.available;
  out.reason = row.reason;
  out.external = row.external;
  out.lines = row.source.lines > 0 ? (uint32_t)row.source.lines : 0u;
  out.subject = row.source.subject;
  out.tags = row.source.tags;
  out.video_exportable = row.videoExportable;
  return out;
}

}  // namespace

RegistryAgent::RegistryAgent(CatalogSources sources)
    : m_sources(std::move(sources)) {}

protocol::Answer<values::ListResult> RegistryAgent::list(
    const values::ListParameters& parameters) {
  values::ListResult result;
  for (const RegistryRow& row : registryRows(parameters.kind))
    result.sketches.push_back(entryOf(row));
  return result;
}

protocol::Answer<values::CatalogResult> RegistryAgent::catalog(
    const values::CatalogParameters& parameters) {
  CatalogSources sources = m_sources;
  for (const std::string& path : parameters.paths)
    sources.files.emplace_back(path);
  values::CatalogResult result;
  for (const CatalogRow& row : sketch::catalog(sources))
    result.rows.push_back(catalogRowOf(row));
  return result;
}

}  // namespace sigil::sketch
