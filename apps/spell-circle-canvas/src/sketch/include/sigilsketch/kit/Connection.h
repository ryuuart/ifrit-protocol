#pragma once

/** @file
 * @ingroup sketch-kit
 *
 * A DOOR'S OWN WORDS: the readout of what a data connection says about
 * itself — where it is, how many messages have arrived, how many fell
 * off behind a reader, how many were no message, and who sent the newest
 * or what went wrong — as rows of a name and the figure that answers it.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/kit/Rows.h>
#include <sigildata/connection/Connection.h>

namespace sigil::sketch::kit {

/** HOW A DOOR'S READOUT IS SET: what its first row calls the door, and
 *  how its rows range. No type and no colour: the names and the figures
 *  are the compose kit's readout rows, set by the sheet in force. */
struct ConnectionReadout {
  /** WHAT THE DOOR ROW READS where the transport bound no address of its
   *  own — a recording has none — which is what the sketch calls the
   *  door. Empty is the URI the connection was opened on. */
  compose::Utf8 door;
  /** The rows' measure, widths and air, as the compose kit's readout
   *  takes them. */
  compose::kit::Rows rows;
};

/** THE READOUT OF @p connection's vitals, taken as it is called: the
 *  door, its generation, how many arrivals it dropped, how many were
 *  undecodable, and the sender of the newest message — `-` where nobody
 *  sent one — or, where the door reports an error, that error standing
 *  in the sender's row. Every figure is what the connection answers;
 *  nothing is computed about the messages.
 *
 *      sketch::kit::connectionReadout(sky, {.rows = {.measure = 360}})
 */
[[nodiscard]] compose::Element connectionReadout(
    const data::Connection& connection, const ConnectionReadout& how = {});

}  // namespace sigil::sketch::kit
