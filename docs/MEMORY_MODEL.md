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

## Publication of a constructed element

Producer order:

1. choose a slot known not to be live;
2. construct `T` into raw storage;
3. update producer-private tail;
4. release-store `published_tail`.

Consumer only accesses a newly enqueued object after an acquire-load of `published_tail` has observed a value proving that logical position published. The release/acquire pair establishes happens-before from construction writes to subsequent consumer reads of the object.

## Publication of completed consumption

Consumer order:

1. access/move the live `T`;
2. destroy it;
3. update consumer-private head;
4. release-store `published_head`.

Producer only reconstructs that physical slot after an acquire-load of `published_head` proves that the old logical position has been consumed. Destruction therefore happens-before subsequent reuse of the same storage.

## Linearization candidates

A successful push becomes visible to the consumer at publication of the new tail. A successful pop relinquishes the slot to the producer at publication of the new head. The caller-visible operation intervals include local object work around those publications, so the independent Porcupine checker validates histories rather than relying only on prose.

## Why relaxed is not used for publication

Weakening the tail publication/observation pair can sever synchronization between construction and consumer access. Weakening the head publication/observation pair can sever synchronization between destruction and producer reuse. x86 testing alone is insufficient evidence because its hardware ordering can hide defects that are permitted by the C++ model on weaker architectures.

The mutation catalogue therefore includes all four acquire/release weakenings and the Relacy lane exists specifically to explore weak-memory schedules.

## Advisory observers

`empty()` and `size_approx()` acquire-load both published cursors. They are observations, not transactional snapshots: producer and consumer can advance between the two loads. Their results are therefore advisory under concurrency.
