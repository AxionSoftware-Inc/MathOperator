# OpForge architecture contract v1

This document freezes the repository-level architecture and records the
implementation boundary. Layers 15 and 16 are implemented as parallel semantic
and quotient paths, Layer 17 is a separate deterministic goal-directed reasoning
path, Layer 18 is a separate deterministic proof-planning path, and Layer 19 is
a separate deterministic verification/evidence path. Layer 21 adds a separate
deterministic constructor-synthesis path and Layer 22 adds a separate
deterministic constraint-guided synthesis path; legacy closure/search migration
remains deferred.

The complete adversarial review is
[OPFORGE_ARCHITECTURE_V1_REVIEW.md](reports/OPFORGE_ARCHITECTURE_V1_REVIEW.md).
The current measured state is in [BASELINES.md](BASELINES.md).
The implementation details and gate results are in
[reports/layer15_semantic_core.md](reports/layer15_semantic_core.md),
[reports/layer16_quotient_search.md](reports/layer16_quotient_search.md),
[reports/layer17_bidirectional_reasoning.md](reports/layer17_bidirectional_reasoning.md),
[reports/layer18_proof_planning.md](reports/layer18_proof_planning.md),
and [reports/layer19_verification.md](reports/layer19_verification.md),
plus [reports/layer21_generative_operator_synthesis.md](reports/layer21_generative_operator_synthesis.md).

## 1. Foundational model

The logical core is:

```text
Theory Σ -> Context Γ -> Judgment -> ProofState
```

These are related but different objects:

- **Theory / signature `Σ`** declares spaces, scalar fields, operator symbols,
  predicates, theorem schemas, admissible construction grammar, and the
  versioned evidence sources available to a run.
- **Context `Γ`** contains scoped variables, parameters, local definitions,
  assumptions, and structure hypotheses. A child context may add assumptions;
  a derivation may not silently discard them.
- **Judgment** is a proposition interpreted in a theory, context, and validity
  regime with an explicit kind, epistemic status, provenance, and obligations.
- **ProofState** is a search artifact containing open goals, local context,
  metavariables, alternatives, delayed constraints, discharged obligations,
  policy, budget, and replay history.

The central notation is:

```text
Σ ; Γ ; R ⊢ φ : status
```

This does not mean that `φ` is universally true. It means that `φ` is accepted
or held at the recorded epistemic status under theory `Σ`, context `Γ`, and
validity regime `R`.

OpForge does not attempt to build a universal dependent-type theory. It uses a
minimum bounded indexed/dependent-lite language sufficient for current
operator-centric mathematics: dimensions, grades, bundles, parameter families,
domains, regularity, and structure assumptions.

## 2. Supporting objects and responsibilities

| Object | Responsibility | Must not do |
|---|---|---|
| `Symbol` | Declare a typed operator, object, predicate, scalar, or indexed family | Carry hidden proof status in its name |
| `Variable` | Bind a typed variable or parameter in a scoped context | Be a free string with implicit type |
| `Space/Type` | Describe mathematical objects, indices, grades, scalar fields, and structures | Be treated as an unvalidated label only |
| `OperatorSignature` | Give domain, codomain, arity, order, locality, linearity, regularity, and structure requirements | Prove an operator identity by itself |
| `Expression/Term` | Denote a typed mathematical object or construction | Contain proposition truth as an untyped node |
| `Domain` | Represent where an object/operator is defined | Be silently widened during derivation |
| `Assumption` | Add a scoped hypothesis to `Context` or `ValidityRegime` | Become a theorem merely because it is metadata |
| `Constraint` | Express type, dimension, regularity, parameter, and regime conditions | Convert `unknown` into `false` |
| `ValidityRegime` | Record the conditions under which a judgment applies | Be a free-form afterthought string |
| `Evidence` | Record an observation, derivation artifact, test, or checker result | Upgrade a status without a defined promotion rule |
| `Provenance` | Track source facts, rules, representations, versions, seeds, and checkers | Count repeated replay as independent proof |
| `SearchScope` | Freeze grammar, depth/cost, resources, equivalence theory, rules, and termination policy | Claim global completeness |
| `Judgment` | Assert a typed proposition under `Σ, Γ, R` with status and obligations | Collapse relation kinds into equality |
| `ProofState` | Track goals, branches, constraints, plans, and replayable progress | Treat a partial match as a proof |

## 3. Terms, propositions, and judgments

Terms and propositions are separate sorts even if they share serialization and
hashing.

### 3.1 Terms

The minimum term language includes:

- variables and typed constants;
- operator application and composition;
- zero and identity objects with inferred type;
- addition and scalar multiplication only in a declared algebraic structure;
- adjoint, direct sum, projection, inclusion, and parameter-family
  instantiation where their side conditions are explicit; and
- indexed spaces and operators such as `Form(M,k)` and `d_k`.

Every term carries or can derive:

```text
type, domain/codomain, arity, free variables, validity obligations
```

Examples:

```text
grad : Scalar(R^3) -> Vector(R^3)
Compose(div, grad)
Apply(grad, f)
d_(k+1) : Form(M,k+1) -> Form(M,k+2)
```

### 3.2 Proposition kinds

The judgment kind must be explicit. At minimum:

```text
Eq(t, u)
Implies(P, Q)
Iff(P, Q)
In(t, S)
Defined(t)
ComposeDefined(B, A)
Commutes(A, B)
InverseLaw(B, A)
Annihilates(B, A, zero)
Decomposition(object, components, reconstruction)
Approx(lhs, rhs, error_measure, order, limit)
Corresponds(lhs, rhs, map, direction)
Includes(S, T)
Predicate(name, arguments)
Forall(variable, proposition)
Exists(variable, proposition)
And/Or/Not
```

Examples:

```text
Σ ; Γ ; R ⊢ Eq(Compose(A, B), C)
Σ ; Γ ; R ⊢ Implies(Invertible(A), Eq(Compose(A⁻¹, A), I))
Σ ; Γ ; R ⊢ In(A, End(S))
Σ ; Γ ; R ⊢ ComposeDefined(B, A)
Σ ; Γ ; R ⊢ Approx(T_h, T, norm_error, O(h^p), h -> 0)
```

`RelatedTo`, `AnalogueOf`, `TransformCorrespondence`, `Factorization`, and
`Decomposition` remain semantically distinct. They never become `Eq` solely
because both sides contain expressions.

### 3.3 Validity regimes

A `ValidityRegime` is a structured formula containing, as needed:

- space/domain membership;
- dimension, grade, and scalar-field conditions;
- regularity and differentiability;
- metric, orientation, bundle, geometry, and boundary conditions;
- parameter inequalities and non-degeneracy;
- continuum/discrete status; and
- limit/asymptotic conditions.

Regimes support intersection and entailment with three-valued outcomes:

```text
compatible | incompatible | unknown
```

Unknown entailment or compatibility is preserved as `UNRESOLVED`; it is not
treated as false, overlap, or contradiction.

## 4. Rewrite and equality-closure contract

A judgment may enter trusted equality closure or become a rewrite only when:

1. its kind is exact equality or a certified equality consequence;
2. both sides are well-typed terms of the same type;
3. the context and validity regime are compatible with every premise;
4. all side conditions are explicit and discharged or retained as obligations;
5. its orientation is explicit, or symmetric equality is proven;
6. provenance/evidence meets the configured trusted level; and
7. the rule application is replayable and bounded by the rewrite policy.

The following are not automatic equality rewrites:

- analogy or correspondence;
- implication;
- approximation;
- generic relation metadata;
- endpoint/family similarity;
- decomposition without a proved reconstruction equality; and
- numerical agreement on test cases.

The legacy `executable_equality` flag is a Phase-0 containment boundary. It is
not the permanent proposition model. A Layer-15 adapter must make the semantic
kind, regime, proof status, and rewrite eligibility explicit.

## 5. Search and proof boundary

Structural search may consume only the frozen theory view, typed signatures,
relations as relations, executable propositions, schemas, proof-safe closure,
and residual classifications. It must not call numerical execution or claim
numerical support.

Numerical work is an explicit downstream consumer of a closed candidate and
fixed proposition. It may falsify a declared test instance or provide
special-case support, but it cannot generate, rank, select, or promote open
discovery candidates.

Candidate lifecycle:

```text
typed term
 -> structural class
 -> observation
 -> closed conjecture + regime
 -> proof obligations
 -> symbolic/formal proof plan
 -> falsification
 -> optional isolated numeric support
 -> evidence bundle
```

No transition may silently upgrade epistemic status.

## 6. Target-blind boundary

The legacy open discovery engine must receive an immutable TheoryView and
SearchScope, not a benchmark target, expected operator, expected composition,
family name, benchmark ID, or scorer callback. The Layer-17 goal-directed
engine is intentionally target-directed, so it receives a semantic target
Judgment inside Problem; it still receives no expected answer, benchmark label,
scorer callback, or target-specific solution metadata. The external scorer
evaluates completed output in a separate fixture layer.

This boundary should eventually be enforced by distinct APIs and, where the
scientific claim requires it, separate processes or serialized input channels.
target = "none" is not an architectural guarantee.

## 7. Layer-17 goal-directed boundary

Layer 17 uses Theory + Context + target Judgment + GoalSearchScope + SearchPolicy
as its machine-readable problem boundary. The forward frontier delegates
construction typing and quotient reduction to Layer 16. The backward frontier
accepts only explicit safe non-heuristic rules, and its AND/OR search records
goal states, substitutions, constraints, provenance, and frontier meetings.
Matching is typed and returns MATCH, NO_MATCH, or UNKNOWN; unknown is never a
match or a rejection.

This path does not select numerical experiments, call an LLM, enumerate
unrestricted linear combinations, or construct proof-obligation DAGs during
search. Layer 18 consumes its output only after search has completed. Its
output is a structural candidate with a relative search status, not a formal
proof.

## 8. Layer-18 proof-planning boundary

Layer 18 consumes an actual Layer-17 `SolutionCandidate` and retained search
snapshots after discovery. It produces a `ProofPlan` containing semantic
obligations, explicit rule applications, evidence nodes, AND/OR alternatives,
context/regime-aware edges, provenance, replay data, and lifecycle accounting.
Structural evidence is never promoted to formal proof. Theory facts must pass
typed semantic matching, regime/side-condition checks, provenance, and
evidence gates; analogy, correspondence, approximation, display names,
numeric support, and partially structured legacy relations cannot discharge
exact obligations. Missing rule provenance is an explicit unsupported
obligation. The plan is backend-neutral and does not call an LLM, numerical
engine, or formal prover.

The same semantic obligation ID is retained across lifecycle changes. Shared
obligations are deduplicated by semantic target/context/regime, while separate
rule alternatives remain separate DAG nodes. Cycles are recorded and make the
plan `CYCLIC`; replay recomputes evidence and reopens obligations when a fact or
certificate disappears.

## 9. Layer-19 verification and evidence boundary

Layer 19 consumes a closed Layer-18 `ProofPlan` through structured
`VerificationRequest` objects. It selects only explicitly declared verifier
capabilities, records `VerificationCertificate` payloads, replays them against
the current Theory, and updates the plan only through the evidence firewall.
Exact internal replay can establish only the bounded Layer-15 structured
fragment. Numerical support is available only from this post-search layer and
cannot discharge symbolic or formal requirements. Exact counterexamples and
numerical suspicious cases are distinct. No Layer-19 result is visible to
candidate generation, ranking, discovery frontiers, or quotient equivalence.

`ResultBundle` is the deterministic handoff artifact for a future utility layer.
Novelty remains an explicit external status; Atlas absence is not novelty.
There is no production formal prover in Layer 19.

## 10. Layer-21 generative constructor boundary

Layer 21 separates Atlas knowledge from generative vocabulary:

```text
Theory / Atlas primitives
  + ConstructorSchema catalog
  + typed/context/regime constraints
  + goal-directed constructor matching
  + Layer-16 quotient
  + Layer-18 obligations / Layer-19 verification
```

`ConstructorSchema` is first-class and records its stable family ID, arity,
input requirements, output-type derivation, context/regime requirements,
parameter/index constraints, generated side conditions, cost/depth,
provenance, open-discovery availability, goal-directed availability, and
UNKNOWN policy. Constructor application has a tri-state prerequisite result:
`VALID`, `INVALID`, or `UNKNOWN`. INVALID applications do not become valid
candidates; UNKNOWN applications are admitted only by an explicit
goal-directed policy and retain unresolved obligations.

The v1 catalog implements typed composition, adjoint candidates, left/right/
two-sided inverse candidates, explicit commutators, conjugation candidates, and
indexed family instantiation. Anti-commutator and restriction/extension remain
deferred because the current semantic core cannot represent their side
conditions soundly. Arbitrary coefficient enumeration is disabled.

Open discovery enables only composition and indexed instantiation by default.
Goal-directed matching uses the target output type before child expansion, then
passes every generated construction through Layer 16. A generated expression
is distinct from an Atlas primitive and carries its schema, child IDs,
indices, context/regime digest, obligations, provenance, and depth/cost.
Constructor creation does not prove adjointness, invertibility, commutator
legitimacy, or transport theorems. Layer 18/19 can leave these obligations
OPEN, UNKNOWN, or UNSUPPORTED.

## 10A. Layer-22 constraint-guided boundary

Layer 22 consumes the same machine-readable `Theory + Context + target
Judgment` problem boundary and adds a reusable `SemanticConstraint` set. The
constraint model wraps Layer-15 terms and judgments; it does not parse natural
language or create a parallel proposition language. Requirements have explicit
hard/open strength, provenance, graph dependencies, substitutions, and one of
`SATISFIED`, `VIOLATED`, `UNKNOWN`, or `UNSUPPORTED` status.

Goal extraction is deterministic. Constructor applicability is tri-state and
is evaluated before child expansion. Supported exact propagation covers type
and composition compatibility, reversed unary types, explicit constructor
forms, represented index offsets, and exact regime checks. Inverse laws,
adjoint identities, commutation theorems, and conjugation transport remain open
unless an already trusted structured fact discharges them. UNKNOWN is retained
only under an explicit policy and always becomes a proof obligation; it is not
an exact solution or a rejection.

The Layer-22 path is separate from open discovery. It never receives benchmark
answers, scorer callbacks, benchmark IDs, expected properties, numerical
results, or runtime LLM output. It does not add unrestricted linear
combinations. Its relative search status is scoped to the recorded theory,
context, regime, constructor grammar, constraint language, propagation rules,
depth, cost, budget, and equivalence contract.

The measured result is
`CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED`; the real Atlas probe remains
`UNSUPPORTED_CONSTRAINT_LANGUAGE` at the current six fully structured facts.
Details are in
[reports/layer22_constraint_guided_synthesis.md](reports/layer22_constraint_guided_synthesis.md).

## 10B. Layer 23 rich mathematical semantics boundary

Layer 23 is a separate semantic path above the frozen Layer-22 constraint
contract. Atlas migration records explicit facts conservatively: parser
defaults are not treated as declared mathematical properties, metric metadata
is not silently promoted to an inner-product law, and partial facts remain
available only as partial evidence. The rich theory distinguishes space
properties/relations, operator properties, declared facts, derived facts,
open obligations, and unknown decisions.

The trusted rule catalog is typed and small: composition linearity,
composition invertibility, tensor typing/linearity, and restriction from an
explicit inclusion. Restriction requires `U inclusion V`; tensor requires
explicit tensor-capable spaces; dual maps require explicit dual-space
relations; adjoints require explicit inner-product spaces. Pullback/pushforward
and Extension are deferred when their side conditions are not explicit.

Rich constructors are goal-directed and target-blind. They receive a typed
problem and constraints, never a benchmark target ID, expected expression,
family, or scorer callback. No rich constructor is enabled in the legacy open
discovery policy. The Layer-22 bridge exposes rich declared/derived property
facts as ordinary semantic observations for entailment, while preserving
`UNKNOWN` and proof obligations.

The measured Layer-23 result is
`RICH_OPERATOR_SEMANTICS_DEMONSTRATED`; the exact probes, opaque-ID test,
negative controls, scaling metrics, and deferred boundaries are recorded in
[reports/layer23_rich_mathematical_semantics.md](reports/layer23_rich_mathematical_semantics.md).

## 10C. Layer 24 constraint-directed lazy search

Layer 24 is a separate goal-directed search path. Its deterministic SearchPlan
compiles Theory, Context, ValidityRegime, target constraints, rich semantic
facts/rules, constructor schemas, bounds, and equivalence identity. Theory
indexes provide typed operator/property/space/relation/indexed-family lookup.
Forward generation is demand-directed and lazy; canonical expression identity,
type/property caches, and context/regime/theory digests isolate replay and
mutation. A bounded forward/backward demand graph supplies relevant slices and
indexed frontier meetings. Internal multi-step operands are retained whenever
final-output filtering would be unsound.

The implementation keeps the search engine in `src/search/layer24.cpp` and the
benchmark text/JSON serialization in `src/search/layer24_report.cpp`; report
formatting is therefore not coupled to discovery execution.

Reference and optimized searches must agree on exact and fully explored UNKNOWN
canonical sets for each declared finite grammar. Resource limits produce
BUDGET_ENDED; bounded UNKNOWN deferral produces INCOMPLETE_UNKNOWN. No target
ID, expected expression, benchmark identity, scorer, numerical experiment, or
runtime LLM enters the search path. Partial Layer-23 facts remain outside
trusted pruning/equality.

The measured Layer-24 result is
SCALABLE_CONSTRAINT_DIRECTED_SEARCH_DEMONSTRATED. The full case disclosure,
finite ledger controls, distractor scaling, real-Atlas probe, and million-scale
stress are in
[reports/layer24_search_scalability_v2.md](reports/layer24_search_scalability_v2.md).

## 11. Relative search completeness

Completeness is always relative to a frozen contract:

```text
grammar G
depth/cost bounds D
resources R
equivalence theory E
Theory/Atlas version Σ
context/regime Γ,R
allowed rules T
```

The search must distinguish:

- `EXHAUSTED_RELATIVE_SPACE`: all legal constructions under the contract were
  processed without heuristic deletion or unresolved branches;
- `BUDGET_ENDED`: resources stopped search;
- `TRUNCATED_BY_POLICY`: heuristic frontier/subsumption pruning removed work;
- `INCOMPLETE_UNKNOWN`: a constraint or entailment decision was unsupported;
- `UNSUPPORTED_FRAGMENT`; and
- `FAILED`.

Lossless canonicalization, alpha-equivalence, certified symmetry quotienting,
and proof-backed theorem consequence elimination are reported separately from
heuristic ranking, top-N, frontier, and resource pruning.

## 12. Layer and gate policy

The corrected Layer 15–20 contract is frozen in [ROADMAP.md](ROADMAP.md).
Every layer must pass the software, scientific, and search gates described
there. A later layer cannot advance if it silently breaks a demonstrated older
capability.

The current state is Phase 0 plus the Scientific Regression Baseline v1,
completed Layer-15 semantic infrastructure, a parallel Layer-16 quotient
search path, a separate Layer-17 goal-directed path, and a separate Layer-18
proof-planning path. None of these parallel paths is wired into legacy
closure/search, so the frozen discovery behavior remains the baseline.
