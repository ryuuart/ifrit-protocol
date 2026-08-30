#pragma once

/** @file
 * The four readings a plan is assembled from, one per file: the order,
 * the selection realisations, the resource lives and their aliasing, and
 * the barriers. Each takes what the ones before it produced and adds
 * nothing to the frame.
 */

#include <sigilworld/graph/Plan.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace sigil::world::graph::detail {

/** Every resource a step touches once the ordering's own coverage
 *  insertion is counted, which is what the lives and the barriers are
 *  read off. */
struct Touches {
  std::vector<std::string> reads;
  std::vector<std::string> writes;
};
Touches touchesOf(const PassWork& work);

/** The execution order of @p passes, as indices into them. Returns what
 *  stopped it — a cycle, naming the passes on it — or an empty string.
 */
std::string order(std::span<const Pass> passes, std::vector<size_t>& into);

/** How each ordered step's selection reaches the pixels, and the
 *  coverage a masked step needs written for it. Returns what stopped
 *  it, naming the pass. */
std::string realise(std::span<const Pass> passes, std::span<const size_t> order,
                    std::vector<PassWork>& into);

/** Each resource's kind and live range, in execution order, and which
 *  of them outlive the frame. */
void lives(std::span<const PassWork> steps, const Frame& frame,
           std::vector<Resource>& into, std::string& present,
           std::vector<std::string>& kept);

/** Give the image resources surfaces, sharing one between resources
 *  whose live ranges do not overlap. */
int alias(std::vector<Resource>& resources);

/** The hazards between consecutive touches of one resource, and between
 *  the resources sharing one surface. */
void barriers(std::span<const PassWork> steps,
              std::span<const Resource> resources, std::vector<Barrier>& into);

}  // namespace sigil::world::graph::detail
