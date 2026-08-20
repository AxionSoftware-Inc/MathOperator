# Layer 22 — Constraint-Guided Mathematical Synthesis v1

## Verdict

`CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED`

This is a bounded, target-blind controlled result. It demonstrates that typed
candidate generation can be narrowed by machine-readable semantic requirements
before external scoring. It does not claim theorem proving, universal
constraint solving, or successful transfer to the current real Atlas.

Command and input:

```text
./build/opforge benchmark constraint_synthesis atlas
```

Deterministic digest:
`layer22_benchmark_digest.d148a171b146b533`; three identical runs produced the
same candidate IDs, classifications, counts, and digest.

## Constraint model and exact ceiling

Layer 22 introduces `SemanticConstraint`, which wraps Layer-15
`Expression`, `Judgment`, `Context`, `ValidityRegime`, `TypeRef`, `IndexTerm`,
and `Provenance`. It is not a second proposition language. Each requirement
has:

- kind: required type, definedness, equality, commutation, inverse law,
  adjoint relation, indexed relation, regime condition, or structured property;
- strength: `HARD_CONSTRAINT` or `OPEN_PROOF_CONSTRAINT`;
- origin and provenance;
- optional original target `Judgment`; and
- a deterministic graph node and derivation edge.

The supported exact solver fragment is deliberately narrow:

- exact `TypeRef` equality and typed definedness;
- composition type checking and intermediate-space propagation;
- reversed unary constructor typing;
- exact constructor-form predicates for adjoint, commutator, conjugation, and
  indexed composition;
- left/right/two-sided inverse candidate form distinction;
- exact index literals and represented offset terms;
- exact context/regime compatibility; and
- trusted structured Theory facts when their judgment and context match.

`UNKNOWN` and `UNSUPPORTED` are preserved. An inverse candidate does not prove
an inverse law; a commutator form does not prove commutation; an adjoint form
does not prove its defining identity; and conjugation does not prove transport.
Open requirements become Layer-18-shaped `ProofObligation` records with
provenance and required evidence level `SYMBOLIC`.

The goal extractor consumes only `Problem` (`Theory + Context + target
Judgment`). It does not parse natural language. Constructor applicability is
tri-state and is evaluated before child expression expansion. Composition,
unary reversal, index, transform, and algebraic side requirements are
propagated through a constraint graph. Layer-17 is used as the problem/search
boundary and substitution vocabulary; the new propagation layer does not alter
the historical Layer-17 engine.

The policy retains `UNKNOWN` candidates by default, never promotes them to
exact solutions, and reports relative exhaustion only for the recorded
Theory/context/regime/grammar/depth/cost/constraint-language contract.

## Case-by-case results

All cases are target-blind. Hidden answers and scorer outcomes exist only in the
benchmark fixture layer. “Exact search output” below means the structural form
or explicit retained-set result emitted by the synthesizer; the scorer is run
after search.

| Case | Hidden target / removed items | Visible prerequisites | Exact search output and classification |
|---|---|---|---|
| `layer22.type-only-ambiguity` | hidden `inverse(op.A)`; no inverse/adjoint property or answer expression supplied | `op.A: V -> W`; goal type `W -> V`; no structure assumption | Four retained type-compatible forms: `Adjoint(op.A)`, `LeftInverse(op.A)`, `RightInverse(op.A)`, `TwoSidedInverseCandidate(op.A)`. `TYPE_ONLY_MATCH`; multiple candidates are the expected result, not a failure. Search `EXHAUSTED_RELATIVE_SPACE`; 5 open constructor obligations are visible. |
| `layer22.adjoint-constrained` | hidden adjoint expression; expected expression removed from target | `op.A: V -> W`; target is structured `adjoint_of(goal, op.A)`; no inverse-law fact | One retained candidate: `Adjoint(op.A)`. `EXACT_CONSTRAINT_SATISFACTION` for the constructor-form constraint, with one open defining-identity obligation. Type-compatible inverse candidates are rejected before child expansion. |
| `layer22.adjoint.opaque` | same mathematical hidden target, but operator ID is `op_017`; English name removed | `op_017: V -> W`; same structured adjoint relation represented using the opaque ID | One retained `Adjoint(op_017)`, exact constructor-form satisfaction, one open identity obligation. This is an opaque-ID success, not name matching. |
| `layer22.inverse.left_inverse` | hidden left-inverse candidate; no invertibility fact | `op.A: V -> W`; target relation `left_inverse(goal, op.A)` | `LeftInverse(op.A)` and `TwoSidedInverseCandidate(op.A)` remain `STRUCTURAL_WITH_OPEN_CONSTRAINTS`; the right-only candidate is rejected. Type-compatible count 3, unknown count 2, exact count 0, and 5 proof obligations are emitted. The left requirement does not become a two-sided claim. |
| `layer22.inverse.two_sided_inverse` | hidden two-sided candidate; no inverse theorem | `op.A: V -> W`; target relation `two_sided_inverse(goal, op.A)` | Only `TwoSidedInverseCandidate(op.A)` remains, `STRUCTURAL_WITH_OPEN_CONSTRAINTS`; one UNKNOWN candidate, four explicit obligations (constructor form plus left/right law obligations). One-sided candidates are rejected. |
| `layer22.commutator-constrained` | hidden `Commutator(op.A, op.B)` | `op.A, op.B: V -> V`; target structured `commutator_form(goal, op.A, op.B)`; no commutation theorem | One retained commutator form. Six candidates are type-compatible before property constraints; five are rejected by the hard structured-form property; one exact candidate remains with two open algebra/definedness obligations. No arbitrary coefficient search is used. |
| `layer22.commutator.opaque` | same holdout with `op_017`, `op_044`; names removed | two opaque endomorphisms `V -> V`; structured commutator-form target | One exact `Commutator(op_017, op_044)` form; two open obligations. This is the second opaque-ID success. |
| `layer22.conjugation-constrained` | hidden `Conjugation(op.T, op.A)` | `op.T: V -> W`, `op.A: W -> W`, `op.plain: V -> V`; explicit transform invertibility assumption; no transport theorem | One retained conjugation form; three type-compatible candidates before property constraints, 39 hard schema/property prunes, and two open obligations for invertibility and transport. `op.plain` is not accepted merely because it has type `V -> V`. |
| `layer22.indexed-constraint` | hidden `d_(k+1) compose d_k` | indexed family `d_k: X_k -> X_(k+1)`; target `X_k -> X_(k+2)`; exact offset relation represented in `IndexTerm` | One retained `d_(k+1) ∘ d_k`; exact index relation. Base-name matching is not used; `d_k` and `d_(k+1)` remain distinct. |
| `layer22.false-property-negative` | hidden adjoint answer; adjoint schema disabled in this negative control | `op.A: V -> W`, `op.false: W -> V`; target requires `adjoint_of(goal, op.A)`, but only composition schema is enabled | The same-type `op.false` is rejected by the explicit property. Candidate count 0, `NO_MATCH`, no false positive. The type-compatible count is 1 and hard-prune count is 5. |
| `layer22.unknown-property` | no expected answer exposed; target is `commutes_with(goal, op.A)` | `op.A, op.B: V -> V`; no trusted commutation fact | 22 type-compatible structural candidates are retained with `UNKNOWN`; none is exact. Classification is `STRUCTURAL_WITH_OPEN_CONSTRAINTS`, search is `EXHAUSTED_RELATIVE_SPACE`, and 48 proof obligations remain. UNKNOWN is neither rejected nor promoted. |

## Candidate-reduction metrics

The major positive cases measured the following reductions:

| Case | type-compatible before property | hard/property prunes | UNKNOWN retained | exact constraint-compatible | final retained | proof obligations |
|---|---:|---:|---:|---:|---:|---:|
| adjoint | 1 | 6 | 0 | 1 | 1 | 1 |
| left inverse | 3 | 5 | 2 | 0 | 2 | 5 |
| two-sided inverse | 3 | 6 | 1 | 0 | 1 | 4 |
| commutator | 6 | 21 | 0 | 1 | 1 | 2 |
| conjugation | 3 | 39 | 0 | 1 | 1 | 2 |
| indexed | 1 | 30 | 0 | 1 | 1 | 0 |
| UNKNOWN commutation | 22 | 0 | 22 | 0 | 22 | 48 |

Raw attempt accounting includes both schema applicability checks and expression
type-check attempts. Therefore it is intentionally larger than the number of
constructed candidates. Every early applicability decision is recorded as
`PRUNED_TYPE`, `PRUNED_PROPERTY`, or `CONSTRAINT_UNKNOWN`; every constructed
candidate is recorded as retained, open, rejected, unsupported, or a canonical
merge. The invariant checked by tests is that raw attempts dominate the typed
and early-pruned partitions, and no retained candidate is outside the typed
partition.

## Layer-21 versus Layer-22 scaling

The Layer-21 column is a type-only view of the same goal type because Layer 21
cannot consume a multi-operand semantic property target (`goal_type` accepts a
single operand). It is therefore not mislabeled as a full property-aware
baseline. Layer 22 receives the full structured target and applies its
constraints before child expansion.

| operators | Layer-21 type-compatible | Layer-22 raw attempts | branches avoided before child expansion | hard prunes | UNKNOWN | retained classes | peak frontier |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 3 | 39 | 51 | 30 | 41 | 0 | 1 | 1 |
| 6 | 132 | 174 | 96 | 137 | 0 | 1 | 1 |
| 9 | 279 | 369 | 198 | 287 | 0 | 1 | 1 |

These numbers are not presented as a single accuracy score. The Layer-22 raw
count includes applicability checks that avoid constructing children; Layer-21
counts are generated type-compatible representatives from its type-only view.
The report preserves that definitional difference instead of forcing numbers
to match.

## Leakage, scorer isolation, and opaque IDs

The audit passed for operator IDs/names, aliases, descriptions, relation IDs,
family names, benchmark IDs, metadata, source references, and target-specific
branches. The synthesizer API receives no hidden target, expected expression,
benchmark ID, scorer callback, or expected property value. The fixture scorer
compares retained canonical expressions only after `synthesize()` returns.

Two positive cases were rerun with deterministic opaque IDs and produced the
same structural classifications. No name similarity, semantic prose,
numerical result, or external scorer state is consumed by entailment.

## Real Atlas probe

The migrated real Atlas contains 6 fully structured facts in this run. A
self-adjoint constraint probe was attempted. The current semantic data is not
rich enough to discharge this property, so the result is
`UNSUPPORTED_CONSTRAINT_LANGUAGE` and is not relabeled as synthetic success.
No missing structured fact was invented.

## Proof, verification, and safety controls

- Nontrivial UNKNOWN requirements generate Layer-18-compatible open proof
  obligations with context, regime, provenance, and required evidence level.
- Layer-19 is not expanded in this task; it may discharge only capabilities it
  already supports.
- Discovery/synthesis numerics: `0`.
- Runtime LLM calls: `0`.
- Unrestricted arbitrary linear combinations: disabled.
- Open discovery remains unchanged and has no target ConstraintSet.
- The false-property negative, UNKNOWN-property control, two inverse-law
  directions, indexed offsets, and opaque-ID cases all pass.

## Remaining limitations and next bottlenecks

The exact constraint language does not yet include a general arithmetic solver,
full equality solving, theorem proving for inverse/adjoint/commutation laws,
property transport laws, rich space structure, or a formal backend. The real
Atlas remains the practical semantic boundary.

Top three bottlenecks:

1. richer structured Theory/Atlas facts and space structure;
2. sound space/regime/property entailment beyond the current exact fragment;
3. constructor grammar breadth and search scalability.

Layer 23 was not implemented in this run. It is safe to begin only as a
separately reviewed task that preserves the open-proof, UNKNOWN, scorer, and
historical Layer-15–21 contracts.

## Final gates

- repository Debug CTest: **9/9 PASS**;
- clean Release build and CTest: **9/9 PASS** in
  `/tmp/opforge-layer22-release-final`;
- ASan/UBSan build and CTest: **9/9 PASS** in
  `/tmp/opforge-layer22-asan-final`;
- `git diff --check`: **PASS**;
- Layer-21 regression executable retained frozen digest
  `layer21_benchmark_digest.4c26807e3ed03a83`;
- Layer-22 CTest retained the three-run deterministic seed replay;
- historical blind/scaling/open/quotient/goal/proof/verification/utility
  probes were rerun; their frozen Layer-15–21 classifications and numerical
  discovery count remained unchanged; and
- no Layer 23 implementation was started.
