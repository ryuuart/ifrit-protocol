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
 *
 * A shutdown returns the path to unconfigured, so a host may take its web
 * work down and stand it up again — the process's renderer outlives both.
 * What the FIRST bring-up fixed is the process's all the same: a later
 * configuration naming different resource roots, a different session store,
 * different threading or a different device is refused by SigilScry.
 */

#include <sigilscry/engine/WebEngine.h>

#include <memory>

namespace sigil::sketch::scry {

/** Selects the one configuration the sketch host will share without booting
 * the engine yet. False while the path is already configured or started;
 * a path that has been shut down takes a configuration again. */
bool configureSharedEngine(::sigil::scry::WebEngineConfig config);

/** The host-configured engine, created lazily on the first call. Null when no
 * host configured it, its bring-up attempt failed, or it was shut down and
 * nothing has configured it since. */
[[nodiscard]] std::shared_ptr<::sigil::scry::WebEngine> sharedEngine();

/** Releases the shared path's ownership and leaves it unconfigured. Every
 * sketch session and view must be released first: the engine ends when the
 * last handle does, and a host that configures again gets a new one over the
 * process's kept renderer. */
void shutdownSharedEngine();

}  // namespace sigil::sketch::scry
