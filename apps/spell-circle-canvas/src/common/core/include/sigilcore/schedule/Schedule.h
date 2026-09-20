#pragma once

/** @file
 * @ingroup core-schedule
 *
 * The schedule feature in one include: where independent items run, and
 * where a call that blocks runs instead.
 */

/** @defgroup core-schedule Where work runs
 *  One parallel for over the task runtime, taking a count, a grain and a
 *  body, so the runtime is named in one file of this repository and in
 *  no header of it; and beside it a fan-out of its own threads for calls
 *  that BLOCK on a disk or a server, which must not sit on the workers a
 *  compute range shares. */

#include <sigilcore/schedule/ConcurrentIo.h>
#include <sigilcore/schedule/Parallel.h>
