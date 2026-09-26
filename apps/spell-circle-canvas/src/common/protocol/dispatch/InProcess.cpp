#include <sigildata/decode/Json.h>
#include <sigilprotocol/dispatch/InProcess.h>

#include "Parameters.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::protocol {
namespace {

/** Why a client moved from sends nowhere. */
constexpr std::string_view kMovedFrom = "this client was moved into another";

/** The envelope refusing @p request's text with @p why: the client's
 *  own notSent, under the request's id where it carried one, in the
 *  words a dispatcher's answer would have used. */
std::string nowhere(const std::optional<data::Json>& request,
                    std::string_view session, std::string_view why) {
  const data::Json id = request ? (*request)["id"] : data::Json();
  const std::string method = request && !(*request)["method"].null()
                                 ? std::string((*request)["method"].string())
                                 : std::string("a request");
  const data::Json answered(data::Json::Object{
      {"id", id},
      {"session", data::Json(std::string(session))},
      {"error",
       data::Json(data::Json::Object{
           {"code",
            data::Json(std::string(EnumNameErrorCode(ErrorCode_notSent)))},
           {"message", data::Json(clientRefusal(ErrorCode_notSent, method,
                                                why)
                                      .message)}})}});
  return jsonText(answered);
}

}  // namespace

/** ONE CLIENT AS BOTH ENDS SEE IT: the dispatcher it is attached to,
 *  while it is, its session, the listeners its caller filed by method,
 *  and why it sends nowhere once it does not. */
struct InProcess::State {
  Dispatcher* dispatcher = nullptr;
  std::string session;
  std::map<std::string, std::vector<std::function<void(std::string_view)>>,
           std::less<>>
      listeners;
  std::string gone;
};

InProcess::InProcess(Dispatcher& dispatcher)
    : m_state(std::make_shared<State>()) {
  const std::weak_ptr<State> held = m_state;
  Attachment attachment;
  attachment.deliver = [held](const std::string&, std::string_view method,
                              std::string_view parameters) {
    const std::shared_ptr<State> state = held.lock();
    if (!state) return;
    const auto listening = state->listeners.find(method);
    if (listening == state->listeners.end()) return;
    // Copied: a listener may file another, which moves the list.
    const std::vector<std::function<void(std::string_view)>> listeners =
        listening->second;
    for (const auto& listener : listeners) listener(parameters);
  };
  attachment.released = [held](std::string_view reason) {
    const std::shared_ptr<State> state = held.lock();
    if (!state) return;
    state->dispatcher = nullptr;
    state->gone = reason.empty()
                      ? std::string("this client has detached")
                      : "the host let this client go: " + std::string(reason);
  };
  m_state->dispatcher = &dispatcher;
  m_state->session = dispatcher.attach(std::move(attachment));
}

InProcess::InProcess(InProcess&&) noexcept = default;

InProcess& InProcess::operator=(InProcess&& other) noexcept {
  if (this != &other) {
    detach();
    m_state = std::move(other.m_state);
  }
  return *this;
}

InProcess::~InProcess() { detach(); }

const std::string& InProcess::session() const {
  static const std::string kNone;
  return m_state ? m_state->session : kNone;
}

Caller InProcess::caller() const {
  Caller out;
  const std::weak_ptr<State> held = m_state;
  out.call = [held](std::string method, std::string parameters,
                    Respond respond) {
    const std::shared_ptr<State> state = held.lock();
    if (!state) {
      respond(clientRefusal(ErrorCode_notSent, method, kMovedFrom));
      return;
    }
    if (!state->dispatcher) {
      respond(clientRefusal(ErrorCode_notSent, method, state->gone));
      return;
    }
    state->dispatcher->answer(state->session, method, parameters,
                              std::move(respond));
  };
  out.listen = [held](std::string method,
                      std::function<void(std::string_view)> hear) {
    if (const std::shared_ptr<State> state = held.lock())
      state->listeners[std::move(method)].push_back(std::move(hear));
  };
  return out;
}

void InProcess::send(std::string_view envelope,
                     std::function<void(std::string answer)> hear) const {
  // Text that is no JSON is handed on as nothing, which the dispatcher
  // answers as a message that is no request.
  const std::optional<data::Json> read = data::decode(envelope, data::Dialect::Json);
  if (!m_state) return hear(nowhere(read, {}, kMovedFrom));
  if (!m_state->dispatcher)
    return hear(nowhere(read, m_state->session, m_state->gone));
  m_state->dispatcher->request(m_state->session, read ? *read : data::Json(),
                               std::move(hear));
}

void InProcess::detach() {
  if (!m_state || !m_state->dispatcher) return;
  // The release the dispatcher runs is what marks this client gone; a
  // session it no longer held is marked here instead.
  m_state->dispatcher->detach(m_state->session);
  if (m_state->dispatcher) {
    m_state->dispatcher = nullptr;
    m_state->gone = "this client has detached";
  }
}

}  // namespace sigil::protocol
