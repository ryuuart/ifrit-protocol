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
start. Callers report that publication is unavailable. The backend tag is
checked before the device pointer is interpreted.

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
Rows retain their top-to-bottom order and alpha stays premultiplied. The
Metal path publishes the stated region; nothing flips the image, reads it
back to the CPU, or divides its alpha out.

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

A name that is not publishing yet is not a refusal. The subscription waits,
and `sigil::io::publish::Subscription::newestFrame` reconnects when a publisher
appears or restarts. The returned texture is borrowed until the next call;
a caller retaining it longer takes its own native reference.

`sigil::io::publish::Subscription::standing` checks both client validity and the
publication directory. `sigil::io::publish::Subscription::generation` counts
arrivals across reconnections. `sigil::io::publish::Subscription::name` is the
requested name; `sigil::io::publish::Subscription::publishingApplication` is the
application the current publication names, empty until connected.

`sigil::io::publish::defaultMetalDevice` returns the process's shared system Metal
device, or null without Metal. A host with its own device passes that one
instead: a received texture belongs to the device that will sample it.

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

```sh
cmake --build build --config Release --target SigilIOPublish io_test Seer
ctest --test-dir build -C Release -R '^Publish' --output-on-failure
```

Factory refusals belong to `io_test`. Seer's texture capture tests cover row
and channel order and late subscription to a static publication. Device cases
require Metal; byte and URI consumers do not inherit that requirement.
