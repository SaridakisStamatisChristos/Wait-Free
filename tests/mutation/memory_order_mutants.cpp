// Curated mutation catalogue.
//
// These mutations are intentionally NOT linked into production. The campaign driver applies
// each mutation to the Relacy mirror / production source in an isolated worktree and requires
// at least one verification layer to reject it. See docs/VERIFIER_EFFECTIVENESS.md.
//
// MUT-01 tail store release -> relaxed
// MUT-02 consumer tail load acquire -> relaxed
// MUT-03 head store release -> relaxed
// MUT-04 producer head load acquire -> relaxed
// MUT-05 publish tail before construction
// MUT-06 publish head before destruction
// MUT-07 incorrect ring mask
// MUT-08 off-by-one full condition

static_assert(true);
