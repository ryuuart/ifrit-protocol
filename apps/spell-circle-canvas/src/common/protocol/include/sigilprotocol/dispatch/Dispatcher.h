#pragma once

/** @file
 * @ingroup protocol-runtime
 * THE DISPATCHER: every handler a host mounts, filed by method; the
 * clients attached, one session each, however each arrived; which of
 * them has enabled which domain's events; and the refusals no agent is
 * asked for. A host holds one and mounts its agents on it once; clients
 * in the same process attach through `InProcess`, clients on a socket
 * through the `Endpoint` put on it, and a command crosses the same code
 * whichever way it came.
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

/** HOW ONE CLIENT IS REACHED: what the way it arrived attaches it with.
 *  It stands beside the dispatcher rather than inside it so a transport
 *  names it without naming the class. */
struct Attachment {
  /** Hands the client one event: the session it is attached as, the
   *  event's method and its parameters' JSON text. The in-process form
   *  calls the client's listeners; the endpoint writes the event's
   *  envelope to the client's peer. */
  std::function<void(const std::string& session, std::string_view method,
                     std::string_view parameters)>
      deliver;
  /** Runs once as the dispatcher lets the client go, after its events
   *  have stopped: with the reason where the host let it go, and empty
   *  where the client left on its own. May be empty. */
  std::function<void(std::string_view reason)> released;
};

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
  /** Lets every client still attached go, telling each that enabled
   *  `host` that the host is closing. No onDetach() listener runs: what
   *  an agent keyed by session goes with the host.
   *  @trap An endpoint put on this dispatcher is let go before it. */
  ~Dispatcher();

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
  Emit events();

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

  /** …for an agent that answers part of what `host.describe` reads — the
   *  clock's policy, the sessions open — to fill in as it is mounted and
   *  take back as it goes, so no host restates what its agents know. */
  Program& program();

  /** A NEW CLIENT, reached as @p attachment says and answered under the
   *  session id this returns: what the way a client arrives calls as it
   *  does. */
  std::string attach(Attachment attachment);

  /** Lets @p session go: its events stop, every onDetach() listener
   *  runs, its attachment is released, and a reply answered for it
   *  afterwards goes nowhere. Where @p reason is not empty and the client
   *  has enabled `host`, it is told first, through `host.detached` — the
   *  host letting a client go says why; a client that left needs no
   *  telling. A session not attached is left alone. */
  void detach(const std::string& session, std::string_view reason = {});

 private:
  /** One event's @p parameters text under @p method, to @p session
   *  alone, through its attachment. */
  void deliver(const std::string& session, std::string_view method,
               std::string_view parameters);

  /** Everything a dispatcher holds, behind one pointer that a reply
   *  answered later knows weakly: a reply outliving its dispatcher
   *  answers nothing rather than reaching into what is gone. */
  struct State;
  std::shared_ptr<State> m_state;
  std::unique_ptr<HostDomain> m_host;
};

}  // namespace sigil::protocol
