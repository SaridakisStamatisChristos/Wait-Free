#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib

FUNCTIONS = (
    "veriqueue_production_push",
    "veriqueue_production_pop",
    "veriqueue_single_owner_push",
    "veriqueue_single_owner_pop",
)


def extract_function(text: str, name: str) -> list[str]:
    lines = text.splitlines()
    start = next(
        (i for i, line in enumerate(lines) if line.lstrip().startswith(f"{name}:")),
        None,
    )
    if start is None:
        raise SystemExit(f"function label not found in assembly: {name}")
    body: list[str] = []
    for line in lines[start + 1 :]:
        stripped = line.strip()
        if stripped.startswith(".size") and name in stripped:
            break
        if stripped and not stripped.startswith(".") and not stripped.endswith(":"):
            stripped = stripped.split("#", 1)[0].strip()
            stripped = stripped.split("//", 1)[0].strip()
            if stripped:
                body.append(stripped)
    return body


def mnemonic(line: str) -> str:
    return line.split(None, 1)[0].lower()


def count_metrics(lines: list[str], arch: str) -> dict[str, int | str]:
    mnemonics = [mnemonic(line) for line in lines]
    if arch == "arm64":
        release = sum(m.startswith(("stlr", "stlxr")) for m in mnemonics)
        acquire = sum(m.startswith(("ldar", "ldaxr")) for m in mnemonics)
        ordinary_stores = sum(m.startswith(("str", "stur", "stp")) for m in mnemonics)
        branches = sum(
            m == "b" or m.startswith(("b.", "cb", "tb", "bl")) or m == "ret"
            for m in mnemonics
        )
        address_generation = sum(m.startswith(("adr", "adrp")) or m == "add" for m in mnemonics)
        ordering_note = "AArch64 acquire/release instructions are explicitly countable (for example ldar/stlr)."
        peer_acquire: int | str = acquire
    else:
        release = 0
        acquire = 0
        ordinary_stores = 0
        for line, m in zip(lines, mnemonics):
            if m.startswith(("mov", "vmov")):
                parts = line.split(None, 1)
                operands = parts[1] if len(parts) == 2 else ""
                if "," in operands and "(" in operands.rsplit(",", 1)[-1]:
                    ordinary_stores += 1
        branches = sum(m.startswith("j") or m.startswith("call") or m.startswith("ret") for m in mnemonics)
        address_generation = sum(m.startswith("lea") for m in mnemonics)
        ordering_note = "x86-64 acquire/release normally lowers to ordinary loads/stores; mnemonic-only separation is not possible."
        peer_acquire = "not-separable-from-ordinary-loads"

    return {
        "instructions": len(lines),
        "ordinary_store_candidates": ordinary_stores,
        "explicit_atomic_release_stores": release,
        "explicit_acquire_loads": acquire,
        "peer_cursor_acquire_candidates": peer_acquire,
        "source_level_peer_cursor_load_sites": 1,
        "branches": branches,
        "address_generation_candidates": address_generation,
        "ordering_note": ordering_note,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare production and single-owner-cursor assembly")
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("--arch", choices=("x64", "arm64"), required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    text = args.input.read_text(encoding="utf-8", errors="replace")
    metrics = {name: count_metrics(extract_function(text, name), args.arch) for name in FUNCTIONS}
    hypothesis: dict[str, bool | None] = {}

    for operation in ("push", "pop"):
        production = metrics[f"veriqueue_production_{operation}"]
        variant = metrics[f"veriqueue_single_owner_{operation}"]
        instruction_delta = int(production["instructions"]) - int(variant["instructions"])
        store_delta = int(production["ordinary_store_candidates"]) - int(variant["ordinary_store_candidates"])
        production["instruction_delta_vs_variant"] = instruction_delta
        production["ordinary_store_delta_vs_variant"] = store_delta
        if args.arch == "arm64":
            hypothesis[operation] = (
                int(production["explicit_atomic_release_stores"]) >= 1
                and int(variant["explicit_atomic_release_stores"]) >= 1
                and store_delta >= 1
            )
        else:
            hypothesis[operation] = None

    payload = {
        "arch": args.arch,
        "compiler": args.compiler,
        "specialization": {"payload_bytes": 8, "capacity": 1024},
        "metrics": metrics,
        "structural_owner_store_hypothesis_supported": hypothesis,
        "dependency_chain_note": (
            "At source level production advances the owner cursor then writes both owner-local and published cursor state; "
            "the ablation advances the cursor then performs only the published release store. Static assembly is used to "
            "test whether that extra ordinary store survives lowering."
        ),
        "remote_control_line_note": (
            "Each push/pop function contains one source-level peer acquire-load site on the refresh path. AArch64 explicit "
            "acquire mnemonics are counted; x86-64 acquire loads are not separable from ordinary loads by mnemonic alone."
        ),
        "interpretation_guardrail": (
            "Instruction counts and store/load mnemonics are structural evidence only; "
            "they do not establish performance causation without paired benchmark data."
        ),
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    lines = [
        "# Hot-path assembly comparison",
        "",
        f"Architecture: `{args.arch}`; compiler: `{args.compiler}`; specialization: 8-byte payload / capacity 1024.",
        "",
        "Counts are static whole-function counts, not dynamic executed-path counts. They are used only alongside paired benchmarks.",
        "",
        "| Function | Instructions | Ordinary store candidates | Explicit release stores | Explicit acquire loads | Peer acquire candidates | Branches | Address-gen candidates |",
        "|---|---:|---:|---:|---:|---|---:|---:|",
    ]
    for name in FUNCTIONS:
        item = metrics[name]
        lines.append(
            f"| {name} | {item['instructions']} | {item['ordinary_store_candidates']} | "
            f"{item['explicit_atomic_release_stores']} | {item['explicit_acquire_loads']} | "
            f"{item['peer_cursor_acquire_candidates']} | {item['branches']} | "
            f"{item['address_generation_candidates']} |"
        )
    lines.extend([
        "",
        f"Structural owner-store hypothesis (push): `{hypothesis['push']}`; pop: `{hypothesis['pop']}`.",
        "",
        "Dependency-chain observation: production has an owner-cursor advance feeding both the owner-local write and publication; the ablation removes the owner-local write. This is structural evidence, not a causal performance proof.",
        "",
        "Remote control-line observation: each operation has one conditional peer-cursor acquire-load site. AArch64 makes acquire instructions directly countable; x86-64 does not.",
    ])
    args.markdown_output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
