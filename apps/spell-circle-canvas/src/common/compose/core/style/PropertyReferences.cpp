/** @file
 * The references a value may be written as — a custom property, by the
 * name interned for it, or the ink in force — and what a fill or a colour
 * written as one resolves to at paint.
 */

#include <sigilmaterial/color/Color.h>

#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "paint/FillLowering.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Names

namespace {

/** The process-wide table a custom property's name is interned in. The
 *  names stand in a deque so a view handed out stays valid when a later
 *  name is added; the table is never freed, because a describe on another
 *  thread may still be reading it while the process exits. */
struct VarNames {
  std::mutex mutex;
  std::deque<std::string> names;  // id − 1 → name
  boost::unordered_flat_map<std::string, uint32_t> ids;
};

VarNames& varNames() {
  static VarNames* const table = new VarNames;
  return *table;
}

}  // namespace

VarRef var(std::string_view name) {
  VarNames& table = varNames();
  const std::lock_guard<std::mutex> lock(table.mutex);
  std::string key(name);
  if (const auto found = table.ids.find(key); found != table.ids.end())
    return {found->second};
  table.names.push_back(key);
  const auto id = (uint32_t)table.names.size();
  table.ids.emplace(std::move(key), id);
  return {id};
}

std::string_view varName(VarRef ref) {
  VarNames& table = varNames();
  const std::lock_guard<std::mutex> lock(table.mutex);
  if (ref.id == 0 || ref.id > table.names.size()) return {};
  return table.names[ref.id - 1];
}

// ---------------------------------------------------------------------------
// A fill written as a reference

Fill resolveRef(const Fill& fill, const PaintContext& ctx) {
  switch (fill.ref) {
    case Fill::Ref::None:
      return fill;
    case Fill::Ref::CurrentInk:
      // The ink is a whole paint wherever one was stated, and a colour
      // everywhere else; both are what a mark naming no colour is
      // painted in.
      if (ctx.inkPaint && ctx.inkPaint->paint)
        return resolveInk(*ctx.inkPaint->paint, ctx);
      return Fill::color(ctx.ink);
    case Fill::Ref::Var: {
      const VarValue* value =
          ctx.vars ? ctx.vars->find(VarRef{fill.varId}) : nullptr;
      if (const material::Color* colour =
              value ? std::get_if<material::Color>(value) : nullptr)
        return Fill::color(*colour);
      // A paint is laid over the box being painted, as that node's own
      // fill of it would be.
      if (const Fill* paint = value ? std::get_if<Fill>(value) : nullptr)
        return resolveFill(*paint, ctx);
      warnNoSuchVar(VarRef{fill.varId}, true);
      return Fill::none();
    }
  }
  return fill;
}

material::Color resolveColor(const ColorTerm& term, const PaintContext& ctx) {
  switch (term.ref) {
    case Fill::Ref::None:
      return term;
    case Fill::Ref::CurrentInk:
      return ctx.ink;
    case Fill::Ref::Var: {
      const VarValue* value =
          ctx.vars ? ctx.vars->find(VarRef{term.varId}) : nullptr;
      if (const material::Color* colour =
              value ? std::get_if<material::Color>(value) : nullptr)
        return *colour;
      warnNoSuchVar(VarRef{term.varId}, true);
      return term;
    }
  }
  return term;
}

}  // namespace sigil::compose
