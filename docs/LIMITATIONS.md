# Limitations

- SPSC only: exactly one producer thread and one consumer thread.
- Fixed power-of-two capacity known at compile time.
- No blocking/wait API.
- No MPSC/MPMC semantics.
- No dynamic resizing or allocator abstraction.
- Queue destruction requires external quiescence.
- `try_pop` requires nothrow move assignment.
- `empty()` and `size_approx()` are advisory concurrent observations.
- Wait-free claim applies to queue synchronization/control logic, not arbitrary user type code.
- Sanitizers, stress, fuzzing, Relacy bounded models, and sampled linearizability histories each cover limited execution sets; none alone proves the full implementation.
- Benchmark results are machine/topology/workload-specific and must not be generalized beyond their evidence.
