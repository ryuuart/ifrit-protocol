/** @file
 * A headless host on the protocol, served until the process is told to
 * end.
 */

#include "ServeLane.h"

#include <sigilio/hub/Hub.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/endpoint/Endpoint.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/live/agent/HostAgents.h>
#include <sigilsketch/python/Python.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include "Arguments.h"
#include "Inspection.h"
#include "SketchbookProgram.h"
#include "Startup.h"

namespace protocol = sigil::protocol;
namespace sketch = sigil::sketch;

namespace {

/** Set by an interrupt or a termination signal: the loop ends at its next
 *  turn, so the endpoint takes its address file back on the way out. */
std::atomic<bool> g_ending{false};

void endServing(int) { g_ending.store(true); }

/** One turn of the loop, the rate a window draws at. */
constexpr std::chrono::microseconds kTurn{16667};

}  // namespace

int runServe(const Arguments& args, const std::filesystem::path& flagsFile,
             std::future<sigil::material::WarmupResult>& materialWarmup) {
  finishMaterialWarmup(materialWarmup);
  const std::filesystem::path stateRoot = inspectionStateRoot();
  if (stateRoot.empty()) {
    std::fprintf(stderr,
                 "--inspect: no state root to write the address under; "
                 "name one with --state\n");
    return 1;
  }
  SharedWebEngineScope sharedWebEngine;
  sketch::installCrashReporter({});
  int result = 0;
  {
    sigil::io::Hub hub;
    protocol::Dispatcher dispatcher(sketchbookProgram(stateRoot));
    sketch::SessionAgentOptions session;
    session.fonts = &fonts();
    session.sketchesDirectory = SIGIL_SKETCH_DIR;
    session.assetsDirectory =
        args.assetsOverride.empty()
            ? std::filesystem::path(SIGIL_SKETCH_ASSET_DIR)
            : args.assetsOverride;
    session.flagsFile = flagsFile;
    session.pythonLoader = &sketch::python::load;
    sketch::HostAgents agents(dispatcher, std::move(session),
                              sketch::CatalogSources{SIGIL_SKETCH_DIR, {}, {}});
    protocol::EndpointPolicy policy;
    policy.port = args.inspectPort.value_or(0);
    protocol::Endpoint endpoint(hub, dispatcher, policy);
    if (!endpoint.listening()) {
      std::fprintf(stderr, "[sketchbook] %s\n", endpoint.error().c_str());
      result = 1;
    } else {
      std::printf("protocol at %s\n", endpoint.address().c_str());
      std::fflush(stdout);
      std::signal(SIGINT, endServing);
      std::signal(SIGTERM, endServing);
      auto next = std::chrono::steady_clock::now();
      while (!g_ending.load()) {
        hub.advance();
        agents.frame();
        next += kTurn;
        const auto now = std::chrono::steady_clock::now();
        if (next < now) next = now;
        std::this_thread::sleep_until(next);
      }
    }
  }
  sharedWebEngine.shutdown();
  releaseDevice();
  return result;
}
