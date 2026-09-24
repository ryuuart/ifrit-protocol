#pragma once

/** @file
 * @ingroup protocol-runtime
 * THE DISPATCHER: every handler a host mounts, filed by method; the
 * clients attached, one session each; which of them has enabled which
 * domain's events; and the refusals no agent is asked for. The endpoint
 * puts it behind a socket and the in-process form behind a caller, so a
 * command crosses the same code whichever way it came.
 */

#include <sigildata/decode/Json.h>
#include <sigilprotocol/definition/Mount.h>
#include <sigilprotocol/dispatch/Program.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::protocol {

class HostDomain;

/** ONE DISPATCHER: handlers by method, sessions by client, events by
 *  domain. It answers `host` itself, through the library's own
 *  HostDomain, and every domain's `enable` and `disable`; a method the
 *  definition does not declare is refused with methodNotFound, and one
 *  of a domain no agent is mounted for with notMounted, each naming the
 *  method, so no command waits on an agent that is not there.
 *  @trap ONE THREAD: every command is answered, every event sent and
 *  every reply called on the thread that drives the dispatcher — the
 *  frame's, inside the hub's dispatch, for an endpoint. */
class Dispatcher {
 public:
  /** A dispatcher answering for @p program, with its host domain
   *  mounted. */
  explicit Dispatcher(Program program = {});
  virtual ~Dispatcher();

  Dispatcher(const Dispatcher&) = delete;
  Dispatcher& operator=(const Dispatcher&) = delete;

  /** Files @p handler under @p method, `clock.step`, replacing whatever
   *  stood there; what a generated `wire()` calls. */
  void mount(std::string method, Handler handler);

  /** THE DOMAINS MOUNTED: every domain one of whose commands has a
   *  handler, `host` always among them, in alphabetical order. */
  std::vector<std::string> domains() const;

  /** The clients attached, one session id each, in the order they
   *  attached. */
  std::vector<std::string> sessions() const;

  /** THE SESSION WHOSE COMMAND IS BEING ANSWERED, inside a handler;
   *  empty outside one. An agent that sets something for one client
   *  alone keys it by this, and takes it along into a reply it answers
   *  later. */
  const std::string& asking() const;

  /** Whether @p session has enabled @p domain's events and not disabled
   *  them since. */
  bool enabled(std::string_view session, std::string_view domain) const;

  /** Runs @p listener with the id of every session that detaches, after
   *  its events have stopped and before it is forgotten, so an agent
   *  clears what that client alone set.
   *  @trap A listener stands as long as the dispatcher does, as a
   *  mounted handler does. */
  void onDetach(std::function<void(const std::string& session)> listener);

  /** WHERE A DOMAIN'S EVENTS GO OUT: what a generated `<Domain>Events`
   *  is made with. An event reaches every session that has enabled its
   *  domain and no other.
   *  @trap It speaks to this dispatcher, so it is used only while the
   *  dispatcher stands. */
  Emit emit();

  /** ONE COMMAND ANSWERED FOR @p session: @p method with its parameters'
   *  JSON text, the answer handed to @p respond once. What a typed
   *  client's caller does in process. */
  void answer(const std::string& session, std::string_view method,
              std::string_view parameters, Respond respond);

  /** ONE REQUEST ANSWERED FOR @p session: @p envelope is
   *  `{"id", "session", "method", "parameters"}`, and @p send is handed
   *  the answer's envelope text once — `{"id", "session", "result"}` or
   *  `{"id", "session", "error"}`. What a socket carries. A message that
   *  is no object, or carries no id or no method, or names a session
   *  other than @p session, is answered invalidRequest. */
  void request(const std::string& session, const data::Json& envelope,
               std::function<void(std::string answer)> send);

  /** The program this dispatcher answers for. */
  const Program& program() const;

 protected:
  /** A new client, answered under the session id this returns. */
  std::string attach();

  /** Lets @p session go: its events stop, every onDetach() listener
   *  runs, and a reply answered for it afterwards goes nowhere. Where
   *  @p reason is not empty and the client has enabled `host`, it is
   *  told first, through `host.detached` — the host letting a client go
   *  says why; a client that left needs no telling. */
  void detach(const std::string& session, std::string_view reason = {});

  /** One event's @p parameters text under @p method, to @p session
   *  alone: the socket writes it to that client's peer, the in-process
   *  form to that client's listeners. */
  virtual void deliver(const std::string& session, std::string_view method,
                       std::string_view parameters) = 0;

 private:
  /** Everything a dispatcher holds, behind one pointer that a reply
   *  answered later knows weakly: a reply outliving its dispatcher
   *  answers nothing rather than reaching into what is gone. */
  struct State;
  std::shared_ptr<State> m_state;
  std::unique_ptr<HostDomain> m_host;
};

}  // namespace sigil::protocol
