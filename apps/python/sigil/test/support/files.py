"""Files a test writes into a directory of its own."""


def writeExecutable(path):
    """An executable at @p path that exits at once with success, its
    directories made."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("#!/bin/sh\nexit 0\n")
    path.chmod(0o755)
    return path
