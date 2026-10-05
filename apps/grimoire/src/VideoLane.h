#pragma once

/** @file
 * The montage lane: the selection encoded into one vertical video.
 */

#include <sigilmaterial/advanced/Program.h>

#include <future>

struct Arguments;

/** THE SELECTION AS ONE CUT, top to bottom. @p chosen is the entry
 *  `--sketch` narrowed to, or -1 for the whole table narrowed by
 *  `--kind` alone. A selection that holds a set is drawn on the device,
 *  so one that did not ask for it is refused. 2 when it was refused, 1
 *  when the device would not come up, otherwise what the montage
 *  answered. */
int runVideo(const Arguments& args, int chosen,
             std::future<sigil::material::WarmupResult>& materialWarmup);
