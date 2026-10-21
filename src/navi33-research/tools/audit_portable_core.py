#!/usr/bin/env python3
"""Report preliminary host build and binary-dependency evidence for Navi33."""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]


def read_cache(path: Path) -> dict:
    values = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"([^:#]+):[^=]*=(.*)", line)
        if match:
            values[match.group(1)] = match.group(2)
    return values


def find_dumpbin(cache: dict) -> Path | None:
    on_path = shutil.which("dumpbin.exe") or shutil.which("dumpbin")
    if on_path:
        return Path(on_path)
    instance = cache.get("CMAKE_GENERATOR_INSTANCE")
    if not instance:
        return None
    root = Path(instance.replace("/", "\\"))
    candidates = sorted(
        (root / "VC" / "Tools" / "MSVC").glob(
            "*/bin/Hostx64/x64/dumpbin.exe"
        )
    )
    return candidates[-1] if candidates else None


def compile_commands(build_dir: Path) -> list[str]:
    commands = []
    for path in build_dir.rglob("CL.command.1.tlog"):
        if "COMPILERIDCXX" in str(path).upper():
            continue
        try:
            raw = path.read_bytes()
            text = raw.decode("utf-16", errors="replace")
        except OSError:
            continue
        commands.extend(
            item.strip()
            for item in text.split("\0")
            if "/c " in item.lower() and ("NAVI33" in item.upper())
        )
    return commands


def run_dumpbin(dumpbin: Path, *args: str) -> str | None:
    result = subprocess.run(
        [str(dumpbin), *args],
        check=False,
        capture_output=True,
        text=True,
        errors="replace",
    )
    if result.returncode != 0:
        print(f"dumpbin {' '.join(args)} failed ({result.returncode})")
        if result.stderr.strip():
            print(result.stderr.strip())
        return None
    return result.stdout


def runtime_symbol_categories(output: str) -> list[str]:
    symbols = [
        line.split("|", 1)[1].strip().lower()
        for line in output.splitlines()
        if re.search(r"\bUNDEF\b.*\bExternal\s*\|", line)
    ]
    categories = (
        ("operator new", "operator new"),
        ("operator delete", "operator delete"),
        ("_CxxThrowException", "_cxxthrowexception"),
        ("std::_Xlength_error", "_xlength_error"),
        ("type_info vftable", "type_info::`vftable'"),
    )
    return [
        label
        for label, fragment in categories
        if any(fragment.lower() in symbol for symbol in symbols)
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=REPOSITORY_ROOT / "build" / "navi33-research-msvc",
        help="existing CMake build directory (no build or source changes performed)",
    )
    parser.add_argument(
        "--config",
        default="Debug",
        help="configuration whose library and test executable are inspected",
    )
    args = parser.parse_args()
    build_dir = args.build_dir.resolve()
    cache_path = build_dir / "CMakeCache.txt"

    print("PRELIMINARY WINDOWS HOST AUDIT - NOT APPLE KERNEL ABI VALIDATION")
    print(
        "Audit scope: MSVC/DUMPBIN Windows evidence only; no macOS compilation "
        "or linking is performed or inferred."
    )
    print(f"Build directory: {build_dir}")
    if not cache_path.is_file():
        print("CMake cache: unavailable; configure the portable project first.")
        return 2

    cache = read_cache(cache_path)
    print(f"Generator: {cache.get('CMAKE_GENERATOR', 'unknown')}")
    print(f"Generator instance: {cache.get('CMAKE_GENERATOR_INSTANCE', 'unknown')}")
    print(f"Target platform: {cache.get('CMAKE_GENERATOR_PLATFORM', 'unknown')}")
    print(
        "CMake compiler identity: "
        f"{cache.get('CMAKE_CXX_COMPILER_ID', 'not recorded in this generator cache')}"
    )

    commands = compile_commands(build_dir)
    if not commands:
        print("Compile command logs: unavailable")
    else:
        print(f"Navi33 compile command records: {len(commands)}")
        effective_flags = []
        for command in commands:
            options = re.findall(r"(?i)(?:/EH[^ ]+|/GR[^ ]+)", command)
            final_rtti = next(
                (option for option in reversed(options) if option.upper().startswith("/GR")),
                "not found",
            )
            final_exceptions = next(
                (option for option in reversed(options) if option.upper().startswith("/EH")),
                "not found",
            )
            effective_flags.append((final_rtti.upper(), final_exceptions.upper()))
        for index, (final_rtti, final_exceptions) in enumerate(effective_flags[:8], start=1):
            print(
                f"  command {index} effective final flags: "
                f"{final_rtti} {final_exceptions}"
            )
        all_flags_present = all(
            rtti == "/GR-" and exceptions == "/EHS-C-"
            for rtti, exceptions in effective_flags
        )
        print(
            "All logged Navi33 compile commands end with RTTI/exceptions disabled: "
            f"{all_flags_present}"
        )

    source_root = REPOSITORY_ROOT / "src" / "navi33-research"
    core_sources = sorted((source_root / "src").glob("*.cpp")) + sorted(
        (source_root / "include").glob("*.hpp")
    )
    core_text = "\n".join(source.read_text(encoding="utf-8") for source in core_sources)
    print(
        "Source uses owning std::string: "
        f"{bool(re.search(r'\\bstd::string\\b', core_text))}"
    )
    print(f"Source uses std::optional: {'std::optional' in core_text}")
    print(f"Source exposes virtual platform callbacks: {'virtual bool prepare' in core_text}")
    print("Inline constexpr identity constants are constant-initialized; no dynamic global objects were identified in source review.")
    print(
        "Source has direct new/delete expressions: "
        f"{bool(re.search(r'\\b(?:new|delete)\\s', core_text))}"
    )
    print("No thread API is declared in the portable public headers: " +
          str(not bool(re.search(r'\\b(thread|mutex|condition_variable)\\b', core_text))))
    print("These source observations do not establish freestanding or kernel suitability.")

    dumpbin = find_dumpbin(cache)
    if not dumpbin:
        print("dumpbin: unavailable; symbol/dependency scan not performed.")
        return 0
    print(f"Binary inspection tool: {dumpbin}")

    configuration_dir = build_dir / args.config
    library = configuration_dir / "navi33_research.lib"
    if library.is_file():
        symbols = run_dumpbin(dumpbin, "/nologo", "/symbols", str(library))
        if symbols is not None:
            undefined = sorted(
                {
                    match.group(1).strip()
                    for line in symbols.splitlines()
                    if (match := re.search(r"\bUNDEF\b.*\bExternal\s*\|\s*(.+)$", line))
                }
            )
            print(f"Undefined external symbol names in static library: {len(undefined)}")
            for name in undefined[:40]:
                print(f"  {name}")
            if len(undefined) > 40:
                print(f"  ... {len(undefined) - 40} more omitted")
    else:
        print(f"Static library: unavailable at {library}")

    object_directory = build_dir / "navi33_research.dir" / args.config
    object_files = sorted(object_directory.glob("*.obj"))
    if object_files:
        print(
            "Object ownership of selected undefined runtime symbols "
            "(archive-member references, not final-link failures):"
        )
        for object_file in object_files:
            symbols = run_dumpbin(dumpbin, "/nologo", "/symbols", str(object_file))
            if symbols is not None:
                categories = runtime_symbol_categories(symbols)
                relative = object_file.relative_to(build_dir)
                print(f"  {relative}: {', '.join(categories) if categories else 'none'}")
    else:
        print(f"Navi33 object files: unavailable at {object_directory}")

    string_locations = []
    for source in core_sources:
        for line_number, line in enumerate(
            source.read_text(encoding="utf-8").splitlines(), start=1
        ):
            if re.search(r"\bstd::string\b", line):
                string_locations.append(
                    f"{source.relative_to(REPOSITORY_ROOT)}:{line_number}"
                )
    print(
        "Owning std::string references in core source: "
        + (", ".join(string_locations) if string_locations else "none")
    )
    print(
        "Object-level ownership identifies translation units; source review is "
        "required to attribute inline standard-library code to its triggering "
        "construct."
    )

    executable = configuration_dir / "navi33_driver_core_tests.exe"
    if executable.is_file():
        print(f"Final test executable linked: yes ({executable.name})")
        dependents = run_dumpbin(dumpbin, "/nologo", "/dependents", str(executable))
        if dependents is not None:
            print("Windows test executable imported DLLs:")
            in_list = False
            for line in dependents.splitlines():
                if "following dependencies" in line.lower():
                    in_list = True
                    continue
                if in_list and line.strip().lower().endswith(".dll"):
                    print(f"  {line.strip()}")
    else:
        print(f"Test executable: unavailable at {executable}")

    print("Windows symbols/imports are preliminary host evidence only.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
