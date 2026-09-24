#pragma once

/** @file
 * @ingroup protocol-runtime
 * THE HOST DOMAIN, answered by this library itself: every dispatcher
 * mounts one, so `host.describe` — the first command a client sends —
 * answers on every host before any other agent is written.
 */

#include <sigilprotocol/host/HostAgent.h>

namespace sigil::protocol {

class Dispatcher;

/** THE HOST ITSELF, as a dispatcher knows it: the definition's revision
 *  and the program's name and version, the domains mounted and the
 *  clients attached, which the dispatcher knows, and the clock's policy,
 *  the state root and the sessions open, which the program supplies. */
class HostDomain final : public host::HostAgent {
 public:
  /** The host domain of @p dispatcher, which outlives it. */
  explicit HostDomain(const Dispatcher& dispatcher);

  /** Everything at once: the version, the domains mounted, the clock's
   *  policy, the state root, the sessions open and the clients
   *  attached. */
  Answer<host::values::DescribeResult> describe() override;

  /** Where the program keeps its state. */
  Answer<host::values::StateRootResult> stateRoot() override;

  /** The definition's revision, and the program's name and version. */
  Answer<host::values::VersionResult> version() override;

 private:
  const Dispatcher& m_dispatcher;
};

}  // namespace sigil::protocol
