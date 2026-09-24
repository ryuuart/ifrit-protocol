#pragma once

/** @file
 * The plate lane: every selected sketch rendered headless, one plate
 * each.
 */

#include <sigilmaterial/stock/Stock.h>

#include <future>

struct Arguments;

/** THE HEADLESS SWEEP over the selection, into the directory
 *  `--headless` named, answering the protocol's `host` and `registry`
 *  between sketches where `--inspect` asked. @p chosen is the entry `--sketch`
 * narrowed to, or -1 for the whole table narrowed by `--kind` alone. 1 when
 * `--gpu` was asked for and no device came up, otherwise what the sweep
 * answered. */
int runSweep(const Arguments& args, int chosen,
             std::future<sigil::material::WarmupResult>& materialWarmup);
