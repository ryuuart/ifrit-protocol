import collections.abc
import os

import sigil.protocol

class InProcess(sigil.protocol.Envelopes):
    def __init__(
        self,
        state: str | os.PathLike[str],
        *,
        sketches: str | os.PathLike[str] | None = ...,
        assets: str | os.PathLike[str] | None = ...,
        patience: float = ...,
    ) -> None: ...
    def exchange(self, envelope: str) -> str: ...
    def listen(
        self,
        method: str,
        listener: collections.abc.Callable[[sigil.protocol.Json], None],
    ) -> None: ...
    def frame(self) -> None: ...
    def readout(self) -> str: ...
    @property
    def state_directory(self) -> os.PathLike[str] | str: ...
