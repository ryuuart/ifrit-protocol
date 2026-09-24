#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/dispatch/HostDomain.h>

namespace sigil::protocol {

HostDomain::HostDomain(const Dispatcher& dispatcher)
    : m_dispatcher(dispatcher) {}

Answer<host::values::DescribeResult> HostDomain::describe() {
  const Program& program = m_dispatcher.program();
  host::values::DescribeResult described;
  described.version = version().result();
  described.domains = m_dispatcher.domains();
  // A program with no clock of its own to report runs by the wall's.
  described.clock =
      program.clockPolicy ? program.clockPolicy() : clock::Policy_Wall;
  described.state_root = program.stateRoot.string();
  if (program.sessions) described.sessions = program.sessions();
  described.attached = m_dispatcher.sessions();
  return described;
}

Answer<host::values::StateRootResult> HostDomain::stateRoot() {
  host::values::StateRootResult root;
  root.path = m_dispatcher.program().stateRoot.string();
  return root;
}

Answer<host::values::VersionResult> HostDomain::version() {
  host::values::VersionResult answered;
  // The revision a value made with nothing set carries IS the revision
  // of the definition this build was made from.
  answered.revision = values::Revision{};
  answered.program = m_dispatcher.program().name;
  answered.program_version = m_dispatcher.program().version;
  return answered;
}

}  // namespace sigil::protocol
