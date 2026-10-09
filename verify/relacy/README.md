# Relacy weak-memory verification

The Relacy lane is optional in ordinary local builds and enabled in CI with `-DVERIQUEUE_BUILD_RELACY=ON`. The dependency is pinned to a commit, not a moving branch.

The model mirrors the production cursor/cached-index algorithm while replacing object lifetime with Relacy-tracked slot variables. Capacity 1 and 2 are explored because small state spaces expose the synchronization protocol while remaining tractable.

The production queue itself is also covered by native sanitizers and model tests. Relacy does not prove arbitrary C++ object-lifetime behavior; it attacks the weak-memory synchronization protocol.
