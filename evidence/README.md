# Evidence directory

CI and reproducibility workflows write raw verification and benchmark outputs under these subdirectories before uploading them as GitHub Actions artifacts.

Actions artifacts are transient transport. Durable multi-run qualification evidence is committed under:

```text
evidence/campaigns/YYYY-MM-DD/<source-sha>/
```

Each frozen campaign contains GitHub run/artifact provenance, extracted payloads, a manifest, and a complete SHA-256 ledger. Repeated independent runs are preserved separately under `run-<run-id>` directories so they cannot overwrite one another.

Dated one-off benchmark captures and historical validation snapshots that are retained for research provenance live under [`research/`](../research/) rather than being mixed with the active evidence pipeline.

See [`docs/EVIDENCE_FREEZING.md`](../docs/EVIDENCE_FREEZING.md) for the fail-closed freeze/verification protocol and [`docs/EVIDENCE_LEDGER.md`](../docs/EVIDENCE_LEDGER.md) for the claim ledger.

Shared GitHub-hosted benchmark results are evidence artifacts, **not hard performance regression gates** and not universal performance claims.
