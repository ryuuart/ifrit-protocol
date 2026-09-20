"""The overview-and-reference layer: a discovery site over the literal API.

The Doxygen sites answer "what exactly does this member do". This layer
answers the two questions they cannot: what is there to reach for, and
what do I make to pass to it. `docs/REFERENCE.md` is the canon.

`build()` is the whole entry point, and the docs verb calls it after the
sites are written.
"""

from sigil.reference.build import Build, Options, build

__all__ = ["Build", "Options", "build"]
