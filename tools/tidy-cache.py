#!/usr/bin/env python3
"""
Lightweight clang-tidy cache wrapper for devpiano.

Computes a deterministic cache key based on:
1. Source file contents and modification time.
2. The compilation flags from compile_commands.json.
3. The content of .clang-tidy configuration.
4. Dependency header timestamps/contents via compiler depfiles or preprocessor.

If cache hits, outputs cached stdout/stderr and exits with cached returncode.
Otherwise, invokes the real clang-tidy binary and caches the result.
"""

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path


def get_cache_dir() -> Path:
    override = os.environ.get("TIDY_CACHE_DIR")
    if override:
        path = Path(override)
    else:
        # Default to repo root .cache/tidy-cache or user cache
        path = Path.cwd() / ".cache" / "tidy-cache"
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
        return {os.path.normpath(e["file"]): e for e in entries}
    except Exception:
        return {}


def compute_key(source_file: str, build_dir: Path, root_dir: Path, compile_entry: dict) -> str:
    hasher = hashlib.sha256()

    # 1. .clang-tidy config
    hasher.update(compute_config_hash(root_dir).encode("utf-8"))

    # 2. Source file path and contents
    source_path = Path(source_file).resolve()
    hasher.update(str(source_path).encode("utf-8"))
    if source_path.exists():
        hasher.update(source_path.read_bytes())
    else:
        hasher.update(b"missing-source")

    # 3. Compiler command arguments
    if compile_entry:
        cmd = compile_entry.get("command") or " ".join(compile_entry.get("arguments", []))
        hasher.update(cmd.encode("utf-8"))

    # 4. Dependency tracking: check header contents in source/ using fast deterministic hashing
    headers_info = []
    source_dir = root_dir / "source"
    if source_dir.exists():
        for root, _, files in os.walk(source_dir):
            for f in sorted(files):
                if f.endswith((".h", ".hpp", ".inc")):
                    hp = Path(root) / f
                    try:
                        rel_path = hp.relative_to(source_dir).as_posix()
                        h_hash = hashlib.sha256(hp.read_bytes()).hexdigest()[:16]
                        headers_info.append(f"{rel_path}:{h_hash}")
                    except OSError:
                        pass
    headers_info.sort()
    hasher.update(";".join(headers_info).encode("utf-8"))
    return hasher.hexdigest()


def main():
    if len(sys.argv) < 2:
        print("Usage: tidy-cache.py [clang-tidy args...]", file=sys.stderr)
        sys.exit(1)

    args = sys.argv[1:]

    # Find the real clang-tidy executable
    real_clang_tidy = os.environ.get("REAL_CLANG_TIDY", "clang-tidy-21")

    # If --clear-cache requested
    if "--clear-cache" in args:
        cache_dir = get_cache_dir()
        count = 0
        for f in cache_dir.glob("*.json"):
            f.unlink()
            count += 1
        print(f"[tidy-cache] Cleared {count} cached entries in {cache_dir}")
        sys.exit(0)

    # Locate build directory (-p) and source file
    build_dir = Path("build-wsl-clang")
    source_files = []

    i = 0
    while i < len(args):
        arg = args[i]
        if arg == "-p" and i + 1 < len(args):
            build_dir = Path(args[i + 1])
            i += 2
            continue
        elif arg.startswith("-p="):
            build_dir = Path(arg[3:])
            i += 1
            continue
        elif not arg.startswith("-"):
            source_files.append(arg)
        i += 1

    # If not a single source file check (e.g. general query), passthrough directly
    if len(source_files) != 1:
        cmd = [real_clang_tidy] + args
        res = subprocess.run(cmd)
        sys.exit(res.returncode)

    source_file = source_files[0]
    # Skip uncompiled strategic extensions (ADR-014)
    norm_source = Path(source_file).as_posix()
    if "source/UI/jive/extensions/" in norm_source:
        sys.exit(0)
    root_dir = Path.cwd()
    compile_commands = parse_compile_commands(build_dir)
    compile_entry = compile_commands.get(os.path.normpath(str(Path(source_file).resolve())), {})

    cache_dir = get_cache_dir()
    cache_key = compute_key(source_file, build_dir, root_dir, compile_entry)
    cache_file = cache_dir / f"{cache_key}.json"

    # Check if cached
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
            pass  # Corrupted cache entry, fall through to re-run

    # Cache miss: run real clang-tidy
    cmd = [real_clang_tidy] + args
    res = subprocess.run(cmd, capture_output=True, text=True)

    # Output to stdout/stderr
    if res.stdout:
        sys.stdout.write(res.stdout)
    if res.stderr:
        sys.stderr.write(res.stderr)

    # Save to cache
    try:
        cache_data = {
            "source": source_file,
            "returncode": res.returncode,
            "stdout": res.stdout,
            "stderr": res.stderr,
        }
        with open(cache_file, "w", encoding="utf-8") as f:
            json.dump(cache_data, f)
    except Exception as e:
        # Failing to write cache should not fail the build/check
        pass

    sys.exit(res.returncode)


if __name__ == "__main__":
    main()
