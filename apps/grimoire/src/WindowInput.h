#pragma once

/** @file
 * The window's input door: `session.pointer` and `session.key` on the
 * endpoint the window mounts.
 *
 * The protocol's own types stand behind the source file, so the window's
 * translation units that read this header compile none of its headers.
 */

namespace sigil::protocol {
class Dispatcher;
}

/** POINTER AND KEY INPUT FROM A CLIENT, mounted on the window's
 *  dispatcher for the sketch on screen: `session.pointer` puts the pointer
 *  at a point on the sketch's canvas, in the canvas units it declared, and
 *  `session.key` sends a key down or up, as the window's own pointer and
 *  keyboard do. The sketch draws on the render thread, so each one is
 *  handed to that thread and reaches the sketch between two frames; the
 *  command is answered once it is handed over, and refused where no
 *  sketch is running.
 *
 *  It is input a person could give the window, so it is mounted whenever
 *  the endpoint is, unlike the evaluate door. */
void mountWindowInput(sigil::protocol::Dispatcher& dispatcher);
