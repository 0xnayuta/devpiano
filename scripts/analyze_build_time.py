#!/usr/bin/env python3
"""devpiano Build Time Trace Analyzer

Aggregates Clang -ftime-trace profiling JSON files to identify compilation
bottlenecks: slowest translation units, cumulative header parse times,
expensive template instantiations, and code generation hotspots.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path


# ANSI Color formatting
class Style:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[1;31m"
    GREEN = "\033[1;32m"
    YELLOW = "\033[1;33m"
    BLUE = "\033[1;34m"
    MAGENTA = "\033[1;35m"
    CYAN = "\033[1;36m"


def format_ms(us: float) -> str:
    ms = us / 1000.0
    if ms >= 1000.0:
        return f"{ms / 1000.0:.2f} s"
    return f"{ms:.1f} ms"


@dataclass
class HeaderStats:
    total_us: float = 0.0
    count: int = 0


@dataclass
class TemplateStats:
    total_us: float = 0.0
    count: int = 0


@dataclass
class FunctionStats:
    total_us: float = 0.0
    count: int = 0


@dataclass
class ProfileReport:
    total_tu_count: int = 0
    total_compile_us: float = 0.0
    slowest_tus: list[tuple[str, float]] = field(default_factory=list)
    headers: dict[str, HeaderStats] = field(default_factory=lambda: defaultdict(HeaderStats))
    templates: dict[str, TemplateStats] = field(default_factory=lambda: defaultdict(TemplateStats))
    functions: dict[str, FunctionStats] = field(default_factory=lambda: defaultdict(FunctionStats))


def analyze_traces(build_dir: Path, min_time_ms: float = 5.0) -> ProfileReport:
    report = ProfileReport()
    trace_files: list[Path] = []

    for root, _, files in os.walk(build_dir):
        for f in files:
            if f.endswith(".json") and not f.startswith("compile_commands") and not f.startswith("combined_"):
                p = Path(root) / f
                trace_files.append(p)

    if not trace_files:
        return report

    report.total_tu_count = len(trace_files)
    min_us = min_time_ms * 1000.0

    all_tu_times: list[tuple[str, float]] = []

    for trace_path in trace_files:
        try:
            with open(trace_path, "r", encoding="utf-8", errors="replace") as f:
                data = json.load(f)
        except Exception:
            continue

        events = data.get("traceEvents", [])
        if not events:
            continue

        # Translation unit total duration: usually the 'Total ExecuteCompiler' or root 'Source' event
        tu_duration_us = 0.0
        for ev in events:
            name = ev.get("name", "")
            dur = ev.get("dur", 0.0)
            args = ev.get("args", {})
            detail = args.get("detail", "") if isinstance(args, dict) else ""

            if name == "ExecuteCompiler":
                tu_duration_us = max(tu_duration_us, dur)
            elif name == "Source" and not detail:
                tu_duration_us = max(tu_duration_us, dur)

            # Header inclusion analysis
            if name == "Source" and detail:
                header_name = detail
                report.headers[header_name].total_us += dur
                report.headers[header_name].count += 1

            # Template instantiation analysis
            elif name in ("InstantiateClass", "InstantiateFunction"):
                if detail and dur >= min_us:
                    report.templates[detail].total_us += dur
                    report.templates[detail].count += 1

            # Backend code generation / optimization
            elif name in ("OptFunction", "CodeGen Function"):
                if detail and dur >= min_us:
                    report.functions[detail].total_us += dur
                    report.functions[detail].count += 1

        rel_name = trace_path.stem
        if rel_name.endswith(".cpp") or rel_name.endswith(".c"):
            pass
        elif ".cpp" in rel_name:
            rel_name = rel_name[: rel_name.find(".cpp") + 4]

        all_tu_times.append((rel_name, tu_duration_us))
        report.total_compile_us += tu_duration_us

    all_tu_times.sort(key=lambda x: x[1], reverse=True)
    report.slowest_tus = all_tu_times[:15]
    return report


def print_report(report: ProfileReport, top_n: int = 10) -> None:
    if report.total_tu_count == 0:
        print(f"{Style.YELLOW}No -ftime-trace profile JSON files found.{Style.RESET}")
        print("Ensure the project was built with ENABLE_TIME_TRACE=ON (e.g. `./scripts/dev.sh time-trace`).")
        return

    print(f"\n{Style.BOLD}{Style.CYAN}=== devpiano C++ Build Profiling Report (-ftime-trace) ==={Style.RESET}\n")
    print(f"  Translation Units Profiled : {Style.BOLD}{report.total_tu_count}{Style.RESET}")
    print(f"  Cumulative Compile Time    : {Style.BOLD}{format_ms(report.total_compile_us)}{Style.RESET}\n")

    # 1. Slowest Translation Units
    print(f"{Style.BOLD}{Style.MAGENTA}1. Slowest Translation Units (Top {min(top_n, len(report.slowest_tus))}){Style.RESET}")
    print(f"{Style.DIM}----------------------------------------------------------------------{Style.RESET}")
    for idx, (name, dur_us) in enumerate(report.slowest_tus[:top_n], start=1):
        bar_len = min(30, int((dur_us / (report.slowest_tus[0][1] or 1.0)) * 30))
        bar = f"{Style.RED}{'=' * bar_len}{Style.RESET}"
        print(f"  {idx:2d}. {name:<45} {format_ms(dur_us):>10}  {bar}")

    # 2. Cumulative Header Parse Times
    sorted_headers = sorted(report.headers.items(), key=lambda x: x[1].total_us, reverse=True)
    print(f"\n{Style.BOLD}{Style.BLUE}2. Most Expensive Headers (Cumulative Parse Time Top {min(top_n, len(sorted_headers))}){Style.RESET}")
    print(f"{Style.DIM}----------------------------------------------------------------------{Style.RESET}")
    for idx, (header, stats) in enumerate(sorted_headers[:top_n], start=1):
        # Shorten path if in repo
        display_header = header
        if "/root/repos/devpiano/" in display_header:
            display_header = display_header.replace("/root/repos/devpiano/", "")
        elif len(display_header) > 48:
            display_header = "..." + display_header[-45:]

        print(f"  {idx:2d}. {display_header:<48} {format_ms(stats.total_us):>10}  (inc: {stats.count:>3}x)")

    # 3. Expensive Template Instantiations
    sorted_templates = sorted(report.templates.items(), key=lambda x: x[1].total_us, reverse=True)
    if sorted_templates:
        print(f"\n{Style.BOLD}{Style.YELLOW}3. Expensive Template Instantiations (Top {min(top_n, len(sorted_templates))}){Style.RESET}")
        print(f"{Style.DIM}----------------------------------------------------------------------{Style.RESET}")
        for idx, (tmpl, stats) in enumerate(sorted_templates[:top_n], start=1):
            short_tmpl = tmpl if len(tmpl) <= 52 else tmpl[:49] + "..."
            print(f"  {idx:2d}. {short_tmpl:<52} {format_ms(stats.total_us):>10}  ({stats.count:>2}x)")

    # 4. Slowest Functions (CodeGen / Optimization)
    sorted_funcs = sorted(report.functions.items(), key=lambda x: x[1].total_us, reverse=True)
    if sorted_funcs:
        print(f"\n{Style.BOLD}{Style.GREEN}4. Slowest Backend CodeGen / Opt Functions (Top {min(top_n, len(sorted_funcs))}){Style.RESET}")
        print(f"{Style.DIM}----------------------------------------------------------------------{Style.RESET}")
        for idx, (fn, stats) in enumerate(sorted_funcs[:top_n], start=1):
            short_fn = fn if len(fn) <= 52 else fn[:49] + "..."
            print(f"  {idx:2d}. {short_fn:<52} {format_ms(stats.total_us):>10}  ({stats.count:>2}x)")

    print(f"\n{Style.CYAN}Tip: Merge traces to a single timeline via --merge-trace to inspect in https://ui.perfetto.dev{Style.RESET}\n")


def merge_traces_for_perfetto(build_dir: Path, output_file: Path) -> int:
    merged_events: list[dict] = []
    pid = 1

    for root, _, files in os.walk(build_dir):
        for f in sorted(files):
            if f.endswith(".json") and not f.startswith("compile_commands") and not f.startswith("combined_"):
                p = Path(root) / f
                try:
                    with open(p, "r", encoding="utf-8", errors="replace") as jf:
                        data = json.load(jf)
                except Exception:
                    continue

                events = data.get("traceEvents", [])
                tu_name = p.stem

                # Assign unique process/thread ID per translation unit for clean flame visualization
                for ev in events:
                    ev_copy = dict(ev)
                    ev_copy["pid"] = pid
                    ev_copy["tid"] = 1
                    merged_events.append(ev_copy)

                # Add process name event
                merged_events.append({
                    "name": "process_name",
                    "ph": "M",
                    "pid": pid,
                    "tid": 1,
                    "args": {"name": tu_name},
                })
                pid += 1

    if not merged_events:
        return 0

    with open(output_file, "w", encoding="utf-8") as out:
        json.dump({"traceEvents": merged_events}, out)

    return len(merged_events)


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze Clang -ftime-trace build profiling data.")
    parser.add_argument("--build-dir", default="build-wsl-clang", help="Build directory containing .json traces")
    parser.add_argument("--top", type=int, default=10, help="Number of top items to display")
    parser.add_argument("--merge-trace", type=str, default="", help="Export merged JSON trace for Chrome Tracing / Perfetto")

    args = parser.parse_args()
    build_path = Path(args.build_dir)
    if not build_path.is_absolute():
        repo_root = Path(__file__).resolve().parent.parent
        build_path = repo_root / args.build_dir

    if not build_path.exists():
        print(f"Error: Build directory not found: {build_path}", file=sys.stderr)
        return 1

    report = analyze_traces(build_path)
    print_report(report, top_n=args.top)

    if args.merge_trace:
        merge_path = Path(args.merge_trace)
        if not merge_path.is_absolute():
            merge_path = build_path / args.merge_trace
        event_count = merge_traces_for_perfetto(build_path, merge_path)
        print(f"{Style.GREEN}[OK] Exported {event_count} events to {merge_path}{Style.RESET}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
