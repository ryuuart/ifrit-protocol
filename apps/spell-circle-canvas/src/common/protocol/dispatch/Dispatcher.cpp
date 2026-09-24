#include <sigildata/decode/Json.h>
#include <sigildata/values/Values.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/dispatch/HostDomain.h>
#include <sigilprotocol/host/HostAgent.h>

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "Declared.h"
#include "Parameters.h"

namespace sigil::protocol {
namespace {

/** The two commands every domain with events has, which the dispatcher
 *  answers itself. */
constexpr std::string_view kEnable = "enable";
constexpr std::string_view kDisable = "disable";

/** A refusal the dispatcher made, its words opening with its seam. */
values::Error dispatcherRefusal(ErrorCode code, std::string_view what) {
  return refusal(code, "dispatcher: " + std::string(what));
}

/** @p error as the JSON an envelope carries it in: the code by its name
 *  in the definition, as the Error table's own JSON form writes it. */
data::Json errorJson(const values::Error& error) {
  return data::Json(data::Json::Object{
      {"code", data::Json(std::string(EnumNameErrorCode(error.code)))},
      {"message", data::Json(error.message)}});
}

/** The opening an answer's envelope shares: `{"id": …, "session": …,`. */
std::string opening(const data::Json& id, std::string_view session) {
  return "{\"id\":" + data::encodeJson(id) +
         ",\"session\":" + data::encodeJson(data::Json(std::string(session)));
}

/** An answer's envelope text: the result's JSON text as the handler
 *  wrote it, or the error. */
std::string answerEnvelope(const data::Json& id, std::string_view session,
                           const Answer<std::string>& answer) {
  if (answer)
    return opening(id, session) + ",\"result\":" + answer.result() + "}";
  return opening(id, session) +
         ",\"error\":" + data::encodeJson(errorJson(answer.error())) + "}";
}

}  // namespace

/** EVERYTHING A DISPATCHER HOLDS. */
struct Dispatcher::State {
  explicit State(Program answering) : program(std::move(answering)) {}

  Program program;
  std::map<std::string, Handler, std::less<>> handlers;
  /** Every session attached, in the order it attached, with the domains
   *  whose events it has enabled; a session is attached exactly while it
   *  stands here. */
  std::vector<std::pair<std::string, std::set<std::string, std::less<>>>>
      sessions;
  std::vector<std::function<void(const std::string&)>> detached;
  /** The session a handler is answering for, empty between. */
  std::string asking;
  /** How many sessions have been made, which is what a new id is
   *  numbered from: an id is never handed out twice. */
  uint64_t made = 0;

  /** The domains @p session has enabled; nothing where it is not
   *  attached. */
  std::set<std::string, std::less<>>* enabledOf(std::string_view session) {
    for (auto& [named, enabled] : sessions)
      if (named == session) return &enabled;
    return nullptr;
  }
};

Dispatcher::Dispatcher(Program program)
    : m_state(std::make_shared<State>(std::move(program))),
      m_host(std::make_unique<HostDomain>(*this)) {
  host::wire(*this, *m_host);
}

Dispatcher::~Dispatcher() = default;

void Dispatcher::mount(std::string method, Handler handler) {
  m_state->handlers[std::move(method)] = std::move(handler);
}

std::vector<std::string> Dispatcher::domains() const {
  std::set<std::string, std::less<>> mounted;
  for (const auto& [method, handler] : m_state->handlers)
    mounted.emplace(domainOf(method));
  return {mounted.begin(), mounted.end()};
}

std::vector<std::string> Dispatcher::sessions() const {
  std::vector<std::string> out;
  out.reserve(m_state->sessions.size());
  for (const auto& [session, enabled] : m_state->sessions)
    out.push_back(session);
  return out;
}

const std::string& Dispatcher::asking() const { return m_state->asking; }

bool Dispatcher::enabled(std::string_view session,
                         std::string_view domain) const {
  const std::set<std::string, std::less<>>* enabled =
      m_state->enabledOf(session);
  return enabled && enabled->contains(domain);
}

void Dispatcher::onDetach(
    std::function<void(const std::string& session)> listener) {
  m_state->detached.push_back(std::move(listener));
}

Emit Dispatcher::emit() {
  return [this](std::string_view method, std::string parameters) {
    const std::string_view domain = domainOf(method);
    // The list is copied first: a listener hearing the event may detach
    // its own client, which edits the list being walked.
    for (const std::string& session : sessions())
      if (enabled(session, domain)) deliver(session, method, parameters);
  };
}

void Dispatcher::answer(const std::string& session, std::string_view method,
                        std::string_view parameters, Respond respond) {
  // ONE COMMAND'S ANSWER, OWED ONCE, shared by every copy of the
  // respond the handler is handed: an answer after the first goes
  // nowhere, one for a session that has since detached goes nowhere, and
  // a reply let go without answering is answered failed as it goes — so
  // no command is left waiting on an agent that dropped it.
  struct Pending {
    Respond respond;
    std::string method;
    std::string session;
    std::weak_ptr<State> dispatcher;
    bool attached = false;
    bool answered = false;

    void give(Answer<std::string> answer) {
      if (answered) return;
      answered = true;
      // A session that has detached is nobody to answer: its socket is
      // gone, or its caller let go. One that was never attached is
      // answered the refusal saying so.
      const std::shared_ptr<State> living = dispatcher.lock();
      if (!living) return;
      if (attached && !living->enabledOf(session)) return;
      respond(std::move(answer));
    }
    ~Pending() {
      if (!answered)
        give(dispatcherRefusal(
            ErrorCode_failed,
            method + ": the agent let its reply go without answering"));
    }
  };
  const auto pending = std::make_shared<Pending>();
  pending->respond = std::move(respond);
  pending->method = std::string(method);
  pending->session = session;
  pending->dispatcher = m_state;
  pending->attached = m_state->enabledOf(session) != nullptr;
  const Respond once = [pending](Answer<std::string> answered) {
    pending->give(std::move(answered));
  };

  if (!pending->attached)
    return once(dispatcherRefusal(
        ErrorCode_invalidRequest,
        std::string(method) + ": no client is attached as session " + session));

  const Declared& definition = declared();
  if (!definition.commands.contains(method))
    return once(dispatcherRefusal(
        ErrorCode_methodNotFound,
        std::string(method) + ": the definition declares no such command"));

  const std::string_view domain = domainOf(method);
  const std::string_view command = method.substr(domain.size() + 1);
  const bool mounted = std::any_of(
      m_state->handlers.begin(), m_state->handlers.end(),
      [domain](const auto& entry) { return domainOf(entry.first) == domain; });

  // THE PARAMETERS ARE HELD AGAINST THEIR TABLE before anyone reads
  // them, so the refusal names the parameter it stopped at; what fits
  // here is read again by the handler, whose reading is the last word.
  if (!parameters.empty()) {
    const std::optional<data::Json> read = data::decodeJson(parameters);
    if (!read)
      return once(dispatcherRefusal(
          ErrorCode_invalidParameters,
          std::string(method) + ": the parameters are no JSON"));
    if (const std::optional<std::string> why = misfit(method, *read))
      return once(dispatcherRefusal(ErrorCode_invalidParameters,
                                    std::string(method) + ": " + *why));
  }

  // ENABLE AND DISABLE ARE THE DISPATCHER'S: they say which sessions a
  // domain's events reach, which no agent knows about.
  if (definition.eventful.contains(domain) &&
      (command == kEnable || command == kDisable)) {
    if (!mounted)
      return once(dispatcherRefusal(
          ErrorCode_notMounted, std::string(method) + ": this host mounts no " +
                                    std::string(domain) + " agent"));
    std::set<std::string, std::less<>>& enabled = *m_state->enabledOf(session);
    if (command == kEnable)
      enabled.emplace(domain);
    else
      enabled.erase(std::string(domain));
    return once(std::string("{}"));
  }

  const auto found = m_state->handlers.find(method);
  if (found == m_state->handlers.end())
    return once(dispatcherRefusal(
        ErrorCode_notMounted, std::string(method) + ": this host mounts no " +
                                  std::string(domain) +
                                  " agent that answers it"));

  // The handler is held, not referred to: an agent may mount another
  // while it answers, which moves the map it stands in.
  const Handler handler = found->second;
  const std::string before = std::exchange(m_state->asking, session);
  handler(parameters, once);
  m_state->asking = before;
}

void Dispatcher::request(const std::string& session, const data::Json& envelope,
                         std::function<void(std::string answer)> send) {
  const data::Json nothing;
  if (envelope.kind() != data::Json::Kind::Record)
    return send(
        answerEnvelope(nothing, session,
                       dispatcherRefusal(ErrorCode_invalidRequest,
                                         "the message is no JSON object")));
  const data::Json& id = envelope["id"];
  const data::Json& method = envelope["method"];
  const std::string named = method.kind() == data::Json::Kind::Text
                                ? std::string(method.text())
                                : std::string("a request");
  if (id.kind() != data::Json::Kind::Number &&
      id.kind() != data::Json::Kind::Text)
    return send(answerEnvelope(
        nothing, session,
        dispatcherRefusal(ErrorCode_invalidRequest,
                          named + ": the request carries no id")));
  if (method.kind() != data::Json::Kind::Text)
    return send(
        answerEnvelope(id, session,
                       dispatcherRefusal(ErrorCode_invalidRequest,
                                         "the request carries no method")));
  const data::Json& claimed = envelope["session"];
  if (!claimed.null() && claimed.text() != session)
    return send(answerEnvelope(
        id, session,
        dispatcherRefusal(ErrorCode_invalidRequest,
                          named + ": names session " +
                              std::string(claimed.text()) +
                              ", and this client's is " + session)));
  const data::Json& parameters = envelope["parameters"];
  const std::string text =
      parameters.null() ? std::string() : data::encodeJson(parameters);
  answer(session, method.text(), text,
         [id, session, send = std::move(send)](Answer<std::string> answered) {
           send(answerEnvelope(id, session, answered));
         });
}

const Program& Dispatcher::program() const { return m_state->program; }

std::string Dispatcher::attach() {
  std::string session = "session-" + std::to_string(++m_state->made);
  m_state->sessions.emplace_back(session, std::set<std::string, std::less<>>{});
  return session;
}

void Dispatcher::detach(const std::string& session, std::string_view reason) {
  std::set<std::string, std::less<>>* enabled = m_state->enabledOf(session);
  if (!enabled) return;
  if (!reason.empty() && enabled->contains(std::string_view("host"))) {
    host::values::DetachedEvent event;
    event.reason = std::string(reason);
    if (const std::optional<std::string> text = data::values::toJson(event))
      deliver(session, "host.detached", *text);
  }
  enabled->clear();
  // The listeners are copied: one may register another, which moves the
  // list being walked.
  const std::vector<std::function<void(const std::string&)>> listeners =
      m_state->detached;
  for (const auto& listener : listeners) listener(session);
  std::erase_if(m_state->sessions, [&session](const auto& entry) {
    return entry.first == session;
  });
}

}  // namespace sigil::protocol
