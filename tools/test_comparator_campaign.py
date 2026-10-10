#!/usr/bin/env python3
from __future__ import annotations

from analyze_comparator_campaign import analyze_records, geometric_mean


def make_record(implementation: str, repetition: int, rate: float) -> dict[str, object]:
    return {
        "comparison_schema": "veriqueue_cross_algorithm_campaign_v1",
        "comparison_lane": "arm64-gcc",
        "comparison_architecture": "arm64",
        "comparison_compiler": "gcc",
        "comparison_source_commit": "deadbeef",
        "capacity": 256,
        "payload_bytes": 8,
        "topology": "synthetic",
        "producer_cpu": 0,
        "consumer_cpu": 1,
        "implementation": implementation,
        "repetition": repetition,
        "warmup": False,
        "valid": True,
        "affinity_valid": True,
        "transfers_per_second": rate,
        "rigtorp_commit": "r",
        "moodycamel_commit": "m",
        "drogalis_commit": "d",
        "boost_version": 108500,
    }


def main() -> None:
    records: list[dict[str, object]] = []
    rates = {
        "veriqueue": 200.0,
        "rigtorp": 100.0,
        "boost_lockfree": 125.0,
        "moodycamel": 150.0,
        "drogalis": 175.0,
    }
    for repetition in range(12):
        for implementation, base in rates.items():
            records.append(make_record(implementation, repetition, base + repetition * 0.01))

    result = analyze_records(records, bootstrap_seed=7, bootstrap_samples=500)
    assert result["source_commit"] == "deadbeef"
    assert len(result["pairwise_cells"]) == 4
    assert all(row["classification"] == "WIN" for row in result["pairwise_cells"])
    assert result["overall_vs_best_external"]["geometric_mean_ratio"] > 1.0
    assert abs(geometric_mean([1.0, 4.0]) - 2.0) < 1e-12

    broken = records[:-1]
    try:
        analyze_records(broken, bootstrap_seed=7, bootstrap_samples=50)
    except ValueError:
        pass
    else:
        raise AssertionError("incomplete repetition must be rejected")

    print("comparator tooling self-test: PASS")


if __name__ == "__main__":
    main()
