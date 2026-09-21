# SigilIOPublish

Native texture publication and subscription between applications. One static
library, `SigilIOPublish`, owns the platform protocol implementations. Its C++
headers use opaque graphics handles and depend on no window toolkit, drawing
library, sketch runtime or resource hub. Namespace `sigil::io::publish`.

This optional SigilIO target shares native textures and their synchronization;
it does not convert them into byte feeds or make the resource hub depend on
a graphics backend. Link it only where an application publishes or subscribes.

A publisher offers the frame its caller drew. A subscription follows another
application's publication by name. Neither owns a render loop or creates a
second rendering device.

## Publishing

* `Publisher.h` — `Backend`, `Publisher`, `createPublisher`

```cpp
#include <sigilio/publish/Publisher.h>

// nativeDevice is this host's id<MTLDevice> as an opaque pointer.
auto publisher = sigil::io::publish::createPublisher(
    "Live Canvas", sigil::io::publish::Backend::Metal, nativeDevice);

// Drawing is already submitted on the same queue. This command buffer
// is still open, and the host commits it after publication.
if (publisher)
  publisher->publishFrame(nativeTexture, nativeCommandBuffer, 1600, 1000);
```

`sigil::io::publish::createPublisher` returns null for an empty name, a missing
device, an unsupported backend, or a platform implementation that could not
start. Null is an ordinary answer: callers report that publication is
unavailable, and treat it as a run that does not publish, never as a reason
to publish another way. The backend tag is checked before the device pointer
is interpreted.

`sigil::io::publish::Backend::Metal` selects Syphon on macOS. The device is an
`id<MTLDevice>`, the texture an `id<MTLTexture>`, and the command buffer an
`id<MTLCommandBuffer>`, each bridged to `void*`. The publisher appends its copy
to that buffer; the host commits it. Drawing must have been submitted on the
same queue first so the copy reads the completed frame.

`sigil::io::publish::Backend::Direct3D11` selects Spout on Windows when its SDK is
configured. The device is an `ID3D11Device*` and the texture an
`ID3D11Texture2D*`. Publication uses the device's immediate context after the
host's draw, and ignores the command-buffer argument. Spout publishes the
whole texture; the stated dimensions match its extent. The Windows path is
conditional and needs validation on a Windows machine with Spout installed.

`sigil::io::publish::Publisher::name` reads back the publication's name.
The server announces itself when constructed and stops when destroyed. The
caller keeps its device alive until after the publisher is destroyed.

### Frame ownership and static scenes

`sigil::io::publish::Publisher::publishFrame` borrows the input texture for the
call. The protocol owns the published image, so the caller can replace its
render target on resize without preserving an old target for subscribers.
The picture arrives the way up it was drawn and alpha stays premultiplied. The
Metal path publishes the stated region; nothing reads it back to the CPU or
divides its alpha out. A publication carries a pixel format of its own, so the
channels are put in that order on the way across; nothing else about a pixel
changes.

### Which way up a frame travels

**The surface a publication is carried on holds its FIRST ROW AT THE IMAGE'S
BOTTOM.** That is the order a host drawing straight into it with OpenGL's axes
writes, and it is the order every application receiving one reads — a video
mixer, a projection mapper, a compositor. A texture a canvas drew holds its
first row at the TOP.

So the two ends are not symmetrical, and each says which way round it is:

* `sigil::io::publish::Publisher::publishFrame` takes a TOP-FIRST texture and
  turns it over on the way across. On Metal that is Syphon's
  `publishFrameTexture:onCommandBuffer:imageRegion:flipped:` with the flag set,
  which costs the copy a redraw rather than a blit. Publishing an unturned
  frame is what makes every subscriber show the picture upside down.
* `sigil::io::publish::Subscription::newestFrame` hands back the carried
  surface itself, BOTTOM-FIRST, because nothing is copied on the way in and a
  turn is a copy. A caller drawing it in a space whose first row is the top
  turns it over as it draws: a scene-graph node mirrors vertically, a canvas
  draws through a flipped transform, a readback walks the rows backwards.

Seer's capture and preview, and the door a sketch wears a publication through,
each do that turn, so what a reader of this repository's own tools looks at is
upright.

On Metal the publication is appended to the caller's open command buffer and
not submitted: it runs when the caller commits, and work on one queue runs in
the order it was committed, which is what lets the copy see a finished frame
rather than a half-drawn one.

**Every explicit publication updates the retained image, even with no client
attached.** A static host publishes after drawing once; clients connecting
later receive that image without another render. Hosts decide when another
frame is needed. The publication library does not poll for subscribers or
request busy rendering to make a static image discoverable.

Qt hosts use the adapter in Ifrit.Qt, which unwraps the current QRhi backend,
texture and command buffer. AppKit hosts use this native seam directly. Both
reach the same protocol implementation.

## Subscribing

* `Subscription.h` — `Publication`, `publications`, `Subscription`,
  `subscribe`, `defaultMetalDevice`

```cpp
#include <sigilio/publish/Subscription.h>

auto incoming = sigil::io::publish::subscribe(
    "Live Canvas", "", sigil::io::publish::defaultMetalDevice());
if (incoming)
  if (void* texture = incoming->newestFrame()) {
    // Use the id<MTLTexture> on this device.
  }
```

`sigil::io::publish::subscribe` currently receives Syphon on Metal. It returns
null off macOS, without a device, or for an empty name. A nonempty application
name restricts the match to that application; an empty one accepts any
publisher of the requested name. Spout subscription is not implemented.

A name that is not publishing yet is not a refusal. THE NAME IS WHAT IS HELD,
not the process behind it: the subscription waits, and
`sigil::io::publish::Subscription::newestFrame` reconnects when a publisher
appears or restarts, so a host subscribes once, asks every frame, and the order
the two applications were started in stops mattering. The returned texture is
borrowed until the next call: the subscription lets the previous frame go
whenever it is asked for another, so a caller retaining one longer takes its
own native reference, which is what wrapping it as an image does. A null
subscription is a run that receives nothing, never a reason to receive another
way.

`sigil::io::publish::Subscription::standing` checks both client validity and the
publication directory — both, because a publisher that retires tells its
subscribers and a publisher that was killed tells nobody, and the second is
noticed by the publication going off the list rather than by the subscription
being closed. `sigil::io::publish::Subscription::generation` counts arrivals
across reconnections, because a publication that came back is the same
publication: it is what a frame rate is read from and what says a frame is NEW
rather than whatever was already there. `sigil::io::publish::Subscription::name` is the
requested name; `sigil::io::publish::Subscription::publishingApplication` is the
application the current publication names, empty until connected.

`sigil::io::publish::defaultMetalDevice` returns the process's shared system Metal
device, or null without Metal. The platform owns it and nothing here releases
it. It is the device a window drawing through Metal stands on where that window
asked for no other. A host with its own device passes that one instead: a
received texture belongs to the device that will sample it.

The Syphon directory receives announcements over the main run loop. Windowed
hosts already run it; command-line receivers turn it explicitly. No
subscription callback calls application code: a private shared counter owns
the frame-notification lifetime independently of a retiring subscription.

## Discovery and inspection

`sigil::io::publish::publications()` returns a current directory snapshot of
`sigil::io::publish::Publication` values: each has a name and application.
It never waits; the native main event loop receives announcements.
An unsupported platform returns an empty list.

Seer owns texture inspection, live preview and PNG capture. The publication
library contains no application target or window implementation.

## Build and tests

[docs/overview/testing.md](../../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under. This
feature has no test binary of its own. The factory refusals belong to
`io_test`, and the texture capture — row and channel order, and late
subscription to a static publication — is covered by Seer's own cases,
which need Metal and carry the `gpu` label for it. A byte or URI
consumer does not inherit that requirement.
