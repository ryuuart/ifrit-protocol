"""Inspect callback signatures without loading a sketch or installing import hooks."""

import inspect


def arity(function, maximum):
    """Resolve the supported positional prefix before invoking a callback."""
    signature = inspect.signature(function, follow_wrapped=False)
    arguments = [None] * maximum
    for count in range(maximum, -1, -1):
        try:
            signature.bind(*arguments[:count])
            return count
        except TypeError:
            pass
    raise TypeError(f"{function.__qualname__} cannot accept the callback arguments")
