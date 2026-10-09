# Mutation campaign summary

Baseline oracle: **PASS**
Overall: **32/32 killed (100.0%)**

## Fault-class coverage

| Fault class | Killed | Total |
|---|---:|---:|
| bulk-capacity | 1 | 1 |
| bulk-publication-order | 2 | 2 |
| bulk-publication-value | 2 | 2 |
| bulk-slot-mapping | 1 | 1 |
| cache-refresh | 3 | 3 |
| capacity | 3 | 3 |
| consume-publication-order | 1 | 1 |
| consume-slot-mapping | 1 | 1 |
| cursor | 4 | 4 |
| data-integrity | 1 | 1 |
| memory-order | 4 | 4 |
| publication-order | 2 | 2 |
| publication-value | 4 | 4 |
| slot-mapping | 3 | 3 |

## Mutants

| ID | Fault class | Mutation | Result |
|---|---|---|---|
| MUT-01 | memory-order | tail-release-to-relaxed | KILLED |
| MUT-02 | memory-order | tail-acquire-to-relaxed | KILLED |
| MUT-03 | memory-order | head-release-to-relaxed | KILLED |
| MUT-04 | memory-order | head-acquire-to-relaxed | KILLED |
| MUT-05 | publication-order | publish-tail-before-slot-write | KILLED |
| MUT-06 | publication-order | publish-head-before-slot-read | KILLED |
| MUT-07 | slot-mapping | collapse-slot-mask-to-zero | KILLED |
| MUT-08 | capacity | full-limit-capacity-plus-one | KILLED |
| MUT-09 | capacity | full-limit-capacity-minus-one | KILLED |
| MUT-10 | cache-refresh | never-refresh-producer-cached-head | KILLED |
| MUT-11 | cache-refresh | never-refresh-consumer-cached-tail | KILLED |
| MUT-12 | cursor | producer-cursor-double-increment | KILLED |
| MUT-13 | cursor | consumer-cursor-double-increment | KILLED |
| MUT-14 | slot-mapping | producer-slot-plus-one | KILLED |
| MUT-15 | slot-mapping | consumer-slot-plus-one | KILLED |
| MUT-16 | publication-value | publish-tail-one-ahead | KILLED |
| MUT-17 | publication-value | publish-head-one-ahead | KILLED |
| MUT-18 | data-integrity | corrupt-stored-value | KILLED |
| MUT-19 | cursor | suppress-producer-local-tail-update | KILLED |
| MUT-20 | cursor | suppress-consumer-local-head-update | KILLED |
| MUT-21 | cache-refresh | invert-consumer-refresh-predicate | KILLED |
| MUT-22 | capacity | omit-full-check | KILLED |
| MUT-23 | publication-value | publish-old-head | KILLED |
| MUT-24 | publication-value | publish-old-tail | KILLED |
| MUT-25 | bulk-publication-order | bulk-publish-tail-before-slot-writes | KILLED |
| MUT-26 | bulk-publication-order | bulk-publish-head-before-slot-reads | KILLED |
| MUT-27 | bulk-publication-value | bulk-publish-only-first-tail-advance | KILLED |
| MUT-28 | bulk-publication-value | bulk-publish-only-first-head-advance | KILLED |
| MUT-29 | bulk-capacity | bulk-overaccept-capacity-by-one | KILLED |
| MUT-30 | bulk-slot-mapping | bulk-second-element-wrong-slot | KILLED |
| MUT-31 | consume-publication-order | consume-publish-head-before-read | KILLED |
| MUT-32 | consume-slot-mapping | consume-read-next-slot | KILLED |
