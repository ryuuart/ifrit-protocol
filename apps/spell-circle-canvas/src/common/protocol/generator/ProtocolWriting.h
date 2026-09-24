/** @file
 * WHAT EVERY WRITER SHARES: how a dotted name is spelled in C++, how a
 * part's documentation becomes a doc comment, and how a written file
 * reaches the disk.
 */

#pragma once

#include <ostream>
#include <string>

#include "ProtocolModel.h"

namespace sigil::protocol::generator {

/** A dotted namespace as C++ spells it: `sigil::protocol::clock`. */
std::string cppSpace(const std::string& dotted);

/** The value type a table of the definition reads as, spelled from
 *  inside the C++ namespace @p from: `values::StepResult` where the
 *  table is declared there, `::sigil::protocol::values::Empty`
 *  qualified from the global namespace everywhere else. */
std::string valueType(const std::string& table, const std::string& from);

/** A name with its first letter raised: `onBudgetExpired` is `on` and
 *  this of `budgetExpired`. */
std::string raised(const std::string& name);

/** @p part's documentation as a doc comment at @p indent, with @p more
 *  as a paragraph after it where it is not empty, and the line every
 *  experimental part carries. */
void writeDocComment(std::ostream& out, const Documented& part,
                     const std::string& indent,
                     const std::string& more = std::string());

/** The first line of every file the generator writes: what wrote it
 *  from what, and that it is never edited by hand. */
void writeBanner(std::ostream& out, const std::string& what);

/** @p text written to @p path, with the directories it stands in made
 *  first. False, with @p why, where it could not be written. */
bool writeFile(const std::string& path, const std::string& text,
               std::string* why);

}  // namespace sigil::protocol::generator
