# OpForge Layer 15–20 roadmap

This roadmap is frozen at the architecture level. Layers 15, 16, 17, and 18
are implemented within their recorded scopes; Layer 19 remains separately
scoped. The foundational review is
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
