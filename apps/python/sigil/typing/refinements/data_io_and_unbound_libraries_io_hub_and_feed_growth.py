"""Hub::onDispatch, Python decoders and transports, image/channel/probe views, Feed::receivedAt.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/io-hub-and-feed-growth package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
