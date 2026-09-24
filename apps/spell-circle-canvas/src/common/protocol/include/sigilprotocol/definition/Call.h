#pragma once

/** @file
 * @ingroup protocol-definition
 * THE CALLER'S SIDE OF A COMMAND: what a generated client speaks
 * through, and the two steps every client member takes — send the
 * parameters as their table's text, read the answer back as the
 * result's table — written once.
 */

#include <sigildata/values/Values.h>
#include <sigilprotocol/definition/Answer.h>
#include <sigilprotocol/definition/Mount.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::protocol {

/** WHAT A GENERATED CLIENT SPEAKS THROUGH: `call` sends one command, its
 *  method and its parameters' JSON text, and hands the answer to the
 *  respond it is given, once; `listen` hands every event of a method to
 *  a listener as its table's JSON text. A socket client and the
 *  in-process dispatcher are two. */
struct Caller {
  std::function<void(std::string method, std::string parameters,
                     Respond respond)>
      call;
  std::function<void(std::string method,
                     std::function<void(std::string_view parameters)> hear)>
      listen;
};

/** A COMMAND SENT: @p parameters as JSON under @p method, and the answer
 *  read back as Result and handed to @p reply. Parameters their own
 *  table cannot hold are refused here, before anything is sent; text
 *  that does not read as Result is failed, naming the method. */
template <class Result, class Parameters>
void callWith(const Caller& caller, std::string method,
              const Parameters& parameters, Reply<Result> reply) {
  std::string why;
  std::optional<std::string> text = data::values::toJson(parameters, &why);
  if (!text) {
    reply(refusal(ErrorCode_invalidParameters, method + ": " + why));
    return;
  }
  if (!caller.call) {
    reply(refusal(ErrorCode_failed, method + ": the caller sends nowhere"));
    return;
  }
  const std::string named = method;
  caller.call(std::move(method), std::move(*text),
              [named, reply = std::move(reply)](Answer<std::string> answer) {
                if (!answer) {
                  reply(answer.error());
                  return;
                }
                std::string trouble;
                std::optional<Result> result =
                    data::values::fromJson<Result>(answer.result(), &trouble);
                if (!result) {
                  reply(refusal(ErrorCode_failed,
                                named +
                                    " answered text its result does not read"
                                    " as: " +
                                    trouble));
                  return;
                }
                reply(std::move(*result));
              });
}

/** EVERY EVENT OF @p method handed to @p listener as its table. Text
 *  that does not read as Event is no event, and is dropped. */
template <class Event>
void listenFor(const Caller& caller, std::string method,
               std::function<void(const Event&)> listener) {
  if (!caller.listen) return;
  caller.listen(std::move(method),
                [listener = std::move(listener)](std::string_view text) {
                  if (const std::optional<Event> event =
                          data::values::fromJson<Event>(text))
                    listener(*event);
                });
}

}  // namespace sigil::protocol
