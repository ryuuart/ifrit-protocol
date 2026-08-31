#pragma once

/** @file
 * Every public header of SigilLoader in one include. Transitional: a
 * consumer that spelled the headers by bare name before they moved under
 * their features includes this one and keeps compiling, then narrows to
 * the feature headers it actually uses.
 */

#include "sigilloader/hub/Hub.h"
#include "sigilloader/hub/Network.h"
#include "sigilloader/source/Source.h"
