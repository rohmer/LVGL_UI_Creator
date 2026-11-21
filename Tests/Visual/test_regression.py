"""Small parser/comparator/budget tests; no editor or optional imaging package needed."""
import copy
import json
import struct
import tempfile
import unittest
from pathlib import Path

from compare import ComparisonError, compare, read_image, update_baselines, write_png
from run import check_budgets


class RegressionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.actual, self.baseline, self.output = (self.root / name for name in ("actual", "baseline", "difference"))
        self.actual.mkdir()
        self.pixels = bytes((20, 40, 60, 255, 80, 100, 120, 255))
        write_png(self.actual / "fixture.png", 2, 1, self.pixels)
        self.manifest = {"format": "lvgl-visual-fixtures", "version": 1, "environment": {"platform": "test", "font": "built-in"}, "cases": [{"id": "fixture", "theme": "slate-light", "width": 2, "height": 1, "file": "fixture.png"}]}
        (self.actual / "manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        update_baselines(self.actual, self.baseline)

    def test_exact_pixels_detect_change_and_write_difference(self):
        self.assertTrue(compare(self.actual, self.baseline, self.output)["passed"])
        changed = bytes((21,)) + self.pixels[1:]
        write_png(self.actual / "fixture.png", 2, 1, changed)
        result = compare(self.actual, self.baseline, self.output)
        self.assertFalse(result["passed"])
        self.assertEqual(result["cases"][0]["differentPixels"], 1)
        self.assertEqual(read_image(self.output / "fixture.diff.png")[:2], (2, 1))

    def test_masks_and_tolerances_require_explicit_reason(self):
        write_png(self.actual / "fixture.png", 2, 1, bytes((21,)) + self.pixels[1:])
        with self.assertRaises(ComparisonError):
            compare(self.actual, self.baseline, self.output, channel_threshold=1)
        with self.assertRaises(ComparisonError):
            compare(self.actual, self.baseline, self.output, {"fixture": [{"x": 0, "y": 0, "width": 1, "height": 1}]})
        masks = {"fixture": [{"x": 0, "y": 0, "width": 1, "height": 1, "reason": "Explicit test region"}]}
        self.assertTrue(compare(self.actual, self.baseline, self.output, masks)["passed"])
        self.assertTrue(compare(self.actual, self.baseline, self.output, channel_threshold=1, tolerance_reason="Test quantization")["passed"])

    def test_case_environment_and_path_changes_fail_closed(self):
        altered = copy.deepcopy(self.manifest)
        altered["environment"]["platform"] = "different"
        (self.actual / "manifest.json").write_text(json.dumps(altered), encoding="utf-8")
        with self.assertRaises(ComparisonError):
            compare(self.actual, self.baseline, self.output)
        altered = copy.deepcopy(self.manifest)
        altered["cases"][0]["file"] = "../outside.png"
        (self.actual / "manifest.json").write_text(json.dumps(altered), encoding="utf-8")
        with self.assertRaises(ComparisonError):
            update_baselines(self.actual, self.baseline)

    def test_bmp_bottom_up_and_png_round_trip_are_equivalent(self):
        # One padded 24-bit BMP row, BGR order, positive (bottom-up) height.
        body = bytes((60, 40, 20, 120, 100, 80, 0, 0))
        header = b"BM" + struct.pack("<IHHI", 54 + len(body), 0, 0, 54) + struct.pack("<IiiHHIIIIII", 40, 2, 1, 1, 24, 0, len(body), 0, 0, 0, 0)
        path = self.actual / "fixture.bmp"
        path.write_bytes(header + body)
        self.assertEqual(read_image(path), (2, 1, self.pixels))
        self.assertEqual(read_image(self.actual / "fixture.png"), (2, 1, self.pixels))
        broken = bytearray((self.actual / "fixture.png").read_bytes())
        broken[30] ^= 1
        (self.actual / "broken.png").write_bytes(broken)
        with self.assertRaises(ComparisonError):
            read_image(self.actual / "broken.png")

    def test_budget_needs_samples_and_rejects_overrun_or_unavailable_measurement(self):
        report = {"format": "lvgl-performance-report", "version": 1, "source": "Measured native display", "frames": 30, "replayActions": 2, "replayAssertionsPassed": True, "frame": {"samples": 30, "p95Ms": 2.0}}
        budgets = {"format": "lvgl-regression-budgets", "version": 1, "limits": [{"metric": "frame.p95Ms", "samples": "frame.samples", "minimumSamples": 20, "maximum": 5}]}
        self.assertTrue(check_budgets(report, budgets)["passed"])
        report["frame"]["p95Ms"] = 6
        self.assertFalse(check_budgets(report, budgets)["passed"])
        report["frame"]["samples"] = 0
        with self.assertRaises(ComparisonError):
            check_budgets(report, budgets)
        report["frame"]["samples"] = 30
        budgets["limits"][0]["available"] = "ramAvailable"
        with self.assertRaises(ComparisonError):
            check_budgets(report, budgets)
        del budgets["limits"][0]["available"]
        report["frame"]["p95Ms"] = float("nan")
        with self.assertRaises(ComparisonError):
            check_budgets(report, budgets)

    def test_baseline_update_is_explicit(self):
        write_png(self.actual / "fixture.png", 2, 1, bytes((99,)) + self.pixels[1:])
        self.assertFalse(compare(self.actual, self.baseline, self.output)["passed"])
        update_baselines(self.actual, self.baseline)
        self.assertTrue(compare(self.actual, self.baseline, self.output)["passed"])


if __name__ == "__main__":
    unittest.main()
