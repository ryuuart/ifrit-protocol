"""The overview-and-reference layer: a discovery site over the literal API.

The Doxygen sites answer "what exactly does this member do". This layer
answers the two questions they cannot: what is there to reach for, and
what do I make to pass to it. `docs/REFERENCE.md` is the canon.

`generate()` is the whole entry point, and the docs verb calls it after
the sites are written. It is not named after the module it lives in:
a name the package re-exports shadows the submodule of that name, so
`sigil.reference.build` would hand back a function.
"""

from sigil.reference.build import Build, Options, generate

__all__ = ["Build", "Options", "generate"]
