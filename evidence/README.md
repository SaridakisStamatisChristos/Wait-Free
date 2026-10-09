# Evidence directory

CI and reproducibility workflows write raw verification/benchmark outputs under these subdirectories before uploading them as GitHub Actions artifacts.

Actions artifacts are transient transport. Durable campaign evidence is committed under:

```text
evidence/campaigns/YYYY-MM-DD/<source-sha>/
```

Each frozen campaign contains GitHub run/artifact provenance, extracted payloads, a manifest, and a complete SHA-256 ledger. Repeated independent runs are preserved separately under `run-<run-id>` directories so they cannot overwrite one another.

See `docs/EVIDENCE_FREEZING.md` for the fail-closed freeze/verification protocol.

Shared GitHub-hosted benchmark results are evidence artifacts, **not hard performance regression gates** and not universal performance claims.
