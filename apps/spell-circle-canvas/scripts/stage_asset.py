#!/usr/bin/env python3
"""
Puts a downloaded archive into the vcpkg asset cache.

Usage:
    python scripts/stage_asset.py <archive> [<archive> ...]

vcpkg looks in the asset cache before it fetches anything, and finds a
file there by the SHA-512 of its contents — the file's own name in the
cache. That is what lets a port name an archive nobody can download
without an account: the port declares the file name and the hash, the
archive is staged here once per machine, and every configure after that
resolves it locally.

The hash this prints is the SHA512 the port's vcpkg_download_distfile
declares.
"""

import argparse
import hashlib
import shutil
import sys
from pathlib import Path

# The cache directory scripts/setup.py writes into the vcpkg preset's
# X_VCPKG_ASSET_SOURCES.
ASSET_DIR = Path.home() / ".local" / "opt" / "vcpkg-assets"


def sha512(path: Path) -> str:
    """Returns the lowercase hex SHA-512 of a file's contents."""
    digest = hashlib.sha512()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def stage(archive: Path) -> int:
    """Copies one archive into the cache under its hash and reports it."""
    if not archive.is_file():
        print(f"ERROR: {archive} is not a file", file=sys.stderr)
        return 1

    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    digest = sha512(archive)
    destination = ASSET_DIR / digest
    if destination.exists():
        print(f"  already staged: {archive.name}")
    else:
        shutil.copyfile(archive, destination)
        print(f"  staged: {archive.name} -> {destination}")
    print(f"    FILENAME {archive.name}")
    print(f"    SHA512 {digest}")
    return 0


def main() -> int:
    """Stages every archive named on the command line."""
    argument_parser = argparse.ArgumentParser(
        description="Put an archive into the vcpkg asset cache under its hash"
    )
    argument_parser.add_argument(
        "archives", nargs="+", type=Path, help="Archives to stage"
    )
    arguments = argument_parser.parse_args()

    print(f"vcpkg asset cache: {ASSET_DIR}")
    return max(stage(archive) for archive in arguments.archives)


if __name__ == "__main__":
    sys.exit(main())
