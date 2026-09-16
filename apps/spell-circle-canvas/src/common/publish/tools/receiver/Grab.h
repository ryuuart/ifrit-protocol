#pragma once

/** @file
 * The grab lane: no window, one file. It subscribes, waits for the frames
 * it was told to wait for, and writes the newest one.
 */

#include "Arguments.h"

namespace receiver {

/** Subscribes to the publication the arguments name, waits for its
 *  frames and writes the newest one. The process's exit status: 0 when the
 *  file was written, 2 when nothing is publishing under that name, 3 when
 *  no frame arrived in time, 4 when the frame could not be written. */
int runGrab(const Arguments& arguments);

}  // namespace receiver
