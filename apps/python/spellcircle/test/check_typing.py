"""Check public authoring contracts against a source tree or installed package.

Pass --extra-path pointing to the SpellCircle project for source checks, or
--python pointing to an installed environment to check its packaged types.
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checker", required=True, type=Path)
    parser.add_argument("--python", default=sys.executable, type=Path)
    parser.add_argument("--extra-path", action="append", default=[], type=Path)
    options = parser.parse_args()
    fixtures = Path(__file__).parent / "typing"
    expected: dict[tuple[str, int], str] = {}
    with TemporaryDirectory(prefix="spellcircle-typing-") as directory:
        root = Path(directory)
        for name in ("authoring.py", "invalid_authoring.py"):
            source = fixtures / name
            shutil.copyfile(source, root / name)
            for number, line in enumerate(source.read_text().splitlines(), 1):
                if match := re.search(r"# error: (\w+)", line):
                    expected[(name, number)] = match[1]
        config = root / "pyrightconfig.json"
        config.write_text(
            json.dumps(
                {
                    "include": ["authoring.py", "invalid_authoring.py"],
                    "typeCheckingMode": "strict",
                    "pythonVersion": "3.11",
                    "extraPaths": [str(path.resolve()) for path in options.extra_path],
                    "enableTypeIgnoreComments": False,
                }
            )
        )
        result = subprocess.run(
            [
                str(options.checker.absolute()),
                "--outputjson",
                "--pythonpath",
                str(options.python.absolute()),
                "--project",
                str(config),
            ],
            cwd=root,
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
        )
        if result.returncode not in (0, 1):
            sys.stderr.write(result.stderr or result.stdout)
            return 1
        report = json.loads(result.stdout)
        seen: set[tuple[str, int]] = set()
        failures: list[str] = []
        for diagnostic in report["generalDiagnostics"]:
            if diagnostic["severity"] == "information":
                continue
            name = Path(diagnostic["file"]).name
            number = diagnostic["range"]["start"]["line"] + 1
            key = (name, number)
            if expected.get(key) == diagnostic.get("rule"):
                seen.add(key)
            else:
                failures.append(f"{name}:{number}: {diagnostic['message']}")
        for name, number in sorted(expected.keys() - seen):
            failures.append(
                f"{name}:{number}: expected {expected[(name, number)]} was not reported"
            )
        if failures:
            sys.stderr.write("\n".join(failures) + "\n")
            return 1
    print(
        f"Public typing passed: authoring checked; {len(expected)} invalid uses rejected."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
