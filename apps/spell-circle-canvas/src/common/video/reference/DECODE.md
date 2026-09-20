# SigilVideo — decoding and the shared queue

The chapter on what a decoder answers when it is asked for a moment, and
on the queue many clocks share when several clips run at once.
`README.md` beside the library is the front page.

## The frame covering a moment

`Video::frameAt` answers the frame covering the requested seconds, or
the one before it when the frame durations leave a gap there. Passing a
Graphite recorder enables direct device-plane composition when the
decoder produced one.

A stream whose frames carry no presentation timestamps is placed by
decode order against the probed frame rate. No time inside such a stream
can be sought to, so an ask behind the playhead reads it again from the
beginning.

`Video::hardwareDecoding` is whether the MOST RECENTLY DECODED frame
arrived as a native device surface. It is false until a frame has been
decoded, so a caller that needs the fact rather than the intent decodes
one frame first; `Video::hardwareConfigured` is the intent.

## Playback: many clocks, one queue

`Playback` is many independent video clocks sharing a bounded decode
queue.

A clip registers once: `Playback::add` answers the handle a clip already
holds, since one `Video` must never be decoded by two workers at once,
and a clip may be added while presentation is running.

`Playback::request` coalesces repeated asks that fall inside the same
source frame, and newer times replace queued stale work.

`Playback::frame` is called on one render thread; it maps a completed
native frame into that thread's Graphite recorder and never waits for a
decoder.

## See also

`README.md` beside the library for the whole pipeline: the streaming
decode, the device composition and the MP4 encode.
