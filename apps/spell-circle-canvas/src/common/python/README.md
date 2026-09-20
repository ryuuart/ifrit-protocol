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

The Python package, convenience functions, stubs and package tests belong to
`apps/python/sigil`. Native callback signature adaptation imports its neutral
`sigil._callbacks` helper. An embedding host supplies the matching package on
its interpreter's module path. A normal extension uses its importing environment.

The sketch adapter assembles the complete `_sigil` module by registering this
library followed by sketch context, session and specimen-kit bindings. An
independent native host can link only `SigilPython` and assemble a different
module. Each interpreter must load one copy of the binding implementation:
registered types and callback ownership registries are shared state.

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

## Conversion and resource seams

* `ValueBindings.h` — `point`, `rect`
* `ComposeBindings.h` — `dimension`, `fill`, `surfacePaint`, `alignment`,
  `justification`, `shape`, `elements`
* `MotionBindings.h` — `motionAnimatable`, `motionInk`, `motionFill`,
  `motionEase`, `motionTransition`
* `IOBindings.h` — `HubHandle`, `retainSessionFeed`
* `DataBindings.h` — `dataDatabase`
* `KitBindings.h` — `bindComposeKit`, `bindDocument`
* `WeaveBindings.h` — `bindWeave`
* `GeometryBindings.h` — `bindGeometry`
* `WorldBindings.h` — `bindWorld`

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
