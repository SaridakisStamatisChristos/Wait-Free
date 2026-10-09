# GitHub evidence freezing

VeriQueue treats GitHub Actions artifacts as transient transport, not permanent evidence. The evidence freezer converts selected successful Actions runs into a repository-backed campaign that can be reviewed, hashed, and preserved in Git history.

## Scope

The freezer accepts one or more GitHub Actions run IDs. Every selected run must:

- belong to this repository;
- have conclusion `success`;
- share exactly one source commit;
- still have unexpired downloadable artifacts.

The freezer does not infer or repair missing benchmark results. If provenance or integrity checks fail, the freeze fails closed.

## Permanent layout

A campaign is stored under:

```text
evidence/campaigns/YYYY-MM-DD/<source-sha>/
```

Platform artifacts retain a stable platform directory and are nested by source run so repeated independent runs do not collide:

```text
linux-x64-gcc/run-<run-id>/...
linux-x64-clang/run-<run-id>/...
linux-arm64-gcc/run-<run-id>/...
linux-arm64-clang/run-<run-id>/...
verification/relacy/run-<run-id>/...
verification/mutation/run-<run-id>/...
verification/linearizability/run-<run-id>/...
```

Unknown artifact types are preserved under `artifacts/<artifact-name>/run-<run-id>/` rather than discarded.

## Manifest

`manifest.json` is generated from GitHub API metadata and extracted artifact payloads. It contains:

- repository;
- campaign date;
- VeriQueue/source commit;
- workflow source commit identifier used for the selected runs;
- source workflow run IDs and run metadata;
- artifact IDs, names, GitHub digests, downloaded archive SHA-256 values, and destinations;
- per-file source-run/source-artifact provenance and SHA-256 values;
- indexed benchmark context where `raw-compare.jsonl` is present, including declared seeds, capacities, payload sizes, transfer counts, implementations, topology labels, environment records, and comparator revisions;
- pointers to captured run-context, protocol, and compile-command files where they exist.

Environment facts that are not present in source artifacts remain `null`. The freezer never invents runner image, CPU, standard-library, compiler-flag, or comparator metadata.

## Integrity controls

The freezer:

- checks GitHub-provided artifact archive digests when available;
- rejects expired artifacts;
- rejects duplicate destinations;
- rejects ZIP absolute paths, `..` traversal, duplicate members, and symbolic links;
- rejects individual files larger than 95 MiB;
- rejects campaigns above the configured campaign-size ceiling;
- writes a complete `SHA256SUMS.txt` ledger over every committed campaign file except the ledger itself.

`tools/verify_frozen_evidence.py` independently recomputes the ledger, validates manifest structure, checks source-commit consistency, and rejects untracked or tampered files.

## Workflow

After this feature is merged, `Freeze GitHub Evidence` is dispatched from the repository default branch with a comma-separated list of successful source run IDs. The workflow:

1. checks out the default branch;
2. downloads and freezes the selected GitHub-only evidence;
3. independently verifies the resulting campaign;
4. rejects oversized Git objects;
5. creates an `evidence/freeze-<run-id>-<attempt>` branch;
6. commits only the frozen campaign;
7. opens an evidence-only pull request against the default branch.

The workflow requires `actions: read`, `contents: write`, and `pull-requests: write`. Repository settings must permit GitHub Actions to create pull requests; otherwise the branch remains the recoverable output and the workflow reports the PR-creation failure.

## Qualification of the freezer itself

`Evidence Freezer Tests` runs on pull requests and exercises the freezer/verifier without external artifact dependence. The adversarial suite covers:

- platform/artifact destination normalization;
- normal ZIP extraction;
- path-traversal rejection;
- symbolic-link rejection;
- checksum tamper detection.

The production dispatch workflow is intentionally not used as the PR qualification mechanism because GitHub only exposes new `workflow_dispatch` workflows reliably after they exist on the default branch.

## Evidence semantics

A frozen campaign proves what its source artifacts prove—nothing more. Freezing improves durability, provenance, and tamper evidence; it does not convert benchmark observations into universal performance claims or bounded verification into a universal correctness proof.
