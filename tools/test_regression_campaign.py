#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import pathlib
import unittest

TOOLS = pathlib.Path(__file__).resolve().parent


def load_analyzer():
    path = TOOLS / "analyze_regression_campaign.py"
    spec = importlib.util.spec_from_file_location("analyze_regression_campaign", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load regression analyzer")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


analyzer = load_analyzer()


def pair(repetition: int, candidate: float, reference: float = 100.0):
    common = {
        "capacity": 1024,
        "payload_bytes": 8,
        "topology": "test-topology",
        "producer_cpu": 0,
        "consumer_cpu": 1,
        "repetition": repetition,
        "warmup": False,
        "valid": True,
    }
    return [
        {**common, "regression_role": "candidate", "transfers_per_second": candidate},
        {**common, "regression_role": "reference", "transfers_per_second": reference},
    ]


class RegressionCampaignTests(unittest.TestCase):
    def test_policy_boundaries_are_predeclared(self):
        self.assertEqual(analyzer.classify(0.949, 0.979), "FAIL")
        self.assertEqual(analyzer.classify(0.949, 0.981), "WARN")
        self.assertEqual(analyzer.classify(0.979, 1.020), "WARN")
        self.assertEqual(analyzer.classify(0.980, 0.970), "PASS")
        self.assertEqual(analyzer.classify(1.000, 1.010), "PASS")

    def test_missing_pair_fails_closed(self):
        records = pair(0, 100.0)
        records.pop()
        with self.assertRaisesRegex(ValueError, "unpaired repetition"):
            analyzer.analyze_records(records, 20261009)

    def test_duplicate_role_fails_closed(self):
        records = pair(0, 100.0)
        records.append(dict(records[0]))
        with self.assertRaisesRegex(ValueError, "duplicate candidate result"):
            analyzer.analyze_records(records, 20261009)

    def test_obvious_regression_is_fail(self):
        records = []
        for repetition in range(15):
            records.extend(pair(repetition, 90.0 + (repetition % 3) * 0.1))
        result = analyzer.analyze_records(records, 20261009)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["classification"], "FAIL")
        self.assertLess(result[0]["median_ratio"], 0.95)
        self.assertLess(result[0]["ci95_high"], 0.98)

    def test_small_noise_is_not_regression(self):
        records = []
        values = [99.0, 100.5, 98.8, 101.0, 100.0] * 3
        for repetition, candidate in enumerate(values):
            records.extend(pair(repetition, candidate))
        result = analyzer.analyze_records(records, 20261009)
        self.assertEqual(result[0]["classification"], "PASS")


if __name__ == "__main__":
    unittest.main()
