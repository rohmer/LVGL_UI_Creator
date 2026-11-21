#!/usr/bin/env python3
"""Capture native fixtures, replay input, enforce measured budgets, and compare baselines."""
from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

from compare import ComparisonError, compare, update_baselines


def metric(report: dict, path: str):
    value = report
    for component in path.split("."):
        if not isinstance(value, dict) or component not in value:
            raise ComparisonError(f"Required measured metric is missing: {path}")
        value = value[component]
    if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value) or value < 0:
        raise ComparisonError(f"Metric must be finite and nonnegative: {path}")
    return value


def check_budgets(report: dict, budgets: dict) -> dict:
    if report.get("format") != "lvgl-performance-report" or report.get("version") != 1:
        raise ComparisonError("Expected an lvgl-performance-report version 1 file")
    if not report.get("replayAssertionsPassed") or metric(report, "replayActions") == 0:
        raise ComparisonError("Input replay assertions did not complete")
    if not report.get("source") or metric(report, "frames") == 0:
        raise ComparisonError("Performance report has no real rendered frames or measurement source")
    if budgets.get("format") != "lvgl-regression-budgets" or budgets.get("version") != 1 or not budgets.get("limits"):
        raise ComparisonError("Expected nonempty lvgl-regression-budgets version 1 limits")
    results = []
    for limit in budgets["limits"]:
        path = limit["metric"]
        if limit.get("available") and not report.get(limit["available"], False):
            raise ComparisonError(f"{path} is unavailable; it cannot satisfy a measured performance budget")
        value = metric(report, path)
        if limit.get("samples"):
            samples = metric(report, limit["samples"])
            if samples < limit.get("minimumSamples", 1):
                raise ComparisonError(f"{path} has only {samples} measured samples")
        if "maximum" not in limit and "minimum" not in limit:
            raise ComparisonError(f"Budget has no bound: {path}")
        lower, upper = limit.get("minimum", 0), limit.get("maximum", math.inf)
        if not isinstance(lower, (int, float)) or isinstance(lower, bool) or not isinstance(upper, (int, float)) or isinstance(upper, bool) or not math.isfinite(lower) or ("maximum" in limit and not math.isfinite(upper)) or lower < 0 or upper < lower:
            raise ComparisonError(f"Invalid budget bounds: {path}")
        results.append({"metric": path, "measured": value, "minimum": lower, "maximum": upper if math.isfinite(upper) else None, "passed": lower <= value <= upper})
    return {"format": "lvgl-performance-comparison", "version": 1, "budget": budgets.get("name", "Custom"), "measurementSource": report["source"], "passed": all(result["passed"] for result in results), "limits": results}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor", type=Path, help="Editor integration test executable with --visual-baselines support")
    parser.add_argument("--existing", action="store_true", help="Check artifacts already captured in --output")
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--budgets", type=Path, default=Path(__file__).with_name("desktop-budgets.json"))
    parser.add_argument("--replay", type=Path, default=Path(__file__).with_name("keyboard-replay.json"))
    parser.add_argument("--masks", type=Path)
    parser.add_argument("--update", action="store_true", help="Explicitly replace reviewed visual baselines after replay and budgets pass")
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args()
    try:
        output, baseline = args.output.resolve(), args.baseline.resolve()
        if output == baseline or output.is_relative_to(baseline) or baseline.is_relative_to(output):
            raise ComparisonError("Capture output and baseline directories must be separate")
        output.mkdir(parents=True, exist_ok=True)
        if not args.existing:
            if not args.editor or not args.editor.is_file():
                raise ComparisonError("Provide --editor <integration executable>, or --existing for saved artifacts")
            replay = args.replay.resolve()
            if replay != output / "replay-input.json":
                shutil.copyfile(replay, output / "replay-input.json")
            environment = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", SDL_RENDER_DRIVER="software")
            started = time.perf_counter()
            with (output / "capture.log").open("w", encoding="utf-8") as log:
                result = subprocess.run([str(args.editor.resolve()), "--visual-baselines", str(output)], cwd=output, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
            capture = {"command": [str(args.editor.resolve()), "--visual-baselines", str(output)], "elapsedSeconds": time.perf_counter() - started, "exitCode": result.returncode}
            (output / "capture-process.json").write_text(json.dumps(capture, indent=2) + "\n", encoding="utf-8")
            if result.returncode:
                raise ComparisonError(f"Native fixture/input replay failed with exit code {result.returncode}; see {output / 'capture.log'}")
        measured = json.loads((output / "performance.json").read_text(encoding="utf-8"))
        trace = json.loads((output / "replay-trace.json").read_text(encoding="utf-8"))
        if not trace or len(trace) != measured.get("replayActions") or any(action.get("passed") is not True for action in trace):
            raise ComparisonError("Replay trace is incomplete or contains failed actions")
        performance = check_budgets(measured, json.loads(args.budgets.read_text(encoding="utf-8")))
        (output / "budget-comparison.json").write_text(json.dumps(performance, indent=2) + "\n", encoding="utf-8")
        if not performance["passed"]:
            failed = [limit["metric"] for limit in performance["limits"] if not limit["passed"]]
            raise ComparisonError("Measured performance budgets exceeded: " + ", ".join(failed))
        if args.update:
            updated = update_baselines(output, baseline)
            print(f"Explicitly updated {updated['updated']} visual baselines; measured replay budgets passed")
        else:
            masks = json.loads(args.masks.read_text(encoding="utf-8")) if args.masks else None
            visual = compare(output, baseline, output / "differences", masks)
            if not visual["passed"]:
                raise ComparisonError(f"Visual regression detected; review {output / 'differences/comparison.json'}")
            print(f"Passed {len(visual['cases'])} exact visual comparisons and {len(performance['limits'])} measured performance budgets")
        return 0
    except (ComparisonError, OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(f"Regression run failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
