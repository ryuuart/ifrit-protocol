/** @file
 * HOW ONE LINE OF THE WRITTEN HEADER IS LAID OUT: the doc comment a
 * schema wrote over a definition, and the two forms a call or an
 * assignment takes depending on whether it fits the width this tree's
 * formatter keeps.
 */

#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace sigil::data::schema {

/** The doc comment a schema wrote over a definition, carried onto what
 *  is generated for it. */
void writeDocComment(std::ostream& out, const std::vector<std::string>& lines,
                     const std::string& indent);

/** A CALL ON ONE LINE, or broken after its opening bracket when the
 *  line would run past the width this tree's formatter keeps. @p head
 *  ends with that bracket and @p tail is the rest of the call. */
void writeCall(std::ostream& out, const std::string& head,
               const std::string& tail, const std::string& indent);

/** AN ASSIGNMENT ON ONE LINE, or with the value on the next when the
 *  line would run past that width. @p head ends with the equals sign. */
void writeAssignment(std::ostream& out, const std::string& head,
                     const std::string& tail, const std::string& indent);

}  // namespace sigil::data::schema
