"""SigilMeasure: the stopwatch, a run's summary, quantile and histogram, a
stream's window, smoothed reading and rate, and the check table.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/measure package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import DURATION, Table

RUN = "collections.abc.Iterable[typing.Any]"
KEY = "collections.abc.Callable[[typing.Any], typing.SupportsFloat] | None"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.parameters("_sigil.measure.timed", block="collections.abc.Callable[[], object]")
    table.parameters("_sigil.measure.summary", values=RUN, key=KEY)
    table.parameters("_sigil.measure.quantile", values=RUN, key=KEY)
    table.parameters("_sigil.measure.Histogram.over", values=RUN, key=KEY)
    table.parameters("_sigil.measure.Window.__init__", span=DURATION)
    table.parameters("_sigil.measure.Window.add", at=f"{DURATION} | None")
    table.parameters("_sigil.measure.Window.advance", now=DURATION)
    table.parameters("_sigil.measure.Smoothed.__init__", timeConstant=DURATION)
    table.parameters("_sigil.measure.Smoothed.add", elapsed=f"{DURATION} | None")
    table.parameters("_sigil.measure.Rate.__init__", span=DURATION)
    table.parameters("_sigil.measure.Rate.mark", at=DURATION)
    table.parameters("_sigil.measure.Rate.advance", now=DURATION)
