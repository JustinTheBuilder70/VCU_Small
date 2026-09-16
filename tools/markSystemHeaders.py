#!/usr/bin/env python3

import argparse
import json
import os
import shlex
import tempfile
from pathlib import Path


def resolved(path_text: str, directory: Path) -> Path:
    path = Path(path_text)

    if not path.is_absolute():
        path = directory / path

    return path.resolve()


def should_be_system_include(
    include_path: str,
    command_directory: Path,
    project_root: Path,
) -> bool:
    path = resolved(include_path, command_directory)

    project_source = (project_root / "src").resolve()
    project_build = (project_root / ".pio").resolve()

    # Keep application headers as normal include paths.
    try:
        path.relative_to(project_source)
        return False
    except ValueError:
        pass

    # Treat PlatformIO dependencies and generated framework paths as system.
    try:
        path.relative_to(project_build)
        return True
    except ValueError:
        pass

    # Toolchains and framework packages normally live outside the project.
    return path != project_root


def transform_tokens(
    tokens: list[str],
    command_directory: Path,
    project_root: Path,
) -> list[str]:
    transformed: list[str] = []
    index = 0

    while index < len(tokens):
        token = tokens[index]

        if token == "-I" and index + 1 < len(tokens):
            include_path = tokens[index + 1]

            if should_be_system_include(
                include_path,
                command_directory,
                project_root,
            ):
                transformed.extend(["-isystem", include_path])
            else:
                transformed.extend(["-I", include_path])

            index += 2
            continue

        if token.startswith("-I") and token != "-I":
            include_path = token[2:]

            if should_be_system_include(
                include_path,
                command_directory,
                project_root,
            ):
                transformed.extend(["-isystem", include_path])
            else:
                transformed.append(token)

            index += 1
            continue

        transformed.append(token)
        index += 1

    return transformed


def transform_entry(entry: dict, project_root: Path) -> dict:
    command_directory = Path(
        entry.get("directory", project_root)
    ).resolve()

    updated = dict(entry)

    if "arguments" in entry:
        updated["arguments"] = transform_tokens(
            entry["arguments"],
            command_directory,
            project_root,
        )

    if "command" in entry:
        tokens = shlex.split(entry["command"])
        transformed = transform_tokens(
            tokens,
            command_directory,
            project_root,
        )
        updated["command"] = shlex.join(transformed)

    return updated


def write_atomically(path: Path, data: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        dir=path.parent,
        prefix=f".{path.name}.",
        delete=False,
    ) as temporary:
        json.dump(data, temporary, indent=4)
        temporary.write("\n")
        temporary_path = Path(temporary.name)

    os.replace(temporary_path, path)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Mark external include directories as -isystem."
    )
    parser.add_argument(
        "database",
        nargs="?",
        default=".pio/build/compile_commands.json",
        type=Path,
        help="Path to the generated compilation database.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Output path. Defaults to modifying the input database.",
    )

    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    input_path = args.database.resolve()
    output_path = (
        args.output.resolve()
        if args.output
        else input_path
    )

    with input_path.open(encoding="utf-8") as database_file:
        database = json.load(database_file)

    transformed_database = [
        transform_entry(entry, project_root)
        for entry in database
    ]

    write_atomically(output_path, transformed_database)

    print(f"Wrote {len(database)} compilation commands to {output_path}")


if __name__ == "__main__":
    main()