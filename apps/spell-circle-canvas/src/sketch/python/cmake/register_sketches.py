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
    return defines_hooks(source)


def defines_hooks(source):
    """Whether the module defines setup, draw or describe as functions at
    its top level: a file of functions is a sketch as a decorated class is,
    and the loader wraps them into one."""
    return any(
        isinstance(statement, ast.FunctionDef)
        and statement.name in ("setup", "draw", "describe")
        for statement in source.body
    )


def declarations(source):
    """Each module-level assignment to a plain name, as (name, value node).
    Only the nodes are read; nothing in the module runs."""
    for statement in source.body:
        if isinstance(statement, ast.Assign):
            targets = statement.targets
        elif isinstance(statement, ast.AnnAssign) and statement.value is not None:
            targets = [statement.target]
        else:
            continue
        for target in targets:
            if isinstance(target, ast.Name):
                yield target.id, statement.value


def declared_text(path, source, name, default):
    """A literal, non-empty string the module assigns to name, or default
    when it assigns none."""
    value = default
    for declared, node in declarations(source):
        if declared != name:
            continue
        try:
            value = ast.literal_eval(node)
        except ValueError:
            raise ValueError(f"{path}: {name} must be a literal string") from None
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"{path}: {name} must be a literal, non-empty string")
        value = value.strip()
    return value


def registration(path, index, source):
    description = ast.get_docstring(source) or ""
    blurb = " ".join(description.split("\n\n", 1)[0].split())
    # The registry key is the file's stem and stays so: what a sketch is
    # called and where it is filed are the author's to state, and a module
    # that states neither reads as its stem in the Python collection.
    name = declared_text(path, source, "NAME", path.stem)
    category = declared_text(path, source, "CATEGORY", "Python")
    requirements = ()
    for declared, node in declarations(source):
        if declared == "REQUIRES":
            requirements = ast.literal_eval(node)
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
    {literal(path.stem)}, {literal(name)}, {literal(category)}, {literal(blurb)},
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
