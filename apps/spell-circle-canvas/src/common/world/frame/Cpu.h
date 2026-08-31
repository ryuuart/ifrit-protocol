#pragma once

/** @file
 * The three kinds of work the CPU executor performs, one per stage, so
 * each stage is one file and the executor itself is only the dispatch.
 */

#include <sigilworld/frame/Pass.h>
#include <sigilworld/frame/Targets.h>
#include <sigilworld/frame/View.h>

#include <string>

namespace sigil::world::cpu {

/** The first name @p pass writes; empty when it writes nothing. */
const std::string* target(const Pass& pass);

/** Paint the bodies @p work leaves into the first target it writes, and
 *  the coverage a masked pass downstream asked for. */
void paintGeometry(const PassWork& work, const View& view, Targets& targets);

/** Cook the pass's chain into the first point set it writes. */
void cookPoints(const PassWork& work, Targets& targets);

/** Apply the pass's op to the images it reads, into the first target it
 *  writes. */
void applyPost(const PassWork& work, Targets& targets);

}  // namespace sigil::world::cpu
