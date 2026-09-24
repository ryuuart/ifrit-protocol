#pragma once

/** @file
 * @ingroup protocol-runtime
 * THE DISPATCHER WITH NO SOCKET: a client attached in the same process
 * speaks to a host's dispatcher through a caller, and a test drives a
 * host's agents exactly as a script over the endpoint would, the same
 * text crossing the same dispatcher — beside whatever clients the
 * endpoint has attached to it.
 */

#include <sigilprotocol/definition/Call.h>
#include <sigilprotocol/dispatch/Dispatcher.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace sigil::protocol {

/** ONE CLIENT ATTACHED IN PROCESS: its session, the caller a generated
 *  client speaks through, and the envelope text a socket would carry.
 *  Letting it go detaches it, as a socket closing does.
 *  @trap A client its dispatcher let go, one that detached itself and
 *  one moved from send nowhere: every call and every envelope is refused
 *  notSent, in the client's own words saying which. */
class InProcess {
 public:
  /** A new client, attached to @p dispatcher under a session of its
   *  own. */
  explicit InProcess(Dispatcher& dispatcher);
  InProcess(InProcess&&) noexcept;
  InProcess& operator=(InProcess&&) noexcept;
  InProcess(const InProcess&) = delete;
  InProcess& operator=(const InProcess&) = delete;
  ~InProcess();

  /** The session id the dispatcher answers this client under; empty
   *  for one moved from. */
  const std::string& session() const;

  /** WHAT A GENERATED CLIENT SPEAKS THROUGH: each call answered by the
   *  dispatcher under this session, each listener handed its method's
   *  events once this client has enabled their domain. `refused` is
   *  left for the caller to set. */
  Caller caller() const;

  /** ONE ENVELOPE'S TEXT, answered as envelope text: the socket's words
   *  with no socket, so a case proves what a script would be told. Text
   *  that is no JSON is answered invalidRequest. @p hear is called once,
   *  whether the dispatcher answers or this client refuses. */
  void send(std::string_view envelope,
            std::function<void(std::string answer)> hear) const;

  /** Detaches now: this client's events stop and what it alone set is
   *  cleared. Once. */
  void detach();

 private:
  struct State;
  std::shared_ptr<State> m_state;
};

}  // namespace sigil::protocol
