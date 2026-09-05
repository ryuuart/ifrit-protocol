# SigilCore

The kernels a retained runtime is built on, and the leaves under them.

**The reconciler** — descriptions built fresh every frame, reconciled onto
a tree the host retains, so that only what changed is touched. It owns the
shape of that tree: which retained node answers to which description
(matched by key, then by position), the memo that skips a describe whose
inputs did not change, the identity prune that leaves an unchanged node
alone, and the counts of what a pass did.

**The caching proof** — what a host may keep between frames. Given what a
host declares about one node and what its children answered, it decides
whether a subtree is provably static for the frame, whether a value memo
may hold it, and whether the artefact in hand should be baked, replayed or
thrown away.

Everything a node retains beyond its place in the tree — layout state,
paint caches, running motions — is the host's, reached through named
operations the host implements on itself. That is the line both kernels
draw: they own the DECISION, the host owns the THING.

Under the kernels are four leaves, which libraries far from any
reconciler link on their own.

**Comparable** — what a value needs before anything can decide it did not
change: type erasure that keeps its equality, so a set of operations can
ride on a value and two holders can still ask whether they carry the
same one; and the field pin, which fails the build when a struct grows a
member a hand-written comparator does not mention.

**Compute** — the arithmetic several libraries have to agree on to the
bit: the seeded mixers a jitter draws from, and the folds a cache key is
accumulated with. The standard library is the whole of its dependencies,
so a shader's CPU twin, a point cook, a text cache and a resource store
all reach the same bodies.

**Schedule** — where independent work runs. One parallel for over the
task runtime, taking a count, a grain and a body, so the runtime is
named in one file of this repository and in no header of it; and beside
it a fan-out of its own threads for calls that BLOCK on a disk or a
server, which must not sit on the workers a compute range shares.

**Hardware** — the GPU device itself: one device and its one command
queue, created here or adopted from a host that owns them, with the
textures and fences living on it named by handles that go stale rather
than dangle, and destruction that waits out the frames still in flight.
It knows nothing about what draws — no Skia, no Diligent, no Qt — which
is exactly why it sits here: a 2D backend and a 3D engine can stand on
one device, name the same texture and read the same fence, and neither
has to link the other. A host that holds one never spells a graphics
API.

Namespace `sigil::core`, with `sigil::core::hardware` for the device
catalog. One target per directory:

| target | directory | holds |
|--------|-----------|-------|
| `SigilCoreComparable` | `comparable/` | comparable type erasure, the field pin |
| `SigilCoreCompute` | `compute/` | the seeded mixers, the identifying folds |
| `SigilCoreSchedule` | `schedule/` | the parallel for and its grain, and the fan-out for calls that block |
| `SigilCoreReconcile` | `reconcile/` | the reconciler, its memo, the inherited-value channel, the phase runner, the order declared reads imply |
| `SigilCoreCache` | `cache/` | the cache policy, the settled-subtree proof, the stability release, the bake seam |
| `SigilCoreHardware` | `hardware/` | the GPU device and its queue, owned or adopted; textures and fences by generation-checked handle; deferred destruction |

`SigilCoreComparable` and `SigilCoreCompute` are header-only, so they are
INTERFACE targets and produce no archive; everything else is a static
library. `SigilCoreComparable` takes the standard library and Boost.PFR,
`SigilCoreCompute` the standard library alone, `SigilCoreSchedule` oneTBB
privately, and `SigilCoreReconcile`
Boost.Unordered for its keyed indices. `SigilCoreHardware` takes the graphics
API and nothing else. Consumers still link only the feature they use, without
pulling in a drawing or layout library.

Every public header lives under `include/sigilcore/<feature>/` and is
spelled `<sigilcore/comparable/X.h>`, `<sigilcore/compute/X.h>`,
`<sigilcore/schedule/X.h>`, `<sigilcore/reconcile/X.h>` or
`<sigilcore/cache/X.h>`;
`<sigilcore/comparable/Comparable.h>`, `<sigilcore/compute/Compute.h>`,
`<sigilcore/schedule/Schedule.h>`,
`<sigilcore/reconcile/Reconcile.h>` and `<sigilcore/cache/Cache.h>`
include their own directory's headers. The hardware feature's are
`<sigilcore/hardware/GpuDevice.h>`, `<sigilcore/hardware/Handle.h>` and
`<sigilcore/hardware/Fence.h>`.

| header | holds |
|--------|-------|
| `comparable/Erased.h` | `Erased<Ops>` — comparable type erasure: a set of operations carried on the value that implements them |
| `comparable/Fields.h` | `kFieldCount<T>` — how many direct non-static data members an aggregate has, and the pin a hand-written comparator sits under |
| `compute/Noise.h` | `noise::hash` (a per-index float in [-1, 1]), the 64-bit avalanche `noise::mix64` with its `noise::kMix64Gamma` and the `noise::Mix64Stream` that walks it (`bits`, `unit`, `signedUnit`, `range`), the PCG family `noise::pcgAdvance`, `noise::pcgMix`, `noise::pcgHash`, `noise::pcgNext`, `noise::pcgUnit`, `noise::pcgUnitNext`, the xorshift stream `noise::xorshiftNext`/`noise::xorshiftUnitNext`, and the grid mixer `noise::lattice` |
| `compute/Hash.h` | `hash::kFnvOffset`, `hash::kFnvPrime`, `hash::fnv1a` over a word or over text, and `hash::combine` — the stir that folds one more word into a hash in hand |
| `schedule/Parallel.h` | `schedule::parallelFor(count, grain, body)` over contiguous chunks and `schedule::parallelForEach(items, grain, body)` over a range's elements |
| `schedule/ConcurrentIo.h` | `schedule::concurrentIo(count \| items, body)` — one blocking call per item, off the task runtime — and `schedule::concurrentIoWidth()`, how many of them run at once |
| `reconcile/Reconciler.h` | `Reconciler<Host, Node, Description>` — `render()`, `replaceContent()`, `patch()`, `patchChildren()`, `resolveMemo()`, `keyOf()`, `matchKeyOf()`, `indexKeys()`, `stats()`, `frame()`, and its `KeyIndex` |
| `reconcile/Host.h` | the `ReconcileHost` concept — the operations a host implements — and `DescriptionValue` |
| `reconcile/Node.h` | `Node<Derived, Description>` — the tree skeleton a host's node derives from: `parent`, `description`, `memoShell`, `children` |
| `reconcile/Memo.h` | `Memo<Produced>` — a deferred describe and its key: `props`, `equal`, `invoke`, `env` |
| `reconcile/Env.h` | `env::Provide`, `env::inherited`, `env::inheritedOr`, `env::bound`, and the `env::Snapshot`, `env::capture`, `env::Restore` a memo is built on |
| `reconcile/Erased.h` | `Erased<Ops>` under the name a description spells it by; the type is `comparable/Erased.h`'s |
| `reconcile/Phases.h` | `Phase<Impl>` and `runPhases` — a host's declared pass list with its converging group |
| `reconcile/Reads.h` | `Facet`, `Read`, `orderByReads` — what one node reads off another, and the order that puts every reader after what it read |
| `reconcile/Stats.h` | `ReconcileStats` — the pass counts, and `report()` into `sigil::measure::Counters` |
| `cache/Policy.h` | `Cache` — the three-valued cache policy: `Auto`, `Always`, `Never` |
| `cache/Volatility.h` | `NodeVolatility`, `SubtreeVerdict`, `ChildVolatility` and `foldSubtree` — the settled-subtree proof |
| `cache/Settle.h` | `Settle<Values>` — the stability release: `observe`, `release`, `moved`, `frames`, `held`, `restart` |
| `cache/Bake.h` | `BakeOps<Target>`, `Bake<Target>`, `BakeState`, `BakeAction`, `decideBake`, `runBake` — the bake seam |
| `hardware/GpuDevice.h` | `GpuDevice`, `Backend`, `NativeDevice`, `VulkanHandles`, `NativeTexture`, `TextureDesc`, `TextureFormat`, `TextureUsage`, `mipLevelsFor` — the device, what it is made of, and what a texture on it is |
| `hardware/Handle.h` | `Handle`, `TypedHandle<Tag>`, `TextureHandle`, `BufferHandle`, `FenceHandle`, `HandleTable<T, H>` — a name that goes stale, and the slot store behind it |
| `hardware/Fence.h` | `FenceValue`, `FenceWait`, `kFenceInitialValue`, `kFenceDefaultTimeout` — a timeline and what waiting on one answers |

## Using it

A host is any class that implements the `ReconcileHost` operations on
itself and holds a `Reconciler` over its own node and description types:

```cpp
#include <sigilcore/reconcile/Reconcile.h>

using namespace sigil::core;

struct Description {                       // what the author builds each frame
  std::string key;
  int value = 0;
  std::vector<std::shared_ptr<Description>> children;
  std::optional<Memo<std::shared_ptr<Description>>> memo;
};
using DescriptionPtr = std::shared_ptr<Description>;

struct Instance : Node<Instance, DescriptionPtr> {   // what the host retains
  int lane = 0;                     // survives every patch
};

struct Host {
  Reconciler<Host, Instance, DescriptionPtr> reconciler{*this};
  std::unique_ptr<Instance> root;

  // reading a description
  static const std::string& keyOf(const DescriptionPtr& d) { return d->key; }
  static bool equal(const DescriptionPtr& a, const DescriptionPtr& b) {
    return a->key == b->key && a->value == b->value;
  }
  static bool reconcilesChildren(const DescriptionPtr&) { return true; }
  static const std::vector<DescriptionPtr>& children(const DescriptionPtr& d) {
    return d->children;
  }
  static const DescriptionPtr& descriptionOf(const DescriptionPtr& child) { return child; }
  static const Memo<DescriptionPtr>* memoOf(const DescriptionPtr& d) {
    return d->memo ? &*d->memo : nullptr;
  }
  static DescriptionPtr produce(const Memo<DescriptionPtr>& m) { return m.invoke(m.props); }

  // acting on a node
  std::unique_ptr<Instance> create(const DescriptionPtr& d, Instance* parent,
                                   size_t ordinal, size_t count) {
    auto node = std::make_unique<Instance>();
    node->parent = parent;
    reconciler.patch(*node, d);     // the first patch is the mount
    return node;
  }
  void onPatched(Instance&, const Description* prev, const Description& next) {}
  void reorder(Instance& parent, bool structureChanged) {}
  bool remountRequired(const Instance&, const Instance&) { return false; }
  void invalidate(Instance&) {}
  void destroy(std::unique_ptr<Instance> node, uint64_t frame) {}
};

Host host;
host.reconciler.render(host.root, describe());   // every frame
host.reconciler.stats().patchedNodes;            // what that cost
```

## Mental model

**A description is a value; a node is what it became.** The author
builds a fresh description every frame and throws it away. The
reconciler walks it beside the retained tree and asks, for every node,
whether the new description is provably identical to the one the node
was last described from (`equal`). If it is, the node PRUNES: nothing is
patched, nothing is dirtied, and only its children keep reconciling. If
it is not, the description is swapped in and the host's `onPatched` runs
with the previous one beside it. A field the host's `equal` leaves out
does not produce a wrong answer at the point of the mistake — it produces
a node that never patches again on that field. That is why `equal` must
answer false for anything it cannot compare.

**Children match by key, then by position.** A parent's children are
matched to the new list by key when they carry one, and among the
unkeyed by position. A matched node keeps its handle and everything it
retains; the host's `reorder` is told whether anything mounted,
unmounted or moved, because a reordered list changes what the parent
paints even when every child is identical. A child the new list does not
name is retired through `destroy`, after the reorder, stamped with the
pass it left in (`frame()`).

**An identity change keeps the handle.** A node whose description changed
kind is still that node: `onPatched` sees the previous description and
rebuilds what the new kind cannot carry over, while the handle and the
lanes on it survive. The one exception is the host's own: a property
fixed at mount, which `remountRequired` names, retires the match and
mounts afresh.

**A reader declares what it reads, and the order follows.** Most of what
a settling pass does depends on the node it is looking at; some of it
does not. A label placed at a word, a rule cut to a block, a connector
between two boxes, a light aimed at a mesh — each is a node whose answer
is a function of ANOTHER node's finished answer, and until it says which
node, the only order a host can run them in is the order they were
written in. A reader written before what it reads is then one pass
behind, every frame.

A `Read` is that declaration — a key, and which facet of that key's node
is read (`Bounds`, `Outline`, `Coverage`, `Units`) — and `orderByReads`
turns a set of them into the order the readers must run in. It knows
nothing of what a facet MEANS or how a key resolves; both are the host's.

It is STABLE, which is the property that makes adopting it free: readers
that read none of each other come out exactly as they went in, so a host
whose readers are independent runs them in the order it always did, and
only a real edge moves anything. A cycle is broken where it closes — the
readers caught in one keep their declaration order — so a cyclic
declaration is a slightly-off pass rather than a hang, which is the same
bargain the convergence cap makes.

**A memo is a pure function of (props, environment).** A description can
be a memo shell: props, a comparison over them, and a deferred describe.
The reconciler compares the shell's captured environment first and its
props second against the shell the node was last described from; on a
hit the node's payload stands and the describe is skipped, on a miss the
describe runs under the environment its author had (`env::Restore`) and
the result becomes the payload. The shell rides on the node as
`memoShell`; the payload is `description`.

**An inherited value lands in the description.** `env::Provide<T>` binds
a value for a describe scope and `env::inherited<T>()` reads it four
levels down; the value is read DURING describe and lands in the reading
node's own description, so the prune is already an exact dependency
tracker and no phase learns a new concept. The environment reaches the
kernel only through the memo, where it is part of the key.

**The one upward signal is `invalidate`.** The reconciler never marks a
host's caches by itself; a node whose content changed under it — a
slot's content replaced through `replaceContent` — is reported through
`invalidate`, and the host stales what it keeps above the node.

**Animation is not the reconciler's.** A patch bends the running motions
of one description onto the endpoints of the next, and every part of
that — the lane that addresses a held motion, the retargets over a fixed
or a positional family, and the comparators that decide two animatable
slots are the same — is SigilMotion's, in `<sigilmotion/values/Lanes.h>`
and beside the values themselves. The reconciler calls a host's
`onPatched` and the host does the retarget; the kernel names no motion
type at all, and links no motion target.

**Phases converge.** A host declares its settling passes as a list of
`Phase<Impl>` — a name, a member function answering whether it moved
anything, and whether it converges. `runPhases` runs each non-converging
phase once and the contiguous converging group until a round changes
nothing or `maxRounds` is reached, calling `settle` after every round
that changed something. The cap is what guarantees termination if two
writers ever disagree permanently.

## The caching proof

**What a host reports is a DECLARATION, never a difference.** `foldSubtree`
takes one `NodeVolatility` — the `Cache` policy the author stated
(`Auto`, `Always`, `Never`), whether they asked for the subtree to be
held by a value memo, and five facts about what this node does off the
describe clock — and the `ChildVolatility` its children folded into. It
answers a `SubtreeVerdict`.
A host that reports a term one frame late has already replayed an artefact
of a frame that has changed; a host that reports one early pays a re-bake
and nothing else. When in doubt, declare.

**The five facts are five different questions.** `ownPaint` is
motion that changes how the node COMPOSITES without changing what it
draws — the node's own artefact still replays, because it is drawn through
the motion, while an ancestor's would contain it. `ownContent` rebuilds
what the node draws, and blocks the node's artefact and every ancestor's.
`memoOpaque` is volatility no value comparison can SEE, which is the one
term a value memo turns on. `readsBackdrop` is not volatility at all: the
node composites against what is already on the canvas, so it cannot be
inside a bake, whose ground is transparent black. `samplesDestination` is
the half of that which refuses to be the bake's ROOT too — a root's blend
and opacity are applied outside its bake, a destination-sampling filter
inside it.

**The promise is one-sided.** A subtree the proof calls settled cannot
change its pixels without the host being told first. A subtree it calls
volatile may well be standing perfectly still — proving THAT is the value
memo's job, and `Settle` is what makes it sound.

**A binding cannot say that it stopped.** It stays connected for the life
of the node it drives, so an entrance that played once declares exactly
what a loop declares. `Settle<Values>` separates them by observation:
`observe` counts consecutive frames on which the values a node draws from
resolved identically, `release` converts a warmed-up count into the
verdict, and `moved` — run once per draw over what was released —
re-declares the frame a value assigned from OUTSIDE moves again. All three
sides are required. Skipping one does not fail loudly; it replays a frame
that has already changed.

**The decision is the kernel's, the artefact is the host's.** `BakeOps`
names four operations over a `Target` the kernel never looks inside: take
the bake, replay it, drop it, and say whether one is held. `decideBake`
answers `Live`, `Take` or `Replay` from three facts. A host with several
tiers — a recorded command list, a rasterized image, a whole subtree
composited into one layer — writes one model per tier and asks the same
question of all of them, which is what stops each tier from growing its
own copy of the rule.

## The leaves

**A comparable value carries its own equality.** `Erased<Ops>` holds a
model behind an abstract interface, and copies of one value are equal by
their shared state; two separately built values are equal when they hold
the same model type and that type's `==` says so. A model with no `==`
is the escape hatch and compares equal to nothing but its own copies —
conservative on purpose, because a value that cannot answer must never
answer "unchanged". That is what lets a seam — which executor draws,
which resolver runs, which painter paints — ride on a description that
something else compares.

**A hand-written comparator forgets silently.** Leave a field out and two
different values compare equal, the holder concludes nothing changed, and
it keeps producing what the old value produced for as long as it lives.
Nothing detects that from outside, because the wrong answer looks exactly
like a value that really did not change. `kFieldCount<T>` reads the member
count off the type, so `static_assert(kFieldCount<T> == N)` beside the
comparator turns adding a field into a build failure that names the
comparator to go and fix.

**A mixer is a contract, not a choice.** Everything in `compute/` is a
bit-exact function of its inputs, and the constants are fixed by the
agreement between the places that compute them: renders stored as bytes
are seeded through `noise::`, a GPU kernel reproduces `pcgAdvance`,
`pcgMix` and `pcgHash` word for word, and cache keys are accumulated with
`hash::`. Changing a constant fails no build. It re-rolls every stored
render, desynchronizes the two ends of every operator chain that runs on
both, and re-buckets everything already keyed. The tests pin exact
outputs — words as words, floats as bits — because determinism, range and
"different for different inputs" all survive a body that drifted.

**Two mixers side by side are two different functions.** `noise::hash`
and `noise::pcgHash` mix differently and answer differently; each seeds
work compared byte-for-byte against stored renders, so neither can become
the other. New code takes `pcgHash`. The same rule decides every
candidate for this directory: a body that differs is a second function
under its own name, never a merge.

The same rule holds for the STREAMS. `noise::Mix64Stream`,
`noise::pcgNext` and `noise::xorshiftNext` walk three different mixers,
and a caller already keyed to one draws a different sequence from the
others. What the splitmix stream buys over the other two is its 64-bit
counter: a caller with two integers to fold into one seed packs them into
a word and does no mixing of its own.

## Where work runs

**A grain, and nothing else.** `schedule::parallelFor(count, grain, body)`
divides `[0, count)` into contiguous chunks and calls `body(first, last)`
on each; the chunks are disjoint and together cover the range exactly
once. The grain is a COUNT OF ITEMS — how many are worth handing to one
worker, which follows from what one item costs — and it is written where
the body is written, because that is the only place the cost of an item
is known. A body that touches one float per item takes a large grain; a
body that compiles a program per item takes a grain of one.

The grain is also the whole of the small-range rule: a count no larger
than one grain IS one chunk and runs on the calling thread, with no task
created and no second constant that could disagree with the first. What
a caller must not do is put a measured number here and call it settled —
a number that only a benchmark could falsify belongs in a ledger, and the
grain that survives in the code is the one that says what an item costs.

```cpp
#include <sigilcore/schedule/Parallel.h>
using namespace sigil::core;

// A pass over point lanes: cheap per item, so a worker takes many.
schedule::parallelFor(count, kLaneGrain, [&](size_t first, size_t last) {
  for (size_t i = first; i != last; ++i) values[i] = displace(values[i]);
});

// One program compiled per element: expensive per item, so a worker
// takes one.
schedule::parallelForEach(work, 1, [&](const Request& request) {
  compile(request);
});
```

**A call that blocks is not a chunk of work.** A read from a disk, a
fetch from a server or a wait on another thread's lock spends nearly all
of its time waiting for something that is not a core. Run through the
parallel for, one such call holds a worker of the one pool every parallel
range in the process shares, and a handful of them stalls all of them —
including ranges written nowhere near the fetch. `schedule::concurrentIo`
is the other seam: one blocking call per item, on threads of its own.

```cpp
#include <sigilcore/schedule/ConcurrentIo.h>

schedule::concurrentIo(pending.size(), [&](size_t i) {
  pending[i].fetched = fetch(pending[i].uri);   // waits on a disk or a server
});
```

Its threads last exactly as long as the call: the fan-out starts them
when it is asked and joins every one before it returns, so nothing is
parked between calls, nothing has to be shut down at exit, and a process
that never fetches has no thread for it. Starting a thread per helper per
call is the price, and it is small beside the wait that motivated the
call — which is also why this seam is wrong for short compute chunks.
`concurrentIoWidth()` is how many run at once, and it is deliberately
larger than the core count: these threads are waiting rather than
computing. A body that throws does not abandon the batch — every item is
still handed out and every thread still joined, and the first exception
is rethrown to the caller afterwards.

## The device

A consumer that wants a GPU device — one it owns, or one an engine
already created — takes `GpuDevice`, and names textures and fences by
handle rather than holding the API's objects:

```cpp
#include <sigilcore/hardware/GpuDevice.h>
using namespace sigil::core::hardware;

// The platform's own device and a fresh queue — or adopt a host's:
std::unique_ptr<GpuDevice> device = GpuDevice::createOwned();
// NativeDevice native{Backend::Metal, mtlDevice, mtlCommandQueue};
// device = GpuDevice::adopt(native);      // never frees them

TextureDesc desc;
desc.width = 1920;
desc.height = 1080;
desc.format = TextureFormat::BGRA8Unorm;
TextureHandle target = device->createTexture(desc);

FenceHandle fence = device->createFence();
for (;;) {
  device->beginFrame();          // retires destroys three frames old
  drawSomehow(*device, target);  // whatever backend stands on this device
  FenceValue done = device->signal(fence);  // behind everything submitted
  // …later: device->waitCpu(fence, done) blocks; device->waitGpu(fence, done)
  // holds later queue work instead.
}
device->destroy(target);         // stale at once, released at frame + 3
```

`device->exportNative(target)` hands the API's own object out, for a
host that draws with the API directly or publishes the texture onwards.
A texture the host made enters the same table through
`importNative(nativeTexture)` — borrowed, so destroy only forgets it —
or `importNative(nativeTexture, /*takeOwnership=*/true)`, after which the
device releases it like one of its own.

**A texture may carry a chain.** `TextureDesc::mipLevels` asks for one,
level 0 at the description's size and each level after it half the last;
`mipLevelsFor(width, height)` is how deep the size allows, and a count
past it is clamped to it. A chain is not only a filtering aid here: a
PREFILTERED ENVIRONMENT is a different image on every level, and the
level a shader reads is the one its roughness picked, so the count has to
be part of the description rather than something generated afterward from
level 0. `exportNative` reports what the texture actually got.

### Adopting a device an engine created

**A VULKAN DEVICE IS ONLY EVER ADOPTED.** 3D engines create the Vulkan
device themselves and cannot attach to one that already exists, so
whoever owns the API in a process makes it and everything else joins it,
rather than the other way round — and a second instance here would mean
two loaders, two queues and a copy between them. `createOwned` therefore
makes the platform's own device (Metal on Apple) and nothing else.

What `adopt` wants is exactly what such an engine exposes: instance,
physical device, device, queue and queue family, the API version, and the
`vkGetInstanceProcAddr` the loader already in the process hands out.
Three conditions come with it:

- **Timeline semaphores must be enabled on that device.** A fence here is
  one, and a device created without them cannot make one. Ask the engine
  for the feature before it creates the device — Diligent spells it
  `NativeFence` — because it cannot be turned on afterwards.
- **The loader must be the engine's.** An adoption with no
  `getInstanceProcAddr` is refused: opening the same library a second
  time would work, but then the two APIs dispatch through separately
  opened copies of it and "one device" stops meaning anything.
- **The queue is now shared, and sharing has a rule.** Every submission
  goes into the one queue in submission order — that is what lets a
  submit be asynchronous and still correct — but only while the streams
  never interleave. Hold whatever lock the engine guards its queue with
  around every foreign submit and around every `signal`, `waitGpu` and
  `waitCpu` on this device.

Nothing is freed by an adopted device: the instance, device and queue
stay the engine's, and keeping them alive for as long as the `GpuDevice`
lives is the caller's business.

### How the device behaves

**Handles are names, not pointers.** A `TextureHandle` or `FenceHandle`
is a slot index plus the generation the slot had when the name was
issued. Destroying a resource frees its slot and bumps the generation,
so a handle kept past the destroy compares unequal to whatever later
lives in that slot: `isValid` says no, `exportNative` returns empty,
`signal` returns the initial value, and nothing reaches the wrong
resource. The typed handles do not convert into each other.
`HandleTable` is the store behind them and can name anything a host
wants named the same way.

**Destruction waits out the frames in flight.** `destroy(texture)` makes
the handle stale at once but releases the native texture only when
`beginFrame()` has advanced `kFramesInFlight` (three) frames past the one
it was destroyed in — a frame that was recording when the destroy came in
may still reference it on the GPU. A host that never calls `beginFrame()`
never releases anything until the device is torn down, which releases
everything.

**A fence is a timeline.** Its value only ever grows; `signal` queues a
raise to the next value behind everything submitted so far and returns
that value, `waitGpu` holds every later submission until the value is
reached, and `waitCpu` blocks for it with a timeout. On Metal a fence is
an `MTLSharedEvent` and every wait and signal is a command buffer on the
device's queue; a queue executes in order, so `waitGpu` is for a value
that is already signalled or will be signalled from *another* queue —
`exportNative(fence)` hands the event to one — and a signal queued on the
same queue behind the wait can never run.

**Every call is safe from any thread except `beginFrame()`**, which
belongs to the one thread that counts frames. A device from
`createOwned` releases its device and queue when it dies; one from
`adopt` never does.

**Two backends, one contract.** What each supports:

| | Metal | Vulkan |
|---|---|---|
| `createOwned` | the system default device and a fresh queue | — a Vulkan device is only ever adopted |
| `adopt` | `mtlDevice` + `mtlCommandQueue` | instance, physical device, device, queue and family index, and the host's own `getInstanceProcAddr`; the device must have timeline semaphores enabled |
| texture | `id<MTLTexture>`; `cpuAccessible` is shared storage | `VkImage` + `VkDeviceMemory`, optimal tiling, sampled, colour-attachment, input-attachment and transfer usage (+ storage for `ShaderWrite`) — input attachment because a 2D backend reads a render target back through one; `cpuAccessible` prefers host-visible coherent memory and falls back to device-local; formats map to `R8G8B8A8_UNORM`, `B8G8R8A8_UNORM`, `R16G16B16A16_SFLOAT` |
| import with ownership | retains the texture | destroys the `VkImage` and frees `vkMemory` when given |
| fence | `MTLSharedEvent` | timeline `VkSemaphore`; `waitCpu` is `vkWaitSemaphores`, queue signal and wait are empty submissions |
| loading | the framework | every entry point resolved from the host's own `vkGetInstanceProcAddr` at run time; nothing links Vulkan |

The Vulkan arms of this feature are therefore exercised where a Vulkan
device is made: `geometry_device_test`, beside the feature that creates
one. They skip, naming why, on a machine with no Vulkan runtime (on
macOS: `brew install molten-vk vulkan-loader`).

## What belongs here

A function earns a place in one of the two header-only leaves when three
things hold:

- **Two libraries need it identically.** Not "could share it" — actually
  compute the same number today, or have to agree with each other
  tomorrow because one reproduces the other (a shader and its CPU
  preview, a device executor and its reference).
- **The contract can be stated.** What it answers, what is fixed about
  it, and what breaks if it changes — in the header, evaluable by
  someone who has opened nothing else.
- **It brings its own test.** For the kernels that means behaviour over
  a fake host; for the leaves it means the outputs themselves, pinned.

Worked both ways: `Erased` qualifies because a reconciler's descriptions,
a mesh runtime and a point-operator runtime all carry a comparable erased
value, and one shape of it is what lets a value cross between them.
`kFieldCount` qualifies because a pin is worth nothing if each library
writes its own and one gets it subtly wrong. The seeded mixers qualify
because a stored render and a GPU kernel have to agree to the bit. An
easing curve does NOT qualify: it is animation's, and SigilMotion owns
it — which is why the comparator over that curve lives there too, beside
the curve, rather than in the kernel that happens to prune with it.
Neither does a numeric constant a single library reaches for — those stay
with the library that spells them.

## Boundary

SigilCoreComparable and SigilCoreCompute link nothing of this project's
at all — the standard library, and Boost.PFR for the field pin. That is
the whole point of them: a library anywhere in the tree can link one
without acquiring a kernel, and SigilMotion is one of the libraries that
does, for the pin its own comparators sit under. SigilCoreReconcile links
SigilCoreComparable (the erased seam value and the field pin) and
SigilMeasure (the published counts), and nothing that draws, lays out,
shapes text or animates. SigilCoreSchedule links nothing of this
project's either: a task runtime, privately, and the standard library's
threads. That privacy is the feature — a consumer hands over a count, a
grain and a body, so no header in this repository spells a task runtime,
and the one that runs every divided range in the process is chosen in one
file. SigilCoreCache
links SigilCoreReconcile alone. SigilCoreHardware links nothing of this
project's either: the platform's graphics API — Metal where there is one,
the Vulkan loader resolved at run time everywhere — and no Skia, no
Diligent, no Qt. That is what makes one device serviceable by two
drawing libraries at once: SigilSkia stands Graphite on it, a Diligent
renderer adopts it, and neither knows the other is there. What stays
with a host: what a node
retains, what a patch does to it, how children are ordered for drawing,
which descriptions compare equal, which of its own values are volatile,
and every artefact.
The leaves are linked from well outside this library: SigilGeometry's
path leaf builds its value-noise field on the mixers, its mesh and
point-operator runtimes are erased values, SigilWorld's geometry
signature is an FNV fold and its element comparator sits under the pin,
and SigilWeave's intercept cache keys with the stir. SigilCompose is one
host — its `Composer` holds a `Reconciler` over its
`Instance` and `ElementNode`, folds its Skia lanes, materials, gates and
text into the proof's declarations, holds a `Settle` over its own content
scalars, and implements `BakeOps` over its picture recordings. Yoga,
text, paint and the meaning of every term stay on its side of the seam.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Release
cmake --build build --config Release
ctest --test-dir build -C Release -R '^core_' --output-on-failure
```

A binary here exists where it links a **strictly smaller** set of targets
than its neighbours, or where a runner has to supply something the others
do not; two binaries over one closure with nothing to tell a runner are
one binary. Every one of them links only the feature it exercises, so an
edge that pulled a drawing, layout or animation library in would show up
as a build failure rather than as a test that still passed:

| binary | what it proves | label |
|---|---|---|
| `core_comparable_test` | the erased value — empty, copies of one value, two comparable models compared by type and by value, the escape hatch equal to nothing but its own copies — and the field pin over aggregates of the shapes a comparable value takes | — |
| `core_compute_test` | the mixers and folds, pinned to the exact words and floats they produce | — |
| `core_schedule_test` | what the work seam promises: chunks disjoint and covering the range exactly once, the grain alone deciding when a range stays on its caller, a body's exception reaching the caller, and the blocking fan-out running every item once, two at a time, and joining every thread even when one item fails | — |
| `core_reconcile_test` | the reconciler over a fake host, the inherited-value channel, the phase runner and the read ordering | — |
| `core_cache_test` | the settled-subtree proof, the stability release and the bake seam over a fake host | — |
| `core_hardware_test` | what the device feature decides without a device: generation-checked handles, and how deep a mip chain a size allows | — |
| `core_hardware_device_test` | a real device — what it comes up with, what it refuses to adopt, when a destroyed resource is really gone, who releases an imported texture, fences as timelines, and the levels a texture is built with | `gpu` |

`core_hardware_device_test` exists on Apple alone and every case in it
skips where there is no GPU, which is why it carries a label: a case that
skips is not coverage on the machine it skipped on, and the label is how
a runner is told. The same questions on the Vulkan backend are asked in
`geometry_device_test`, since that is where a Vulkan device exists to ask
them of.

One file per subject, named for what it asserts: `HashTest` and
`NoiseTest` in `compute/test/`; `ErasedTest` in `comparable/test/` (the
erasure and the field pin are one subject — what a value needs before
anything can decide it did not change — and a consumer takes both or
neither); `ReconcilerTest`, `EnvTest`, `PhasesTest` and `ReadsTest` in
`reconcile/test/`; `VolatilityTest`, `SettleTest` and `BakeTest` in
`cache/test/`; `HandleTest`, `MipChainTest` and `DeviceTest` in
`hardware/test/`.

A case asserts one thing a public header promises and is named that
promise as a sentence, so a failure line reads as the claim that broke.
It pins only what editing this library could falsify. The exact words and
floats in `compute/test/` are exactly that: these bodies exist so a
second implementation of one of them agrees to the bit — a GPU kernel
reproduces the PCG three word for word, and a jitter, a point cook and a
shader's CPU twin all have to draw the same number for the same index —
and no property catches a drifted mixer, because "in range", "the same
twice" and "different for different seeds" are all still true of the
wrong stream. A claim made N times with one thing varying is one `TEST_P`
whose parameter is that thing, with its rows named: the FNV folds are one
over seven inputs, and the ranges the headers state for their draws are
one over six draws.

Each kernel is exercised over a fake host: `reconcile/test/FakeHost.h`, a
host with nothing behind it that records every operation the reconciler
asks of it as a structured event — the operation, whose key, and that
operation's own arguments — so a claim about what was asked and in what
order survives any rewording of what a host would print; and
`cache/test/FakeCacheHost.h`, whose nodes declare volatility, hold a
numbered artefact instead of pixels and count every operation asked of
them. A fake host is the subject of a measurement as much as of a test,
so each feature's benchmark compiles with its own `test/` on its include
path and drives the host defined there — one definition, and the two
binaries cannot disagree about what they are exercising. Each fake lives
in its own `sigil::core::test::<feature>` namespace, which is what lets
every one of them spell the plainest name for what it is — `FakeHost`,
`FakeNode` — with no feature's test reaching into another's directory to
find out.

The benchmarks are executables, not tests: `core_comparable_bench` times
each erased comparison against the same question asked of the model
directly, so what erasure costs is the difference between two arms;
`core_compute_bench` times each mixer one call at a time, which is how
they are spent; `core_schedule_bench` times a divided range over a body
that does nothing but touch its item, at three sizes, so what is measured
is the split rather than any consumer's arithmetic; `core_reconcile_bench`
and `core_cache_bench` time the reconciler and the proof over the fake
hosts at several node counts; and `core_hardware_bench` times the device.
They build through the `benches` target and run through
`scripts/bench_ledger.py`, which is where any number about them belongs.
