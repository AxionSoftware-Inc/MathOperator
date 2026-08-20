# OpForge Layer 15–21 roadmap

This roadmap is frozen at the architecture level. Layers 15, 16, 17, 18, 19,
and 20 are implemented/evaluated within their recorded scopes. The
foundational review is
[OPFORGE_ARCHITECTURE_V1_REVIEW.md](reports/OPFORGE_ARCHITECTURE_V1_REVIEW.md).

Epistemic status, provenance, target blindness, and the distinction between
lossless and heuristic operations apply from Layer 15 onward; they are not
features that may be postponed to Layer 19.

## Layer 15 — Mathematical Semantic Core

### Purpose

Replace the overloaded semantic-record boundary with a minimum typed language
and judgment kernel for operator-centric mathematics.

### Input

- frozen current Atlas snapshot;
- legacy `Identity`/relation records through a one-way migration adapter;
- current operator signatures and spaces; and
- Phase-0 semantic classification.

### Output

- `Theory`/signature;
- `Context`;
- `ValidityRegime`;
- typed `Expression`/term language;
- explicit `Judgment` kinds;
- `ProofState`; and
- stable serialization, hashes, evidence, and provenance.

Minimum supported judgment kinds include equality, implication, inclusion,
defined composition, commutation, inverse law, annihilation/nilpotence,
decomposition, approximation, correspondence, predicates, and quantification.

### Invariants

- terms and propositions are separate sorts;
- every operator application is typed;
- indexed spaces support grades, dimensions, bundles, and parameter families;
- assumptions and validity regimes are carried through derivations;
- relation kinds cannot become equality without a proof-producing conversion;
- `unknown` compatibility/entailment is not `false`; and
- old Atlas records cannot directly enter trusted equality closure.

### Dependencies

Phase-0 loader, current Atlas model, evidence/provenance records, and the
scientific regression baseline. No quotient search or new discovery heuristic
is a dependency.

### Machine-testable Definition of Done

- existing benchmark mathematics round-trips into the new representation
  without semantic collapse;
- the six currently executable equalities remain executable and the remaining
  semantic statements remain non-equality evidence;
- `d_(k+1) ∘ d_k` type-checks over indexed form spaces;
- conditional inverse, commutation, decomposition, approximation, and analogy
  examples produce distinct judgment kinds;
- regime intersection returns compatible/incompatible/unknown;
- invalid applications, dropped assumptions, and relation-to-equality
  promotion are rejected;
- serialization round-trips with stable hashes; and
- target-blind, scaling, and open-search baselines remain unchanged.

### Required regression tests

- Atlas validation and equality-closure boundary;
- semantic statement versus executable equality;
- indexed nilpotence typing;
- disjoint-regime contradiction control;
- analogy/approximation rewrite rejection;
- blind leakage and opaque-ID benchmark;
- deterministic blind export; and
- no numerical experiments in discovery.

## Layer 16 — Principled Quotient Search

### Purpose

Reduce typed construction spaces by certified equivalence, constraints,
symmetry, and theorem consequences rather than opaque heuristic top-N ranking.

### Input

Layer-15 terms/judgments plus explicit grammar `G`, depth/cost `D`, resources
`R`, equivalence theory `E`, allowed theorem rules `T`, context/regime, and
search policy.

### Output

- canonical classes and representatives;
- typed structural hashes;
- proof-backed equivalence/merge edges;
- sound-reduction ledger;
- heuristic-pruning ledger; and
- relative-completeness search certificate.

### Invariants

- alpha-equivalence is lossless;
- associativity/commutativity is used only when declared in `E`;
- symmetry quotienting is certified for the current goal and regime;
- bounded e-graph/congruence saturation is sound but reported incomplete;
- theorem-consequence elimination has an entailment certificate; and
- heuristic pruning never supports an exhaustion claim.

### Dependencies

Layer 15 typed terms, judgments, regimes, canonical serialization, and a finite
reference grammar. Layer 16 must not invent missing proposition semantics.

### Machine-testable Definition of Done

- reference enumeration and quotient enumeration agree on a finite grammar;
- every removed candidate has a lossless proof or is labeled heuristic;
- sound and heuristic counts are exported separately;
- canonical duplicate, rejected, pruned, and unknown counts are not conflated;
- `EXHAUSTED_RELATIVE_SPACE` differs from `BUDGET_ENDED`; and
- unrestricted arbitrary linear-combination enumeration remains disabled.

### Required regression tests

- alpha-equivalent terms;
- safe/unsafe AC normalization;
- regime-sensitive equality classes;
- certified symmetry versus unlabeled graph similarity;
- bounded quotient completeness;
- pruning provenance replay;
- deterministic scaling at 12, 50, and 98 operators; and
- no numerical discovery.

## Layer 17 — Goal-Directed / Bidirectional Reasoning

### Purpose

Represent a problem primarily as context plus target judgment and connect
forward Atlas reasoning with backward goal decomposition.

### Input

- Layer-15 `Theory`, `Context`, `ValidityRegime`, and target `Judgment`;
- Layer-16 canonical classes and search contract; and
- typed metavariables and admissible rule schemas.

### Output

- `Problem` container when policy/budget are needed;
- typed unification constraints;
- forward and backward frontiers;
- AND/OR proof states; and
- replayable frontier-meet records.

The default problem core is:

```text
context Γ + validity regime R + target judgment φ
```

A separate `Problem` object is allowed for theory snapshot, policy, budget,
metavariables, and output requirements; it must not introduce a second
proposition language.

### Invariants

- backward steps create obligations; they do not assume conclusions;
- every metavariable is typed and regime-scoped;
- AND branches all need discharge;
- OR alternatives remain separate;
- forward/backward meeting requires a typed judgment match, compatible regimes,
  and a unifier; and
- legacy target-blind discovery APIs cannot receive a Layer-17 goal target.

### Dependencies

Layers 15 and 16. Proof-state serialization and context entailment must be
available before large bidirectional search.

### Machine-testable Definition of Done

- known operator problems parse and type-check;
- a backward rule with a missing assumption creates an open obligation;
- a type-incompatible near miss is rejected;
- a forward result meets a backward goal only through certified matching;
- unresolved/unsupported branches persist with reasons; and
- replay reproduces the same proof-state and judgment hashes.

### Required regression tests

- forward composition against a backward Laplacian goal;
- indexed `d^2=0` goal decomposition;
- inverse with missing invertibility assumption;
- relation/analogy goal that cannot meet equality;
- typed unification mismatch;
- AND/OR branch preservation;
- target-blind scorer isolation; and
- deterministic proof-state replay.

### Recorded implementation result

Layer 16 acceptance closed and Layer 17 was implemented as a separate
goal-directed path. Its detailed results are in
reports/layer17_bidirectional_reasoning.md. The Layer-17 gate covers semantic
Problem/GoalState input, explicit safe backward rules, typed MATCH/NO_MATCH/
UNKNOWN matching, indexed operators, AND/OR branches, finite exhaustion versus
budget, target-blind scorer isolation, negative controls, deterministic replay,
Layer-16 quotient integration, and frozen-baseline preservation.

The verified result was: **LAYER 17 DoD SATISFIED — LAYER 18 SAFE TO BEGIN**.
Layer 18 has now been implemented separately; no structural candidate is
thereby formally proved.

## Layer 18 — Proof Planning

### Purpose

Turn closed conjectures and goal matches into explicit proof-obligation DAGs,
track unresolved obligations, and replay derivations.

### Input

Layer-17 proof states, closed candidate propositions, typed rules, contexts,
validity regimes, and provenance.

### Output

- proof-obligation DAG;
- rule applications and premises;
- side-condition goals;
- discharge status per obligation;
- proof candidate/certificate plan; and
- unresolved/failure reasons.

### Invariants

- all AND obligations are required;
- no obligation is discharged by a score or numerical pass;
- context/regime is retained on every edge;
- proof plans are replayable against a frozen theory; and
- partial structural recovery cannot enter verified status.

### Dependencies

Layers 15–17 and a declared trusted rule catalog. A formal backend is not a
dependency for the initial proof-planning layer.

### Machine-testable Definition of Done

- known identities produce a replayable proof DAG;
- missing hypotheses remain open;
- rule application with incompatible regimes is blocked;
- proof replay yields the same conclusion and provenance hash;
- circular proof plans are rejected or explicitly marked unresolved; and
- candidate lifecycle statuses cannot skip proof-obligation creation.

### Required regression tests

- equality substitution and composition substitution;
- conditional inverse and commutation;
- decomposition reconstruction obligations;
- nilpotence with indexed assumptions;
- circular and missing-premise proof plans;
- contradiction versus potential conflict; and
- old semantic identities cannot create proof edges.

### Verified implementation result

Layer 18 is implemented in `opforge::proof::ProofPlanner`. The plan contains
stable target/candidate/context/regime identity, semantic obligations, explicit
AND/OR rule nodes, shared DAG dependencies, evidence envelopes, provenance,
cycle paths, replay, invalidation, and lossless obligation accounting. The
Layer-17 composition candidate is converted through an explicit proof-rule
contract; Layer-17 search rules are never silently promoted.

The controlled suite verifies trusted-fact discharge, open premises, unknown
regimes, falsified and contradicted inputs, shared obligations, cycles, indexed
nilpotence, separate plans for three quotient-distinct Layer-17 candidates,
weak-relation/display-name/numeric/provenance negative controls, deterministic
IDs, and fact-removal replay. The exact counts and statuses are recorded in
`reports/layer18_proof_planning.md`.

The verified result was: **LAYER 18 DoD SATISFIED — LAYER 19 SAFE TO BEGIN**.
That gate authorized the separately scoped implementation recorded below.

## Layer 19 — Verification and Scientific Hygiene

### Purpose

Provide replayable certificates, falsification, symbolic/formal backend
adapters, optional numerical confirmation, novelty hygiene, independence
profiles, hold-out validation, and reproducibility bundles.

### Input

Closed propositions, proof plans, fixed regimes, checker policies, numerical
test policies, benchmark fixtures, and versioned source data.

### Output

- symbolic/formal certificate or explicit failure;
- counterexamples and falsification records;
- isolated numerical support records;
- hold-out/leakage results;
- independence profile;
- reproducibility bundle; and
- final epistemic status history.

### Invariants

- numerics consume but do not steer open discovery;
- test pass is not theorem proof;
- `unsupported`, `not_run`, `inconclusive`, `falsified`, and `verified` differ;
- every evidence record contains proposition/regime/executor hashes; and
- novelty is an external status, never inferred from Atlas absence.

### Dependencies

Layers 15–18, stable provenance, replayable proof plans, scientific fixture
isolation, and explicit test generators. Specific backends remain optional.

### Machine-testable Definition of Done

- symbolic/formal adapter results replay from immutable inputs;
- numerical falsification and numerical support receive distinct labels;
- numerical discovery experiments remain zero in open-search mode;
- hold-out leakage is detected;
- repeated deterministic runs are not counted as independent;
- evidence bundles reconstruct assumptions, derivation, pruning, and status; and
- backend failure never becomes theorem failure without a valid counterexample.

### Required regression tests

- clean proof-gate rejection of incomplete candidates;
- numerical executor isolation;
- unsupported versus failed and not-run versus failed;
- blind benchmark leakage/negative controls;
- reproducibility bundle replay;
- provenance-overlap independence levels; and
- formal/symbolic certificate tamper detection.

### Verified implementation result

Layer 19 is implemented in `opforge::verification::VerificationOrchestrator`.
The exact internal verifier is capability-gated and limited to structured
Layer-15 type/definedness checks, exact literal counterexamples, and bounded
trusted rewrite replay. Numerical verification is explicit post-search
support only. Certificates record Theory/context/regime/input digests and are
replayed against current semantic inputs. `ResultBundle` preserves proof plans,
certificates, counterexamples, numerical evidence, unresolved obligations, and
conservative novelty state.

The controlled suite verifies exact rewrite and typing, unsupported versus
inconclusive behavior, exact versus numerical counterexamples, numerical
support that cannot satisfy a formal requirement, certificate invalidation,
capability mismatch, multiple certificates, ResultBundle determinism, and the
Layer-16/17/open-discovery numerics firewall. The formal backend status remains
`FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED`.

The verified result is: **LAYER 19 DoD SATISFIED — LAYER 20 SAFE TO BEGIN AS A
SEPARATE TASK**. This does not implement Layer 20 or claim a theorem is proved.

## Layer 20 — Practical Utility Gate

### Purpose

Demonstrate bounded practical value on previously unseen, in-scope operator
problems without claiming to solve arbitrary mathematics.

### Input

A frozen real problem with context, assumptions, validity regime, target
judgment, theory snapshot, resource contract, and hold-out policy.

### Output

A small auditable result set or explicit failure report containing:

- context and assumptions;
- validity regime;
- derivation and search provenance;
- proof obligations;
- evidence status;
- unresolved items;
- completeness/termination label; and
- reproducibility bundle.

### Invariants

- no-result is acceptable;
- output size is not optimized by hiding uncertainty;
- utility, truth, novelty, and proof status are separate measurements;
- a bounded failure is not a global impossibility claim; and
- all candidates are auditable and reproducible.

### Dependencies

Layers 15–19 and a frozen held-out problem suite with acceptance criteria.

### Machine-testable Definition of Done

- held-out problems are frozen before evaluation;
- every output is bounded and status-labeled;
- a human auditor can reconstruct the result without hidden campaign state;
- failure, partial, supported, and verified outcomes remain distinct;
- utility report includes proof completion, false-positive, reproducibility,
  audit-time, and failure metrics; and
- no report claims universal mathematical solving.

### Required regression tests

- held-out target-blind and near-miss problems;
- budget-ended versus exhausted-relative reports;
- auditable provenance bundle reconstruction;
- no hidden target/scorer data in discovery;
- utility output with zero serious candidates; and
- preservation of all Phase-0 and Scientific Regression Baseline v1 controls.

### Verified Layer-20 result

The generic Layer-20 harness is implemented in
include/opforge/utility/layer20.hpp and src/utility/layer20.cpp. It evaluates
15 cases through the Layer-17 -> Layer-18 -> Layer-19 handoff, plus a
target-free forward-discovery fixture.

Measured result:

- verdict: LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED;
- structural exact/valid-alternative/partial/miss/false-positive:
  1/6/0/8/0;
- negative controls: 5/5;
- target-blind leakage audit: PASS;
- opaque-ID synthesis: PASS;
- determinism: 3/3;
- legacy open discovery generated/pruned/serious: 62/650/0;
- discovery numerics: 0;
- formal backend: NOT YET IMPLEMENTED.

The budgeted synthesis control is BUDGET_ENDED with relative_complete=false,
while its unbudgeted counterpart is EXHAUSTED_RELATIVE_SPACE. The adjoint
probe remains UNSUPPORTED_LANGUAGE. Real-Atlas transfer is not run as a sound
claim because the structured bridge contract is absent. The complete audit is
in reports/layer20_practical_utility.md and the deterministic summary is in
reports/layer20_practical_utility.json.

## Layer 21 — Generative Operator Synthesis v1

### Purpose

Expand the mathematical construction grammar while keeping Atlas primitives,
typed/context/regime safety, quotienting, proof obligations, verification, and
target blindness separate.

### Implemented scope

Layer 21 adds a generic `ConstructorSchema` catalog with tri-state prerequisite
classification, deterministic cost/depth/provenance, separate open and
goal-directed policies, target-output matching before child expansion, and
generated-expression identity distinct from Atlas operators. Implemented
families are typed composition, adjoint, left/right/two-sided inverse
candidates, commutator, conjugation, and indexed instantiation. Anti-commutator
and restriction/extension are deferred for semantic reasons; unrestricted
linear combinations and tensor/product construction remain disabled.

Every generated construction is sent through the Layer-16 quotient path and
then receives Layer-18 obligations and Layer-19 evidence status. Constructor
creation is not a proof. The formal verification backend remains unavailable.

### Verified Layer-21 result

The target-blind controlled suite has 12 cases, including two opaque-ID
successes, adjoint and inverse UNKNOWN controls, an incompatible commutator,
and a missing tensor constructor. The result is:

- `LIMITED_GENERATIVE_SYNTHESIS_DEMONSTRATED`;
- leakage audit PASS, opaque-ID PASS, false positives 0;
- open discovery raw/valid/invalid/unknown: 144/50/94/0;
- open retained classes/serious/budget-pruned: 62/0/0;
- discovery numerics 0, runtime LLM calls 0;
- deterministic replay 3/3, digest
  `layer21_benchmark_digest.4c26807e3ed03a83`;
- Release CTest 8/8 and ASan/UBSan CTest 8/8; and
- Layer 22 was not part of this Layer-21 result; its separate implementation
  and baseline are recorded below.

The detailed case report is in
`reports/layer21_generative_operator_synthesis.md`; the machine summary is in
`reports/layer21_synthesis_utility.json`. Layer 20's historical result remains
frozen and is not recomputed as a Layer-21 result.

## Layer 22 — Constraint-Guided Mathematical Synthesis v1

### Purpose and boundary

Layer 22 makes synthesis goal/property constrained instead of relying only on
output type. It reuses Layer-15 semantic terms and judgments, extracts a
deterministic `SemanticConstraint` set from the target, propagates supported
requirements backward through Layer-21 constructor schemas, and applies
tri-state constructor applicability before child expansion. It remains a
separate target-directed path; open discovery has no target ConstraintSet and
is unchanged.

Hard constraints may reject only decisive type, regime, index, or structured
form violations. Open proof constraints retain candidates with explicit
`ProofObligation` records. `UNKNOWN` is never `SATISFIED`, and
`UNSUPPORTED` is never converted to false. Constructor contracts now expose
definition-level form guarantees separately from theorem requirements without
changing the frozen Layer-21 canonical output.

The exact v1 ceiling is type equality/definedness, composition intermediate
typing, reversed unary typing, explicit adjoint/commutator/conjugation forms,
represented index offsets, exact regimes, and trusted structured facts. It is
not a theorem prover, arithmetic SMT solver, formal verifier, numerical ranker,
or LLM runtime.

### Verified Layer-22 result

The controlled suite has 11 target-blind cases, two opaque-ID positives, a
false-property negative, an UNKNOWN-property control, and separate left and
two-sided inverse-law cases. The result is:

- `CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED`;
- leakage/scorer isolation PASS; opaque-ID PASS;
- deterministic replay 3/3, digest
  `layer22_benchmark_digest.d148a171b146b533`;
- discovery/synthesis numerics 0, runtime LLM calls 0, and unrestricted
  arbitrary linear combinations disabled;
- scaling type-only Layer-21 compatible / Layer-22 retained classes:
  `39/1`, `132/1`, `279/1` for 3/6/9 operators;
- real Atlas probe: 6 fully structured facts and
  `UNSUPPORTED_CONSTRAINT_LANGUAGE` for the self-adjoint property; and
- Layer 23 is implemented separately below; it was not part of the frozen
  Layer-22 result.

The full case-by-case, candidate-reduction, proof-obligation, scaling, leakage,
and limitation report is in
`reports/layer22_constraint_guided_synthesis.md`; machine output is in
`reports/layer22_constraint_guided_synthesis.json`.

## Layer 23 — Rich Mathematical Semantics and Construction Grammar v2

### Purpose and boundary

Layer 23 makes mathematical space/operator semantics explicit enough for
conservative construction, while retaining the Layer-22 target-directed
constraint boundary. It adds typed space properties and relations, operator
property facts, declared/derived/open/unknown status, reusable trusted rule
schemas, and context/regime-aware goal-directed constructors. Restriction,
tensor, dual-map, adjoint, product, composition, and controlled scalar
descriptors are distinct forms. Extension, pullback/pushforward, arbitrary
coefficient search, and formal theorem proving remain deferred.

### Verified Layer-23 result

The target-blind suite measures:

- `RICH_OPERATOR_SEMANTICS_DEMONSTRATED`;
- 14 controlled cases, including two opaque-ID positives and explicit negative
  controls for missing inclusion, dual-versus-adjoint, partial facts, and
  numerical closeness;
- migration `6` previously fully structured, `321` newly structured, `327`
  cumulative fully structured, `220` partial, and `0` unsupported facts;
- theory size: 47 spaces, 47 space-property facts, 274 operator-property
  facts, 0 explicit space relations, and 4 trusted rule schemas;
- real Atlas probes: 88 linear, 47 space, 0 indexed, and 10
  inverse/commutation facts;
- Layer-21 attempts / Layer-23 attempts `7/21`, `7/78`, `7/171` at 3/6/9
  operators, with 2 retained classes, 0 unknown type decisions, and peak
  retained frontier 2;
- leakage and opaque-ID PASS, discovery numerics 0, runtime LLM calls 0, and
  no partial-fact promotion; and
- deterministic digest
  `layer23_benchmark_digest.6b6b46b6e7002750`.

The rich path is not enabled by the legacy open-discovery policy. Layer-23
constructors are goal-directed, use no scorer/target data, preserve UNKNOWN
and proof obligations, and do not change the frozen Layer-20/21/22 metrics.
Full case disclosure and machine output are in
`reports/layer23_rich_mathematical_semantics.md` and
`reports/layer23_rich_mathematical_semantics.json`.

## Layer 24 — Search Scalability v2

Layer 24 adds constraint-directed lazy mathematical search as a separate path.
The implementation includes deterministic SearchPlan compilation, typed
Theory indexes, bounded backward/forward demand slicing, lazy constructor
expansion, incremental constraints, context/regime/theory cache isolation,
canonical quotient insertion, indexed frontier meetings, explicit UNKNOWN and
resource budgets, and streaming million-scale accounting.

Measured result:

- `SCALABLE_CONSTRAINT_DIRECTED_SEARCH_DEMONSTRATED`;
- 8/8 finite reference-versus-optimized canonical equivalence cases passed;
- finite exhaustive control ended `EXHAUSTED_RELATIVE_SPACE` with conservation
  accounting pass;
- the same finite grammar under budget ended `BUDGET_ENDED` and did not claim
  exhaustion;
- UNKNOWN budget control ended `INCOMPLETE_UNKNOWN` with 10 UNKNOWN and 8
  deferred states;
- distractor scaling held the relevant slice at 4 operators while the full
  theory grew to 1006 operators; and
- the 1,000,000-state structural stress materialized 1 target-relevant state.

Layer 24 does not implement Layer 25, Layer 26, physics, or grammar expansion.
Its full measured report is `reports/layer24_search_scalability_v2.md` and its
machine summary is `reports/layer24_search_scalability_v2.json`.

## Layer Gate policy

Advancement from any layer to the next requires all three gates.

### Software gate

- clean configure/build;
- unit and integration tests;
- sanitizer checks where supported;
- deterministic serialization/replay tests; and
- no unreviewed production behavior change.

### Scientific gate

- blind rediscovery;
- negative controls;
- leakage tests;
- semantic boundary tests;
- explicit epistemic statuses; and
- preservation of honest MISS, partial, unsupported, and not-run outcomes.

### Search gate

- deterministic search;
- scaling benchmark;
- no numerical discovery;
- no unrestricted linear-combination enumeration;
- sound versus heuristic pruning accounting;
- relative-completeness accounting; and
- reproducible termination reason.

A layer is blocked if any older demonstrated capability regresses without an
updated decision record and baseline explanation. The Layer-17 prerequisite
gate for Layer 18 is recorded as satisfied above; Layer 19 remains separately
scoped and must preserve the Layer-18 proof-boundary and replay contracts.
