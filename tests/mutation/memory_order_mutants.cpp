// Curated mutation catalogue.
//
// These mutations are intentionally NOT linked into production. The campaign driver selects
// each mutation in the Relacy mirror and requires the baseline model to pass before any kill is
// counted. See docs/VERIFIER_EFFECTIVENESS.md.
//
// Memory ordering
// MUT-01 tail store release -> relaxed
// MUT-02 consumer tail load acquire -> relaxed
// MUT-03 head store release -> relaxed
// MUT-04 producer head load acquire -> relaxed
//
// Publication ordering
// MUT-05 publish tail before slot write
// MUT-06 publish head before slot read
//
// Slot mapping / capacity
// MUT-07 collapse ring mask to slot zero
// MUT-08 full limit = Capacity + 1
// MUT-09 full limit = Capacity - 1
// MUT-14 producer selects next physical slot
// MUT-15 consumer selects next physical slot
// MUT-22 omit full check
//
// Remote-cache refresh
// MUT-10 never refresh producer cached head
// MUT-11 never refresh consumer cached tail
// MUT-21 invert consumer refresh predicate
//
// Owner-local cursor integrity
// MUT-12 producer cursor double increment
// MUT-13 consumer cursor double increment
// MUT-19 suppress producer local-tail update
// MUT-20 suppress consumer local-head update
//
// Published cursor integrity
// MUT-16 publish tail one ahead
// MUT-17 publish head one ahead
// MUT-23 publish old head
// MUT-24 publish old tail
//
// Payload integrity
// MUT-18 corrupt stored value

static_assert(true);
