# SigilPython

Reusable Python bindings for the native drawing, composition, typography,
motion, material, geometry, image, data and resource libraries. The static
`SigilPython` target has no sketch runtime, application, window or interpreter
startup dependency. Namespace `sigil::python`.

The bindings construct the native values and call their native algorithms.
They do not provide another element tree, motion engine, renderer or IO loop.
The supported surface is curated; linking a native library does not imply
that every C++ API is exposed to Python.

Callable inputs are named in the bindings so runtime help, keyword calls and
generated editor signatures agree. Positional variadic APIs retain typed
overloads in the package. An element's `children` method accepts individual
elements or one iterable, including a list, tuple or generator. Each call
appends in order and returns the same element; all inputs are converted before
any are appended.

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
  `justification`, `shape`
* `MotionBindings.h` — `motionAnimatable`, `motionInk`, `motionFill`,
  `motionEase`, `motionTransition`
* `IOBindings.h` — `HubHandle`, `retainSessionFeed`
* `DataBindings.h` — `dataDatabase`
* `KitBindings.h` — `bindComposeKit`

These headers let another native adapter use the same conversions and wrapper
types. A checked hub accepts host-provided access and feed-retention functions;
owned hubs have independent lifetimes. IO transport workers execute no Python.
Overlapping host sessions can share a feed lease, and the last lease closes its
transport without holding the interpreter lock. Escaped checked wrappers cannot
keep a closed host session alive.

## Build and validation

`SigilPython` links the native libraries it binds and pybind11. Public headers
carry Python development includes. Executable consumers link the matching
embedding library. A module target sets `SIGIL_PYTHON_EXTENSION` so Python
symbols resolve from the importing interpreter instead of another libpython.

`python_test` links this library without any sketch target. It checks common
module registration, neutral Compose kit callbacks, explicit callback cleanup
and native scope unwinding. The sketch adapter separately tests hot reload,
context invalidation and its session-owned resources. Package tests exercise
the combined extension and its Python convenience surface.
