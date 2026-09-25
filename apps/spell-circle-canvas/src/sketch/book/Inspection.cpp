/** @file
 * Sketchbook's protocol state root, and the endpoint a run whose frames
 * are its own mounts.
 */

#include "Inspection.h"

#include <sigilio/hub/Hub.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/endpoint/Endpoint.h>
#include <sigilsketch/core/State.h>
#include <sigilsketch/core/agent/RegistryAgent.h>

#include <QtCore/QStandardPaths>
#include <cstdio>
#include <system_error>
#include <utility>

#include "SketchbookProgram.h"

namespace protocol = sigil::protocol;
namespace sketch = sigil::sketch;

struct Inspection::State {
  sigil::io::Hub hub;
  // Made in this order and let go in the reverse, so the endpoint goes
  // first and the hub it leases last.
  std::unique_ptr<protocol::Dispatcher> dispatcher;
  std::unique_ptr<sketch::RegistryAgent> registry;
  std::unique_ptr<protocol::Endpoint> endpoint;

  ~State() {
    endpoint.reset();
    registry.reset();
    dispatcher.reset();
  }
};

protocol::Program sketchbookProgram(std::filesystem::path stateRoot) {
  protocol::Program program;
  program.name = "Sketchbook";
  program.version = SIGIL_SKETCHBOOK_VERSION;
  program.stateRoot = std::move(stateRoot);
  return program;
}

std::filesystem::path inspectionStateRoot() {
  std::filesystem::path root = sketch::stateDirectory();
  if (root.empty())
    root =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            .toStdString();
  if (root.empty()) return {};
  std::error_code error;
  std::filesystem::create_directories(root, error);
  return error ? std::filesystem::path{} : root;
}

Inspection::Inspection(uint16_t port, sketch::CatalogSources catalog,
                       Sessions sessions)
    : m_state(std::make_unique<State>()) {
  protocol::Program program = sketchbookProgram(inspectionStateRoot());
  if (sessions)
    program.sessions = [sessions = std::move(sessions)] {
      std::vector<protocol::session::values::Summary> open;
      for (const OpenSession& session : sessions()) {
        protocol::session::values::Summary summary;
        summary.sketch = session.sketch;
        summary.kind = session.kind;
        summary.width = session.width;
        summary.height = session.height;
        summary.moment = session.moment;
        open.push_back(std::move(summary));
      }
      return open;
    };
  m_state->dispatcher =
      std::make_unique<protocol::Dispatcher>(std::move(program));
  m_state->registry =
      std::make_unique<sketch::RegistryAgent>(std::move(catalog));
  protocol::registry::wire(
      *m_state->dispatcher,
      static_cast<protocol::registry::RegistryAgent&>(*m_state->registry));
  protocol::EndpointPolicy policy;
  policy.port = port;
  m_state->endpoint = std::make_unique<protocol::Endpoint>(
      m_state->hub, *m_state->dispatcher, policy);
  if (m_state->endpoint->listening())
    std::fprintf(stderr, "[sketchbook] protocol at %s\n",
                 m_state->endpoint->address().c_str());
  else
    std::fprintf(stderr, "[sketchbook] %s\n",
                 m_state->endpoint->error().c_str());
}

Inspection::~Inspection() = default;

void Inspection::advance() { m_state->hub.advance(); }

bool Inspection::listening() const { return m_state->endpoint->listening(); }

protocol::Dispatcher& Inspection::dispatcher() { return *m_state->dispatcher; }
