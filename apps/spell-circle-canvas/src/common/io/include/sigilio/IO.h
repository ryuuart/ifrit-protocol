#pragma once

/** @file
 * Every public SigilIO header in one include.
 */

/** @defgroup io-source Sources and sinks
 *  Where bytes come from and where they go: the byte source seam, the
 *  places a URI is mounted on, archives read as one source, and the
 *  sinks bytes are written through.
 *  @{ */
/** @} */

/** @defgroup io-hub The hub
 *  One door onto every resource: a URI fetched, cached, decoded into
 *  the type asked for, watched for change, and reloaded when it moves.
 *  @{ */
/** @} */

/** @defgroup io-transport Transports and feeds
 *  Bytes that keep arriving rather than bytes that sit still: the feed
 *  a URI is listened on, the schemes a feed speaks, and the recordings
 *  one is written to and played back from.
 *  @{ */
/** @} */

/** @defgroup io-publish Native frame sharing
 *  A texture handed to another application on the same machine, and one
 *  taken from it.
 *  @{ */
/** @} */

#include "sigilio/hub/Hub.h"
#include "sigilio/hub/Network.h"
#include "sigilio/hub/TextCatalog.h"
#include "sigilio/source/Sink.h"
#include "sigilio/source/Source.h"
