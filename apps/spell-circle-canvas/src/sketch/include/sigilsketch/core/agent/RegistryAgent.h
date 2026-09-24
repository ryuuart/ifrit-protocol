#pragma once

/** @file
 * @ingroup sketch-core
 *
 * The registry domain's agent: the sketches a host can open and the rows
 * a browser shows for them, answered as the protocol's tables out of the
 * registry's own values.
 */

#include <sigilprotocol/registry/RegistryAgent.h>
#include <sigilsketch/core/Catalog.h>

namespace sigil::sketch {

/** THE REGISTRY, ANSWERED OVER THE PROTOCOL: `registry.list` is
 *  `registryRows()` and `registry.catalog` is `catalog()`, each row put
 *  into the definition's table and nothing added. It keeps no state
 *  beyond where the catalog's files stand, so one agent serves every
 *  client and any thread the host answers on.
 *
 *  A host mounts it with `sigil::protocol::registry::wire`. */
class RegistryAgent final : public protocol::registry::RegistryAgent {
 public:
  /** An agent whose catalog reads the registry's files under
   *  @p sources' sketch directory, and lists @p sources' files — the ones
   *  a run was pointed at — after the registry, before the ones a client
   *  names. */
  explicit RegistryAgent(CatalogSources sources = {});

  /** Every sketch this binary was built with, of one runtime or of all,
   *  in the registry's order; an unavailable one with its reason. */
  protocol::Answer<protocol::registry::values::ListResult> list(
      const protocol::registry::values::ListParameters& parameters) override;

  /** The rows a browser shows: the registry's, then the run's files, then
   *  the files asked for. A file opened by path has no kind until it is
   *  built, and no row carries a canvas: a sketch declares that from
   *  inside its own setup. */
  protocol::Answer<protocol::registry::values::CatalogResult> catalog(
      const protocol::registry::values::CatalogParameters& parameters)
      override;

 private:
  CatalogSources m_sources;
};

}  // namespace sigil::sketch
