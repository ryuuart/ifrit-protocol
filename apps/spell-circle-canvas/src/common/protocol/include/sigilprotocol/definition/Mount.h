#pragma once

/** @file
 * @ingroup protocol-definition
 * THE HOST'S SIDE OF A COMMAND: what a generated `wire()` mounts, and
 * the three steps every handler takes — read the parameters as their
 * table, ask the agent, answer its result as text — written once, so a
 * generated handler is one call.
 */

#include <sigildata/values/Values.h>
#include <sigilprotocol/definition/Answer.h>

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::protocol {

/** Where a handler sends a command's answer as text, once: the result's
 *  JSON form, or the error. */
using Respond = std::function<void(Answer<std::string>)>;

/** ONE COMMAND AS A DISPATCHER HOLDS IT: the parameters' JSON text in,
 *  one answer out through the respond it is handed, now or after frames
 *  have been drawn. */
using Handler =
    std::function<void(std::string_view parameters, Respond respond)>;

/** WHAT A GENERATED `wire()` MOUNTS ONTO: anything that files a handler
 *  under a method name — `clock.step`. The endpoint and the in-process
 *  dispatcher are two; a test's own map is a third. */
template <class Endpoint>
concept Mounts =
    requires(Endpoint& endpoint, std::string method, Handler handler) {
      endpoint.mount(std::move(method), std::move(handler));
    };

/** Where an event goes out: its method name and its table's JSON text. */
using Emit =
    std::function<void(std::string_view method, std::string parameters)>;

/** THE PARAMETERS @p text CARRIES, read as the command's table. Nothing
 *  where they do not fit, and then @p respond has already been handed
 *  invalidParameters naming @p method and what the reader stopped at;
 *  no text at all is the empty table. */
template <class Parameters>
std::optional<Parameters> readParameters(std::string_view method,
                                         std::string_view text,
                                         const Respond& respond) {
  std::string why;
  std::optional<Parameters> read = data::values::fromJson<Parameters>(
      text.empty() ? std::string_view("{}") : text, &why);
  if (!read)
    respond(
        refusal(ErrorCode_invalidParameters, std::string(method) + ": " + why));
  return read;
}

/** @p answer AS TEXT: the result's JSON form, or the error as it came. A
 *  result its own table cannot hold is failed, naming @p method. */
template <class Result>
Answer<std::string> answerText(std::string_view method, Answer<Result> answer) {
  if (!answer) return answer.error();
  std::string why;
  std::optional<std::string> text = data::values::toJson(answer.result(), &why);
  if (!text)
    return refusal(ErrorCode_failed,
                   std::string(method) +
                       " answered a result its table cannot hold: " + why);
  return std::move(*text);
}

/** A HANDLER FOR A COMMAND ITS AGENT ANSWERS AT ONCE: the parameters
 *  read, @p call asked with them, and its answer sent as text.
 *  Parameters that do not fit never reach @p call. */
template <class Parameters, class Result, class Call>
  requires std::invocable<Call&, const Parameters&>
Handler answerNow(std::string method, Call call) {
  return [method = std::move(method), call = std::move(call)](
             std::string_view text, Respond respond) mutable {
    const std::optional<Parameters> parameters =
        readParameters<Parameters>(method, text, respond);
    if (!parameters) return;
    respond(answerText<Result>(method, call(*parameters)));
  };
}

/** A HANDLER FOR A COMMAND ITS AGENT ANSWERS LATER, through the reply
 *  @p call is handed: the answer is sent as text whenever that reply is
 *  called. Parameters that do not fit never reach @p call. */
template <class Parameters, class Result, class Call>
  requires std::invocable<Call&, const Parameters&, Reply<Result>>
Handler answerLater(std::string method, Call call) {
  return [method = std::move(method), call = std::move(call)](
             std::string_view text, Respond respond) mutable {
    const std::optional<Parameters> parameters =
        readParameters<Parameters>(method, text, respond);
    if (!parameters) return;
    call(*parameters, [method, respond](Answer<Result> answer) {
      respond(answerText<Result>(method, std::move(answer)));
    });
  };
}

/** AN EVENT SENT: @p event's JSON form under @p method through @p send.
 *  False, and nothing sent, where the event's table cannot hold it or
 *  there is nowhere to send it; the sender is made to look at that. */
template <class Event>
[[nodiscard]] bool emitEvent(const Emit& send, std::string_view method,
                             const Event& event) {
  if (!send) return false;
  std::optional<std::string> text = data::values::toJson(event);
  if (!text) return false;
  send(method, std::move(*text));
  return true;
}

}  // namespace sigil::protocol
