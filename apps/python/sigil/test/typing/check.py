"""Check authoring examples and expected diagnostics against a Python installation."""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python", default=sys.executable, type=Path)
    parser.add_argument("--checker", required=True, type=Path)
    parser.add_argument("--extra-path", action="append", default=[], type=Path)
    parser.add_argument("--example", action="append", default=[], type=Path)
    parser.add_argument("--invalid", action="append", default=[], type=Path)
    options = parser.parse_args()
    fixtures = Path(__file__).parent
    with TemporaryDirectory(prefix="sigil-static-authoring-") as directory:
        root = Path(directory)
        valid = [
            fixtures / "authoring.py",
            fixtures / "colors.py",
            fixtures / "native_surface.py",
            fixtures / "typography.py",
            fixtures / "world.py",
            *options.example,
        ]
        invalid = [
            fixtures / "invalid_authoring.py",
            fixtures / "invalid_native.py",
            *options.invalid,
        ]
        sources = [*valid, *invalid]
        names = set()
        expected = {}
        for source in sources:
            if source.name in names:
                parser.error(f"duplicate fixture filename: {source.name}")
            names.add(source.name)
            shutil.copyfile(source, root / source.name)
            if source in invalid:
                for number, line in enumerate(source.read_text().splitlines(), 1):
                    match = re.search(r"# error: ([\w,]+)", line)
                    if match:
                        expected[(source.name, number)] = set(match[1].split(","))
        config = root / "pyrightconfig.json"
        config.write_text(
            json.dumps(
                {
                    "include": sorted(names),
                    "typeCheckingMode": "strict",
                    "pythonVersion": "3.12",
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
        seen = set()
        unexpected = []
        for diagnostic in report["generalDiagnostics"]:
            if diagnostic["severity"] == "information":
                continue
            source = Path(diagnostic["file"]).name
            line = diagnostic["range"]["start"]["line"] + 1
            key = (source, line)
            if diagnostic.get("rule") in expected.get(key, set()):
                seen.add((key, diagnostic["rule"]))
            else:
                unexpected.append(
                    f"{source}:{line}: {diagnostic.get('rule', 'error')}: "
                    + diagnostic["message"]
                )
        for (source, line), rules in sorted(expected.items()):
            for rule in sorted(rules):
                if ((source, line), rule) not in seen:
                    unexpected.append(
                        f"{source}:{line}: expected {rule} was not reported"
                    )
        if unexpected:
            sys.stderr.write("\n".join(unexpected) + "\n")
            return 1
        print(
            f"Static authoring passed: {len(valid)} positive files; "
            f"{len(expected)} expected invalid calls rejected."
        )
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
