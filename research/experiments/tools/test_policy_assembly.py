#!/usr/bin/env python3
from __future__ import annotations

from analyze_policy_assembly import parse_assembly, summarize_body


def main() -> None:
    sample = """
        .text
        .globl vq_push_p16
vq_push_p16:                            # @vq_push_p16
        movq %rsi, %rax                  # clang-style inline comment
        retq
        .size vq_push_p16, .-vq_push_p16

        .globl raw_seq_push_p16
raw_seq_push_p16:
        ldar x8, [x0]
        stlr x9, [x1]
        ret
        .size raw_seq_push_p16, .-raw_seq_push_p16

        .set typed_seq_push_p16, raw_seq_push_p16
    """

    bodies, aliases = parse_assembly(sample)
    assert "vq_push_p16" in bodies
    assert "raw_seq_push_p16" in bodies
    assert aliases["typed_seq_push_p16"] == "raw_seq_push_p16"

    x64 = summarize_body(bodies["vq_push_p16"])
    assert x64["instruction_count"] == 2
    assert x64["forbidden_sync_reference"] is False

    arm = summarize_body(bodies["raw_seq_push_p16"])
    assert arm["aarch64_acquire_count"] == 1
    assert arm["aarch64_release_count"] == 1
    assert arm["instruction_count"] == 3
    print("policy assembly parser checks passed")


if __name__ == "__main__":
    main()
