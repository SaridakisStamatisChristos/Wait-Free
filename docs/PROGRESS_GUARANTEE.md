# Progress guarantee

## Precise claim

For valid single-producer/single-consumer use, the queue's synchronization/control path for non-blocking enqueue and dequeue operations is **wait-free**: completion of that control path does not contain a retry loop whose termination depends on another thread.

Each push performs a local occupancy check, at most one acquire refresh of the consumer head, optional object construction, and one release publication. Each pop mirrors this structure with at most one acquire refresh of producer tail and one release publication.

## Type-dependent caveat

The queue cannot make arbitrary user code wait-free. `try_emplace` executes a user-supplied constructor. `try_push` executes a copy/move constructor. `try_pop` executes a move assignment and destructor. Their execution time, blocking behavior, and internal synchronization are properties of `T`.

`try_pop` therefore requires nothrow move assignment so a throwing assignment cannot leave ambiguous partial-consumption semantics.

## Not a global liveness promise

A producer may receive `false` forever if the consumer never frees space; a consumer may receive `false` forever if the producer never publishes an element. Wait-freedom here concerns termination of each non-blocking attempt, not guaranteed success of the requested data transfer.
