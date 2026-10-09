# API contract

This document records the semantic contract of the production `veriqueue::spsc_queue` surface. It is intentionally narrower than the implementation notes in the memory-model and progress documents.

## Thread ownership

Exactly one producer thread may call producer operations (`try_emplace`, `try_push`) and exactly one consumer thread may call `try_pop`. The producer and consumer may run concurrently. Destruction requires external quiescence: no queue operation may overlap the destructor.

## Element requirements

The element type `T` must be nothrow destructible. This is a class-level requirement because successful `try_pop()` and queue destruction destroy live elements on `noexcept` paths.

`try_pop(T&)` additionally participates only when `T` is nothrow move assignable. Construction performed by `try_emplace` may throw; if construction throws, the producer cursor is not advanced and no element is published.

## Capacity

`capacity()` is an exact compile-time constant. The queue can contain values in the closed range `[0, capacity()]`; all slots are usable.

## Producer operations

A successful producer operation constructs one element and then publishes the advanced tail with release ordering. A failed producer operation returns `false` without modifying queue contents.

## Consumer operations

A successful `try_pop` move-assigns the oldest available element to the supplied output object, destroys the queued object, advances the consumer cursor, and publishes the advanced head with release ordering. A failed `try_pop` returns `false` without modifying queue contents.

## Concurrent observation

`empty()` and `size_approx()` are advisory observations. They load independently published producer and consumer cursors and are not linearizable snapshots.

`empty()` may therefore become stale immediately and must not be used as a synchronization predicate for a subsequent push or pop.

`size_approx()` guarantees the physical range:

```text
0 <= size_approx() <= capacity()
```

Under concurrent mutation, it may combine cursor values observed at different logical instants and therefore does not promise that the returned value was the exact queue size at any instant. The bound is preserved even when the logical counters wrap.

## Progress scope

The wait-free claim applies to the queue synchronization/control path described in `PROGRESS_GUARANTEE.md`. It does not make arbitrary user-provided constructors, assignment operators, destructors, scheduler behavior, or hardware execution latency wait-free.

## Evidence scope

Tests, sanitizers, model checking, linearizability checking, fuzzing, and mutation testing are complementary evidence layers. None is represented as a universal proof of the complete C++ implementation.
