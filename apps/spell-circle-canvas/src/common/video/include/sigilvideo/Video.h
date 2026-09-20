#pragma once

/** @file
 * Every public SigilVideo header in one include.
 */

/** @defgroup video-decode Streaming decode
 *  Encoded bytes opened as a seekable clip, the frames decoded around a
 *  playhead, and the presentation pool that keeps many clocks fed without
 *  blocking the thread that draws.
 *  @{ */
/** @} */

/** @defgroup video-encode MP4 encode
 *  Skia pixels appended one frame at a time and finished as the bytes of a
 *  container.
 *  @{ */
/** @} */

#include "sigilvideo/Types.h"
#include "sigilvideo/decode/Decode.h"
#include "sigilvideo/decode/Playback.h"
#include "sigilvideo/encode/Encode.h"
