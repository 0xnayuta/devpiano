#!/usr/bin/env python3
"""
Dependency-aware clang-tidy cache wrapper for devpiano.

The cache key includes:
1. Source file contents.
2. Compilation command and compiler/tool identity.
3. .clang-tidy configuration.
4. The exact dependencies reported by Ninja for the translation unit.

When Ninja dependencies are unavailable the wrapper falls back to hashing all
project headers, preserving the previous conservative behavior. Cache hits
replay the recorded stdout/stderr and return code.
"""

import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

CACHE_VERSION = b"v4"
LAUNCHER_NAMES = {"ccache", "sccache", "distcc", "icecc"}


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def get_cache_dir(root_dir: Path) -> Path:
    override = os.environ.get("TIDY_CACHE_DIR")
    if override:
        path = Path(override)
        if not path.is_absolute():
            path = root_dir / path
    else:
        path = root_dir / ".cache" / "tidy-cache"
    path.mkdir(parents=True, exist_ok=True)
    return path


def compute_config_hash(root_dir: Path) -> str:
    config_file = root_dir / ".clang-tidy"
    if config_file.exists():
        return hashlib.sha256(config_file.read_bytes()).hexdigest()
    return "no-config"


def parse_compile_commands(build_dir: Path) -> dict:
    cc_file = build_dir / "compile_commands.json"
    if not cc_file.exists():
        return {}
    try:
        with open(cc_file, "r", encoding="utf-8") as f:
            entries = json.load(f)
    except Exception:
        return {}

    result = {}
    for entry in entries:
        if not isinstance(entry, dict) or "file" not in entry:
            continue
        file_path = Path(entry["file"])
        if not file_path.is_absolute():
            directory = Path(entry.get("directory", "."))
            if not directory.is_absolute():
                directory = (cc_file.parent / directory).resolve()
            file_path = directory / file_path
        try:
            key = os.path.normpath(str(file_path.resolve()))
        except OSError:
            continue
        # Keep the first entry so duplicate app/test targets are deterministic.
        result.setdefault(key, entry)
    return result


def command_tokens(compile_entry: dict) -> list[str]:
    arguments = compile_entry.get("arguments")
    if isinstance(arguments, list) and arguments:
        return [str(value) for value in arguments]
    command = compile_entry.get("command")
    if isinstance(command, str):
        try:
            return shlex.split(command)
        except ValueError:
            return []
    return []


def extract_object_path(compile_entry: dict) -> Path | None:
    output = compile_entry.get("output")
    if isinstance(output, str) and output:
        return Path(output)
    tokens = command_tokens(compile_entry)
    for index, token in enumerate(tokens):
        if token == "-o" and index + 1 < len(tokens):
            return Path(tokens[index + 1])
        if token.startswith("-o") and len(token) > 2 and not token.startswith("-objc"):
            return Path(token[2:])
        if token.startswith("/Fo") and len(token) > 3:
            return Path(token[3:])
    return None


def compiler_command(compile_entry: dict) -> str:
    for token in command_tokens(compile_entry):
        name = Path(token).name
        if name in LAUNCHER_NAMES:
            continue
        if token.startswith("-"):
            continue
        return token
    return ""


def file_identity(command: str) -> str:
    if not command:
        return "unknown"
    resolved = shutil.which(command) or command
    try:
        path = Path(resolved).resolve()
        stat = path.stat()
        return f"{path}:{stat.st_mtime_ns}:{stat.st_size}"
    except OSError:
        return str(command)


def ninja_dependencies(build_dir: Path, compile_entry: dict) -> list[Path] | None:
    object_path = extract_object_path(compile_entry)
    if object_path is None:
        return None

    base_dir = Path(compile_entry.get("directory", build_dir))
    if not base_dir.is_absolute():
        base_dir = (build_dir / base_dir).resolve()
    object_abs = object_path if object_path.is_absolute() else base_dir / object_path

    try:
        object_stat = object_abs.stat()
    except OSError:
        return None

    source_file = compile_entry.get("file")
    if isinstance(source_file, str) and source_file:
        source_abs = Path(source_file)
        if not source_abs.is_absolute():
            source_abs = base_dir / source_abs
        try:
            if source_abs.stat().st_mtime_ns > object_stat.st_mtime_ns:
                return None
        except OSError:
            return None

    try:
        object_rel = object_abs.resolve().relative_to(build_dir.resolve())
    except (OSError, ValueError):
        object_rel = object_path

    ninja = shutil.which("ninja") or shutil.which("ninja-build")
    if ninja is None:
        return None

    try:
        result = subprocess.run(
            [ninja, "-C", str(build_dir), "-t", "deps", str(object_rel)],
            capture_output=True,
            text=True,
            timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode != 0:
        return None
    if "(STALE)" in result.stdout:
        return None

    dependencies = []
    for line in result.stdout.splitlines():
        if line.startswith("    "):
            dependency = line.strip()
            if dependency:
                dependency_path = Path(dependency)
                if not dependency_path.is_absolute():
                    dependency_path = build_dir / dependency_path
                try:
                    if dependency_path.stat().st_mtime_ns > object_stat.st_mtime_ns:
                        return None
                except OSError:
                    return None
                dependencies.append(dependency_path)
    return dependencies or None


def project_header_dependencies(root_dir: Path, build_dir: Path) -> list[Path]:
    paths: set[Path] = set()
    source_dir = root_dir / "source"
    if source_dir.exists():
        paths.update(
            path
            for path in source_dir.rglob("*")
            if path.is_file() and path.suffix in {".h", ".hpp", ".inc", ".inl"}
        )

    # Generated JUCE headers are not under source/, but changes to them must
    # invalidate a conservative cache entry as well.
    if build_dir.exists():
        paths.update(build_dir.rglob("JuceLibraryCode/*.h"))
        paths.update(build_dir.rglob("BinaryData.h"))

    juce_modules = root_dir / "submodules" / "JUCE" / "modules"
    if juce_modules.exists():
        paths.update(
            path
            for path in juce_modules.rglob("*")
            if path.is_file() and path.suffix in {".h", ".hpp", ".inl"}
        )

    return sorted(paths, key=str)


def update_dependency_hash(hasher, dependencies: list[Path]) -> None:
    unique = sorted({str(path.resolve()) for path in dependencies}, key=str)
    for dependency in unique:
        hasher.update(dependency.encode("utf-8"))
        try:
            data = Path(dependency).read_bytes()
            hasher.update(hashlib.sha256(data).digest())
        except OSError:
            hasher.update(b"missing")


def compute_key(
    source_file: str,
    build_dir: Path,
    root_dir: Path,
    compile_entry: dict,
    dependencies: list[Path] | None,
    tool_identity: str,
    tidy_args: list[str],
) -> str:
    hasher = hashlib.sha256()
    hasher.update(CACHE_VERSION)
    hasher.update(compute_config_hash(root_dir).encode("utf-8"))
    hasher.update(b"tidy-args:")
    hasher.update(
        json.dumps(tidy_args, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    )

    source_path = Path(source_file).resolve()
    hasher.update(str(source_path).encode("utf-8"))
    try:
        hasher.update(source_path.read_bytes())
    except OSError:
        hasher.update(b"missing-source")

    if compile_entry:
        command = compile_entry.get("command")
        if not command:
            command = " ".join(compile_entry.get("arguments", []))
        hasher.update(str(command).encode("utf-8"))

    hasher.update(str(build_dir.resolve()).encode("utf-8"))
    hasher.update(tool_identity.encode("utf-8"))

    if dependencies is None:
        hasher.update(b"deps:fallback")
        dependencies = project_header_dependencies(root_dir, build_dir)
    else:
        hasher.update(b"deps:ninja")
    update_dependency_hash(hasher, dependencies)
    return hasher.hexdigest()


def main():
    if len(sys.argv) < 2:
        print("Usage: tidy-cache.py [clang-tidy args...]", file=sys.stderr)
        sys.exit(1)

    args = sys.argv[1:]
    root_dir = repo_root()
    real_clang_tidy = os.environ.get("REAL_CLANG_TIDY", "clang-tidy-21")

    if "--clear-cache" in args:
        cache_dir = get_cache_dir(root_dir)
        count = 0
        for path in cache_dir.glob("*.json"):
            path.unlink()
            count += 1
        print(f"[tidy-cache] Cleared {count} cached entries in {cache_dir}")
        sys.exit(0)

    build_dir = root_dir / "build-wsl-clang"
    source_files = []

    index = 0
    while index < len(args):
        arg = args[index]
        if arg == "-p" and index + 1 < len(args):
            build_dir = Path(args[index + 1])
            index += 2
            continue
        if arg.startswith("-p="):
            build_dir = Path(arg[3:])
            index += 1
            continue
        if not arg.startswith("-"):
            source_files.append(arg)
        index += 1

    if not build_dir.is_absolute():
        build_dir = root_dir / build_dir

    if len(source_files) != 1:
        cmd = [real_clang_tidy] + args
        result = subprocess.run(cmd)
        sys.exit(result.returncode)

    source_file = source_files[0]
    source_path = Path(source_file)
    if not source_path.is_absolute():
        source_path = root_dir / source_path
    source_path = source_path.resolve()

    compile_commands = parse_compile_commands(build_dir)
    compile_entry = compile_commands.get(
        os.path.normpath(str(source_path)), {}
    )
    dependencies = ninja_dependencies(build_dir, compile_entry) if compile_entry else None
    tool_identity = (
        f"tidy:{file_identity(real_clang_tidy)};"
        f"compiler:{file_identity(compiler_command(compile_entry))}"
    )

    cache_dir = get_cache_dir(root_dir)
    cache_key = compute_key(
        str(source_path),
        build_dir,
        root_dir,
        compile_entry,
        dependencies,
        tool_identity,
        args,
    )
    cache_file = cache_dir / f"{cache_key}.json"

    if cache_file.exists():
        try:
            with open(cache_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            if data.get("stdout"):
                sys.stdout.write(data["stdout"])
            if data.get("stderr"):
                sys.stderr.write(data["stderr"])
            sys.exit(data.get("returncode", 0))
        except Exception:
            pass

    cmd = [real_clang_tidy] + args
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)

    try:
        cache_data = {
            "source": source_file,
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
        with open(cache_file, "w", encoding="utf-8") as f:
            json.dump(cache_data, f)
    except Exception:
        pass

    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
