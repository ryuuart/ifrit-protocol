/** @file
 * How a pass's selection reaches the pixels. The rule reads the
 * declaration: a geometry pass draws only what it selected; a post pass
 * paints everything and reaches the selection through coverage, which
 * the geometry pass before it is made to write; and a pass naming a
 * variant surface draws its selection again in that surface. A pass
 * that knows better says so, and its word stands.
 */

#include <string>
#include <vector>

#include "Build.h"

namespace sigil::world::graph::detail {

namespace {

Selection ruledOn(const Pass& pass) {
  if (pass.realisation() != Selection::Auto) return pass.realisation();
  if (pass.variant()) return Selection::Variant;
  if (!pass.narrowed()) return Selection::None;
  return pass.stage() == Stage::Post ? Selection::Mask : Selection::Cull;
}

}  // namespace

Touches touchesOf(const PassWork& work) {
  Touches touches;
  for (const std::string& name : work.pass->reads())
    touches.reads.push_back(name);
  if (!work.coverageIn.empty()) touches.reads.push_back(work.coverageIn);
  for (const std::string& name : work.pass->writes())
    touches.writes.push_back(name);
  if (!work.coverageOut.empty()) touches.writes.push_back(work.coverageOut);
  return touches;
}

std::string realise(std::span<const Pass> passes, std::span<const size_t> order,
                    std::vector<PassWork>& into) {
  into.clear();
  into.reserve(order.size());
  for (size_t index : order) {
    PassWork work;
    work.pass = &passes[index];
    work.realisation = ruledOn(passes[index]);
    into.push_back(std::move(work));
  }

  for (size_t step = 0; step < into.size(); ++step) {
    if (into[step].realisation != Selection::Mask) continue;
    const Pass& pass = *into[step].pass;
    if (pass.stage() != Stage::Post) continue;
    // Coverage is where the bodies are, so it is painted by the last
    // pass before this one that paints bodies. A post pass with nothing
    // painting bodies ahead of it has no selection to mask.
    size_t producer = step;
    while (producer > 0) {
      --producer;
      if (into[producer].pass->stage() == Stage::Geometry) break;
    }
    if (producer >= step || into[producer].pass->stage() != Stage::Geometry)
      return "the post pass \"" + pass.name() +
             "\" narrows its selection, and no pass before it paints the "
             "bodies its coverage would be taken from";
    into[step].coverageIn = pass.name() + ".coverage";
    into[producer].coverageOut = into[step].coverageIn;
    into[producer].coverageOf = pass.selector();
  }
  return {};
}

}  // namespace sigil::world::graph::detail
