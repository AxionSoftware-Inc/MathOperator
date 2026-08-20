# OpForge Layer 20 — Practical Utility Gate

Date: 2026-08-20

Verdict: LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED

This is an evaluation result, not a claim of universal mathematical solving,
formal proof, novelty, or new mathematics. No Layer-17 search grammar,
ranking, proof rule, verifier, numerical path, or production Atlas behavior
was changed to improve a score.

## Scope and boundary

The evaluated path was:

~~~text
machine-readable Problem
  -> Layer 17 bidirectional reasoning
  -> Layer 16 typed quotient path
  -> structural candidate/expression
  -> Layer 18 ProofPlan
  -> Layer 19 verification/evidence
  -> ResultBundle + Layer-20 audit wrapper
~~~

Expected answers, hidden operator IDs, removed facts, and scorer labels were
held in the fixture/scorer layer. The Problem passed to GoalSearchEngine
contained only the problem target, visible Theory, Context, validity regime,
generic safe rules, and resource contract. For synthesis cases the target was
only a typed variable such as h : Scalar -> Scalar; the expected composition
was not placed in the target.

Layer 19 remains:

~~~text
FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED
~~~

## Aggregate result

| Measure | Count |
|---|---:|
| cases | 15 |
| structural EXACT | 1 |
| structural VALID_ALTERNATIVE | 6 |
| structural PARTIAL | 0 |
| structural MISS | 8 |
| structural FALSE_POSITIVE | 0 |
| proof COMPLETE_AT_REQUIRED_LEVEL | 0 |
| proof PARTIAL | 0 |
| proof OPEN | 11 |
| proof UNSUPPORTED | 1 |
| proof FALSIFIED | 3 |
| search EXHAUSTED_RELATIVE_SPACE | 9 |
| search BUDGET_ENDED | 1 |
| search INCOMPLETE_UNKNOWN/under-specified/invalid | 3 |
| search UNSUPPORTED_LANGUAGE | 2 |
| negative controls | 5 |
| negative controls passed | 5 |

Structural recovery is deliberately not called recall. The six valid
alternatives include the held-out/synthesis and typed-probe cases; the
real-Atlas transfer limitation is not counted as a success.

## Case-by-case results

The expressions below are the human-readable forms of the deterministic
canonical expressions in the JSON result. raw is the Layer-17 forward
construction count after the Layer-16 handoff; it is not the number of final
solutions.

### Tier A — known problem, hidden solution path

| Field | Result |
|---|---|
| benchmark | tier-a.known-problem-composition |
| target | Defined(compose(op.B, op.A)), visible as the problem target |
| removed | none; expected path was not supplied |
| visible prerequisites | op.A : Scalar -> Vector, op.B : Vector -> Scalar, generic typed composition rules |
| exact search output | SOLVED_STRUCTURALLY; raw=6, forward_states=4, backward_states=3, meetings=6, budget_pruned=0, unresolved=0, relative_complete=true |
| candidate output | compose(op.B, op.A) plus leaf lineage op.A, op.B |
| classification | structural EXACT; proof OPEN; search EXHAUSTED_RELATIVE_SPACE; novelty EXTERNAL_CHECK_REQUIRED |
| ResultBundle | 7 obligations; 4 exact certificates; 1 structural discharge; 3 unsupported; 3 open; status PROOF_PLAN_GENERATED |

This is target-directed by design and is not used as evidence of blind
rediscovery.

### Tier B — held-out fact/operator reconstruction

| Field | Result |
|---|---|
| benchmark | tier-b.held-out-fact |
| hidden target | laplacian |
| removed | hidden operator ID/name/provenance and fact op.laplacian = compose(divergence, gradient) |
| visible prerequisites | gradient : Scalar -> Vector, divergence : Vector -> Scalar |
| problem given to engine | typed variable goal holdout : Scalar -> Scalar; no laplacian and no expected composition |
| exact search output | SOLVED_STRUCTURALLY; raw=6, forward_states=4, backward_states=1, meetings=18, budget_pruned=0, unresolved=0, relative_complete=true |
| candidate output | compose(divergence, gradient) |
| classification | structural VALID_ALTERNATIVE; scorer STRUCTURAL_EQUIVALENT_RECOVERY; proof OPEN; search EXHAUSTED_RELATIVE_SPACE; novelty EXTERNAL_CHECK_REQUIRED |
| ResultBundle | 9 obligations; 5 exact certificates; 2 structural discharges; 4 unsupported; 3 open; status PROOF_PLAN_GENERATED |

### Tier C — missing-operator synthesis

| Field | Result |
|---|---|
| benchmark | tier-c.missing-operator-synthesis |
| hidden target | op.C |
| removed | hidden operator ID/name/provenance and its identity fact |
| visible prerequisites | op.A : Scalar -> Vector, op.B : Vector -> Scalar |
| problem given to engine | typed variable goal holdout : Scalar -> Scalar; no expected expression |
| exact search output | SOLVED_STRUCTURALLY; raw=6, forward_states=4, backward_states=1, meetings=18, budget_pruned=0, unresolved=0, relative_complete=true |
| candidate output | compose(op.B, op.A) |
| classification | structural VALID_ALTERNATIVE; scorer SYNTHESIZED_VALID_EXPRESSION; proof OPEN; search EXHAUSTED_RELATIVE_SPACE; novelty EXTERNAL_CHECK_REQUIRED |
| ResultBundle | 9 obligations; 5 exact certificates; 2 structural discharges; 4 unsupported; 3 open; status PROOF_PLAN_GENERATED |

The engine selected/generated an expression, not a known Atlas node.

### Tier C — opaque-ID synthesis robustness

| Field | Result |
|---|---|
| benchmark | tier-c.opaque-synthesis |
| hidden target | laplacian |
| removed | hidden operator ID/name/provenance and its identity fact; remaining names replaced by op_017, op_044 |
| visible prerequisites | op_017 : Scalar -> Vector, op_044 : Vector -> Scalar |
| problem given to engine | typed variable goal holdout : Scalar -> Scalar; no English mathematical names |
| exact search output | SOLVED_STRUCTURALLY; raw=6, forward_states=4, backward_states=1, meetings=18, budget_pruned=0, unresolved=0, relative_complete=true |
| candidate output | compose(op_044, op_017) |
| classification | structural VALID_ALTERNATIVE; scorer SYNTHESIZED_VALID_EXPRESSION; proof OPEN; search EXHAUSTED_RELATIVE_SPACE; novelty EXTERNAL_CHECK_REQUIRED |
| ResultBundle | 9 obligations; 5 exact certificates; 2 structural discharges; 4 unsupported; 3 open |

Opaque-ID robustness: PASS. Success depends on typed structure, not the English
names.

### Tier D — never-named construction

| Field | Result |
|---|---|
| benchmark | tier-d.never-named |
| hidden target | no standalone target node exists |
| removed | none; expected three-step expression is scorer-only |
| visible prerequisites | op.source : Scalar -> Vector, op.middle : Vector -> Matrix, op.target : Matrix -> Output |
| problem given to engine | typed variable goal expression : Scalar -> Output; no expected expression |
| exact search output | MULTIPLE_STRUCTURAL_SOLUTIONS; raw=147, forward_states=7, backward_states=1, meetings=63, budget_pruned=0, unresolved=0, relative_complete=true |
| candidate output | compose(op.target, compose(op.middle, op.source)); also compose(compose(op.target, op.middle), op.source) |
| classification | structural VALID_ALTERNATIVE; scorer SYNTHESIZED_VALID_EXPRESSION; proof OPEN; search EXHAUSTED_RELATIVE_SPACE; novelty EXTERNAL_CHECK_REQUIRED |
| ResultBundles | 2 separate plans; each has 13 obligations, 7 exact certificates, 2 structural discharges, 6 unsupported, 5 open |

The two parenthesizations remain separate in this current quotient contract;
they were not collapsed merely because a human may regard composition as
associative.

### Tier E — space/regime transfer

#### Synthetic typed bridge probe

tier-e.typed-bridge-probe used op.source : Scalar -> SourceSpace and
op.transport : SourceSpace -> TargetSpace. The engine received only a typed
variable goal holdout : Scalar -> TargetSpace; it returned
compose(op.transport, op.source) with raw=6, forward_states=3,
backward_states=1, meetings=17, and EXHAUSTED_RELATIVE_SPACE.

This is classified as VALID_ALTERNATIVE only for
TYPE_LEVEL_ONLY_NOT_TRANSFER. Its proof is OPEN, with 9 obligations, 5 exact
certificates, 2 structural discharges, 4 unsupported and 3 open. It is not a
transport theorem and is not counted as a real cross-space transfer success.

#### Real Atlas transfer

tier-e.real-atlas-transfer was NOT_RUN_REAL_ATLAS_LIMITATION. The migrated
real Atlas had 6 fully structured facts but no structured
Correspondence/Analogy bridge usable by the current generic production rule
contract. No bridge fact was invented. Classification:
structural MISS, proof UNSUPPORTED, search UNSUPPORTED_LANGUAGE.

### Tier F — missing primitive

| Field | Result |
|---|---|
| benchmark | tier-f.missing-primitive |
| hidden expected construction | adjoint(op.forward) |
| removed/absent | adjoint synthesis construction family is absent from the generated grammar |
| visible prerequisites | op.forward : Scalar -> Vector |
| exact search output | no candidates; raw=2, forward_states=1, backward_states=1, meetings=14, budget_pruned=0, unresolved=0, relative_complete=true; underlying status NO_SOLUTION_IN_RELATIVE_SPACE |
| classification | structural MISS; scorer UNSUPPORTED; proof OPEN; search UNSUPPORTED_LANGUAGE; novelty EXTERNAL_CHECK_REQUIRED |
| ResultBundle | 3 obligations; 2 exact certificates; 1 structural discharge; 1 unsupported; 1 open |

This is an honest language-boundary result, not a false proof of global
impossibility.

### Tier G — budget distinction

tier-g.budget-ended-vs-exhausted uses the same masked synthesis problem as
Tier C but with candidate budget 1.

Exact output: BUDGET_ENDED; raw=1, forward_states=1, backward_states=1,
meetings=14, budget_pruned=6, unresolved=0, relative_complete=false, with
reason explicit forward or total search budget ended the run. No candidate was
scored as recovered. The proof result is OPEN.

The unbudgeted counterpart ends EXHAUSTED_RELATIVE_SPACE; the report never
relabells the budget-ended run as exhaustive.

### Tier H — multiple solutions

tier-h.multiple-quotient-solutions retained three distinct same-type operator
solutions: op.A, op.B, op.C. Output:
MULTIPLE_STRUCTURAL_SOLUTIONS, raw=3, forward_states=3, backward_states=1,
meetings=3, budget_pruned=0, unresolved=0, relative_complete=true.

Classification: structural VALID_ALTERNATIVE; proof OPEN; search
EXHAUSTED_RELATIVE_SPACE; novelty EXTERNAL_CHECK_REQUIRED. Three separate
ResultBundles were produced, each with 5 obligations, 3 exact certificates, 2
structural discharges, 2 unsupported and 1 open.

### Tier G — negative controls

| Case | Exact output | Structural | Proof | Search |
|---|---|---|---|---|
| impossible type | malformed target rejected: target contains an ill-typed expression | MISS | FALSIFIED | INVALID_PROBLEM |
| incompatible regime | target rejected: target validity regime is incompatible with problem context | MISS | FALSIFIED | INVALID_PROBLEM |
| missing prerequisite | no candidate; raw=1, forward=1, backward=3, meetings=4 | MISS | OPEN | EXHAUSTED_RELATIVE_SPACE |
| under-specified type | target rejected: target contains an expression with unknown type | MISS | OPEN / INCONCLUSIVE | UNDER_SPECIFIED |
| approximation/near match | no exact candidate; raw=2, forward=3, backward=1, meetings=6 | MISS | FALSIFIED | EXHAUSTED_RELATIVE_SPACE |

Negative-control false positives: 0/5.

## Forward discovery

The existing open discovery campaign was run separately with its frozen
target-free configuration:

| Metric | Value |
|---|---:|
| candidates generated | 62 |
| candidates pruned | 650 |
| serious candidates | 0 |
| discovery numerical experiments | 0 |

These are legacy campaign-stage quantities and are not conflated with the
Layer-17 forward construction count.

The controlled target-free hidden-operator fixture used the masked
gradient/divergence Theory:

| Metric | Value |
|---|---:|
| raw constructions | 6 |
| retained quotient classes | 4 |
| lossless reductions | 2 |
| unresolved | 0 |
| status | EXHAUSTED_RELATIVE_SPACE |
| relative complete | true |
| reconstructed | compose(divergence, gradient) |
| serious candidates | not claimed; open discovery has no scorer target or proof plan |

No numeric result entered generation, ranking, quotienting, or goal meeting.

## Leakage and scorer isolation

| Audit | Result |
|---|---|
| benchmark ID in solver input | no |
| hidden operator ID/name in solver input | no |
| aliases/descriptions | no semantic alias/description channel exists in the masked solver Theory |
| expected expression in solver input | no for target-blind B/C/D/E/F/open cases |
| relation/identity/metadata leakage | no; hidden identity and provenance were removed |
| scorer data in solver input | no |
| target-specific generation branch | not found |
| opaque-ID robustness | PASS |
| runtime LLM calls | 0 |
| discovery numerical experiments | 0 |

Tier A is intentionally target-directed and therefore is not called
target-blind. Its visible target is the machine-readable problem, not a
hidden expected path.

## ResultBundle auditability

Every executed candidate/partial/negative result has a Layer-19
ResultBundle. The Layer-20 JSON additionally exposes:

- candidate expression and deterministic candidate/plan/bundle IDs;
- inferred operator type, domain and codomain;
- Context, assumptions and validity regime;
- forward and backward lineage;
- quotient/provenance entries;
- ProofPlan ID and obligation counts;
- exact, structural, numerical, unsupported, open and falsified counts;
- formal-backend availability;
- final evidence status;
- Layer-19 novelty status; and
- Theory version.

For successful structural cases the evidence status is generally
PROOF_PLAN_GENERATED, not PROVED. No result is labelled NEW MATHEMATICS.

## Construction grammar coverage

Currently exercised/generated:

- non-indexed, non-parameterized operator atoms;
- explicitly seeded indexed/parameterized atoms;
- typed composition;
- Layer-15 type and validity-regime checks;
- Layer-16 lossless quotienting; and
- explicit safe Layer-17 backward composition decomposition.

Not generated or not soundly available in this gate:

adjoint, inverse synthesis, restriction/extension, conjugation, commutator,
tensor/product, pullback/pushforward, controlled linear combinations, integral
transforms, discretization, dualization, and Correspondence/transport proof
rules.

Unrestricted arbitrary linear-combination enumeration remains disabled.

## Determinism and machine output

The selected Layer-20 suite was run three times with identical inputs and
configuration. Candidate expressions, classifications, search statuses,
ProofPlan/ResultBundle identities and the semantic digest matched:

~~~text
repetitions = 3
determinism = PASS
digest = layer20_benchmark_digest.2700ae2a3b3d24c4
~~~

Runtime durations are present as measurements but excluded from the semantic
identity digest.

The machine-readable export is
[layer20_practical_utility.json](layer20_practical_utility.json).

## Verification gates

The final verification run used a fresh Release build directory and a fresh
ASan/UBSan build directory:

| Gate | Result |
|---|---|
| Release build | PASS |
| Release CTest | 7/7 passed |
| ASan/UBSan build | PASS |
| ASan/UBSan CTest | 7/7 passed |
| Layer 15-19 tests | PASS as part of both CTest runs |
| frozen baseline commands | PASS; frozen metrics preserved |
| `git diff --check` | PASS |

macOS' AddressSanitizer runtime does not support `detect_leaks=1`; the first
sanitizer invocation was rejected by that runtime option, not by a code
finding. The sanitizer gate was rerun with AddressSanitizer and UBSan active
and without the unsupported leak-detection option, and all seven tests passed.

## Atlas-dependence scorecard

| Capability | Finding |
|---|---|
| known operator selection | works in target-directed typed matching; not an open-discovery claim |
| held-out fact derivation | structural equivalent composition recovered after masking |
| held-out operator reconstruction | composition synthesized without hidden operator node; opaque IDs pass |
| never-named expression synthesis | three-step expression generated from current composition grammar |
| cross-space transfer | not demonstrated on real Atlas; only a type-level probe was run |
| missing primitive | adjoint remains unsupported rather than fabricated |

## Top three actual bottlenecks

1. CONSTRUCTION_GRAMMAR — adjoint and the other construction families are not
   generated, and associative alternatives remain distinct.
2. FORMAL_VERIFICATION — every positive utility case retains open or
   unsupported obligations; the formal backend is unavailable.
3. CROSS_SPACE_TRANSFER — real Atlas lacks the structured bridge/rule contract
   needed for a sound transfer benchmark.

## Final assessment

LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED.

The complete path demonstrates bounded typed composition and hidden
operator-expression reconstruction with auditable provenance, while preserving
honest misses, unsupported language boundaries, budget termination, open proof
obligations, and zero false positives. The result is too narrow for
PRACTICAL_OPERATOR_REASONING_DEMONSTRATED: real cross-space transfer and
formal proof remain unshown.
