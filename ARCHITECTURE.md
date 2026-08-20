# OpForge architecture contract v1

This document freezes the repository-level architecture and records the
implementation boundary. Layers 15 and 16 are implemented as parallel semantic
and quotient paths, Layer 17 is a separate deterministic goal-directed reasoning
path, Layer 18 is a separate deterministic proof-planning path, and Layer 19 is
a separate deterministic verification/evidence path; legacy closure/search
migration remains deferred.

The complete adversarial review is
[OPFORGE_ARCHITECTURE_V1_REVIEW.md](reports/OPFORGE_ARCHITECTURE_V1_REVIEW.md).
The current measured state is in [BASELINES.md](BASELINES.md).
The implementation details and gate results are in
[reports/layer15_semantic_core.md](reports/layer15_semantic_core.md),
[reports/layer16_quotient_search.md](reports/layer16_quotient_search.md),
[reports/layer17_bidirectional_reasoning.md](reports/layer17_bidirectional_reasoning.md),
[reports/layer18_proof_planning.md](reports/layer18_proof_planning.md),
and [reports/layer19_verification.md](reports/layer19_verification.md).

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

## 10. Relative search completeness

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

## 11. Layer and gate policy

The corrected Layer 15–20 contract is frozen in [ROADMAP.md](ROADMAP.md).
Every layer must pass the software, scientific, and search gates described
there. A later layer cannot advance if it silently breaks a demonstrated older
capability.

The current state is Phase 0 plus the Scientific Regression Baseline v1,
completed Layer-15 semantic infrastructure, a parallel Layer-16 quotient
search path, a separate Layer-17 goal-directed path, and a separate Layer-18
proof-planning path. None of these parallel paths is wired into legacy
closure/search, so the frozen discovery behavior remains the baseline.
