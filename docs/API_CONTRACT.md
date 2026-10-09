# API contract

This document records the semantic contract of the production `veriqueue::spsc_queue` surface. It is intentionally narrower than the implementation notes in the memory-model and progress documents.

## Thread ownership

Exactly one producer thread may call producer operations (`try_emplace`, `try_push`, `try_push_bulk`) and exactly one consumer thread may call consumer operations (`try_pop`, `try_pop_bulk`, `try_consume`). Producer and consumer operations may run concurrently. Destruction requires external quiescence: no queue operation may overlap the destructor.

## Element requirements

The element type `T` must be nothrow destructible because successful consuming paths and queue destruction destroy live elements on `noexcept` paths.

`try_pop(T&)` and `try_pop_bulk(std::span<T>)` participate only when `T` is nothrow move assignable.

`try_push_bulk(std::span<const T>)` participates only when `T` is nothrow copy constructible. A batch is constructed before its tail is published, so forbidding throwing copy construction prevents a partially constructed unpublished batch from becoming an ambiguous lifetime state.

`try_consume(F&&)` participates only when `F` is nothrow invocable with `T&`. The callback executes while the oldest element is live and before the consumer cursor advances.

Construction performed by scalar `try_emplace`/`try_push` may throw; if construction throws, the producer cursor is not advanced and no element is published.

## Capacity

`capacity()` is an exact compile-time constant. The queue can contain values in the closed range `[0, capacity()]`; all slots are usable.

## Scalar producer operations

A successful scalar producer operation constructs one element and then publishes the advanced tail with release ordering. A failed scalar producer operation returns `false` without modifying queue contents.

## Bulk producer operation

`try_push_bulk(std::span<const T>)` appends the longest prefix of the supplied span that fits according to the producer's observed head. It returns the exact number appended.

Selected elements are constructed in FIFO order and the final advanced tail is published exactly once with release ordering. A return value of zero leaves queue contents unchanged. Consumers cannot observe an intermediate prefix of that same call before its final tail publication.

## Scalar consumer operation

A successful `try_pop` move-assigns the oldest available element to the supplied output object, destroys the queued object, advances the consumer cursor, and publishes the advanced head with release ordering. A failed `try_pop` returns `false` without modifying queue contents.

## Bulk consumer operation

`try_pop_bulk(std::span<T>)` removes the longest prefix that fits in the supplied output span from the elements visible to the consumer. It returns the exact number removed.

Selected elements are move-assigned and destroyed in FIFO order. The final advanced head is published exactly once with release ordering, so the producer cannot reuse an intermediate subset of slots retired by the same bulk call before that final publication.

## Callback consumer operation

`try_consume(F&&)` invokes the supplied nothrow callback exactly once with a mutable reference to the oldest available element. On success the callback completes, the element is destroyed, the head advances, and the advanced head is published with release ordering. If the queue is empty it returns `false` without invoking the callback.

The callback may inspect, mutate, or move from the element. References or pointers to the queued object must not escape the callback because the object is destroyed immediately after callback completion.

## Concurrent observation

`empty()` and `size_approx()` are advisory observations. They load independently published producer and consumer cursors and are not linearizable snapshots.

`empty()` may become stale immediately and must not be used as a synchronization predicate for a subsequent producer or consumer operation.

`size_approx()` guarantees the physical range:

```text
0 <= size_approx() <= capacity()
```

Under concurrent mutation it may combine cursor values observed at different logical instants and therefore does not promise that the returned value was the exact queue size at any instant. The bound is preserved even when logical counters wrap.

## Progress scope

The wait-free claim applies to the queue synchronization/control path described in `PROGRESS_GUARANTEE.md`. Scalar queue operations execute a bounded number of queue-control steps. Bulk operations execute a number of element operations bounded by both the caller-provided span and `capacity()` and do not retry on peer progress.

The claim does not make arbitrary user-provided constructors, assignment operators, callbacks, destructors, scheduler behavior, or hardware execution latency wait-free.

## Evidence scope

Tests, sanitizers, model checking, linearizability checking, fuzzing, mutation testing, and benchmarks are complementary evidence layers. None is represented as a universal proof of the complete C++ implementation.
