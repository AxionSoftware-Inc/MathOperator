# Layer 16 — Principled Quotient Search

Date: 2026-08-20
Status: implemented as a parallel streaming search path; final gates passed.

## Scope and boundary

Layer 16 adds `opforge::search::QuotientSearchEngine` in:

- `include/opforge/search/quotient.hpp`;
- `src/search/quotient.cpp`; and
- `tests/quotient_search_tests.cpp`.

The legacy composition search, open campaign, blind rediscovery harness, and
numerical executor were not replaced or used by this path. Layer 17 backward
solving, LLM reasoning, numerical discovery, arbitrary linear-combination
enumeration, and proof-backend selection were not implemented.

## Architecture

The run contract is:

```text
SearchScope Σ, Γ, R, G, D, E, B
  → streamed Construction
  → depth/grammar scope checks
  → Layer-15 type and side-condition checks
  → canonical structural key
  → exact/canonical class lookup
  → trusted equality/equivalence or explicit symmetry lookup
  → known-consequence check
  → retained representative or ledger entry
```

`SearchScope` records theory ID/version, grammar ID and allowed construction
kinds, depth and resource bounds, equivalence-theory ID, context/regime, and a
deterministic seed. A result cannot claim global completeness; its strongest
completion label is `EXHAUSTED_RELATIVE_SPACE`.

`ConstructionSource` is an incremental callback. The engine retains
representatives, class summaries, maps, counts, and optional audit members; it
does not retain the raw input stream. Full member certificates can be enabled
for a small audit run. Million-scale mode retains only class summaries and a
deterministic ledger digest.

## Reduction policy

| Reason | Category | Current rule |
|---|---|---|
| `TYPE_INVALID` | lossless | Layer-15 typing proves the construction impossible |
| `REGIME_INCOMPATIBLE` | lossless | context/side-condition incompatibility is explicit |
| `EXACT_DUPLICATE` | lossless | deterministic construction identity repeats |
| `CANONICAL_DUPLICATE` | lossless | typed structural key repeats |
| `PROVEN_EQUIVALENT` | lossless | trusted Equality/Equivalence fact matches both terms |
| `SYMMETRY_EQUIVALENT` | lossless | explicit domain/context/regime-scoped symmetry certificate |
| `KNOWN_CONSEQUENCE` | lossless | supplied proposition is already a trusted Theory fact |
| `DEPTH_LIMIT` | lossy | outside declared depth scope |
| `FRONTIER_BUDGET` | lossy | new class exceeds retained-frontier budget |
| `RESOURCE_LIMIT` | lossy | resource/time bound stops the stream |
| `UNKNOWN` | unresolved | type/regime information is insufficient; candidate remains available |
| `UNSUPPORTED` | unresolved | requested construction/rule is outside the supported fragment |

No dominance rule is currently implemented. No heuristic score is used to
delete a candidate. Unknown and unsupported material is not silently converted
to a mathematical rejection or an exhaustive result.

Partially structured Layer-15 facts are never loaded into the equivalence
alias index. Analogy, correspondence, approximation, name matches, relation
endpoints, and prose are not quotient sources.

## Equivalence classes and ledger

Each class has a deterministic ID derived from scope ID and structural key, a
representative, type status/type, member count, member digest, and optional
member certificate records. A merge certificate records the source
construction, rule/fact ID, context, regime, reason, and explanation.

The pruning ledger counts every terminal construction category. In audit mode
it retains machine-readable records; in streaming mode it emits records to an
optional sink and retains only counts plus a deterministic digest. This is the
explicit memory tradeoff: complete per-member replay requires audit storage,
while large runs retain replayable aggregate accounting and class summaries.

## Finite completeness and soundness

The finite reference grammar is an externally supplied, independently
enumerable four-item stream. Its grammar is deliberately small and contains:

1. `atom(A)`;
2. the same `atom(A)` construction identity again;
3. `atom(B)`, with a trusted `A = B` fact; and
4. a composition whose domain/codomain types are incompatible.

The exhaustive run has no candidate, frontier, depth, or resource limit. Its
ledger is:

```text
status: EXHAUSTED_RELATIVE_SPACE
raw: 4
retained classes: 1
RETAINED_REPRESENTATIVE: 1
EXACT_DUPLICATE: 1
PROVEN_EQUIVALENT: 1
TYPE_INVALID: 1
```

The conservation equation is explicit:

```text
raw = retained representatives + lossless terminals + lossy terminals + unresolved terminals
4 = 1 + (EXACT_DUPLICATE 1 + PROVEN_EQUIVALENT 1 + TYPE_INVALID 1) + 0 + 0
```

The ledger total is therefore `4`; no raw construction disappears. Equivalently,
subtracting the non-representative categories leaves one retained
representative: `4 - 1 - 1 - 1 = 1`.

The same externally enumerated stream with an intentionally restrictive
two-candidate budget returns:

```text
status: BUDGET_ENDED
raw: 2
retained representatives: 1
EXACT_DUPLICATE: 1
unconsumed source constructions: 2
```

It is not reported as exhausted. Its separate conservation equation is
`2 = 1 retained representative + 1 exact duplicate`, and its ledger total is
`2`. The tests verify accounting for both finite runs and verify that
`relative_complete()` is false for the budgeted run.

Soundness tests cover:

- same display name with different IDs does not merge;
- analogy, approximation, and correspondence do not merge;
- incompatible-regime equality does not merge;
- unknown side-condition equality does not merge;
- compatible trusted equality does merge;
- explicit valid symmetry does merge; and
- indexed `d_k` and `d_(k+1)` remain distinct while valid composition types and
  `d_k ∘ d_k` is rejected.

## Million-scale streamed stress benchmark

The deterministic synthetic source emits 1,000,000 constructions. It uses
repeated exact/canonical forms, trusted equalities, explicit symmetry,
known-consequence propositions, typed-invalid compositions, and unknown terms.
It does not allocate one million retained candidate objects.

Measured result:

```text
raw constructions: 1,000,000
type valid/invalid/unknown: 997,000 / 1,000 / 2,000
lossless reductions: 999,995
lossy reductions: 0
retained classes: 5
peak retained frontier: 5
ledger records retained in stress mode: 0
termination: INCOMPLETE_UNKNOWN
runtime: 26,475.828 ms in the final clean acceptance build
```

The exact ledger is:

| Ledger reason | Count | Treatment |
|---|---:|---|
| `RETAINED_REPRESENTATIVE` | 3 | retained known-type representatives |
| `TYPE_INVALID` | 1,000 | lossless typed rejection |
| `EXACT_DUPLICATE` | 999 | lossless repeated deterministic identity |
| `CANONICAL_DUPLICATE` | 994,996 | lossless repeated typed structural key |
| `PROVEN_EQUIVALENT` | 1,000 | trusted equality alias |
| `SYMMETRY_EQUIVALENT` | 1,000 | explicit certified symmetry |
| `KNOWN_CONSEQUENCE` | 1,000 | trusted proposition already in theory |
| `UNKNOWN` | 2 | unresolved retained representatives |

The two `UNKNOWN` entries are the first representatives of two unresolved
classes: 1,000 operator-reference candidates whose operator is not declared,
and 1,000 literals with an unknown declared type. Thus `type_unknown=2,000`,
but only two constructions create unresolved classes; the other 1,998 are
canonical duplicates of those same unknown structures. They were neither
rejected nor inserted into the trusted equivalence alias index, and they were
not counted as `PROVEN_EQUIVALENT`.

The stress conservation equation is:

```text
1,000,000 = 3 retained representatives
             + 999,995 lossless terminals
             + 0 lossy terminals
             + 2 unresolved UNKNOWN terminals
```

`INCOMPLETE_UNKNOWN` is therefore correct: the source is exhausted, but two
unknown representatives remain unresolved. It would be scientifically wrong to
relabel this run `EXHAUSTED_RELATIVE_SPACE`.

## Atlas scaling comparison

The new path enumerates an explicit ordered-pair grammar and reports its own
raw/type/class stages. These are not cosmetically substituted for the legacy
scaling counts.

Legacy path, from the final baseline rerun:

| Operators | Compatible edges / raw candidates | Retained candidates/classes | Pruned | Rejected |
|---:|---:|---:|---:|---:|
| 12 | 25 / 25 | 25 | 0 | 3 |
| 50 | 150 / 150 | 150 | 86 | 0 |
| 98 | 391 / 391 | 391 | 325 | 2 |

Layer-16 quotient path, from the same Atlas snapshot:

| Operators | Constructions | Type-invalid | Type-unknown | Exact merges | Canonical merges | Proven-equivalent merges | Symmetry merges | Known consequences | Classes | Lossy | Unresolved/unknown | Status | Peak frontier | Runtime ms |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|
| 12 | 144 | 86 | 0 | 0 | 0 | 0 | 0 | 0 | 58 | 0 | 0 | `EXHAUSTED_RELATIVE_SPACE` | 58 | 14.178 |
| 50 | 2,500 | 2,283 | 0 | 0 | 0 | 0 | 0 | 0 | 217 | 0 | 0 | `EXHAUSTED_RELATIVE_SPACE` | 217 | 187.060 |
| 98 | 9,604 | 9,116 | 0 | 0 | 0 | 0 | 0 | 0 | 488 | 0 | 0 | `EXHAUSTED_RELATIVE_SPACE` | 488 | 699.433 |

For all three quotient runs, type-valid counts equal retained classes and the
only lossless terminal reason is `TYPE_INVALID`; all other quotient reason
counts are exactly zero. The ledger total equals the construction count in
each run.

The legacy path’s 25/150/391 values use its Atlas-specific structural
compatibility checks; the Layer-16 path currently has the Layer-15 typed
domain/codomain fragment and therefore intentionally reports a different
stage. Legacy `pruned` is a bounded multi-stage frontier count, while quotient
`lossy` counts only constructions discarded by an explicitly declared lossy
scope limit. The paths therefore cannot be compared as a single recall or
speedup metric.

## Partial-fact quotient audit

The Atlas migration reports 186 facts: 6 fully structured equalities and 180
partially structured facts (61 semantic identities plus 119 relations). The
quotient alias index accepts only trusted Equality/Equivalence judgments and
validated explicit symmetry certificates. Generic relation, analogy,
approximation, correspondence, names, prose, and relation metadata are not
quotient sources.

The regression test removes all 180 partial facts by migration-record
occurrence, reruns the same operator construction stream, and compares raw,
typing, lossless, unresolved, class, and per-reason ledger counts. The full
migration and the 180-fact-removed theory are identical on all of those
quotient metrics. This confirms that Layer 16 performs no unsound quotient
using the partial Layer-15 facts.

## Frozen scientific baseline

The legacy target-blind baseline remains unchanged after the Layer-16 source
was added:

```text
structural recovery: 1/6
partial: 4/6
miss: 1/6
false positives: 0
negative controls: 3/3
leakage: 0
numerical discovery: 0
opaque-ID: passed
deterministic reruns: passed
```

The old scaling and open-search metrics remain separately measurable. Layer 16
does not improve their scores or reinterpret their counts.

## Final gate rerun

The final legacy rerun after Layer-16 integration reproduced the frozen
results. The blind command reported structural `1/6`, partial `4/6`, miss
`1/6`, false positives `0`, negative controls `3/3`, leakage `0`, and
numerical discovery `0`; its candidate-ID digest and deterministic result
digest remained unchanged. The separate legacy scaling command reported
compatible/raw values `25`, `150`, and `391` for the 12-, 50-, and 98-operator
runs. The open campaign reported `generated=62`, `pruned=650`, `rejected=4`,
`serious=0`, and `numerical_experiments=0`.

These numbers are intentionally not combined with the Layer-16 numbers. In
particular, `generated_candidates=62` is a bounded multi-stage open-search
output, while `391` is the legacy direct structural compatibility count and
Layer 16's `9,604` is its ordered-pair raw construction count.

Verification completed:

- clean configure/build: `/tmp/opforge-layer16-accept-clean-v2`;
- clean CTest: `3/3` passed;
- ASan/UBSan configure/build: `/tmp/opforge-layer16-accept-asan-v2`;
- ASan/UBSan CTest: `3/3` passed; and
- `git diff --check`: passed.

The quotient CLI benchmark and the quotient test both exercise the million-
construction stream. No production discovery behavior was changed, and no
Layer-17 code was added.

## Limitations and deferred decisions

- The current equivalence source is trusted pairwise Equality/Equivalence, not
  a theorem prover or general congruence/e-graph engine.
- Symmetry is an explicit certified source-to-target action, not a universal
  automorphism detector.
- The typed Layer-15 migration does not yet encode all Atlas regularity,
  geometry, and structure metadata in the quotient type judgment.
- No lossless dominance/subsumption rule is enabled.
- Resource-time limits are inherently not a completeness claim; deterministic
  tests use no wall-clock limit.
- The legacy discovery path is not automatically replaced by the quotient path.

## Completion decision

Layer 16’s representation, ledger, finite completeness distinction, soundness
controls, indexed behavior, streaming stress path, partial-fact audit, and
baseline-preservation requirements are implemented. The clean build,
sanitizer, final benchmark rerun, finite/budget/stress accounting, and
documentation gates all passed.

**LAYER 16 DoD SATISFIED — LAYER 17 SAFE TO BEGIN.** Layer 17 was not started
in this run.
