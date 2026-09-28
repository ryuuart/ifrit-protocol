#pragma once

/** @file
 * @ingroup io-hub
 * A resource that could not be made into what it was asked for, as the
 * hub lists it for whoever shows a failure to a person.
 */

#include <optional>
#include <string>

namespace sigil::io {

/** WHAT WENT WRONG WITH ONE RESOURCE: the URI it was asked for by, what
 *  the library that read it has to say, and the line of the resource that
 *  message is about when it names one. A hub holds at most one per URI:
 *  a later report replaces it, and a later load that succeeds takes it
 *  back. */
struct Problem {
  std::string uri;
  std::string message;
  /** The line of the resource the message is about, counted from one;
   *  unset when it is about the resource as a whole. */
  std::optional<int> line;
  bool operator==(const Problem&) const = default;
};

}  // namespace sigil::io
