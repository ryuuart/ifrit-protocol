#pragma once

/** @file
 * Internal to the kernel — what a style sheet is compiled into once, when
 * it is built: its rules in order, each rule filed under the rightmost
 * compound of every one of its alternatives, and the class and role
 * names the arguments of its `:has()` pseudo-classes test for.
 */

#include <sigilcompose/core/StyleSheet.h>

#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace sigil::compose::detail {

/** ONE NAME A `:has()` ARGUMENT TESTS FOR: a style class, or a role. */
struct HasName {
  bool styleClass = false;
  std::string name;

  bool operator==(const HasName&) const = default;
};

/** A SHEET'S COMPILED FORM. A rule is filed by the SUBJECT compound of
 *  each alternative — under a class that compound names, else under its
 *  role, else among the rules any element may match — so a node is
 *  tested only against the rules that can speak about it. */
struct SheetBody {
  explicit SheetBody(std::vector<Rule> stated);

  std::vector<Rule> rules;
  boost::unordered_flat_map<std::string, std::vector<uint32_t>> byClass;
  boost::unordered_flat_map<std::string, std::vector<uint32_t>> byRole;
  std::vector<uint32_t> anyElement;
  /** Every name a `:has()` argument of these rules tests for; empty
   *  exactly when no rule uses `:has()` at all. */
  std::vector<HasName> hasNames;
  bool usesHas = false;
};

/** The one door onto a sheet's compiled form: null for a sheet that
 *  states nothing. */
struct SheetAccess {
  static const SheetBody* body(const StyleSheet& sheet) {
    return sheet.m_body.get();
  }
};

}  // namespace sigil::compose::detail
