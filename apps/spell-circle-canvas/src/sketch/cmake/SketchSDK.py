#!/usr/bin/env python3
"""Generate native target boundary identities and stamp compiled plugin metadata."""

import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

BEGIN = b"SIGIL_SKETCH_METADATA_BEGIN\n"
END = b"\0SIGIL_SKETCH_METADATA_END"


def write_changed(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def digest_file(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def cmake_string(text: str) -> str:
    return (
        '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("$", "\\$") + '"'
    )


def embedded_metadata(artifact: Path) -> tuple[bytes, str]:
    data = artifact.read_bytes()
    start = data.find(BEGIN)
    end = data.find(END, start + len(BEGIN))
    if start < 0 or end < 0 or end - start > 65536:
        sys.exit(f"{artifact} has no compiled native plugin metadata")
    payload = data[start + len(BEGIN) : end].decode("ascii")
    if not re.fullmatch(r"[0-9a-f]{64}(?:\n[A-Za-z0-9_:.-]+ [0-9a-f]{64})*", payload):
        sys.exit(f"{artifact} has invalid compiled native plugin metadata")
    return data, payload


def stamp(artifact: Path) -> None:
    data, payload = embedded_metadata(artifact)
    digest = hashlib.md5(data, usedforsecurity=False).hexdigest()
    metadata = Path(str(artifact) + ".sigil-build")
    pending = metadata.with_name(metadata.name + f".{os.getpid()}.tmp")
    pending.write_text(f"sigil-sketch-plugin-2\n{digest}\n{payload}\n")
    pending.replace(metadata)


def command_mode() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validate", type=Path)
    parser.add_argument("--stamp", type=Path)
    parser.add_argument("--ensure-sidecar", type=Path)
    parser.add_argument("--plugin-header", type=Path)
    parser.add_argument("--boundaries", type=Path)
    parser.add_argument("--origin-inputs", type=Path)
    parser.add_argument("--libraries", nargs="*", default=[])
    parser.add_argument("--configuration")
    parser.add_argument("--expected-config")
    args = parser.parse_args()
    if args.expected_config and args.configuration != args.expected_config:
        sys.exit(
            f"The plugin must be built in {args.expected_config}, matching this SigilSketchSDK"
        )
    if args.validate:
        checks = args.validate.read_text().splitlines()
        if args.origin_inputs:
            inputs = json.loads(args.origin_inputs.read_text())
            for name in args.libraries:
                if name not in inputs:
                    sys.exit(
                        f"Originating library {name} is absent from the host metadata"
                    )
                checks.extend(inputs[name])
        for line in set(checks):
            digest, name = line.split(" ", 1)
            path = Path(name)
            if not path.is_file() or digest_file(path) != digest:
                sys.exit(
                    f"SigilSketchSDK is stale ({name}); rebuild the native libraries and metadata before compiling plugins"
                )
    if args.plugin_header:
        if not args.boundaries:
            parser.error("--plugin-header requires --boundaries")
        boundaries = json.loads(args.boundaries.read_text())
        libraries = sorted({"SigilSketch", *args.libraries})
        try:
            payload = boundaries["SigilSketch"] + "".join(
                f"\n{name} {boundaries[name]}" for name in libraries
            )
        except KeyError as error:
            sys.exit(
                f"Originating library {error.args[0]} is absent from the host metadata"
            )
        write_changed(
            args.plugin_header,
            "#pragma once\n#define SIGIL_SKETCH_PLUGIN_METADATA "
            + json.dumps(payload)
            + "\n",
        )
    if (
        args.ensure_sidecar
        and args.ensure_sidecar.is_file()
        and not Path(str(args.ensure_sidecar) + ".sigil-build").is_file()
    ):
        stamp(args.ensure_sidecar)
    if args.stamp:
        stamp(args.stamp)


def origin_description(path: Path) -> dict[str, dict[str, list[str]]]:
    origins = {}
    aliases = {}
    current = None
    for line in path.read_text().splitlines():
        key, value = line.split("\t", 1)
        if key == "ALIAS":
            alias, canonical = value.split("\t", 1)
            aliases[alias] = canonical
        elif key == "TARGET":
            current = origins.setdefault(value, {})
        elif value and current is not None:
            current.setdefault(key, []).append(value)
    for origin in origins.values():
        origin["DEPENDENCY"] = [
            aliases.get(name, name) for name in origin.get("DEPENDENCY", [])
        ]
    return origins


def compiler_context(flags: list[str]) -> list[str]:
    # Include search paths locate headers; the headers' contents guard their
    # ABI. Build-directory paths must not make the same libraries incompatible.
    result = []
    skip = False
    for flag in flags:
        if skip:
            skip = False
        elif flag in {"-I", "-isystem", "-iquote", "-idirafter"}:
            skip = True
        elif not flag.startswith(("-I", "-isystem")):
            result.append(flag)
    return result


def generate() -> None:
    from SketchFlags import find_command, response_lines

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compdb", type=Path, required=True)
    parser.add_argument("--anchor", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--flags", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--origins", type=Path, required=True)
    parser.add_argument("--depfile", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--compiler-id", required=True)
    parser.add_argument("--compiler-version", required=True)
    parser.add_argument("--architecture", default="")
    parser.add_argument("--system-processor", required=True)
    args = parser.parse_args()
    command = find_command(args.compdb, args.anchor, args.config)
    lines = response_lines(command, args.anchor, [])
    flags = [line.replace('\\"', '"') for line in lines]
    compiler_target = subprocess.check_output(
        [args.compiler, "-dumpmachine"], text=True
    ).strip()
    compiler_digest = digest_file(Path(args.compiler))
    context = json.dumps(
        [
            args.config,
            args.compiler_id,
            args.compiler_version,
            compiler_target,
            compiler_digest,
            args.architecture,
            args.system_processor,
            compiler_context(flags),
        ],
        sort_keys=True,
    )
    origins = origin_description(args.origins)
    args.out.mkdir(parents=True, exist_ok=True)
    closures = {}
    for name in origins:
        closure = set()
        queue = [name]
        while queue:
            dependency = queue.pop()
            if dependency not in closure:
                closure.add(dependency)
                queue.extend(
                    link
                    for link in origins[dependency].get("DEPENDENCY", [])
                    if link in origins
                )
        closures[name] = closure

    def library_origin(name: str) -> bool:
        """Whether the named origin is one of the tree's own libraries
        rather than a target another package declares."""
        return (
            name.startswith("Sigil")
            and "::" not in name
            and not origins[name].get("IMPORTED")
        )

    def framework_options(name: str) -> list[str]:
        roots = {
            str(bundle.parent)
            for dependency in closures[name]
            for framework in origins[dependency].get("FRAMEWORK", [])
            for bundle in Path(framework).parents
            if bundle.suffix == ".framework"
        }
        return ["-F" + root for root in sorted(roots)]

    def dependencies(item: tuple[str, dict[str, list[str]]]) -> tuple[str, set[Path]]:
        name, origin = item
        headers = origin.get("HEADER", [])
        paths = {Path(path) for path in origin.get("BINARY", [])}
        paths.update(
            Path(path)
            for path in origin.get("DEPENDENCY", [])
            if Path(path).is_absolute() and Path(path).is_file()
        )
        if headers:
            depfile = args.out / f"{name.replace('::', '_')}.d"
            options = ["-I" + path for path in origin.get("INCLUDE_DIRECTORIES", [])]
            options += framework_options(name)
            options += [
                "-D" + definition
                for definition in origin.get("COMPILE_DEFINITIONS", [])
            ]
            for option in origin.get("COMPILE_OPTIONS", []):
                options.extend(
                    shlex.split(option[6:]) if option.startswith("SHELL:") else [option]
                )
            standards = [
                int(feature[8:])
                for feature in origin.get("COMPILE_FEATURES", [])
                if feature.startswith("cxx_std_")
            ]
            if standards:
                options.append(f"-std=c++{max(20, *standards)}")
            source = "".join(f"#include {json.dumps(header)}\n" for header in headers)
            result = subprocess.run(
                [
                    args.compiler,
                    *flags,
                    *options,
                    "-M",
                    "-MF",
                    str(depfile),
                    "-MT",
                    "metadata",
                    "-x",
                    "c++",
                    "-",
                ],
                input=source,
                text=True,
                capture_output=True,
            )
            if result.returncode:
                sys.exit(
                    f"Cannot read public header dependencies of {name}:\n{result.stderr}"
                )
            dependencies_text = depfile.read_text().replace("\\\n", "")
            paths.update(
                Path(path)
                for path in shlex.split(dependencies_text.split(":", 1)[1])
                if path != "<stdin>"
            )
        return name, paths

    with ThreadPoolExecutor(max_workers=4) as workers:
        dependencies_by_origin = dict(workers.map(dependencies, origins.items()))
    inputs = {path for paths in dependencies_by_origin.values() for path in paths}
    digests = {path: digest_file(path) for path in sorted(inputs)}
    identities = {}
    origin_inputs = {}
    for name in sorted(origins):
        payload = [context, name]
        consumed = set()
        for dependency in sorted(closures[name]):
            origin = origins[dependency]
            payload.append(
                [
                    dependency,
                    sorted(origin.get("COMPILE_DEFINITIONS", [])),
                    sorted(origin.get("COMPILE_OPTIONS", [])),
                    sorted(origin.get("COMPILE_FEATURES", [])),
                    sorted(
                        (path.name, digests[path])
                        for path in dependencies_by_origin[dependency]
                    ),
                ]
            )
            consumed.update(dependencies_by_origin[dependency])
        identities[name] = hashlib.sha256(
            json.dumps(payload, sort_keys=True).encode()
        ).hexdigest()
        origin_inputs[name] = [f"{digests[path]} {path}" for path in sorted(consumed)]
    write_changed(args.flags, "".join(f"{line}\n" for line in lines))
    header = (
        '#pragma once\n#include <string_view>\n#define SIGIL_SKETCH_BUILD_ID "'
        + identities["SigilSketch"]
        + '"\n'
    )
    header += "namespace sigil::sketch {\ninline constexpr std::string_view nativeBoundaryIdentity(std::string_view name) {\n"
    for name, digest in sorted(identities.items()):
        header += f'  if (name == "{name}") return "{digest}";\n'
    header += "  return {};\n}\n}\n"
    write_changed(args.out / "SigilSketchBuildIdentity.h", header)
    write_changed(
        args.out / "boundaries.json",
        json.dumps(identities, indent=2, sort_keys=True) + "\n",
    )
    write_changed(
        args.out / "origin-inputs.json",
        json.dumps(origin_inputs, indent=2, sort_keys=True) + "\n",
    )
    settings = {
        "COMPILER_ID": args.compiler_id,
        "COMPILER_VERSION": args.compiler_version,
        "COMPILER_TARGET": compiler_target,
        "CONFIGURATION": args.config,
        "COMPILER_DIGEST": compiler_digest,
        "ARCHITECTURE": args.architecture,
        "SYSTEM_PROCESSOR": args.system_processor,
        "FLAGS_FILE": str(args.flags.resolve()),
    }
    config = "# Native sketch plugin metadata for one Sigil configuration.\n"
    for name, value in settings.items():
        config += f"set(SigilSketchSDK_{name} {cmake_string(value)})\n"
    config += 'set(SigilSketchSDK_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")\n'
    # The package declares the tree's own libraries and nothing else: a
    # consumer that later finds Qt, Boost or any other package itself must
    # not meet a target of that name already standing. A plugin resolves
    # every implementation from the host, so what a library needs from its
    # dependencies is their compile requirements, carried flattened on the
    # library itself; no requirement here needs the real package found.
    for name in sorted(origins):
        if not library_origin(name):
            continue
        reached = [name] + sorted(closures[name] - {name})
        config += f"if(NOT TARGET {name})\n  add_library({name} INTERFACE IMPORTED)\n"
        for field in (
            "INCLUDE_DIRECTORIES",
            "COMPILE_DEFINITIONS",
            "COMPILE_OPTIONS",
            "COMPILE_FEATURES",
        ):
            values = [
                value
                for dependency in reached
                for value in origins[dependency].get(field, [])
            ]
            if field == "COMPILE_OPTIONS":
                values += framework_options(name)
            values = list(dict.fromkeys(values))
            if values:
                config += f"  set_property(TARGET {name} PROPERTY INTERFACE_{field} {cmake_string(';'.join(values))})\n"
        config += "endif()\n"
    config += 'include("${CMAKE_CURRENT_LIST_DIR}/SketchPlugin.cmake")\n'
    write_changed(args.out / "SigilSketchSDKConfig.cmake", config)
    for name in ("SketchPlugin.cmake", "SketchSDK.py"):
        write_changed(args.out / name, Path(__file__).with_name(name).read_text())
        inputs.add(Path(__file__).with_name(name))
    inputs.add(Path(__file__).with_name("SketchFlags.py"))
    inputs.add(Path(args.compiler))
    package_inputs = [
        f"{digest_file(Path(__file__).with_name(name))} {Path(__file__).with_name(name)}"
        for name in ("SketchSDK.py", "SketchPlugin.cmake", "SketchFlags.py")
    ]
    package_inputs.append(f"{compiler_digest} {args.compiler}")
    write_changed(
        args.out / "inputs.sha256",
        "\n".join(origin_inputs["SigilSketch"] + package_inputs) + "\n",
    )
    escaped = lambda path: (
        str(path)
        .replace("\\", "\\\\")
        .replace(" ", "\\ ")
        .replace("#", "\\#")
        .replace("$", "$$")
    )
    write_changed(
        args.depfile,
        escaped(args.out / "SigilSketchBuildIdentity.h")
        + ": "
        + " ".join(escaped(path) for path in sorted(inputs))
        + "\n",
    )


def main() -> None:
    if any(
        option in sys.argv
        for option in ("--validate", "--stamp", "--ensure-sidecar", "--plugin-header")
    ):
        command_mode()
    else:
        generate()


if __name__ == "__main__":
    main()
