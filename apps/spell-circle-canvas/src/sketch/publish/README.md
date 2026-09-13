# SigilSketchPublish — the door a drawn frame leaves by

A frame that has been drawn into a texture can be handed to another
application on the same machine, which composites it live: a VJ program,
a projection mapper, a recorder. This feature is the door that hands it
over — one abstract publisher, one factory, and the macOS
implementation behind them.

It is **one static archive, `SigilSketchPublish`, and one header**. It
knows no window, no toolkit and no drawing: the texture and the command
buffer cross the seam as opaque pointers, so a host that owns its own
device reaches the same publisher as one drawing inside a window
somebody else owns.

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

**What a subscriber receives is the texture as it stands**: its own
pixel format, and its alpha premultiplied the way the canvas wrote it.
Nothing here converts. The frame is published flipped, because a canvas
draws from its top-left corner and a publication is read from its
bottom-left, so a subscriber composites the frame the way up it was
drawn.

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

## What is not here

* **No lane, no schedule, no clock.** This feature publishes the frame
  it is handed, when it is handed one. Which frames those are is the
  host's.
* **No protocol choice for the caller.** The factory is the one place
  that knows what this build can publish over, the way a device's
  bring-up is the one place that knows which graphics API it is on.
* **No readback and no conversion.** A frame that has to become a file
  is a capture and a frame that has to become a video is an encode;
  both are elsewhere, and both leave the GPU. This one never does.
