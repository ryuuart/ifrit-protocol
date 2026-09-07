#pragma once

/** @file
 * The opt-in process-owned WebEngine a sketch host configures and several
 * sketches may borrow.
 *
 * This is a SigilSketch integration, not SigilScry's ordinary ownership
 * model. A standalone consumer creates its explicitly configured engine with
 * `sigil::scry::WebEngine::create()` and owns that value directly. A host
 * whose sketches must coexist calls `configureSharedEngine()` once before it
 * opens any of them; the first `sharedEngine()` call then boots exactly that
 * configuration and every later call returns the same engine.
 */

#include <sigilscry/engine/WebEngine.h>

#include <memory>

namespace sigil::sketch::scry {

/** Selects the one configuration the sketch host will share without booting
 * the engine yet. False when this process has already configured, started or
 * shut down the shared path. */
bool configureSharedEngine(::sigil::scry::WebEngineConfig config);

/** The host-configured engine, created lazily on the first call. Null when no
 * host configured it, its one bring-up attempt failed, or it was shut down. */
[[nodiscard]] std::shared_ptr<::sigil::scry::WebEngine> sharedEngine();

/** Releases the shared path's ownership. Every sketch session and view must
 * be released first. Ultralight does not permit another renderer afterward,
 * so shutdown is final for this process. */
void shutdownSharedEngine();

}  // namespace sigil::sketch::scry
