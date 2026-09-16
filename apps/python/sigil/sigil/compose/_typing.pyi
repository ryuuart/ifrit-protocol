from collections.abc import Iterable
from typing import TypeAlias

from _sigil.compose import Element

Child: TypeAlias = Element | str | None | Iterable[Child]
