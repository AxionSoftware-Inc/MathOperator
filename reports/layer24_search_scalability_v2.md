# OpForge Layer 24 — Search Scalability v2

## Verdict

SCALABLE_CONSTRAINT_DIRECTED_SEARCH_DEMONSTRATED

This is a bounded, target-blind, relative result. It does not claim universal
mathematical-search speedup or global completeness. Frozen Layer-20, Layer-21,
Layer-22, and Layer-23 verdicts were not rewritten.

Layer 24 does not implement Layer 25 formal proof integration, Layer 26 grand
evaluation, physics, unrestricted linear combinations, or a broader grammar.

## Search architecture

SearchPlan is compiled from the Theory version/digest, Context identity,
ValidityRegime identity, target type and constraints, rich Layer-23 facts and
trusted rules, allowed schemas, depth/cost/resource limits, and the
equivalence-theory ID. It contains deterministic backward demands, required
forms/properties, relevant operators/spaces/facts, considered and avoided
schemas, and an indexed theory digest.

The theory index covers operators by domain, codomain, and pair; trusted
operator properties; spaces by structured property; trusted space relations;
indexed family membership; and deterministic theory/index digests.

Optimized expansion uses output/property schema contracts, typed operand
indexes, a bounded forward/backward typed demand graph, incremental type and
property checks, expression memoization, quotient-at-insertion canonical keys,
and indexed frontier-meeting accounting. Deep composition frontiers
deliberately keep internal prefix/suffix operands; final-output filtering is
used only where sound for the declared depth. Indexed operands require
concrete endpoint typing and the adjacent offset predicate.

UNKNOWN type/property results remain on goal-relevant branches. An explicit
UNKNOWN budget records deferred branches and terminates as INCOMPLETE_UNKNOWN;
it never converts UNKNOWN to false. Raw/resource budgets terminate as
BUDGET_ENDED and set relative_complete=false.

The ledger supports schema/output/property skips, type/regime/property/index
pruning, canonical duplicates, UNKNOWN retained/deferred, resource pruning,
retained states, and frontier meeting attempts/successes. No e-graph was
added; deterministic canonical memoization was sufficient here.

## Scorer isolation and leakage

Hidden target, expected expression, case label, and opaque-ID mapping are
benchmark/scorer data. Layer24 Problem and SearchPlan contain only Theory,
Context, Regime, target semantic constraints, schemas, and policy. The scorer
evaluates after both searches finish.

Measured audit: target in solver=false; expected expression in solver=false;
benchmark ID in solver=false; operator-name dependence=false; numerical
guidance=false; runtime LLM=false; partial-fact trusted pruning=false; opaque
ID replay=pass.

The strongest commutator replay uses opaque deterministic operator names
op_044 and op_018. The external scorer maps these IDs to the hidden structure
only after discovery.

## Case-by-case disclosure

All eight controlled cases ran in REFERENCE_EXHAUSTIVE and OPTIMIZED_LAZY modes.
The reference and optimized retained canonical sets were identical in every
case. The output below is the optimized retained search output; the reference
output was identical.

| Case | Hidden target | Removed/masked | Visible prerequisites | Classification |
|---|---|---|---|---|
| commutator | Commutator(B,T) | unrelated schemas and non-endomorphism operands; target mapping scorer-only | typed endomorphisms B,T; opaque IDs in run | STRUCTURAL_RECOVERY |
| conjugation | Conjugation(T,B) | adjoint/tensor/restriction schemas; target mapping scorer-only | T invertible; B endomorphism | STRUCTURAL_RECOVERY |
| restriction | Restrict(A,U) | target mapping; unrelated schemas | A:V→W; explicit U inclusion into V | STRUCTURAL_RECOVERY |
| restriction negative | Restrict(A,U) | U inclusion fact | A:V→W; U is named but inclusion absent | STRUCTURAL_WITH_OPEN_CONSTRAINTS |
| tensor | Tensor(A1,B1) | unrelated operand pairs | tensor-capable V1,V2,W1,W2; typed A1,B1 | STRUCTURAL_RECOVERY |
| indexed | Compose(d_k1,d_k) | wrong-offset family members | k+1 outer, k inner, target V→W | STRUCTURAL_RECOVERY |
| multi-step | Compose(M2,Compose(M1,M0)) | no direct Atlas primitive for target | three typed linear steps M0,M1,M2 | STRUCTURAL_RECOVERY |
| unknown explosion | invertible composition | invertibility facts | many typed candidates; no invertibility proof | STRUCTURAL_WITH_OPEN_CONSTRAINTS |

Exact retained output:

    commutator:
      commutator(op_018,op_018) EXACT
      commutator(op_018,op_044) EXACT
      commutator(op_044,op_018) EXACT
      commutator(op_044,op_044) EXACT

    conjugation:
      conjugation(B,B) UNKNOWN
      conjugation(B,T) UNKNOWN
      conjugation(T,B) EXACT
      conjugation(T,T) EXACT

    restriction:
      Restrict(A,U) EXACT
      Restrict(R,U) UNKNOWN

    restriction negative:
      Restrict(A,U) UNKNOWN
      Restrict(R,U) UNKNOWN

    tensor:
      Tensor(op_020,op_021) EXACT

    indexed:
      Compose(d_k1,d_k) EXACT

    unknown explosion:
      Compose(A,B) UNKNOWN
      Compose(A,T) UNKNOWN
      Compose(A,unknown_0..unknown_7) UNKNOWN

The multi-step set contains 22 exact representatives, including the required
Compose(M2,Compose(M1,M0)) path and its valid parenthesized/typed alternatives.
The complete list is in the JSON report.

## Reference-equivalence scorecard

Reference/optimized attempted counts are schema applications. Materialized
counts are expression allocations after optimized prechecks. Retained and
UNKNOWN counts are retained exact and goal-relevant UNKNOWN classes.

| Case | Ref attempted | Opt attempted | Ref materialized | Opt materialized | Operand skips | Exact | UNKNOWN | Status | Equiv. |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| commutator | 42 | 4 | 10 | 8 | 16 | 4 | 0 | exhausted | pass |
| conjugation | 42 | 4 | 10 | 8 | 16 | 2 | 2 | exhausted | pass |
| restriction | 12 | 2 | 12 | 6 | 2 | 1 | 1 | exhausted | pass |
| restriction negative | 12 | 2 | 12 | 6 | 2 | 0 | 2 | exhausted | pass |
| tensor | 42 | 1 | 42 | 3 | 5 | 1 | 0 | exhausted | pass |
| indexed | 72 | 15 | 9 | 7 | 25 | 1 | 0 | exhausted | pass |
| multi-step | 850 | 627 | 95 | 93 | 0 | 22 | 0 | exhausted | pass |
| unknown explosion | 210 | 22 | 124 | 22 | 133 | 0 | 10 | exhausted | pass |

The multi-step path conservatively keeps internal operands, so its materialized
reduction is small. This is an honest limitation of sound multi-step slicing.

## Finite exhaustive and budget controls

The independent finite grammar has three primitive terminals and one
composition schema over the ordered 3×3 operand space:

    raw = 12 = 3 primitive terminals + 1 retained constructor representative
                + 8 type-invalid + 0 resource-pruned - 0 merges

It ends EXHAUSTED_RELATIVE_SPACE, relative_complete=true, with accounting pass.
The engine raw constructor counter is 9 because the three primitive terminals
are seeded separately and are included explicitly in the declared raw total.

The same grammar with raw_schema_attempts=3 ends BUDGET_ENDED,
relative_complete=false, with 9 explicitly resource-pruned constructions:

    12 = 3 primitive terminals + 0 retained constructor representatives
         + 0 type-invalid + 9 resource-pruned

It does not claim relative exhaustion. The regression test asserts both
statuses and both accounting equations.

The UNKNOWN-budget control records 10 UNKNOWN states, retains 2, defers 8, and
ends INCOMPLETE_UNKNOWN with relative_complete=false. UNKNOWN is not rejected
and is not treated as proven equivalent.

## Distractor scaling

Baseline is independent finite accounting for the primitive+commutator grammar:
all operators and ordered pairs are considered, while only type-valid pairs are
materialized. Optimized uses the problem slice.

| Distractors | Full ops | Slice ops | Baseline attempted | Optimized attempted | Baseline materialized | Optimized materialized | Operand skips |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 10 | 16 | 4 | 272 | 8 | 20 | 8 | 16 |
| 50 | 56 | 4 | 3192 | 8 | 60 | 8 | 16 |
| 100 | 106 | 4 | 11342 | 8 | 110 | 8 | 16 |
| 250 | 256 | 4 | 65792 | 8 | 260 | 8 | 16 |
| 500 | 506 | 4 | 256542 | 8 | 510 | 8 | 16 |
| 1000 | 1006 | 4 | 1013042 | 8 | 1010 | 8 | 16 |

All six ended EXHAUSTED_RELATIVE_SPACE. These are problem-specific measured
reductions, not a universal speedup claim.

## Million-scale stress

The streaming stress represents 1,000,000 ordered raw possibilities. One pair
is target-relevant:

| Raw | Avoided | Materialized | Retained | UNKNOWN | Resource-pruned | Peak | Status |
|---:|---:|---:|---:|---:|---:|---:|---|
| 1000000 | 999999 | 1 | 1 | 0 | 0 | 1 | EXHAUSTED_RELATIVE_SPACE |

Every ordered pair was checked by the declared streaming type index. This
complete status is only for that explicit synthetic relative space.

## Controlled seed and full production Atlas probes

The previous “real Atlas” row was seed-based: the regression test called
`make_vector_calculus_seed()` before running the benchmark. That valid
controlled measurement is preserved here as a separate seed probe:

```text
full operators=12, full facts=39
slice operators=11, slice facts=33
materialized=19, exact retained=8, UNKNOWN=0
termination=EXHAUSTED_RELATIVE_SPACE
```

The production integration uses the actual CLI loading boundary:
`AtlasLoader::load("atlas")`. It is not a vector-calculus seed, reduced
fixture, or synthetic Theory.

### Atlas identity

```text
operators=98
spaces=47
relations=119
statements/identities=67
executable_equalities=6
semantic_statements=61
atlas_digest=layer24-atlas-snapshot.267014ce981723bb
atlas_version=mixed module schemas 0.2/0.12/0.25; Atlas object has no single version field
```

These are measured from the loaded repository directory. Atlas validation and
v3 audit both report zero issues. The historical 98 count is not a hard-coded
pass condition.

### Layer-23 migration on full Atlas

```text
Atlas facts before Layer 23=186
previously fully structured=6
newly structured=321
cumulative fully structured=327
remaining partial=220
unsupported=0

Rich theory objects:
spaces=47
structured space properties=47
structured space relations=0
structured operator properties=274
trusted rule schemas=4
rich-object fully structured=325
theory_version=atlas-semantic-core-v1
theory_digest=layer24-theory.04ba8700aedf039e
```

The cumulative migration count includes the six pre-Layer-23 structured Atlas
facts; the rich-object count counts currently represented Layer-23 objects.
The 220 partial facts remain outside trusted equality/property pruning.

### Full-Atlas target-blind search

The machine-readable integration goal is a depth-1 composition request with a
linear property demand over the first deterministically ordered migrated
operator type. It contains no hidden expected expression or scorer target:

```text
target type = Operator(operator.space, operator.space)
constraints = constructor_form=composition; property=linear
```

This is an integration probe, not a claim that the Atlas contains a particular
theorem.

| Metric | Full Atlas / optimized |
|---|---:|
| full theory operators | 98 |
| full structured search facts | 460 |
| full spaces / rules | 47 / 4 |
| slice operators / facts / spaces / rules | 8 / 24 / 1 / 4 |
| schemas considered | 1 |
| schemas skipped by output demand | 9 |
| schemas skipped by property demand | 0 |
| optimized attempted | 64 |
| materialized expressions | 72 |
| canonical retained | 64 |
| exact retained | 64 |
| UNKNOWN retained | 0 |
| peak frontier | 64 |
| frontier meeting attempts | 64 |
| termination | `EXHAUSTED_RELATIVE_SPACE` |
| relative_complete | `true` |

The full Atlas was tractable under the same bounded depth-1 grammar, so the
reference was not weakened to a reduced fixture:

```text
reference method=FULL_ATLAS_REFERENCE_EXHAUSTIVE
reference attempted=58016
reference materialized=20579
reference peak frontier=20481
reference exact canonical set=64
optimized exact canonical set=64
reference UNKNOWN=0, optimized UNKNOWN=0
canonical equivalence=PASS
scope=entire supplied Atlas after Layer-23 migration; depth=1; all declared schemas; no reduced fixture
```

The full snapshot contributes 90 operators outside the relevance slice. The
optimized run materialized 72 expressions versus 20,579 in the full reference
path. These are actual Atlas distractors; none were manufactured for this
probe.

### Relevance-slicing audit

The inclusion audit is recorded item-by-item in the JSON report. The selected
operators are:

```text
alg.adjoint_v012
alg.anticommutator
alg.commutator
alg.conjugation
alg.exponential
alg.normal_operator
alg.unitary_operator
fa.resolvent
```

Each selected operator has `target_type_dependency`,
`target_property_dependency`, and `constructor_dependency`. The selected space
is `operator.space` with `target_type_dependency`. The 24 selected trusted
operator-property fact IDs and their exact per-fact reasons are in the JSON
`production_atlas.relevance_slice.inclusion_audit`; eight of those facts also
carry `target_property_dependency` because they are linear facts. The four
trusted rules are retained globally with `trusted_rule_dependency`.

Dependency totals are: target type 8, target property 8, constructor 8,
trusted rule 4, space relation 0, direct Context 0. No name/text similarity
drives relevance. Exclusions are 90 operators, 436 non-slice facts, and 46
spaces. Partial semantic facts are not used to justify exclusion; they are not
consumed by this Layer-24 trusted-index contract. All four trusted rules remain
present rather than being silently excluded.

The 24 fact IDs included in the slice are:

```text
layer23_operator_property.745d8298c67a739e
layer23_operator_property.47d5f12902939d95
layer23_operator_property.7e48cec96b361489
layer23_operator_property.088065d92dcf4472
layer23_operator_property.f5c95547059077c1
layer23_operator_property.c374a6bb776e37bd
layer23_operator_property.418d3f7e4152e44b
layer23_operator_property.701f4d49d1f5dd38
layer23_operator_property.c9ee612aa15f7012
layer23_operator_property.1f2b3e8dbe3c39e0
layer23_operator_property.46110e6866f07ebe
layer23_operator_property.0bf1adef90cd5640
layer23_operator_property.773ad77d0106da28
layer23_operator_property.cc92cd89db8a382e
layer23_operator_property.2e3b30bf2821d324
layer23_operator_property.03ba16db21a6c053
layer23_operator_property.4127a4a6e2cc81b3
layer23_operator_property.accca742810343d1
layer23_operator_property.1a4e7959c46c4321
layer23_operator_property.41e41cb0cbd10942
layer23_operator_property.581af2f26aad1c57
layer23_operator_property.b907dd2718aadca3
layer23_operator_property.e0d3b7f1c5be7f60
layer23_operator_property.5cda537a5e7e5c5a
```

## Caches, determinism, and boundaries

Type, property, and applicability cache keys include Theory digest, Context
identity, Regime identity, expression/candidate keys, and property/schema
inputs. Theory mutation changes the digest; Context and Regime identity changes
also isolate caches.

The selected opaque commutator replay ran three times with identical semantic
output and ledger identity. Its current digest is
`layer24-selected-replay.6fb079d756cb8f52`.

The full production optimized replay also ran three times with identical
SearchPlan, candidate IDs, slice IDs/counts, ledger counts, and termination:

```text
layer24-full-atlas-replay.08821fff5b3fd1ee
layer24-full-atlas-replay.08821fff5b3fd1ee
layer24-full-atlas-replay.08821fff5b3fd1ee
```

The report digest is `layer24-benchmark.aa669813af7dfc67`.

Open discovery remains separate; no target is injected into it. Discovery
numerics and runtime LLM calls remain zero. Unrestricted coefficient
enumeration remains disabled. Partial Layer-23 facts remain outside trusted
property/equality pruning.

## Limitations and next boundary

Planning/index, search, quotient, and proof-plan timing fields are separate;
wall-clock timing is excluded from semantic identity. The main bottlenecks are:

1. property entailment depth and missing derived property certificates;
2. limited explicit space-relation coverage; and
3. multi-step indexed demand propagation.

Formal proof integration status is unchanged. Layer 25 was not implemented.

## Gate record

The Layer-24 executable regression test covers reference equivalence, finite
exhaustion, budget distinction, UNKNOWN deferral, million-scale streaming,
opaque IDs, cache invalidation, Context isolation, numerical/LLM/linear
combination firewalls, and deterministic replay.

Verified gates:

- Debug CTest: 11/11 passed;
- full production Atlas integration: 98 operators / 47 spaces / 119 relations,
  passed;
- full-Atlas reference equivalence: 64/64 canonical sets, passed;
- full-Atlas deterministic replay: 3/3, passed;
- Release configure/build: passed;
- Release CTest: 11/11 passed;
- ASan/UBSan configure/build: passed;
- ASan/UBSan CTest: 11/11 passed;
- blind rediscovery baseline: passed with 0 leakage, 0 false positives, and
  0 discovery numerics;
- stored JSON report validation: passed;
- git diff --check: passed; and
- Layer 25/26 implementation search: no implementation symbols or targets
  found; formal backend status unchanged.
