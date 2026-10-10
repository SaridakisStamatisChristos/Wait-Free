#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import re
from typing import Any

IMPLEMENTATIONS = (
    "vq",
    "raw_seq",
    "raw_tiled",
    "typed_seq",
    "memcpy_seq",
    "rigtorp",
    "boost",
    "moody",
    "drogalis",
)
OPERATIONS = ("push", "pop")
PAYLOADS = ("p16", "p64", "p256")


def expected_symbols() -> list[str]:
    return [
        f"{impl}_{op}_{payload}"
        for payload in PAYLOADS
        for impl in IMPLEMENTATIONS
        for op in OPERATIONS
    ]


def parse_assembly(text: str) -> tuple[dict[str, list[str]], dict[str, str]]:
    symbols: dict[str, list[str]] = {}
    aliases: dict[str, str] = {}
    label_re = re.compile(r"^([A-Za-z_][A-Za-z0-9_$.]*):$")
    alias_re = re.compile(
        r"^\s*\.set\s+([A-Za-z_][A-Za-z0-9_$.]*),\s*([A-Za-z_][A-Za-z0-9_$.]*)"
    )
    size_re = re.compile(r"^\s*\.size\s+([A-Za-z_][A-Za-z0-9_$.]*),")

    current: str | None = None
    for raw in text.splitlines():
        stripped = raw.strip()
        alias = alias_re.match(raw)
        if alias:
            aliases[alias.group(1)] = alias.group(2)
        label = label_re.match(stripped)
        if label and not label.group(1).startswith(".L"):
            current = label.group(1)
            symbols.setdefault(current, [])
            continue
        if current is None:
            continue
        size = size_re.match(raw)
        if size and size.group(1) == current:
            current = None
            continue
        symbols[current].append(raw)
    return symbols, aliases


def summarize_body(lines: list[str]) -> dict[str, Any]:
    instructions: list[str] = []
    for raw in lines:
        stripped = raw.strip()
        if (
            not stripped
            or stripped.startswith(".")
            or stripped.endswith(":")
            or stripped.startswith("#")
        ):
            continue
        code = stripped.split("#", 1)[0].strip()
        if code:
            instructions.append(code)

    mnemonics = [line.split(None, 1)[0].lower() for line in instructions if line.split()]
    text = "\n".join(instructions).lower()
    return {
        "instruction_count": len(instructions),
        "call_count": sum(m.startswith("call") or m in {"bl", "blr"} for m in mnemonics),
        "branch_count": sum(
            m.startswith("j")
            or m.startswith("b.")
            or m in {"b", "br", "cbz", "cbnz", "tbz", "tbnz"}
            for m in mnemonics
        ),
        "address_generation_count": sum(
            m in {"lea", "add", "adds", "sub", "subs", "adr", "adrp"}
            for m in mnemonics
        ),
        "aarch64_load_count": sum(m.startswith("ldr") or m.startswith("ldp") for m in mnemonics),
        "aarch64_store_count": sum(m.startswith("str") or m.startswith("stp") for m in mnemonics),
        "aarch64_acquire_count": sum(
            m.startswith("ldar") or m.startswith("ldapr") for m in mnemonics
        ),
        "aarch64_release_count": sum(m.startswith("stlr") for m in mnemonics),
        "memcpy_reference": "memcpy" in text,
        "memmove_reference": "memmove" in text,
        "forbidden_sync_reference": any(
            token in text
            for token in (
                "lock ",
                "mfence",
                "sfence",
                "lfence",
                "dmb",
                "dsb",
                "ldaxr",
                "stlxr",
                "cas ",
                "swp",
            )
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize VeriQueue forensic assembly wrappers")
    parser.add_argument("assembly", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    text = args.assembly.read_text(encoding="utf-8", errors="replace")
    bodies, aliases = parse_assembly(text)
    rows: list[dict[str, Any]] = []
    missing: list[str] = []
    for symbol in expected_symbols():
        if symbol in bodies:
            row = {"symbol": symbol, "alias_of": None, **summarize_body(bodies[symbol])}
        elif symbol in aliases:
            target = aliases[symbol]
            row = {
                "symbol": symbol,
                "alias_of": target,
                **summarize_body(bodies.get(target, [])),
            }
        else:
            missing.append(symbol)
            continue
        rows.append(row)

    if missing:
        raise SystemExit("missing forensic assembly symbols: " + ", ".join(missing))

    result = {
        "schema": "veriqueue_policy_assembly_v1",
        "assembly": str(args.assembly),
        "symbols": rows,
        "forbidden_sync_symbols": [
            row["symbol"] for row in rows if row["forbidden_sync_reference"]
        ],
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    lines = [
        "# Forensic hot-path assembly summary",
        "",
        "Structural evidence only; instruction counts are not treated as causal performance proof.",
        "",
        "| Symbol | Alias | Instr | Calls | Branches | Addr-gen | A64 Ld | A64 St | Acquire | Release | memcpy | forbidden sync |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|",
    ]
    for row in rows:
        lines.append(
            f"| {row['symbol']} | {row['alias_of'] or '-'} | {row['instruction_count']} | "
            f"{row['call_count']} | {row['branch_count']} | {row['address_generation_count']} | "
            f"{row['aarch64_load_count']} | {row['aarch64_store_count']} | "
            f"{row['aarch64_acquire_count']} | {row['aarch64_release_count']} | "
            f"{'yes' if row['memcpy_reference'] else 'no'} | "
            f"{'yes' if row['forbidden_sync_reference'] else 'no'} |"
        )
    args.markdown_output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(args.markdown_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
