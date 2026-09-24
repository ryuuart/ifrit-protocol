/** @file
 * WHAT THE DEFINITION DECLARES, as the dispatcher checks a method
 * against it: every command's method, the table its parameters are, and
 * the domains that have events.
 */

#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>

namespace reflection {
// The reflected schema's own types, which only the reading sees.
struct Object;
struct Schema;
}  // namespace reflection

namespace sigil::protocol {

/** The definition's methods, read once out of its reflected schema. */
struct Declared {
  /** Every command's method, `clock.step`, `enable` and `disable`
   *  among them where a domain has events. */
  std::set<std::string, std::less<>> commands;
  /** Every domain with an events service beside its own. */
  std::set<std::string, std::less<>> eventful;
  /** Each command's parameter table, by method, as the reflected schema
   *  holds it; it lives as long as the definition's bytes do. */
  std::map<std::string, const reflection::Object*, std::less<>> parameters;
  /** The reflected schema those tables are read against. */
  const reflection::Schema* schema = nullptr;
};

/** The definition this build speaks, as methods. */
const Declared& declared();

/** The domain @p method belongs to: what stands before its dot, or the
 *  whole of it where it has none. */
std::string_view domainOf(std::string_view method);

}  // namespace sigil::protocol
