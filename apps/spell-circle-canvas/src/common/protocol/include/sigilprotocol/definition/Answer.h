#pragma once

/** @file
 * @ingroup protocol-definition
 * WHAT A COMMAND ANSWERS: its result, or the error a seam refused it
 * with — never both, and never neither — as the standard expected answer.
 */

#include <sigilprotocol/protocol_values.h>

#include <sigilcore/report/Error.h>

#include <expected>
#include <functional>
#include <string>
#include <utility>

namespace sigil::protocol {

/** WHAT A COMMAND ANSWERS: its result, or the error a seam refused it
 *  with — a `std::expected<Result, values::Error>`, so every verb a
 *  caller knows from the standard answer reads it. An agent returns one,
 *  a handler answers one as text and a client is handed one; `explicit
 *  operator bool` is whether it holds the result. Made from either, so an
 *  agent writes `return result;` or `return refusal(ErrorCode_failed,
 *  "…");`. The wire carries the result or the error as before.
 *  @trap `result()` and `error()` each hold only on their own side. */
template <class Result>
class Answer : public std::expected<Result, values::Error> {
 public:
  using Expected = std::expected<Result, values::Error>;

  Answer(Result result) : Expected(std::move(result)) {}
  Answer(values::Error error)
      : Expected(std::unexpect, std::move(error)) {}
  Answer(Expected answered) : Expected(std::move(answered)) {}

  /** The result, where this holds it; const on a const answer. */
  template <typename Self>
  auto& result(this Self&& self) {
    return self.value();
  }
};

/** Where an agent that answers later sends its answer, once. */
template <class Result>
using Reply = std::function<void(Answer<Result>)>;

/** A REFUSAL, named by its seam: @p code says which seam, and @p message
 *  what it refused, in words that name the method, the parameter or the
 *  reason. */
inline values::Error refusal(ErrorCode code, std::string message) {
  values::Error error;
  error.code = code;
  error.message = std::move(message);
  return error;
}

/** The same, from the error a library's door answered: its message, and
 *  its line ahead of it where it names one. */
inline values::Error refusal(ErrorCode code, const core::Error& error) {
  return refusal(code, error.line ? "line " + std::to_string(*error.line) +
                                        ": " + error.message
                                  : error.message);
}

/** @p error as the error every library's door answers: its message, and
 *  its code as the number the protocol gives that kind of refusal. */
inline core::Error coreError(const values::Error& error) {
  return {error.message, {}, static_cast<int>(error.code)};
}

}  // namespace sigil::protocol
