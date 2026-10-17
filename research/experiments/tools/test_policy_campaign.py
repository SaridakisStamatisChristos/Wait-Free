#!/usr/bin/env python3
from __future__ import annotations

from analyze_policy_campaign import analyze

CANDIDATES = ("veriqueue", "candidate_fast")
EXTERNALS = ("rigtorp", "drogalis")


def record(impl: str, repetition: int, rate: float) -> dict:
    return {
        "comparison_schema": "veriqueue_cross_algorithm_campaign_v1",
        "comparison_lane": "x64-gcc",
        "comparison_architecture": "x64",
        "comparison_compiler": "gcc",
        "comparison_source_commit": "abc",
        "capacity": 256,
        "payload_bytes": 16,
        "topology": "test",
        "producer_cpu": 0,
        "consumer_cpu": 1,
        "repetition": repetition,
        "implementation": impl,
        "transfers_per_second": rate,
        "valid": True,
        "affinity_valid": True,
        "warmup": False,
        "rigtorp_commit": "r",
        "drogalis_commit": "d",
        "boost_version": "1",
        "moodycamel_commit": "m",
    }


def main() -> None:
    records = []
    for repetition in range(8):
        records.extend(
            [
                record("veriqueue", repetition, 90.0),
                record("candidate_fast", repetition, 120.0),
                record("rigtorp", repetition, 100.0),
                record("drogalis", repetition, 105.0),
            ]
        )
    result = analyze(records, CANDIDATES, EXTERNALS, 7, 500)
    summary = {row["candidate"]: row for row in result["overall"]}
    assert summary["candidate_fast"]["vs_best_external_geomean"] > 1.0
    assert summary["candidate_fast"]["vs_production_geomean"] > 1.0
    assert summary["veriqueue"]["vs_best_external_geomean"] < 1.0
    assert result["cells"][0]["best_external"] == "drogalis"
    print("policy campaign analyzer checks passed")


if __name__ == "__main__":
    main()
