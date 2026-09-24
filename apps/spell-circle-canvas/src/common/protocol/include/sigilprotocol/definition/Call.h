#pragma once

/** @file
 * @ingroup protocol-definition
 * THE CALLER'S SIDE OF A COMMAND: what a generated client speaks
 * through, and the two steps every client member takes — send the
 * parameters as their table's text, read the answer back as the
 * result's table — written once. What this side refuses it refuses with
 * its own two codes, notSent and unreadable, and words that open with
 * `client:`, so a refusal made here is never taken for a host's.
 */

#include <sigildata/values/Values.h>
#include <sigilprotocol/definition/Answer.h>
#include <sigilprotocol/definition/Mount.h>

#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::protocol {

/** WHAT A GENERATED CLIENT SPEAKS THROUGH: `call` sends one command, its
 *  method and its parameters' JSON text, and hands the answer to the
 *  respond it is given, once; `listen` hands every event of a method to
 *  a listener as its table's JSON text; `refused` is handed the refusal
 *  for every event whose text does not read as its table, which no
 *  listener hears. A socket client and the in-process dispatcher are
 *  two.
 *  @trap A caller with no `refused` has such a refusal written to the
 *  standard error instead: an event is never lost unseen. */
struct Caller {
  std::function<void(std::string method, std::string parameters,
                     Respond respond)>
      call;
  std::function<void(std::string method,
                     std::function<void(std::string_view parameters)> hear)>
      listen;
  std::function<void(values::Error refusal)> refused;
};

/** A REFUSAL THE CLIENT MADE: @p code, notSent or unreadable, and @p what
 *  after the method it concerns, both after `client:`. */
inline values::Error clientRefusal(ErrorCode code, std::string_view method,
                                   std::string_view what) {
  return refusal(code,
                 "client: " + std::string(method) + ": " + std::string(what));
}

/** @p refused handed to the caller's @p refuse, or written to the
 *  standard error where the caller has none. */
inline void reportRefusal(
    const std::function<void(values::Error refusal)>& refuse,
    values::Error refused) {
  if (refuse) {
    refuse(std::move(refused));
    return;
  }
  std::fprintf(stderr, "sigil::protocol: %s\n", refused.message.c_str());
}

/** A COMMAND SENT: @p parameters as JSON under @p method, and the answer
 *  read back as Result and handed to @p reply. Parameters their own
 *  table cannot hold, and a caller with nowhere to send, are refused
 *  with notSent before anything is sent; an answer whose text does not
 *  read as Result is refused with unreadable. Each names the method, and
 *  an error the host answered is handed on as it came. */
template <class Result, class Parameters>
void callWith(const Caller& caller, std::string method,
              const Parameters& parameters, Reply<Result> reply) {
  std::string why;
  std::optional<std::string> text = data::values::toJson(parameters, &why);
  if (!text) {
    reply(clientRefusal(ErrorCode_notSent, method,
                        "the parameters' table cannot hold them: " + why));
    return;
  }
  if (!caller.call) {
    reply(clientRefusal(ErrorCode_notSent, method, "the caller sends nowhere"));
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
                  reply(clientRefusal(
                      ErrorCode_unreadable, named,
                      "the answer does not read as its result: " + trouble));
                  return;
                }
                reply(std::move(*result));
              });
}

/** EVERY EVENT OF @p method handed to @p listener as its table. Text
 *  that does not read as Event reaches no listener: its refusal,
 *  unreadable and naming the method, goes to the caller's `refused`, as
 *  does notSent where the caller listens nowhere. */
template <class Event>
void listenFor(const Caller& caller, std::string method,
               std::function<void(const Event&)> listener) {
  if (!caller.listen) {
    reportRefusal(caller.refused, clientRefusal(ErrorCode_notSent, method,
                                                "the caller listens nowhere"));
    return;
  }
  const std::string named = method;
  caller.listen(std::move(method), [named, refuse = caller.refused,
                                    listener = std::move(listener)](
                                       std::string_view text) {
    std::string why;
    if (const std::optional<Event> event =
            data::values::fromJson<Event>(text, &why)) {
      listener(*event);
      return;
    }
    reportRefusal(
        refuse, clientRefusal(ErrorCode_unreadable, named,
                              "the event does not read as its table: " + why));
  });
}

}  // namespace sigil::protocol
