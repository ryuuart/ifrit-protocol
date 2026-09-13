#pragma once

/** @file
 * The window lane: a publication on screen, at the size it is drawn at,
 * for as long as the window is open.
 */

#include "Arguments.h"

namespace receiver {

/** Opens a window on the publication the arguments name and runs until it
 *  is closed. The process's exit status: 0 when the window ran, 4 when
 *  this machine has no device to receive on. A publication that is not
 *  there yet is not a failure — the window waits for it, and waits again
 *  whenever it goes away. */
int runWindow(const Arguments& arguments);

}  // namespace receiver
