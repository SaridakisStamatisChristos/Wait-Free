# Frozen validation evidence

This snapshot records the validation state for code commit `d051ba8c362259e46ada68c2fb9b25e60725eb26` on 2026-10-09. The following GitHub Actions runs completed successfully:

- CI `37925976300`: format, GCC, Clang, clang-tidy, benchmark/baseline build, assembly audit.
- Sanitizers `37925976467`: ASan+UBSan and TSan lanes.
- Model Check `37925976310`: Relacy correct model, 64 Porcupine histories, and 8/8 mutation campaign.
- Fuzz Smoke `37925976391`: libFuzzer smoke campaign.

Preserved artifact identifiers and SHA-256 digests:

- Relacy: `11613792831`, `sha256:9bffbdf8351640527334bc24433dc8b31bd8bf804da0db43cb22de0f82e22b04`.
- Mutation: `11613968321`, `sha256:81d9de510c8cccc8962c95013d8af5e24646922b0691474d7cfccc8889f7a048`.
- Linearizability: `11614266051`, `sha256:ee98bd3a35c18a76421c5a2ad82db28afd67073dc9c7382e6b4c5bba4f05d9ee`.
- ASan+UBSan: `11613633175`, `sha256:c85aec47a29e72d9a65c25de04204995a16f70c4cf366b1a5583d3b63290297c`.
- TSan: `11614500625`, `sha256:58a32fc3d645e1326f3395084fe22be0791f81c8faa53146ba24f40d96149f30`.
- Fuzz: `11614332252`, `sha256:5d4d57b013aa651c79a45172c463b278a4c9e46e2d03633e369639bc76690fe5`.

These are correctness/verification/build results. They do **not** establish a universal performance ranking. Comparative benchmark claims require a named machine/topology campaign and raw benchmark data.
