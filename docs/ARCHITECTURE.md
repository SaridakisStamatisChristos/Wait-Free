# Architecture

## Objective

The production artifact is a bounded SPSC ring. The surrounding artifact is a verification laboratory designed to attack algorithmic semantics, C++ memory ordering, object lifetime, progress claims, and performance claims independently.

## Layout

The queue has three conceptual regions:

1. **producer control region**: producer-private `local_tail`, producer-private `cached_head`, shared atomic `published_tail`;
2. **consumer control region**: consumer-private `local_head`, consumer-private `cached_tail`, shared atomic `published_head`;
3. **raw slot storage**: `Capacity` instances of `detail::slot<T>`.

The two control regions are independently aligned to the configured destructive-interference size (64 bytes by default). `VERIQUEUE_DISABLE_PADDING` is an experiment-only build switch used to measure the cost/benefit rather than assume it.

## Cursor ownership

Only the producer writes `local_tail` and `cached_head`. Only the consumer writes `local_head` and `cached_tail`. Cross-thread communication occurs only through `published_tail` and `published_head`.

## Cached remote cursors

The producer normally checks capacity using `cached_head`. Only when the cache says "full" does it acquire-load `published_head`. The consumer mirrors that behavior for `cached_tail`. This removes a cross-core shared-line read from the common successful path when the queue is neither full nor empty.

## Slot lifetime

A slot transitions through:

`raw storage -> live T -> raw storage`

Producer constructs with `std::construct_at`; consumer obtains a live pointer with `std::launder`, move-assigns to the caller, and ends lifetime with `std::destroy_at`. The queue destructor destroys every still-live element after external quiescence.

## Indexing and wraparound

Slot selection is `logical_index & (Capacity - 1)`. Capacity is constrained to a power of two.

Logical occupancy is unsigned modular subtraction `tail - head`. Correctness depends on maintaining a bounded live distance of at most `Capacity` and constraining `Capacity <= max(Index)/2`. The verifier deliberately instantiates `Index=uint8_t` and `Index=uint16_t` so counter rollover is ordinary test behavior rather than a theoretical corner.

## Non-goals

The v1 queue does not attempt MPSC/MPMC, dynamic resizing, blocking waits, cross-process operation, allocator customization, or reclamation schemes. Those would materially change both the algorithm and the progress proof.
