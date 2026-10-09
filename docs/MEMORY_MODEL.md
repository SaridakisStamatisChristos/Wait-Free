# C++ memory-model rationale

## Inter-thread operations

The production synchronization protocol has exactly four kinds of inter-thread atomic operation:

| Operation | Order |
|---|---|
| producer publishes tail | release |
| consumer observes tail | acquire |
| consumer publishes head | release |
| producer observes head | acquire |

There are no explicit fences and no `seq_cst` operations in the queue.

On ARM64, the producer and consumer also read their own published atomic cursor with `memory_order_relaxed`; each cursor still has exactly one writer. Those owner-side relaxed loads are local bookkeeping, not peer synchronization. On non-ARM64 production builds VeriQueue retains the separate owner-private cursor used by the original implementation.

## Publication of a constructed element

Producer order is architecture-independent at the synchronization boundary:

1. obtain the producer-owned tail (from the private cursor on non-ARM64, or a relaxed load of `published_tail` on ARM64);
2. choose a slot known not to be live;
3. construct `T` into raw storage;
4. advance the producer-owned tail value;
5. release-store `published_tail`.

Consumer only accesses a newly enqueued object after an acquire-load of `published_tail` has observed a value proving that logical position published. The release/acquire pair establishes happens-before from construction writes to subsequent consumer reads of the object.

The ARM64 owner-side relaxed load does not weaken publication: the producer is the sole writer of tail, and the slot construction still precedes the same release publication observed by the peer.

## Publication of completed consumption

Consumer order is symmetric:

1. obtain the consumer-owned head (from the private cursor on non-ARM64, or a relaxed load of `published_head` on ARM64);
2. access/move the live `T`;
3. destroy it;
4. advance the consumer-owned head value;
5. release-store `published_head`.

Producer only reconstructs that physical slot after an acquire-load of `published_head` proves that the old logical position has been consumed. Destruction therefore happens-before subsequent reuse of the same storage.

## Bulk operations

Bulk producer and consumer operations preserve the same ordering but publish only once after the bounded batch. The ARM64 owner cursor is loaded relaxed once at batch entry and release-published once after the batch. This preserves the existing all-at-once visibility/reuse semantics and explains why eliminating a duplicate owner-private store is expected to matter less for bulk throughput than for scalar throughput; that performance expectation must still be established by measurement.

## Destruction and external quiescence

Queue destruction requires external quiescence: no producer or consumer operation may overlap the destructor. Non-ARM64 builds read their owner-private cursors directly. ARM64 builds obtain the final owner cursors with relaxed atomic loads. These relaxed loads are sufficient under the destruction contract because the destructor is not being used as an inter-thread synchronization edge; external quiescence must already have been established by the caller.

## Linearization candidates

A successful push becomes visible to the consumer at publication of the new tail. A successful pop relinquishes the slot to the producer at publication of the new head. The caller-visible operation intervals include local object work around those publications, so the independent Porcupine checker validates histories rather than relying only on prose.

## Why relaxed is not used for peer publication/observation

Weakening the tail publication/peer-observation pair can sever synchronization between construction and consumer access. Weakening the head publication/peer-observation pair can sever synchronization between destruction and producer reuse. x86 testing alone is insufficient evidence because its hardware ordering can hide defects that are permitted by the C++ model on weaker architectures.

The mutation catalogue therefore includes the peer acquire/release weakenings, and the Relacy lane explores weak-memory schedules. The ARM64 single-owner protocol additionally has a dedicated Relacy model in which owner cursor reads are relaxed while peer reads remain acquire and publications remain release.

## Advisory observers

`empty()` and `size_approx()` acquire-load both published cursors. They are observations, not transactional snapshots: producer and consumer can advance between the two loads. Their results are therefore advisory under concurrency.
