#include <sigildata/decode/Json.h>
#include <sigilprotocol/dispatch/InProcess.h>

#include <optional>
#include <utility>
#include <vector>

namespace sigil::protocol {

/** ONE CLIENT AS BOTH ENDS SEE IT: the dispatcher it is attached to,
 *  while that stands, its session, and the listeners its caller filed
 *  by method. */
struct InProcess::Client::State {
  InProcess* dispatcher = nullptr;
  std::string session;
  std::map<std::string, std::vector<std::function<void(std::string_view)>>,
           std::less<>>
      listeners;
};

InProcess::Client::Client(std::shared_ptr<State> state)
    : m_state(std::move(state)) {}

InProcess::Client::Client(Client&&) noexcept = default;

InProcess::Client& InProcess::Client::operator=(Client&& other) noexcept {
  if (this != &other) {
    detach();
    m_state = std::move(other.m_state);
  }
  return *this;
}

InProcess::Client::~Client() { detach(); }

const std::string& InProcess::Client::session() const {
  return m_state->session;
}

Caller InProcess::Client::caller() const {
  Caller out;
  const std::weak_ptr<State> held = m_state;
  out.call = [held](std::string method, std::string parameters,
                    Respond respond) {
    const std::shared_ptr<State> state = held.lock();
    if (!state || !state->dispatcher) {
      respond(clientRefusal(ErrorCode_notSent, method,
                            "the in-process dispatcher is gone"));
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

void InProcess::Client::send(
    std::string_view envelope,
    std::function<void(std::string answer)> hear) const {
  if (!m_state) return;
  // Text that is no JSON is handed on as nothing, which the dispatcher
  // answers as a message that is no request.
  const std::optional<data::Json> read = data::decodeJson(envelope);
  if (!m_state->dispatcher) {
    // Nowhere to send is the client's own refusal, answered in the
    // envelope a dispatcher would have used, under the request's id.
    const data::Json id = read ? (*read)["id"] : data::Json();
    const std::string method =
        read ? std::string((*read)["method"].text()) : std::string("a request");
    const data::Json answered(data::Json::Object{
        {"id", id},
        {"session", data::Json(m_state->session)},
        {"error",
         data::Json(data::Json::Object{
             {"code",
              data::Json(std::string(EnumNameErrorCode(ErrorCode_notSent)))},
             {"message",
              data::Json(clientRefusal(ErrorCode_notSent, method,
                                       "the in-process dispatcher is gone")
                             .message)}})}});
    hear(data::encodeJson(answered));
    return;
  }
  m_state->dispatcher->request(m_state->session, read ? *read : data::Json(),
                               std::move(hear));
}

void InProcess::Client::detach() {
  if (!m_state || !m_state->dispatcher) return;
  InProcess* const dispatcher = std::exchange(m_state->dispatcher, nullptr);
  dispatcher->m_clients.erase(m_state->session);
  dispatcher->detach(m_state->session);
}

InProcess::InProcess(Program program) : Dispatcher(std::move(program)) {}

InProcess::~InProcess() {
  // Copied first: letting a client go takes it out of the map.
  const auto clients = m_clients;
  for (const auto& [session, held] : clients) {
    Dispatcher::detach(session, "the host is closing");
    if (const std::shared_ptr<Client::State> state = held.lock())
      state->dispatcher = nullptr;
  }
}

InProcess::Client InProcess::connect() {
  auto state = std::make_shared<Client::State>();
  state->dispatcher = this;
  state->session = attach();
  m_clients[state->session] = state;
  return Client(std::move(state));
}

void InProcess::deliver(const std::string& session, std::string_view method,
                        std::string_view parameters) {
  const auto found = m_clients.find(session);
  if (found == m_clients.end()) return;
  const std::shared_ptr<Client::State> state = found->second.lock();
  if (!state) return;
  const auto listening = state->listeners.find(method);
  if (listening == state->listeners.end()) return;
  // Copied: a listener may file another, which moves the list.
  const std::vector<std::function<void(std::string_view)>> listeners =
      listening->second;
  for (const auto& listener : listeners) listener(parameters);
}

}  // namespace sigil::protocol
