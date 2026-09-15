# SigilSketchPublish — the door a drawn frame goes through

A frame that has been drawn into a texture can be handed to another
application on the same machine, which composites it live: a VJ program,
a projection mapper, a recorder. And a frame another application drew can
be received the same way, so a host may wear somebody else's picture as
easily as one of its own. This feature is **both ends of that door** —
one abstract publisher, one abstract subscription, a factory each, and
the macOS implementations behind them.

It is **one static archive, `SigilSketchPublish`, and two headers**. It
knows no window, no toolkit and no drawing: every texture and command
buffer crosses the seam as an opaque pointer, so a host that owns its own
device reaches the same publisher as one drawing inside a window somebody
else owns.

**Receiver** stands beside it, in `receiver/`: an application over the
subscription, so what is being offered can be looked at and written down
without another application in the middle. The two ends of one protocol
are one subject, and a door with nothing on the other side of it is a
door nobody has opened.

## The seam

* `<sigilsketch/publish/Publisher.h>` — `Publisher`, `createPublisher`

`sigil::sketch::createPublisher` answers with the publisher this build
has for the device it is handed, under the name subscribers will find it
by. On macOS that is a Syphon server; elsewhere there is nothing to
answer with and the answer is null.

```cpp
#include <sigilsketch/publish/Publisher.h>

// mtlDevice is an id<MTLDevice> as an opaque pointer: the device whose
// textures will be handed over. The caller stays its owner.
std::unique_ptr<sigil::sketch::Publisher> publisher =
    sigil::sketch::createPublisher("Sketchbook", mtlDevice);

// …once a frame has been drawn and submitted on that device's queue,
// and while the command buffer the host will commit is still open:
if (publisher) publisher->publishFrame(texture, commandBuffer, 1600, 1000);
```

**Null is an ordinary answer.** This build publishes over no protocol,
or there is no device to publish from, or the name is empty and nothing
could be found under it. A caller reports a run that does not publish —
it never publishes another way instead.

`sigil::sketch::Publisher::name` is the name a subscriber finds the
publication under, read back from the publisher rather than remembered
by the caller.

## What crosses it

`sigil::sketch::Publisher::publishFrame` takes four things: the
texture, the command buffer, and the width and height of the region to
publish. Both handles are the graphics API's own — on Metal an
`id<MTLTexture>` and an `id<MTLCommandBuffer>` as `void*`, the same way
Graphite's Metal bring-up takes a device and a queue.

**The texture is borrowed for the call.** No reference to it outlives
the call, so a host whose texture is reallocated — by a resize, by a new
device, by anything at all — hands the new one over on the next frame
and owes nothing for the last. A host that draws into a texture it does
not own, and is handed a fresh one whenever its size changes, therefore
has nothing to keep in step.

**What a subscriber receives is the frame as it was drawn**: its rows in
the order the texture holds them — the first of them the top of the
picture — and its alpha premultiplied the way the canvas wrote it.
Nothing here turns the picture over or divides the alpha out. A
publication carries a pixel format of its own, so the channels are put in
that order on the way across; nothing else about a pixel changes.

**The work is appended, not submitted.** The publication rides the
command buffer the caller is still filling and runs when the caller
commits it, so the drawing has to have been submitted on the same queue
already: work on one queue runs in the order it was committed, which is
what lets the copy see a finished frame instead of a half-drawn one. A
host drawing through Skia submits its surface first and publishes after.

**Nobody watching costs nothing.** A frame published with no subscriber
is a check and no GPU work, so a host may publish every frame and pay
for it only while somebody is looking.

## The macOS implementation

Syphon is the macOS protocol for exactly this, and the server is a
`SyphonMetalServer` standing on the device the factory was handed. Every
line that talks to Objective-C is in the one translation unit behind the
factory, and the server is stopped and released when the publisher goes.
The framework is linked on macOS alone; on every other platform the
archive is the factory and its refusal.

The server announces itself by name as soon as it exists, so it is
stood up when a host is asked to publish and not before — a publisher
that exists is a publisher other applications can already see in their
own menus.

## The seam the other way

* `<sigilsketch/publish/Subscription.h>` — `Subscription`, `subscribe`,
  `defaultMetalDevice`

`sigil::sketch::subscribe` answers with a subscription to the
publication that announced itself under a name — and, where an
application is named too, only that application's.

```cpp
#include <sigilsketch/publish/Subscription.h>

// The device the frames will be made textures on: the caller's own, or
// this machine's where the caller holds none.
std::unique_ptr<sigil::sketch::Subscription> guest = sigil::sketch::subscribe(
    "Guest", "", sigil::sketch::defaultMetalDevice());

// …every frame, wherever the drawing is about to happen:
if (guest)
  if (void* texture = guest->newestFrame()) {  /* an id<MTLTexture> */
  }
```

**Null is an ordinary answer** here for the same three reasons: this
build subscribes over no protocol, there is no device to receive on, or
the name is empty and nothing could have announced itself under it.

**A name nothing publishes yet is not one of them.** The subscription
stands and waits, and `sigil::sketch::Subscription::newestFrame` is what
opens it onto the publication once one appears — so the order the two
applications were started in never matters, and a publisher that stops
and comes back is followed rather than lost. That is also why the ask is
worth making when the answer is nothing.

`sigil::sketch::Subscription::standing` is whether frames can still
arrive: the publication answered AND the directory still knows of it.
Both, because a publisher that retires tells its subscribers and a
publisher that was killed tells nobody — the second is noticed by the
publication going off the list.
`sigil::sketch::Subscription::generation` counts the frames that have
arrived, across a publisher that came back, so a host can tell a NEW
frame from whatever was already there and read a frame rate off it; and
`sigil::sketch::Subscription::publishingApplication` is the application
the standing publication is drawn in, as the directory named it.

**The frame is borrowed until the next call.** The subscription holds
the texture it handed over and lets it go when it is asked for another,
so a caller that must keep one past that call takes its own reference —
which is what wrapping it as an image does.

`sigil::sketch::defaultMetalDevice` is this machine's own Metal device,
for the caller that holds none: a texture belongs to the device it was
made on and to no other, so a frame is received on the device the
drawing that will sample it stands on.

On macOS the subscription is a `SyphonMetalClient` onto a description
read off Syphon's own directory, and every line that talks to
Objective-C is in the one translation unit behind the factory.
**The directory is heard over a run loop**: what one process has to say
to another arrives there, so a process that has only just started knows
nothing until it has let the loop turn — which a host with a window of
its own does as part of running, and a run without one does deliberately.
A subscription tells the system to keep delivering those announcements
while its application is not the active one, since an application wearing
another's picture is by definition behind the one drawing it.

## Receiver — the other side

`Receiver` is a macOS application of its own (AppKit and Metal, no Qt),
built to `build/bin/<config>/Receiver.app`. It holds the archive's
subscription and adds what an application adds: a listing, a window and a
file. It takes three shapes:

```sh
Receiver --list                          # every publication, one per line
Receiver <name> [--app <application>]    # a window on that publication
Receiver <name> --grab <png> [--frames <n>] [--timeout <seconds>]
```

`<name>` is what a publication announced itself as, which is the first
column of the listing; the application drawing it is the second, and
`--app` picks between two applications publishing one name. A listing
turns the run loop for long enough to hear everyone answer, since what is
being offered is something the other processes on this machine have to be
asked for.

**The window waits for its publication.** A name nothing is publishing
yet is not an error: the title says it is waiting, and the first frame
that arrives is shown. The frame is drawn at the size it was published,
one screen pixel each where the screen has room and fitted to its shape
where it has not, and the title reads the publication's own frame rate.
A publisher that goes away is waited for again — which takes noticing
that it has gone: one that stops properly tells its subscribers, and one
that was killed tells nobody, so what is watched is also whether the
publication is still on the directory's list. That list is pruned when
anything on the machine asks what is publishing, which a publisher coming
back does for itself.

**A grab is a measurement, so it refuses rather than waits.** It
subscribes with no window, waits for NEW frames — one, unless told
otherwise, so what it writes was drawn after it subscribed — reads the
newest one back to the CPU and writes it as a PNG, rows and channels as
the texture holds them. It exits 2 when nothing is publishing under that
name, 3 when the frames did not arrive inside the timeout, and 4 when the
frame could not be written.

Which makes the door checkable end to end on one machine: publish from
Sketchbook with `--publish`, grab with `Receiver`, capture the same
sketch with `--frame`, and compare the two pictures.

## What is not here

* **No lane, no schedule, no clock.** This feature publishes the frame
  it is handed, when it is handed one. Which frames those are is the
  host's.
* **No protocol choice for the caller.** The factory is the one place
  that knows what this build can publish over, the way a device's
  bring-up is the one place that knows which graphics API it is on.
* **No readback and no conversion in the door itself.** A frame that has
  to become a file is a capture and a frame that has to become a video is
  an encode; both are elsewhere, and both leave the GPU. What goes
  through the door never does — a subscriber that wants a file reads one
  back on its own side, which is what a grab is.
