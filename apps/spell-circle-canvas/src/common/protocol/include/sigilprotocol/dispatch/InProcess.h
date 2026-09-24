#pragma once

/** @file
 * @ingroup protocol-runtime
 * THE DISPATCHER WITH NO SOCKET: a client attached in the same process
 * speaks to it through a caller, and a test drives a host's agents
 * exactly as a script over the endpoint would, the same text crossing
 * the same dispatcher.
 */

#include <sigilprotocol/definition/Call.h>
#include <sigilprotocol/dispatch/Dispatcher.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace sigil::protocol {

/** A DISPATCHER CLIENTS ATTACH TO IN PROCESS, one session each. */
class InProcess final : public Dispatcher {
 public:
  /** ONE CLIENT ATTACHED: its session, the caller a generated client
   *  speaks through, and the envelope text a socket would carry. Letting
   *  it go detaches it, as a socket closing does.
   *  @trap A client outliving its dispatcher sends nowhere: every call
   *  and every envelope is refused notSent, in the client's own
   *  words. */
  class Client {
   public:
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client();

    /** The session id the dispatcher answers this client under. */
    const std::string& session() const;

    /** WHAT A GENERATED CLIENT SPEAKS THROUGH: each call answered by the
     *  dispatcher under this session, each listener handed its method's
     *  events once this client has enabled their domain. `refused` is
     *  left for the caller to set. */
    Caller caller() const;

    /** ONE ENVELOPE'S TEXT, answered as envelope text: the socket's
     *  words with no socket, so a case proves what a script would be
     *  told. Text that is no JSON is answered invalidRequest. */
    void send(std::string_view envelope,
              std::function<void(std::string answer)> hear) const;

    /** Detaches now: this client's events stop and what it alone set is
     *  cleared. Once. */
    void detach();

   private:
    friend class InProcess;
    struct State;
    explicit Client(std::shared_ptr<State> state);
    std::shared_ptr<State> m_state;
  };

  /** A dispatcher answering for @p program, no client attached. */
  explicit InProcess(Program program = {});
  /** Lets every client still attached go, telling each that enabled
   *  `host` that the host is closing. */
  ~InProcess() override;

  /** A new client, attached under a session of its own. */
  Client connect();

 protected:
  void deliver(const std::string& session, std::string_view method,
               std::string_view parameters) override;

 private:
  /** Every client attached, by session, known weakly: a client lets
   *  itself go. */
  std::map<std::string, std::weak_ptr<Client::State>, std::less<>> m_clients;
};

}  // namespace sigil::protocol
