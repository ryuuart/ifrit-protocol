#pragma once

/** @file
 * @ingroup io-hub
 * The reporting side of the hub's problems: how a library that reads a
 * resource through the hub says it could not make sense of it, and takes
 * that back once it can.
 */

#include <string_view>

#include "sigilio/hub/Problem.h"

namespace sigil::io {

class Hub;

/** Lists @p problem on @p hub under its URI, replacing whatever was said
 *  about that URI before, until it is cleared or a load of the URI
 *  succeeds. */
void reportProblem(Hub& hub, Problem problem);

/** Takes back what was said about @p uri on @p hub; nothing when nothing
 *  was. */
void clearProblem(Hub& hub, std::string_view uri);

/** Takes back every problem on @p hub: what a host does before a program
 *  asks for its resources again, so a URI the program no longer asks for
 *  stops being listed. */
void clearProblems(Hub& hub);

}  // namespace sigil::io
