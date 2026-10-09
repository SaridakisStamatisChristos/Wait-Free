#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
from typing import Any

PREFIXES = (
    "vq_production",
    "vq_single_owner",
    "vq_cached_limit",
    "vq_cached_limit_unlikely",
    "vq_guard64",
    "vq_guard128",
    "vq_guard64_skew32",
    "vq_uint32_cursor",
)


def extract_function(text: str, name: str) -> list[str]:
    lines = text.splitlines()
    start = next((i for i, line in enumerate(lines) if line.lstrip().startswith(f"{name}:")), None)
    if start is None:
        raise SystemExit(f"function label not found in assembly: {name}")
    body: list[str] = []
    for line in lines[start + 1 :]:
        stripped = line.strip()
        if stripped.startswith(".size") and name in stripped:
            break
        if not stripped or stripped.startswith(".") or stripped.endswith(":"):
            continue
        stripped = stripped.split("#", 1)[0].split("//", 1)[0].strip()
        if stripped:
            body.append(stripped)
    return body


def normalize(lines: list[str]) -> list[str]:
    normalized: list[str] = []
    for line in lines:
        line = re.sub(r"\.L[A-Za-z0-9_.$]+", ".L", line)
        line = re.sub(r"\s+", " ", line).strip()
        normalized.append(line)
    return normalized


def mnemonic(line: str) -> str:
    return line.split(None, 1)[0].lower()


def metrics(lines: list[str], arch: str) -> dict[str, Any]:
    mnemonics = [mnemonic(line) for line in lines]
    if arch == "arm64":
        stores = sum(m.startswith(("str", "stur", "stp", "stlr")) for m in mnemonics)
        loads = sum(m.startswith(("ldr", "ldur", "ldp", "ldar")) for m in mnemonics)
        release = sum(m.startswith("stlr") for m in mnemonics)
        acquire = sum(m.startswith("ldar") for m in mnemonics)
        branches = sum(m == "b" or m.startswith(("b.", "cb", "tb", "bl")) or m == "ret" for m in mnemonics)
        address = sum(m.startswith(("adr", "adrp")) or m == "add" for m in mnemonics)
        forbidden = [line for line, m in zip(lines, mnemonics) if m.startswith(("cas", "ldadd", "swp")) or m in {"dmb", "dsb", "isb"}]
    else:
        stores = sum(
            m.startswith(("mov", "vmov")) and "," in line and "(" in line.rsplit(",", 1)[-1]
            for line, m in zip(lines, mnemonics)
        )
        loads = sum(
            m.startswith(("mov", "vmov")) and "(" in line.split(",", 1)[0]
            for line, m in zip(lines, mnemonics)
        )
        release = 0
        acquire = 0
        branches = sum(m.startswith("j") or m.startswith("call") or m.startswith("ret") for m in mnemonics)
        address = sum(m.startswith("lea") for m in mnemonics)
        forbidden = [line for line, m in zip(lines, mnemonics) if m.startswith(("lock", "cmpxchg", "xadd"))]
    return {
        "instructions": len(lines),
        "loads": loads,
        "stores": stores,
        "explicit_release_stores": release,
        "explicit_acquire_loads": acquire,
        "branches": branches,
        "address_generation": address,
        "forbidden_instructions": forbidden,
        "normalized_sha256": hashlib.sha256("\n".join(normalize(lines)).encode()).hexdigest(),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit VeriQueue hot-path v2 assembly")
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("--arch", choices=("x64", "arm64"), required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--payload", type=int, required=True, choices=(8, 16, 64, 256))
    parser.add_argument("--json-output", type=pathlib.Path, required=True)
    parser.add_argument("--markdown-output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    text = args.input.read_text(encoding="utf-8", errors="replace")
    functions: dict[str, dict[str, Any]] = {}
    bodies: dict[str, list[str]] = {}
    for prefix in PREFIXES:
        for operation in ("push", "pop"):
            name = f"{prefix}_{operation}"
            body = extract_function(text, name)
            bodies[name] = body
            functions[name] = metrics(body, args.arch)

    forbidden = {
        name: item["forbidden_instructions"]
        for name, item in functions.items()
        if item["forbidden_instructions"]
    }
    unlikely_changed = {
        operation: normalize(bodies[f"vq_cached_limit_{operation}"])
        != normalize(bodies[f"vq_cached_limit_unlikely_{operation}"])
        for operation in ("push", "pop")
    }

    payload = {
        "arch": args.arch,
        "compiler": args.compiler,
        "specialization": {"capacity": 1024, "payload_bytes": args.payload},
        "functions": functions,
        "unlikely_codegen_changed": unlikely_changed,
        "forbidden_instruction_hits": forbidden,
        "forbidden_gate_pass": not forbidden,
        "interpretation_guardrail": "Static assembly supports structural claims only; performance causation requires paired benchmark evidence.",
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    lines = [
        "# Hot-path v2 assembly audit",
        "",
        f"Architecture: `{args.arch}`; compiler: `{args.compiler}`; specialization: capacity 1024 / payload {args.payload} B.",
        "",
        f"Forbidden instruction gate: `{'PASS' if not forbidden else 'FAIL'}`.",
        f"`[[unlikely]]` changes normalized push codegen: `{unlikely_changed['push']}`; pop codegen: `{unlikely_changed['pop']}`.",
        "",
        "| Function | Instructions | Loads | Stores | Release | Acquire | Branches | Address-gen |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for name in sorted(functions):
        item = functions[name]
        lines.append(
            f"| {name} | {item['instructions']} | {item['loads']} | {item['stores']} | "
            f"{item['explicit_release_stores']} | {item['explicit_acquire_loads']} | "
            f"{item['branches']} | {item['address_generation']} |"
        )
    if forbidden:
        lines.extend(["", "## Forbidden instruction hits", ""])
        for name, hits in forbidden.items():
            lines.append(f"- `{name}`: `{'; '.join(hits)}`")
    args.markdown_output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    if forbidden:
        raise SystemExit("forbidden synchronization instruction detected")


if __name__ == "__main__":
    main()
