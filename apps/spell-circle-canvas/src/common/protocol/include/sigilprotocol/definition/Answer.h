#pragma once

/** @file
 * @ingroup protocol-definition
 * WHAT A COMMAND ANSWERS: its result, or the error a seam refused it
 * with — never both, and never neither.
 */

#include <sigilprotocol/protocol_values.h>

#include <functional>
#include <string>
#include <utility>
#include <variant>

namespace sigil::protocol {

/** WHAT A COMMAND ANSWERS: its result, or the error a seam refused it
 *  with. An agent returns one, a handler answers one as text and a
 *  client is handed one; `explicit operator bool` is whether it holds
 *  the result. Made from either, so an agent writes `return result;` or
 *  `return refusal(ErrorCode_failed, "…");`.
 *  @trap `result()` and `error()` each hold only on their own side. */
template <class Result>
class Answer {
 public:
  Answer(Result result) : m_held(std::in_place_index<0>, std::move(result)) {}
  Answer(values::Error error)
      : m_held(std::in_place_index<1>, std::move(error)) {}

  /** Whether this holds the result. */
  explicit operator bool() const { return m_held.index() == 0; }

  /** The result, where this holds it. */
  const Result& result() const { return std::get<0>(m_held); }
  Result& result() { return std::get<0>(m_held); }

  /** The error, where this holds no result. */
  const values::Error& error() const { return std::get<1>(m_held); }

 private:
  std::variant<Result, values::Error> m_held;
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

}  // namespace sigil::protocol
