#pragma once

/** @file
 * The montage lane: the selection encoded into one vertical video.
 */

#include <sigilmaterial/stock/Stock.h>

#include <future>

struct Arguments;

/** THE SELECTION AS ONE CUT, top to bottom. @p chosen is the entry
 *  `--sketch` narrowed to, or -1 for the whole table narrowed by
 *  `--kind` alone. A selection that holds a set is lit on the device, so
 *  one that did not ask for it is refused rather than encoded on the CPU
 *  mesh executor: the cut under that sketch's name would be a picture no
 *  recipe ran in. 2 when it was refused, 1 when the device would not
 *  come up, otherwise what the montage answered. */
int runVideo(const Arguments& args, int chosen,
             std::future<sigil::material::WarmupResult>& materialWarmup);
