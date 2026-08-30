#pragma once

/** @file
 * Whether the OpenUSD runtime this library was built against is usable
 * in this process: the file formats a stage is written to and read from
 * are plugins, discovered on disk when USD first runs, and a build whose
 * plugin registry is missing links and starts but can open nothing.
 */

#include <string>

namespace sigil::usd::runtime {

/**
 * True when a stage can be created and the crate (`.usdc`), ASCII
 * (`.usda`) and package (`.usdz`) formats are all registered. When
 * false, @p why names what is missing. Cheap after the first call: USD
 * discovers its plugins once per process.
 */
bool available(std::string* why = nullptr);

}  // namespace sigil::usd::runtime
