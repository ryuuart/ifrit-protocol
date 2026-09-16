"""Generate lazy native registry entries from Python sketch source metadata."""

import argparse
import ast
import json
from pathlib import Path


def declares_sketch(source):
    for statement in source.body:
        if not isinstance(statement, ast.ClassDef):
            continue
        for decorator in statement.decorator_list:
            target = decorator.func if isinstance(decorator, ast.Call) else decorator
            if (
                isinstance(target, ast.Name)
                and target.id == "sketch"
                or isinstance(target, ast.Attribute)
                and target.attr == "sketch"
            ):
                return True
    return False


def registration(path, index, source):
    description = ast.get_docstring(source) or ""
    blurb = " ".join(description.split("\n\n", 1)[0].split())
    requirements = ()
    for statement in source.body:
        if isinstance(statement, ast.Assign):
            targets = statement.targets
        elif isinstance(statement, ast.AnnAssign):
            targets = [statement.target]
        else:
            continue
        if any(
            isinstance(target, ast.Name) and target.id == "REQUIRES"
            for target in targets
        ):
            requirements = ast.literal_eval(statement.value)
            if not isinstance(requirements, (tuple, list)) or any(
                not isinstance(module, str)
                or not module
                or not all(part.isidentifier() for part in module.split("."))
                for module in requirements
            ):
                raise ValueError(
                    f"{path}: REQUIRES must be a literal list of module names"
                )

    def literal(value):
        return json.dumps(value, ensure_ascii=False)

    modules = ", ".join(literal(module) for module in dict.fromkeys(requirements))
    return f"""
sigil::sketch::Kind kind{index}() {{
  return sigil::sketch::python::source({literal(str(path))});
}}
bool available{index}(std::string* why) {{
  return sigil::sketch::python::available({literal(str(path))}, {{{modules}}}, why);
}}
[[maybe_unused]] const bool registered{index} = sigil::sketch::add(
    {literal(path.stem)}, {literal(path.stem)}, "Python", {literal(blurb)},
    &kind{index}, &available{index});
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("sources", nargs="*", type=Path)
    args = parser.parse_args()
    sources = []
    for path in sorted(path.resolve() for path in args.sources):
        source = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
        if declares_sketch(source):
            sources.append((path, source))
    names = [path.stem for path, _ in sources]
    if len(names) != len(set(names)):
        parser.error("Python sketch stems must be unique")
    content = (
        "// Generated registry entries; sketch code is imported when a session opens.\n"
        "#include <sigilsketch/core/Registry.h>\n"
        "#include <sigilsketch/python/Python.h>\n\n"
        "namespace {\n"
        + "".join(
            registration(path, index, source)
            for index, (path, source) in enumerate(sources)
        )
        + "}  // namespace\n"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    main()
