# Layer 15 — Mathematical Semantic Core

Date: 2026-08-20
Status: implemented; final gates recorded below.
Scope: representation infrastructure only. Layer 16, bidirectional solving,
formal backend selection, quotient search, numerical discovery, and benchmark
optimization were not implemented.

## Implementation summary

Layer 15 adds an independent semantic-core API in:

- `include/opforge/semantic/core.hpp`;
- `src/semantic/core.cpp`;
- `tests/semantic_core_tests.cpp`.

The existing `semantic::ConsequenceClosureEngine`, structural search,
candidate synthesis, numerical executor, and blind benchmark behavior remain
unchanged. The new core is available for migration and future proof-planning
work but is not silently wired into the old discovery path.

## Semantic model introduced

The core now represents:

```text
Theory -> Context -> Judgment -> ProofState
```

Supporting types include:

- deterministic semantic IDs for theory, spaces, symbols, operators, variables,
  expressions, contexts, regimes, judgments, evidence, proof obligations, and
  rewrite rules;
- `TypeRef`, `TypeArgument`, and explicit `IndexTerm` values;
- variable, symbol, operator, indexed-operator, parameterized-operator,
  application, composition, addition, scalar multiplication, direct sum,
  adjoint, literal, zero, and identity expressions;
- structured constraints and `ValidityRegime` compatibility with
  `Equal`, `Compatible`, `Incompatible`, and `Unknown`;
- scoped `Context` with variables, structured assumptions, legacy/unparsed
  assumptions, parent context reference, and active regime;
- explicit `JudgmentKind` values for equality, implication, equivalence,
  membership, definedness, inclusion, commutation, inverse law, annihilation,
  nilpotence, decomposition, approximation, correspondence, analogy, and
  generic relation;
- `ProofObligation` lifecycle states: unresolved, unsupported, falsified, and
  discharged;
- `ProofState` with target, obligations, evidence, and provenance; and
- `Theory::add_rewrite_rule` guarded by the explicit `rewrite_safety` contract.

The implementation is dependent-lite: indexed type arguments and integer
offsets support cases such as `d_k : Form(M,k) -> Form(M,k+1)`. It is not a
general dependent type theory or CAS.

## Rewrite safety

Only an `Equality` judgment with:

- exactly two operands;
- valid and equal operand types;
- matching context;
- compatible context/regime;
- satisfied or represented side conditions;
- explicit rewrite direction; and
- trusted structural/symbolic/formal evidence or machine equality evidence

may enter a `Theory` rewrite-rule collection.

Analogy, correspondence, implication, approximation, and generic relation
judgments are rejected. Unknown operand typing, side-condition satisfaction, or
regime overlap returns `Unknown`, not permission to rewrite.

## Conservative contradiction API

`classify_conflict` is deliberately not a satisfiability engine. It reports:

- `DisjointRegimes` for clearly incompatible regimes;
- `Unknown` when overlap is unresolved;
- `PotentialConflict` when different equality conclusions need an explicit
  incompatible proposition before contradiction can be claimed;
- `Incomparable` for unsupported kinds/types; and
- `NoContradiction` for the same proposition.

The legacy closure implementation is not switched to this API in Layer 15,
because changing its discovery behavior would violate the frozen scientific
baseline. Full regime-aware closure integration is deferred to a later proof/
closure migration with its own gate.

## Legacy Atlas migration

`AtlasTheoryAdapter` performs a one-way, visible migration:

| Migrated material | Count | Layer-15 treatment |
|---|---:|---|
| Atlas identity records | 67 | preserved as facts with original provenance |
| Atlas relation records | 119 | generic non-equality judgments |
| Equality judgments | 6 | fully structured equality judgments with machine-equality evidence |
| Semantic identity judgments | 61 | generic non-rewriteable semantic judgments |
| Fully structured identity migrations | 6 | complete equality ASTs |
| Partially structured migrations | 180 | 61 semantic identities + 119 relations |
| Legacy/unparsed migrations | 0 | no current record required this fallback |
| Unsupported migrations | 0 | no current record required this fallback |
| Migrated theory facts | 186 | 67 identity facts + 119 relation facts |

The equality count is compared to the current Atlas executable-equality count
at runtime; it is not hard-coded as a target. Original operator/space/source
IDs remain traceable. Non-executable semantic records never become equality
facts.

## Deterministic serialization and IDs

Canonical serialization uses explicit tagged, length-delimited components,
sorted collections where order is semantically irrelevant, and ordered child
lists where order is meaningful. IDs use a deterministic FNV-1a-derived digest
over canonical serialization. Pointer addresses, locale, process order, and
unordered-container iteration are not inputs.

Obligation IDs identify the obligation itself and therefore remain stable when
its lifecycle status changes. `ProofState` serialization includes obligation
statuses, so state identity changes when proof progress changes.

## Focused Layer-15 tests

`tests/semantic_core_tests.cpp` covers:

- distinct equality, implication, equivalence, analogy, correspondence,
  approximation, and generic-relation kinds;
- rewrite rejection for all non-equality kinds;
- guarded `Theory` rewrite-rule insertion;
- unresolved missing side conditions are not treated as satisfied;
- incompatible and unknown validity regimes;
- conservative contradiction/potential-conflict behavior;
- indexed `d_k` versus `d_(k+1)` identity;
- valid `d_(k+1) ∘ d_k` typing and invalid `d_k ∘ d_k` typing;
- deterministic context IDs under semantically irrelevant construction order;
- stable obligation IDs and distinct ProofState lifecycle statuses;
- current Atlas migration counts and semantic/equality separation; and
- future problem-as-goal viability through `Context + target Judgment +
  ProofState`.

## Verification results

Software and semantic tests:

```text
CTest: 2/2 passed
  opforge_tests: passed
  opforge_semantic_tests: passed
clean build: passed in /tmp/opforge-layer15-clean-v1
ASan/UBSan build: passed in /tmp/opforge-layer15-asan-v1
ASan/UBSan CTest: 2/2 passed
git diff --check: clean
```

Frozen scientific baseline after Layer 15:

```text
blind structural recovery: 1/6
partial: 4/6
miss: 1/6
false positives: 0
negative controls: 3/3
leakage events: 0
numerical discovery experiments: 0
opaque-ID test: passed
deterministic candidate IDs/counts: passed
```

Scaling remained:

```text
12 operators -> 25 structural candidates
50 operators -> 150 structural candidates
98 operators -> 391 structural candidates
```

Open search remained epistemically honest:

```text
generated candidates: 62
pruned by budget: 650
serious candidates: 0
numerical experiments: 0
```

The implementation comparison is exact for the recorded baseline fields:

| Metric | Before | After | Result |
|---|---:|---:|---|
| blind structural recovery | 1/6 | 1/6 | unchanged |
| blind partial recovery | 4/6 | 4/6 | unchanged |
| blind misses | 1/6 | 1/6 | unchanged |
| blind false positives | 0 | 0 | unchanged |
| negative controls | 3/3 | 3/3 | unchanged |
| leakage events | 0 | 0 | unchanged |
| opaque-ID result | passed | passed | unchanged |
| blind graph candidates | 3850 | 3850 | unchanged |
| blind compatible edges | 3503 | 3503 | unchanged |
| blind generated candidates | 3503 | 3503 | unchanged |
| open generated candidates | 62 | 62 | unchanged |
| open pruned candidates | 650 | 650 | unchanged |
| numerical discovery experiments | 0 | 0 | unchanged |

Scaling remained `12 -> 25`, `50 -> 150`, and `98 -> 391` compatible structural
candidate counts; the scaling-stage definitions are unchanged and are not
combined with the open-search `62` generated / `650` pruned counts.

## Deferred limitations

- The new semantic core is not yet the implementation behind the old closure
  engine; that integration belongs to a separately gated migration.
- Regime comparison is conservative and intentionally not general logical
  implication.
- Indexed types support bounded index variables and offsets, not arbitrary
  dependent type theory.
- No Layer-16 quotient/e-graph search or completeness engine was added.
- No Layer-17 bidirectional solver or Layer-18 proof planner was added.
- No formal proof backend was selected.
- No numerical discovery or benchmark score optimization was added.
- Full theorem-level contradiction and entailment remain unresolved by design.

## Layer-15 completion decision

Layer 15 satisfies its current Definition of Done as a minimum semantic-core
representation layer: the current operator-centric Atlas can be migrated
without semantic collapse, focused semantic tests pass, and the frozen
discovery/scientific behavior is unchanged.

Layer 16 is **not yet authorized to begin implementation**. The next required
step is review of this implementation against the Layer Gate and a separately
approved Layer-16 quotient-search specification.
