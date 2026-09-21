# SigilPython

Reusable Python bindings for the native drawing, composition, typography,
motion, material, geometry, World, image, data and resource libraries. The static
`SigilPython` target has no sketch runtime, application, window or interpreter
startup dependency. Namespace `sigil::python`.

The bindings construct the native values and call their native algorithms.
They do not provide another element tree, motion engine, renderer or IO loop.
The authoring target is the full native sketch catalog. Coverage is incomplete;
linking a native library does not imply that every C++ API is exposed to Python.

Callable inputs are named in the bindings so runtime help, keyword calls and
generated editor signatures agree. Positional variadic APIs retain typed
overloads in the package. An element's `children` method accepts individual
elements or one iterable, including a list, tuple or generator. Each call
appends in order and returns the same element; all inputs are converted before
any are appended. Container factories accept the same child forms; `layout`
takes its scheme before its children.

## Layout

ONE DIRECTORY PER NATIVE LIBRARY, ONE FILE PER SUBJECT. Every binding
source sits under the directory of the library it binds — `core/`,
`skia/`, `image/`, `material/`, `geometry/`, `motion/`, `weave/`,
`compose/`, `draw/`, `world/`, `data/`, `io/`, `measure/`, `video/`,
`scry/`, `substance/`, `usd/` — and the four files at the root are the
kernel every one of them is written out of: the module assembly, the
conversions and callback ownership, the reach into another file's
registration, and the public naming. The sketch adapter is laid out the
same way under `src/sketch/python/`: `host/` is what a sketch is handed,
`kit/` the specimen values it dresses with.

A directory's headers live under `include/sigilpython/<library>/`, and a
library has them only where another binding file needs what they carry: a
conversion the whole tree shares, or the one registration header that
declares every bind function the library's files define.

One subject is one source file with one bind function in it, and that
file has one author. An author fills in a file that already exists,
adds rows to a table and writes their own refinement fragment; they do
not add a source to a CMake list, a call to the module assembly or a
declaration to a registration header, because all three already name
every subject this library plans to bind. That is what lets a hundred
subjects be written at once without two hands in one file.

## Module assembly

* `Python.h` — `bindLibraries`

A host initializes its interpreter and registers these bindings in its module:

```cpp
#include <sigilpython/Python.h>

sigil::python::bindLibraries(module);
```

Register once per interpreter. This function installs the common submodules
in dependency order, including neutral Compose kit and layout values. The host
can then add its own checked service views and execution model. Registration
does not create a sketch context or import a sketch loader.

ORDER IS THE ONE RULE OF THE ASSEMBLY. A type has to be registered
before any signature names it: pybind11 writes a parameter's spelling
when the function carrying it is registered, so a signature naming a
class that does not exist yet carries the C++ name instead of the Python
one, and the generated declarations carry it too. `bindLibraries` is
therefore one list in dependency order — the libraries in the order they
depend on each other, and within a library the subjects in the order
they do. Registration is the only thing that list does; a subject that
must run late says so by where its line sits.

The Python package, convenience functions, declarations and package tests
belong to `apps/python/sigil`. Native callback signature adaptation imports
its neutral `sigil._callbacks` helper. An embedding host supplies the
matching package on its interpreter's module path. A normal extension uses
its importing environment.

* `PublicNames.h` — `namePublicly`

Every class, enumeration and free function reports the module an author
imports it from, so a repr and `pydoc` name the package rather than the
extension. Call `namePublicly` once, after the last registration, because a
class records its module when it is registered. The table it reads is
written out again in the package's `typing/surface.py`, which generates the
declarations and the package's own modules; a module the table does not
name keeps the extension's own naming, and the package's surface check
reports it. The modules keep the extension's naming either way: that is
what a stub generator reads to find what a module declares.

The sketch adapter assembles the complete `_sigil` module by registering this
library followed by sketch context, session and specimen-kit bindings. An
independent native host can link only `SigilPython` and assemble a different
module. Each interpreter must load one copy of the binding implementation:
registered types and callback ownership registries are shared state.

## Reaching another file's registration

* `Extend.h` — `submodule`, `extend`, `registeredClass`

A subject publishes under a submodule and several subjects share one, so
`submodule` answers the module a dotted path names and creates it where
nothing has yet. It is idempotent: whichever registration reaches a path
first makes the module, and the rest are handed the same one, so no
subject owns a namespace its peers have to wait for.

`extend` is the same courtesy for a class. A class belongs to the file
that registered it, and a binding in another file adds a verb to it by
borrowing the class object back out of the module rather than editing
that file:

```cpp
extend<Element>(module, "compose.Element").def("ripple", &rippleOf);
```

It throws when nothing of that name is registered, which is what an
author sees when their line in the assembly sits above the registration
they meant to extend. `registeredClass` is that check on its own, for a
caller that wants the type object rather than a class handle.

A borrowed class carries whichever holder the file that registered it
gave the type, and the template cannot read that back, so the holder is
the second template argument wherever it is not pybind11's own
`std::unique_ptr`:

```cpp
extend<ImageAsset, std::shared_ptr<ImageAsset>>(module, "image.ImageAsset");
```

Adding methods and properties through the wrong one still works; adding
a constructor or a factory through it allocates a holder the rest of the
bindings do not expect. `submodule` refuses rather than replaces when a
step of its path already names something that is not a module, because a
silent replacement would unregister the class or function another package
put there.

## Callback ownership

* `Bindings.h` — `PythonValue`, `PythonCallback`, `CallbackLifetime`,
  `CallbackScope`, `CallbackBoundary`, `BorrowedPen`, `retainCallback`,
  `retainValue`, `retainScope`, `isCurrentScope`, `closeScope`, `invokePen`

A host owns a callback lifetime and opens a callback scope while Python creates
retained descriptions. Native elements, brushes and animation descriptions can
then hold Python values without tying their cleanup to the Python wrapper's
lifetime. Closing the host lifetime clears those Python references, including
bound methods that would otherwise retain their owner through a native element.
An escaped description cannot call a cleared value.

The lifetime registry holds weak references. A value outside any host scope
has ordinary shared ownership and stays callable until its native owners release
it. Python reference destruction acquires the interpreter lock; the interpreter
must outlive native values that can release Python references. Native embedding
hosts keep the interpreter alive through their process teardown.

A callback boundary unwinds native scopes that Python opened but failed to
close. A scope registers an owned handle and a nonthrowing native close function;
explicit close and automatic unwinding both leave escaped handles inert. Scope
owners and boundaries operate on the same thread. Typed scope adapters still
validate their native environment and thread before requesting explicit close.
Boundaries close before an enclosing native environment restore, so deferred
memo descriptions cannot leave providers in a different stack.

The pen wrapper checks both its drawing thread and callback lifetime. Every
native drawing callback receives this same wrapper. Retaining it in Python
does not extend access to the borrowed pen.

A native call whose callback is handed a canvas rather than a pen gets one
built for it, so the author draws with the same verbs and the same checks
everywhere. A panel is that case: its body is lent a pen of its own, opened
on the canvas the perspective transform left standing, whose `width` and
`height` are the panel's size in world units and whose origin is the panel's
centre. What the body raises is carried back out of the native draw and
raised after it returns, because the call stands inside a canvas save that an
exception through it would leave open.

## Conversion and resource seams

* `Bindings.h` — `color`
* `skia/Values.h` — `point`, `rect`
* `compose/Convert.h` — `dimension`, `fill`, `surfacePaint`, `alignment`,
  `justification`, `shape`, `elements`
* `compose/Kit.h` — `converted`, `record`, `field`, `wellFields`,
  `contentType`
* `material/Convert.h` — `materialColor`
* `motion/Convert.h` — `TickerHandle`, `TimelineHandle`,
  `motionAnimatable`, `motionInk`, `motionFill`, `motionEase`,
  `motionTransition`
* `draw/Canvas.h` — `CanvasSource`, `BorrowedCanvas`, `penCanvasSource`,
  `borrowedCanvas`, `canvas`, `invalidateCanvas`
* `compose/PaintPrograms.h` — `BorrowedPaintContext`, `PaintContextLoan`,
  `CanvasLoan`, `paintProgram`, `penProgram`
* `compose/Composer.h` — `ComposerHandle`
* `io/Hub.h` — `HubHandle`, `retainSessionFeed`
* `data/Convert.h` — `dataDatabase`, `loadData`
* `geometry/Casters.h` — the casters that let a Python sequence stand for
  a glm vector of floats or of whole numbers, and three sequences of three
  for a rotation counted by columns; they declare no name of their own

ONE COLOUR CLASS REACHES PYTHON. Skia's colour value and SigilMaterial's are
the same four straight sRGB floats, so only `material::Color` is registered:
a caster gives every `SkColor4f` parameter the reading `color` performs — the
colour class, a CSS string, a three- or four-channel sequence — and answers
every `SkColor4f` return as that class, which is the one with the colour
verbs on it. `bindColor` registers it ahead of every other library, because
a signature that names a colour is written when its function is registered
and reads the class's own name only once the class exists.

Three named unions describe what a colouring parameter accepts, and each
parameter takes the widest its slot can honour: a colour through `color`, a
flat mark through `fill`, and anything that colours a surface through
`surfacePaint`. `fill` collapses a static paint onto the one comparable Fill
a flat mark holds and refuses a live or geometry-dependent one, naming the
verb that takes it — which is why `Element.textStroke`, whose outline is one
such Fill, is declared as a flat mark while `Element.textFill` is declared as
a surface. A recipe instance converts to a paint wherever Python takes a
paint; the native constructor stays spelled, because a C++ overload set
holding both would be ambiguous.

A slot that can hold only part of the union it is declared with says so when
it is given the rest, rather than painting something nobody asked for.
`Element.textFill` stores one paint resolved without the tree, so the ink in
force, a custom property and a bound fill raise there and `None` is how an
override is cleared; every other surface verb takes the whole set. A paired
`kit.line` is the one place a value is narrowed silently, and the narrowing
is the rails', not the verb's: each rail stores one comparable fill, so a
paint that needs a frame rules the pair in the ink in force.

A TICKER AND A COMPOSER ARE EACH ONE PYTHON CLASS, OWNED OR LENT. A handle
holds the value it made, or an access function a host supplies, which is
asked at every call and reports a closed session in the host's own words.
An owned composer keeps its ticker and a system-backed font context beside
it, refuses another thread, and refuses every call made from inside its own
draw; a lent one refuses the size, the clock and the draw, which stay the
host's. A timeline is read back through its ticker's access, so one that
outlives its session refuses instead of reading storage that has gone.

A canvas reaches Python as `draw.Canvas` over a canvas source, and the
source is asked for the canvas at every verb, so a wrapper kept past its
loan refuses instead of drawing. A pen lends its canvas through
`penCanvasSource`; a native call that hands a program a canvas and a paint
context lends both through a loan that ends when it is destroyed, and a
paint program or a pen program names none, one or both of what it is lent.

These headers let another native adapter use the same conversions and wrapper
types. A checked hub accepts host-provided access and feed-retention functions;
owned hubs have independent lifetimes. IO transport workers execute no Python.
Overlapping host sessions can share a feed lease, and the last lease closes its
transport without holding the interpreter lock. Escaped checked wrappers cannot
keep a closed host session alive.

The Compose document submodule binds native semantic components. Containers
accept the same Element children as ordinary Compose containers; paragraphs
accept plain strings or native rich text. Role rules and default properties
remain native declarations resolved from the eventual parent tree. They do
not read a Python theme or capture an authoring-time environment.

## The registration index

ONE HEADER PER LIBRARY, AND EVERY ONE OF ITS REGISTRATIONS IN IT. A
registration header is the whole list of what that library's packages add
to the extension module, so an author reads one file to see which
subjects their library already plans and what each is called. Four
libraries are declared only where their SDK is present, and the module
assembly guards their calls the same way.

* `core/Registration.h` — `bindCore`, `bindCoreCompute`, `bindCoreValues`,
  `bindOptionalLibraries`, `bindRecordProtocol`
* `skia/Registration.h` — `bindValues`, `bindSkiaEffects`, `bindSkiaFonts`,
  `bindSkiaPaths`, `bindSkiaSurfaces`
* `image/Registration.h` — `bindImageValues`, `bindImageMeaning`
* `material/Registration.h` — `bindColor`, `bindMaterial`,
  `bindMaterialCore`, `bindMaterialEnvironment`, `bindMaterialField`,
  `bindMaterialKitGrained`, `bindMaterialKitText`,
  `bindMaterialPaintEffect`, `bindMaterialPattern`, `bindMaterialShading`,
  `bindMaterialSkiaDraw`, `bindMaterialTexture`, `bindMaterialTextureSets`
* `geometry/Registration.h` — `bindGeometry`, `bindGeometryCharts`,
  `bindGeometryDeviceHandle`, `bindGeometryFrames`, `bindGeometryMesh`,
  `bindGeometryMeshCamera`, `bindGeometryMeshCodec`,
  `bindGeometryMeshCurve`, `bindGeometryMeshPoints`,
  `bindGeometryMeshRender`, `bindGeometryPathEditing`,
  `bindGeometryPathOperations`, `bindGeometryPointBuilder`,
  `bindGeometryPointKernels`, `bindGeometryPointOperations`,
  `bindGeometryPolylines`, `bindGeometryProfiles`, `bindGeometryRegions`,
  `bindGeometrySeams`, `bindGeometryShapes`, `bindGeometryStructures`
* `motion/Registration.h` — `bindMotion`, `bindMotionAnimatableForms`,
  `bindMotionClock`, `bindMotionLanes`, `bindMotionParticles`,
  `bindMotionPhysics`, `bindMotionSchedule`, `bindMotionSignals`
* `weave/Registration.h` — `bindWeave`, `bindWeaveCascade`,
  `bindWeaveChoreography`, `bindWeaveFlows`, `bindWeaveFonts`,
  `bindWeaveLayout`, `bindWeavePorts`, `bindWeaveSelectorUnicode`,
  `bindWeaveShaping`, `bindWeaveTables`
* `compose/Registration.h` — `bindCompose`, `bindComposeBrushComposites`,
  `bindComposeBrushMarks`, `bindComposeComposer`,
  `bindComposeDecorationPrimitives`, `bindComposeDecorationSeam`,
  `bindComposeDerive`, `bindComposeFeed`, `bindComposeInstancing`,
  `bindComposeKit`, `bindComposeKitAnnotations`, `bindComposeKitEras`,
  `bindComposeKitKinetic`, `bindComposeKitLegibility`,
  `bindComposeKitOrnament`, `bindComposeKitPixelType`,
  `bindComposeKitRoutes`, `bindComposeKitRows`, `bindComposeKitSprites`,
  `bindComposeKitStrokes`, `bindComposeKitTypeset`,
  `bindComposeLayerStyles`, `bindComposeLines`, `bindComposeMasks`,
  `bindComposeMediaLeaves`, `bindComposePaintPrograms`,
  `bindComposePixelStyles`, `bindComposeSchemes`, `bindComposeSelectors`,
  `bindComposeSheets`, `bindComposeTextEffects`,
  `bindComposeTextureScene`, `bindDocument`
* `draw/Registration.h` — `bindPen`, `bindBrush`, `bindDrawCanvasSeam`,
  `bindDrawStandalonePen`
* `world/Registration.h` — `bindWorld`, `bindWorldDescription`,
  `bindWorldDevice`, `bindWorldEnvironment`, `bindWorldGeometry`,
  `bindWorldPasses`, `bindWorldPlan`, `bindWorldTargets`, `bindWorldView`
* `io/Registration.h` — `bindIO`, `bindIOHubGrowth`, `bindIOPublish`,
  `bindIOSources`
* `data/Registration.h` — `bindData`, `bindDataConnection`,
  `bindDataSchema`, `bindDataTables`
* `measure/Registration.h` — `bindMeasure`
* `video/Registration.h` — `bindVideo`
* `scry/Registration.h` — `bindScry`
* `substance/Registration.h` — `bindSubstance`
* `usd/Registration.h` — `bindUsd`

## World ownership

World declarations use the native element, frame, light, selector and kit
values. Meshes and cameras remain Geometry values; surface parameters and
factories remain Material values. The Python scene owner contains a ticker
before its native scene, because the scene borrows that ticker. The owner
cannot move and checks its creating thread on every operation. It retains
the current frame so advancing time can resample its native motion lanes.
Scene destruction precedes ticker destruction.

The supported executor is the native CPU renderer. Image capture returns
an owned image; drawing into a sketch accepts the existing checked pen.
Camera, lights and statistics are copied on read. No device handles, custom
Python pass bodies or additional callback lifetime are introduced.

## Build and validation

`SigilPython` links the native libraries it binds and pybind11. Public headers
carry Python development includes. Executable consumers link the matching
embedding library. A module target sets `SIGIL_PYTHON_EXTENSION` so Python
symbols resolve from the importing interpreter instead of another libpython.

Every source is named in the one CMake list, so a subject nobody has
written yet still compiles and links: its bind function has an empty
body until its author fills it in. A library whose SDK is a licensed
download may be absent from a build, and then its target does not exist,
its sources are left out, and the definition that says it is there —
`SIGIL_PYTHON_HAS_SCRY`, `SIGIL_PYTHON_HAS_SUBSTANCE`,
`SIGIL_PYTHON_HAS_USD`, `SIGIL_PYTHON_HAS_WEB`, `SIGIL_PYTHON_HAS_VIDEO` —
is not defined. The module those bindings would register is then not
registered either, so a Python process asks `importlib.util.find_spec`
whether it is there; the package's surface table names those modules as
the ones a build may lack, and its generator leaves their declarations
and modules alone instead of deleting them.

`python_test` links this library without any sketch target. It checks common
module registration, neutral Compose kit callbacks, explicit callback cleanup,
native scope unwinding and retained World ticker, frame and image ownership. The sketch adapter separately tests hot reload,
context invalidation and its session-owned resources. Package tests exercise
the combined extension and its Python convenience surface.

## Direct typography, color and data

The direct typography owner constructs a system-backed font context and checks
its creating thread. Paragraphs retain pattern hyphenators passed to layout;
layout values retain a native paragraph snapshot that owns their ordinary shaped
words, and returned metrics are copies. Drawing and measurements check that an
explicit paragraph still matches the placement before reading its styles.
Drawing uses the same checked pen as the other drawing bindings. UTF-16 ranges
remain explicit rather than being silently reinterpreted as Python indices.

Material colors preserve the native color-space calculations and pass through
the shared color conversion used by drawing, composition and paint uniforms.
Palettes provide Python indexing and iteration beside the native clamped `at`.
Optional paint-layer materials copy native values across the Python boundary.

Owned database connections expose writes; cached and byte-backed database
views reject every writing statement, through query as well as through
execute and insert, and the file a cached view reads is opened for reading
only. Registering the data decoders on a hub registers the database decoder
with them, so an owned hub loads a store as a session hub does.
Resource leases retain an owned
hub or check a borrowed host before access. Closing a lease releases residency and
makes subsequent access fail; context-manager exit performs that close even
when the body raises. Lease methods check their creating thread so closing a
lease cannot race a preload while blocking hub work releases the interpreter lock.
