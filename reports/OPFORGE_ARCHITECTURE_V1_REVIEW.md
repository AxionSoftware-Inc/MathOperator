# OPFORGE_ARCHITECTURE_V1_REVIEW

Date: 2026-08-20  
Scope: adversarial architecture and research review only.  
Implementation status: no production source code was modified; Layer 15 was not implemented; benchmarks and rediscovery scores were not optimized.

## Executive verdict

The proposed Typed Proposition/Expression IR is necessary, but it is not the
correct foundational abstraction by itself. An AST can describe a mathematical
sentence without saying:

- which theory and axioms make the sentence meaningful;
- which variables and declarations are in scope;
- under which domain, regularity, geometry, boundary, and parameter regime it is
  valid;
- whether the sentence is an assumption, a goal, a derived judgment, a rewrite
  rule, a conjecture, or merely an analogy;
- what proof obligations justify it; or
- whether two derivations are actually independent.

The foundation should therefore be a small typed judgment kernel, not a larger
collection of proposition node types:

```text
Theory/signature Σ
  + context Γ
  + validity regime R
  + typed terms and propositions
  + proof/evidence state
        ↓
judgment: Σ ; Γ ; R ⊢ proposition : epistemic-status
        ↓
proof state / search state / evidence bundle
```

The near-term recommendation is to proceed with a narrow, test-first Layer 15
kernel, while explicitly freezing search-engine improvement. Improving the
current search before this kernel would make the system better at exploiting
the current representation's accidental correlations. The audited `1/6`
genuine structural recovery is not evidence for a strong discovery engine, but
it is sufficient to expose the representation failures that must be repaired
before scaling search.

The most important architectural correction is this:

> A proposition is syntax. A judgment is a proposition interpreted in a theory,
> context, and validity regime with explicit evidence and obligations.

## 1. Critical flaws in the current plan

### 1.1 “Proposition IR” is underspecified

The current plan names the right symptom but not the kernel boundary. A
`Proposition` node is not enough because the same expression can be:

- an assumption in one context;
- a goal in another;
- a theorem under a restricted regime;
- a false statement outside that regime;
- an approximation rather than an equality; or
- an analogy that must never enter equality closure.

If Layer 15 only adds `Proposition`, `Quantifier`, and `ProofObligation` nodes
while retaining untyped contexts and string-valued conditions, the system will
have a more expressive syntax tree but the same epistemic failure mode.

### 1.2 The current AST mixes terms, operators, and propositions

The current model's `Expression` node contains operator references,
composition, addition, scalar multiplication, zero, identity, adjoint, direct
sum, and equality in one enum. This is a useful Phase-0 containment mechanism,
but it is not a safe mathematical language boundary:

- equality is a proposition, not an operator term;
- `ZeroOperator` and `IdentityOperator` require a type/domain;
- composition is only meaningful when codomain and domain match;
- addition requires a common hom-space and algebraic structure;
- adjoint requires an inner-product or duality regime;
- a parameter reference cannot be treated like an operator reference; and
- quantification, predicates, implication, negation, and indexed objects are
  absent.

The current `Expression::Kind` list is therefore not a sufficient basis for
proof planning or sound rewriting.

### 1.3 `executable_equality` is a valuable guard, not a semantic foundation

The boolean distinction between executable equalities and heterogeneous
semantic statements repaired a serious problem. It must not become the final
architecture. A boolean cannot express:

- equality versus equivalence versus isomorphism;
- a conditional equality with side conditions;
- an equality valid only on a subdomain;
- a rewrite theorem with a permitted orientation;
- an approximation with an error order;
- a relation that is informative but not substitutive; or
- a statement whose proof status is unresolved.

The replacement must make illegal transitions unrepresentable or at least
require an explicit proof-producing adapter.

### 1.4 Current relation metadata is still too close to executable semantics

`OperatorRelation` contains a `RelationKind`, target ID, and string condition and
evidence. This is appropriate as Atlas evidence, but a relation kind must not
implicitly determine a theorem. `Factorization`, `Decomposition`, `RelatedTo`,
`AnalogueOf`, `TransformCorrespondence`, and `Implies` have different logical
effects. They cannot share a generic “identity-like” path.

The architecture needs a typed distinction between:

1. a fact about two objects;
2. a proposition that can be proved or assumed;
3. a theorem schema that can derive new judgments; and
4. a rewrite rule whose use is sound under explicit side conditions.

### 1.5 Regimes are not just metadata fields

Domain, regularity, geometry, boundary, dimension, scalar field, orientation,
and parameter constraints determine whether an operator and a theorem are
defined. They form a logical regime. Storing them as unrelated strings and
copying a subset into a derived record is not enough.

The current closure implementation visibly derives assumptions from operator
`required_structures`, but it does not provide a general entailment engine for
identity assumptions, regularity constraints, dimension constraints, boundary
conditions, and parameter inequalities. A future layer must not silently drop
these conditions when applying a rule.

### 1.6 The current contradiction test is too strong and not regime-aware

The current closure logic treats a second conclusion with the same left key and
a different right key as an incompatible conclusion. That is not a valid
general contradiction test. `A = B` and `A = C` are not contradictory unless the
context also establishes that `B` and `C` are unequal, or the equality classes
are otherwise inconsistent. The current check also does not establish that the
two conclusions have overlapping compatible regimes.

The correct result in the absence of a regime entailment or inequality proof is
`UNRESOLVED` or `POTENTIAL_CONFLICT`, not `CONTRADICTION`.

### 1.7 Search quotienting is being discussed before equivalence is specified

Canonicalization, e-graphs, symmetry quotienting, and dominance are not
foundations. Each assumes a precisely defined equivalence or implication
relation. If the system has not specified:

- the term language;
- the theory of equality;
- the validity regime attached to each equality;
- the admissible symmetry group; and
- the objective for which dominance is safe,

then “quotient search” can silently delete mathematically relevant candidates.

### 1.8 Target blindness is currently a harness boundary, not an architectural capability boundary

The scientific regression harness demonstrates good target-blind behavior, but
the long-term engine should make target access impossible through its API. A
convention such as `target = "none"` is not sufficient. A campaign may still
receive target-specific strings, benchmark IDs, or expected structures through
memory, actions, scorer callbacks, or serialized checkpoints.

The discovery kernel should receive an immutable `TheoryView` and a search
policy, while the scorer should receive a completed result and a private
fixture. These should be different types and different process boundaries if
the claim is important.

### 1.9 Independence is not solved by repeated deterministic campaigns

Three identical deterministic runs demonstrate reproducibility, not
independent derivation. Distinct campaign IDs, seeds, or frontier orders do not
create independent evidence if all runs use the same facts, rules, canonicalizer,
and proof mechanism.

The architecture needs provenance overlap analysis and an explicit independence
scale. “Independent lineages” must not mean merely “different paths in one
shared derivation DAG.”

### 1.10 Practical utility is not a final feature toggle

Layer 20 should not be defined only as “return a small set of serious
candidates.” The utility gate needs an explicit task contract, acceptance
criteria, hold-out problems, human-audit time, proof status, and failure
handling. Otherwise the system can optimize for small output and hide search
failure.

### 1.11 Current pattern confidence is not an epistemic status

Pattern confidence values such as `0.45`, `0.75`, and `0.9` are useful ranking
signals, but they do not mean probability of truth. A structural family pattern
and a formally verified theorem cannot share an undifferentiated confidence
scale.

The architecture must separate:

- heuristic priority;
- logical derivability;
- empirical support;
- formal verification; and
- unresolved uncertainty.

### 1.12 Numerics isolation is necessary but not sufficient

Preventing numerical experiments from generating candidates is correct. It does
not by itself define what a numerical result means. Discretization error,
boundary treatment, floating-point stability, test-generator coverage, and
executor correctness must be part of the evidence record. “Passed several
samples” must never become “verified.”

## 2. Correct foundational model

The recommended kernel has six separate concepts.

### 2.1 Theory/signature `Σ`

`Σ` declares the vocabulary and rules available to a run:

- sorts and spaces;
- scalar fields and algebraic structures;
- operator and constant symbols;
- indexed families and parameters;
- predicates and relation symbols;
- theorem schemas and rewrite rules;
- admissible construction grammar; and
- versioned source/provenance identifiers.

`Σ` is not the Atlas itself. The Atlas is one source of declarations and facts;
the kernel must preserve whether a declaration came from an axiom, definition,
curated theorem, imported source, or generated consequence.

### 2.2 Context `Γ`

`Γ` contains scoped declarations and assumptions:

- variables and their types;
- parameter declarations;
- local definitions;
- hypotheses and inequalities;
- typeclass-like structure assumptions such as metric, orientation, or
  inner-product structure; and
- local names for objects introduced by a proof step.

Contexts are ordered and scoped. A child context may add assumptions; it may not
silently remove assumptions from a parent derivation.

### 2.3 Validity regime `R`

`R` is a structured condition under which a judgment is claimed. It may include:

- space and domain membership;
- dimension and grade constraints;
- scalar-field and characteristic constraints;
- regularity and differentiability;
- geometry, metric, orientation, and boundary conditions;
- parameter inequalities and non-degeneracy conditions;
- discretization or continuum status; and
- limiting or asymptotic conditions.

`R` is a logical object with an explicit compatibility result. It is not a
free-form string attached after derivation.

### 2.4 Terms and propositions

Terms denote mathematical objects. Propositions assert facts about terms.
Their grammars must be separate even if they share serialization and hashing.

### 2.5 Judgment

The central unit is a judgment:

```text
J = (theory Σ, context Γ, regime R, proposition φ,
     polarity, epistemic status, provenance, obligations)
```

The notation

```text
Σ ; Γ ; R ⊢ φ
```

means that `φ` is accepted in that theory, context, and regime with the
recorded evidence. It does not mean that `φ` is universally true.

### 2.6 Proof state

A problem-solving state is not merely a proposition:

```text
S = (Σ, Γ, R, local-definitions, open-goals,
     discharged-obligations, rule-policy, resource-policy, provenance)
```

A proof state can contain several AND-goals, alternative OR-branches,
metavariables, delayed side conditions, and unresolved regime checks.

## 3. Proposition and problem representation

### 3.1 Common proposition representation

Propositions and goals should share the same proposition AST. This avoids a
second, incompatible language for “the thing we want to prove.” A goal is a
proposition with metavariables and a proof-state status, not a different truth
kind.

### 3.2 A problem is more than `Γ + target`

The recommended representation is:

```text
Problem = {
  theory_snapshot: Σ,
  context: Γ,
  validity_regime: R,
  target: Proposition,
  metavariables,
  admissible_rules,
  search_contract,
  evidence_policy,
  resource_budget,
  output_contract
}
```

`Γ + target` is the mathematical core, but not the complete computational
problem. The same target under a different theory, rule set, regime, or proof
budget is a different problem.

### 3.3 Recommendation

Use one proposition language, but three distinct containers:

1. `TheoremStatement`: a closed or parameterized proposition with regime;
2. `Problem`: a theorem statement plus context, metavariables, policy, and
   budget;
3. `ProofState`: a mutable search artifact containing open goals and proof
   history.

Do not encode a goal by setting a string field such as `target = "none"`, and
do not let a `Problem` be serialized as an untyped bag of strings.

## 4. Minimum serious mathematical language

The near-term scope should be operator-centric, not universal mathematics. The
following is the smallest language that can faithfully represent the current
operator families and their important limitations.

| Concept | Required representation | Operator-centric example |
|---|---|---|
| Symbol | Typed declaration with stable ID, sort, parameters, provenance | `grad : Scalar(R^3) -> Vector(R^3)` |
| Space/type | Structured type, possibly indexed or bundled | `Form(M,k)`, `Matrix(n)` |
| Domain | First-class domain object and membership predicate | `x ∈ C^2(M)` |
| Variable | Scoped binder with type and optional regime | `f : C^2(R^3)` |
| Constant/object | Typed term declaration | `0 : Vector(R^3)` |
| Expression/term | Typed AST with inferred type and regime obligations | `div(grad(f))` |
| Operator application | Application node with arity, domain/codomain, side conditions | `Apply(div, Apply(grad,f))` |
| Composition | Term constructor with composability proof obligation | `Compose(div,grad)` |
| Parameter family | Family symbol, parameter space, body, constraints | `L_alpha = alpha_0 I + alpha_1 Δ` |
| Predicate | Typed predicate symbol/application | `Linear(A)`, `Preserves(A,V)` |
| Quantification | Scoped `forall`/`exists` over typed variables and parameters | `∀ f : C^2(M), ...` |
| Assumption | Context entry or regime formula, never hidden metadata | `orientation(M)` |
| Equality | Proposition `Eq(t,u)` with equal types and regime | `Eq(Compose(div,grad), Laplace)` |
| Implication | Proposition `Implies(P,Q)` | `Invertible(A) -> ...` |
| Equivalence | Proposition `Iff(P,Q)` or typed isomorphism, not automatic equality | `Iff(Closed(P), Exact(P))` |
| Inclusion | Predicate or typed morphism between spaces/subobjects | `Incl(Symmetric(n), Matrix(n))` |
| Commutation | Derived proposition with explicit composability | `Eq(A∘B,B∘A)` |
| Inverse law | Conditional equality with inverse and identity types | `Eq(B∘A,I)` under `Inverse(B,A)` |
| Annihilation/nilpotence | Conditional equality to typed zero | `Eq(d_(k+1)∘d_k,0)` |
| Decomposition | Equality plus reconstruction/orthogonality obligations | `Eq(P_sym+P_skew,I)` |
| Approximation | Relation with metric, error, order, and limit regime | `Approx(T_h,T, O(h^p))` |
| Correspondence/analogy | Typed relation predicate with direction and evidence | `Corresponds(grad, discrete_grad)` |
| Validity regime | Structured formula and compatibility/entailment status | `R3 ∧ C^2 ∧ boundary=none` |

### 4.1 Indexed spaces are mandatory

The current exterior-derivative limitation demonstrates why plain string space
IDs are insufficient. `d : Form(M,k) -> Form(M,k+1)` must carry the grade
index. The composition `d_(k+1) ∘ d_k` is well-typed even though two generic
`form.m.k -> form.m.k1` strings may not be composable.

The first implementation need not support arbitrary dependent type theory. It
does need a bounded indexed-type mechanism for grades, dimensions, bundles,
and parameterized operator families.

### 4.2 Propositions are not relation records

`RelatedTo`, `AnalogueOf`, `ContinuousAnalog`, and
`TransformCorrespondence` should serialize as typed relation propositions or
evidence records. They must not become `Eq` unless a separate theorem proves an
equality and supplies the regime and obligations.

### 4.3 Approximation is not a weak equality

Approximation needs at least:

```text
Approx(lhs, rhs,
       error_measure,
       asymptotic_parameter,
       order_or_bound,
       limit_regime)
```

It must not enter exact equality closure, canonical equality classes, or exact
rewrite rules.

## 5. Rewrite safety

### 5.1 Rules that may become rewrites

Only a proposition with all of the following may become a rewrite rule:

1. both sides are well-typed terms of the same type;
2. the proposition is an equality or a certified equality consequence;
3. all premises and side conditions are explicit;
4. the validity regime is inherited and checked;
5. the rule has a proof/evidence status sufficient for the requested use;
6. orientation is explicit; and
7. termination or bounded application policy is recorded.

An equality may be used in both directions only when symmetry is part of the
trusted equality kernel. A derived equality can be a rewrite rule only if its
derivation is replayable.

### 5.2 Default treatment by proposition kind

| Proposition kind | Default action | Rewrite eligibility |
|---|---|---|
| Exact equality | Register typed equality theorem | Yes, with regime and side conditions |
| Certified equivalence | Use as `Iff`/isomorphism reasoning | Only after a typed equality or explicit conversion theorem |
| Implication | Create a forward derivation rule | No direct term rewrite |
| Inclusion | Create coercion/subtype obligation | No equality rewrite |
| Commutation | Rewrite only its two composed terms | Only under its stated regime |
| Inverse law | Rewrite composition to identity | Only with inverse and domain conditions |
| Annihilation/nilpotence | Rewrite to typed zero | Only under exact hypotheses |
| Decomposition | Rewrite only if reconstruction equality is proven | Otherwise relation/obligation |
| Approximation | Propagate error/limit obligations | Never exact rewrite |
| Correspondence/analogy | Generate an analogy or transfer obligation | Never exact rewrite |
| Metadata similarity | Search heuristic only | Never rewrite |

### 5.3 Rule object

A rewrite rule should contain at least:

```text
RewriteRule {
  lhs, rhs,
  premises,
  side_conditions,
  validity_regime,
  orientation,
  proof_reference,
  soundness_class,
  termination_weight,
  source_theory_version
}
```

`soundness_class` must distinguish `axiom`, `formally_replayed`,
`symbolically_derived`, `heuristic_normalization`, and `untrusted_proposal`.
Only the first three may enter the trusted equality kernel.

## 6. Contexts and validity regimes

### 6.1 Representation

Represent a regime as a typed formula, not as a flat list:

```text
R = And(
  InSpace(f, C2(M)),
  Dimension(M, 3),
  HasStructure(M, Metric),
  Regularity(f, 2),
  BoundaryCondition(M, None),
  Nonzero(alpha)
)
```

The formula may be normalized into a bounded internal form, but the original
structured form and provenance must be retained.

### 6.2 Context and regime operations

| Operation | Required semantics |
|---|---|
| Extend | Add a declaration or assumption to a child context |
| Inherit | Derived judgment receives the conjunction of premise regimes and rule side conditions |
| Intersect | Compute `R1 ∧ R2`; return compatible, incompatible, or unknown |
| Weaken | Remove assumptions only with an entailment proof |
| Strengthen | Add assumptions; the result is narrower, never globally stronger |
| Compare | Return entails, reverse-entails, equivalent, incompatible, or unknown |
| Close | Derive only consequences justified by the configured theory and budget |

Unknown entailment must not be treated as false. Unknown compatibility must not
be treated as overlap or contradiction.

### 6.3 Contradiction policy

Report `CONTRADICTION` only when all conditions hold:

1. propositions have compatible types and overlapping scopes;
2. the combined regime is proven compatible;
3. one judgment asserts `P` and another asserts `Not(P)`, or a trusted theory
   proves an explicit inequality incompatible with the equality; and
4. neither proposition is merely a heuristic observation.

Otherwise report one of:

- `POTENTIAL_CONFLICT`: same typed subject, incompatible-looking results, but
  regime overlap or inequality is unresolved;
- `DISJOINT_REGIMES`: the statements apply to different regimes;
- `INCOMPARABLE`: types or theories do not align; or
- `UNRESOLVED`.

### 6.4 Regime inheritance example

For a theorem

```text
R1 ⊢ d_(k+1) ∘ d_k = 0
```

the derived judgment must retain the grade compatibility, differentiability,
manifold/complex structure, and any boundary assumptions in `R1`. It may be
used under `R2` only if the kernel proves `R2 ⊢ R1` or explicitly records the
conversion theorem. Dropping `R1` during canonicalization is unsound.

## 7. Million-scale search: corrected reduction pipeline

The search should reduce a mathematically specified space before ranking. The
pipeline must report every reduction and its soundness class.

### 7.1 Pipeline

| Stage | Operation | Guarantee |
|---|---|---|
| 0 | Freeze `Σ, Γ, R, G, D, E, T` | Reproducible search contract |
| 1 | Typed grammar generation | Lossless relative to grammar and complete type checker |
| 2 | Definedness and constraint propagation | Lossless only when the solver is complete; unknown is retained |
| 3 | Alpha-normalization of binders | Lossless |
| 4 | Stable AST/structural hashing | Lossless cache key, not a semantic proof |
| 5 | Canonicalization under certified theory `E` | Lossless relative to `E`; never assume unproved AC laws |
| 6 | Certified symmetry/orbit quotienting | Lossless only for a proven goal/regime-preserving group action |
| 7 | Guarded congruence closure/e-graph saturation | Sound for supplied rules; incomplete if bounded |
| 8 | Theorem-consequence elimination | Lossless only with an entailment/proof certificate |
| 9 | Subsumption/dominance | Heuristic unless the search objective proves dominance safe |
| 10 | Frontier, time, memory, and beam pruning | Heuristic/incomplete; never reported as exhaustion |

The system should interleave stages 1–8 instead of generating a huge opaque
pool and applying a score. A typed signature automaton, constraint propagation,
memoized subproblem results, and incremental quotient classes are preferable to
unrestricted enumeration.

### 7.2 Canonicalization rules

- Alpha-equivalence is lossless.
- Associativity and commutativity are lossless only when declared in `E` for a
  specific operation and regime.
- Sorting equality sides is safe for equality identity, but not for implication,
  approximation, correspondence, or directed rewriting.
- Reordering a composition is never safe merely because two operators share a
  signature.
- Scalar normalization requires a declared scalar theory; it cannot assume
  characteristic zero or nonzero coefficients.
- Symmetry quotienting must preserve operator roles, target goals, regimes, and
  proof obligations, not only an unlabeled graph.

### 7.3 E-graphs: useful but not foundational

E-graphs can compactly represent many equivalent terms, but they do not solve:

- conditional rewrite side conditions;
- regime-indexed equality;
- dependent/indexed types;
- proof obligations for theorem application;
- infinite saturation; or
- the difference between a theorem and a heuristic similarity rule.

OpForge should use a guarded, proof-producing congruence structure only after
the judgment kernel exists. An e-graph class must carry the regime and proof
certificates for every merge. Unjustified merges are not quotienting; they are
information loss.

### 7.4 Lossless versus heuristic pruning

The search report must have two ledgers:

```text
sound_reductions:
  candidate -> reason -> proof/rule -> affected equivalence class

heuristic_pruning:
  candidate/class -> policy -> budget -> score/order -> discarded state
```

No candidate discarded by heuristic pruning may contribute to an
`EXHAUSTED_SEARCH_SPACE` claim. A later run must be able to replay the same
reduction decision and distinguish a canonical duplicate from a budget loss.

## 8. Completeness accounting

Global mathematical completeness is not realistic. The honest claim is:

> Complete relative to grammar `G`, maximum depth/cost `D`, resources `R`,
> equivalence theory `E`, theory snapshot `Σ`, context/regime `(Γ,R)`, and
> rule set `T`.

### 8.1 Search certificate

Every search must export:

```text
SearchCertificate {
  theory_hash,
  context_hash,
  regime_hash,
  grammar_hash,
  depth_and_cost_limits,
  equivalence_theory_hash,
  allowed_rule_hash,
  generator_version,
  lossless_reduction_counts,
  heuristic_pruning_counts,
  unknown_constraints,
  resource_usage,
  termination_reason,
  replay_hash
}
```

### 8.2 Required termination labels

| Label | Meaning |
|---|---|
| `EXHAUSTED_RELATIVE_SPACE` | All legal constructions under the frozen contract were processed; no heuristic pruning or unresolved generator branch remains |
| `BUDGET_ENDED` | Time, memory, candidate, frontier, or experiment budget stopped search |
| `TRUNCATED_BY_POLICY` | A deliberate top/frontier/subsumption policy removed branches; completeness is unavailable |
| `INCOMPLETE_UNKNOWN` | A constraint, entailment, or type decision was undecidable/unsupported |
| `UNSUPPORTED_FRAGMENT` | The expression or proposition is outside the implemented language |
| `FAILED` | Execution or input failure |

“No candidates” is meaningful only together with one of these labels. In
particular, zero serious candidates after a bounded open campaign is not an
exhaustion claim.

### 8.3 Relative completeness test

For a small finite grammar, OpForge should have a reference enumerator and a
quotient enumerator. The quotient result is acceptable only when:

1. every reference term maps to a quotient class;
2. every removed term has a recorded lossless proof;
3. every class representative has a replayable canonical form; and
4. heuristic pruning is disabled for the exhaustion test.

## 9. Bidirectional mathematical reasoning

### 9.1 Forward frontier

Forward reasoning starts from judgments in `Γ` and `Σ`, applies typed
construction and theorem rules, and emits new judgments with proof terms and
regimes.

### 9.2 Backward frontier

Backward reasoning starts from a goal proposition and applies admissible
introduction or inversion rules. Each rule produces:

- zero or more subgoals;
- metavariable/type constraints;
- regime side conditions;
- a proof-construction template; and
- a provenance edge.

Backward search is not allowed to “assume” the conclusion. It only transforms a
goal into obligations whose conjunction is sufficient for the original goal.

### 9.3 AND/OR proof graph

The central structure should be an AND/OR graph:

- OR nodes represent alternative derivations;
- AND nodes represent obligations that all must be discharged;
- delayed nodes represent unresolved type or regime constraints; and
- failed nodes retain a reason and evidence.

### 9.4 Meeting condition

Forward and backward frontiers may meet only when they produce the same typed
judgment modulo certified `E`, with compatible contexts and regimes. A shared
operator signature or endpoint family is not a proof meet.

The meet record must include:

```text
forward_judgment,
backward_goal,
unifier,
regime_entailment,
equivalence_certificate,
remaining_obligations
```

### 9.5 Unification scope

Start with typed first-order and bounded indexed unification. Do not promise
general higher-order unification or arbitrary dependent type inference. When
unification is incomplete, return `UNRESOLVED` and preserve the constraint.

## 10. Independent derivation

“Independent” should be an evidence classification, not a binary fact claimed
from repeated runs.

### 10.1 Provenance representation

Every judgment should have a provenance DAG whose leaves identify:

- source facts/axioms;
- theory and rule versions;
- representation/canonicalization version;
- proof method;
- external checker or backend;
- random seed and generated test data, if any; and
- hidden/hold-out status.

### 10.2 Machine-checkable approximation

Use an independence profile with levels:

| Level | Evidence |
|---:|---|
| 0 | Same derivation replayed; reproducibility only |
| 1 | Different path/order but same premises and rule family |
| 2 | Different rule family, shared source premises |
| 3 | Largely disjoint premise/source support, same kernel |
| 4 | Distinct proof method or representation with controlled translation |
| 5 | Independent trusted checker or formal proof artifact with auditable translation |

The profile should compute overlap of source leaves and rule families. It should
never claim statistical independence. A useful approximation is a minimum
provenance cut: two derivations with a large shared source/rule cut are not
independent even if their surface paths differ.

### 10.3 Benchmark implication

The current three identical blind runs establish determinism, not independent
rediscovery. The report must continue to use the word “deterministic” unless the
independence profile meets a declared level.

## 11. Candidate lifecycle

The proposed lifecycle is directionally correct but skips type/regime closure,
proof-state construction, and the distinction between a candidate and a closed
proposition.

### 11.1 Corrected lifecycle

```text
scope/theory snapshot
  → well-formed typed terms
  → regime and definedness obligations
  → canonical structural class
  → structural observation
  → closed conjecture with variables and validity regime
  → proof-obligation DAG
  → symbolic proof search / bidirectional planning
  → countermodel and falsification attempts
  → formal or replayable symbolic certificate
  → optional numerical stress or special-case confirmation
  → hold-out and novelty/utility review
  → final evidence bundle
```

### 11.2 Status rules

- A raw expression is not yet a candidate theorem.
- A structurally valid construction is not a conjecture until its proposition,
  variables, and regime are closed.
- A conjecture is not a proof obligation DAG until all missing hypotheses are
  explicit.
- A failed numerical test can falsify a fixed computational instance, but a
  passing test cannot promote a conjecture to proof.
- A formally verified proposition can still be a known theorem rather than a
  novel result.
- Novelty is an external, open-world claim and is never inferred from an empty
  Atlas match.
- `UNRESOLVED`, `FALSIFIED`, `SUPPORTED`, and `VERIFIED` must be distinct terminal
  states.

## 12. Role of numerics

Numerics may enter only after a fixed proposition, fixed regime, and fixed test
generator have been declared. Numerical execution must be a consumer of a
candidate, never a producer or ranker in open discovery.

| Use | Permitted epistemic label | What it does not mean |
|---|---|---|
| Falsification on a valid test case | `NUMERICALLY_FALSIFIED_ON_CASE` | Not necessarily a proof of global falsity if the executor/model is approximate |
| Stress testing | `NOT_FALSIFIED_ON_TEST_SUITE` | Not verified, and not evidence of exhaustive validity |
| Sanity checking | `NUMERICALLY_CONSISTENT_ON_TEST_SUITE` | Not symbolic or formal proof |
| Parameter estimation | `PARAMETER_FIT` | Not a mathematical theorem or discovery score |
| Special-case confirmation | `SPECIAL_CASE_NUMERIC_SUPPORT` | Does not generalize to the full proposition |
| Interval/validated numerical proof | `VALIDATED_NUMERIC_RESULT` | Still limited to the formalized numerical domain and assumptions |

Every numerical record must include discretization, precision, boundary policy,
test-generator version, executor version, seeds, tolerances, and the exact
proposition/regime hash. `NOT_RUN`, `UNSUPPORTED`, and `INCONCLUSIVE` must remain
different from `FAIL`.

Numerics may reject a fixed candidate after a counterexample is found, because
that is falsification of a declared object. They may not create new operators,
choose among open-search candidates, tune a grammar, or silently alter ranking.

## 13. Benchmark implications and Layer 15 timing

### 13.1 Should Layer 15 proceed now?

Yes, but only as a narrow foundational kernel and migration benchmark. Do not
wait for a higher rediscovery score, and do not improve search first. The
current `1/6` result already exposes three architectural requirements:

1. indexed spaces are needed for nilpotent complexes;
2. endpoint/family signal must not be reported as full relation recovery; and
3. semantic statements must remain outside equality closure unless explicitly
   promoted by a proof-producing rule.

However, the result does not justify building a universal mathematical language.
Layer 15 should be driven by a finite set of operator-centric litmus tests, not
by more features or better benchmark scores.

### 13.2 Mandatory Layer-15 litmus tests before search work

- `d_(k+1) ∘ d_k = 0` with indexed form spaces;
- conditional inverse laws with disjoint and overlapping regimes;
- commutation under a metric/inner-product assumption;
- decomposition with reconstruction and orthogonality obligations;
- approximation that cannot enter exact equality closure;
- analogy/correspondence that cannot become a rewrite;
- implication that generates obligations but not equality edges;
- contradiction under overlapping versus disjoint regimes;
- target-blind discovery API with no target type available; and
- finite grammar exhaustive search with an auditable quotient certificate.

## 14. Corrected Layer 15–20 roadmap

Epistemic statuses, provenance, and the distinction between lossless and
heuristic operations are cross-cutting requirements from Layer 15 onward. They
must not first appear in Layer 19.

### Layer 15 — Mathematical judgment kernel

**Purpose:** Replace overloaded semantic records with typed terms,
propositions, contexts, regimes, judgments, and proof obligations.

**Input:** Versioned Atlas snapshot plus an explicit legacy-adapter policy.

**Output:** Typed `Theory`, `Context`, `Regime`, `Term`, `Proposition`,
`Judgment`, and `ProofState` schemas with stable hashes and provenance.

**Invariants:**

- terms and propositions are separate sorts;
- every application has a checked type and regime obligations;
- no relation or analogy is an equality without a proof-producing conversion;
- assumptions are never dropped during derivation;
- unknown entailment is not false;
- epistemic status is separate from heuristic priority; and
- legacy records cannot directly enter the trusted rewrite kernel.

**Definition of Done:**

- all current operator-centric statement kinds have typed serializations;
- the six executable equalities and non-executable semantic statements remain
  distinguishable after round-trip serialization;
- indexed forms, dimensions, parameters, and scalar fields type-check;
- conditional composition, inverse, nilpotence, decomposition, approximation,
  and correspondence all produce the correct proposition kind;
- regime intersection returns compatible/incompatible/unknown;
- an invalid type or dropped assumption is rejected by a machine test; and
- no source search or benchmark score is changed by the kernel migration test.

### Layer 16 — Problem, goal, and proof-state language

**Purpose:** Represent goal-directed solving without creating a second
mathematical language.

**Input:** Layer-15 theory, context, regime, and target proposition.

**Output:** `Problem`, typed metavariables, AND/OR proof states, backward rule
applications, unification constraints, and an explicit search contract.

**Invariants:**

- a goal is the same proposition language as a theorem statement;
- backward steps create obligations rather than assume conclusions;
- every metavariable has a type and regime;
- proof-state mutation is scoped and replayable; and
- target/problem data is not exposed to target-blind discovery APIs.

**Definition of Done:**

- the operator-centric problem examples can be parsed and type-checked;
- forward facts and backward goals can meet only via a typed, regime-compatible
  judgment;
- failed, unknown, and unsupported branches are retained with reasons;
- a proof state can be serialized, replayed, and hashed; and
- no untyped goal string is accepted by the solver boundary.

### Layer 17 — Certified quotient search

**Purpose:** Reduce search by mathematical equivalence and constraints, not
heuristic top-N ranking.

**Input:** Layer-16 problem/proof state, grammar `G`, depth/cost limits, theory
`E`, and admissible rules `T`.

**Output:** Canonical classes, certified equivalence/provenance edges, search
certificate, and separate sound/heuristic pruning ledgers.

**Invariants:**

- canonicalization is relative to an explicit equivalence theory;
- alpha-equivalent terms collapse losslessly;
- symmetry quotienting is certified for the current goal and regime;
- bounded saturation is reported incomplete, not complete;
- heuristic pruning cannot be included in an exhaustion claim; and
- every class has at least one replayable representative.

**Definition of Done:**

- a finite reference grammar and quotient implementation agree on all classes;
- AC, symmetry, substitution, and regime-sensitive tests distinguish sound
  from unsafe reductions;
- each discarded item has a reason and soundness class;
- `EXHAUSTED_RELATIVE_SPACE` and `BUDGET_ENDED` are machine-distinct; and
- the quotient layer does not introduce arbitrary linear-combination search.

### Layer 18 — Bidirectional proof planning

**Purpose:** Turn structural candidates and goals into proof-obligation DAGs and
connect forward mathematics with backward requirements.

**Input:** Canonical candidate classes and Layer-16 proof states.

**Output:** Proof plans, rule applications, subgoals, side conditions,
unifiers, and replayable obligation DAGs.

**Invariants:**

- AND obligations all need discharge;
- OR alternatives remain distinct;
- a meet is a certified judgment match, not a signature match;
- proof plans retain regimes and source premises; and
- unresolved obligations block promotion.

**Definition of Done:**

- a known operator identity can be reconstructed with a replayable proof plan;
- a missing assumption produces an open obligation rather than a false proof;
- a near-miss type mismatch is rejected;
- proof-plan replay yields the same judgment hash; and
- a partial structural match cannot enter the verified-result state.

### Layer 19 — Verification and scientific hygiene

**Purpose:** Attach symbolic/formal verification, counterexample search,
optional numerics, hold-out validation, independence profiles, and reproducible
evidence without contaminating discovery.

**Input:** Closed propositions, proof plans, regimes, and fixed test policies.

**Output:** Proof certificates or failures, counterexamples, isolated numerical
records, hold-out results, independence profiles, and evidence bundles.

**Invariants:**

- numerics cannot generate or rank open candidates;
- pass is not proof;
- a counterexample and an unsupported test are different;
- source, rule, representation, and checker versions are recorded; and
- repeated runs are not called independent unless the profile justifies it.

**Definition of Done:**

- every result has a complete status transition history;
- numerical records replay from a proposition/regime/test hash;
- falsified, unresolved, unsupported, and verified states are distinct;
- hold-out leakage is detected; and
- an evidence bundle can be audited without access to hidden campaign state.

### Layer 20 — Practical utility gate

**Purpose:** Demonstrate bounded usefulness on previously unseen, in-scope
operator problems without claiming general mathematical intelligence.

**Input:** A versioned real problem, theory snapshot, context, regime, target,
and resource policy.

**Output:** A small auditable set of candidates or a justified failure report,
with proof obligations, assumptions, provenance, pruning certificate, and
verification status.

**Invariants:**

- “no result” is an acceptable outcome;
- output size is not optimized at the expense of completeness claims;
- utility and truth are separate measurements;
- each candidate is reproducible and auditable; and
- novelty is not asserted solely from Atlas absence.

**Definition of Done:**

- a held-out suite of real, in-scope problems is frozen before evaluation;
- the engine produces bounded outputs or explicit `BUDGET_ENDED`/
  `UNSUPPORTED_FRAGMENT` failures;
- human auditors can reconstruct assumptions, derivation, pruning, and status;
- partial and verified results are not conflated; and
- the utility report includes failure rate, proof completion rate, audit time,
  reproducibility, and false-positive rate.

## 15. Decisions required before implementation

These decisions change the kernel's meaning and must be resolved first:

1. **Type scope:** adopt a bounded indexed/dependent-lite type system for
   grades, dimensions, bundles, and parameter families; explicitly reject
   general dependent type theory for v1.
2. **Judgment semantics:** define the trusted forms of equality, implication,
   equivalence, inclusion, approximation, and correspondence.
3. **Regime language:** choose the initial structured constraints and the exact
   behavior when entailment is unknown.
4. **Proof boundary:** define which evidence statuses can create trusted
   rewrites and which remain observations or conjectures.
5. **Legacy migration:** specify a one-way adapter from Atlas records into the
   new kernel and prohibit reverse promotion without proof.
6. **Search contract:** freeze the definition of grammar, depth/cost, theory,
   equivalence, resource budget, and termination labels.
7. **Target-blind API:** separate discovery inputs from benchmark/scorer inputs
   in types, serialization, and ideally process boundaries.
8. **Independence policy:** choose the minimum provenance overlap and rule/source
   diversity required for each independence level.
9. **Benchmark corpus:** freeze the Layer-15 litmus tests and their leakage
   policy before implementing search quotienting.
10. **Canonical serialization:** define stable hashes, alpha-normalization, and
    version migration rules before using hashes as scientific identifiers.

## 16. Decisions that can safely be postponed

The following should not block the foundational architecture:

- selecting Lean, Mathematica, SymPy, Z3, or another backend;
- full e-graph adoption;
- arbitrary higher-order or full dependent unification;
- categorical abstraction beyond the operator/morphism vocabulary required by
  current examples;
- distributed search and million-scale parallel execution;
- broad PDE, probability, category-theory, or set-theory coverage;
- automated literature novelty checking;
- LLM-based theorem planning;
- high-order validated numerics;
- user-facing utility dashboards; and
- optimization of the current rediscovery score.

These can be evaluated after the kernel and relative-completeness tests make
their assumptions explicit.

## 17. Risks

| Risk | Failure mode | Required mitigation |
|---|---|---|
| Overformalization | Kernel becomes a theorem prover project before operator scope works | Bounded indexed types and litmus tests |
| Regime explosion | Every derivation carries an intractable condition formula | Explicit bounded fragment and `UNKNOWN` fallback |
| Unsound canonicalization | AC, symmetry, or scalar assumptions merge unequal terms | Versioned theory `E` and proof-producing merges |
| E-graph blow-up | Saturation hides resource loss or memory failure | Guarded saturation budgets and certificates |
| False independence | Different paths share all premises/rules | Provenance overlap profile |
| Target leakage | Scorer/benchmark state enters discovery memory | Separate types and boundary tests |
| Proof/evidence confusion | Numeric or structural status promoted to theorem | Monotone status machine and promotion gates |
| Legacy migration | Old semantic records silently regain equality power | One-way adapter and negative tests |
| Search optimism | Zero serious candidates reported as empty mathematics | Relative completeness and termination labels |
| Scope creep | Layer 15 attempts to model all mathematics | Operator-centric language contract |
| Novelty overclaim | Atlas absence treated as literature novelty | External hold-out and novelty status separate |

## 18. Long-term ceiling

Even a successful Layer 20 would not make OpForge a general mathematical
reasoning engine.

### Representation ceiling

Every IR chooses a fragment. Many equivalent mathematical presentations will
remain outside the chosen type and theory language. Translation between
representations can itself require proof.

### Search and complexity ceiling

Term generation, unification, equivalence, and proof planning can be
combinatorial or undecidable. Quotienting reduces duplication; it does not make
the remaining search polynomial or complete in the global sense.

### Theorem-availability ceiling

The engine cannot derive a theorem whose required definitions, lemmas, or
axioms are absent from its theory unless it discovers and proves the missing
material. Atlas density is not theorem coverage.

### Proof-complexity ceiling

True propositions may require proofs larger than available time, memory, or
formal infrastructure. `UNRESOLVED` can remain the correct result indefinitely.

### Continuous and infinite-dimensional ceiling

Function spaces, operators on them, limits, distributions, boundary values, and
functional-analytic hypotheses require semantics that finite typed signatures do
not provide. Discretization can test a model, not replace the continuum proof.

### Undecidability ceiling

General symbolic equivalence, theoremhood, and program-like operator properties
contain undecidable fragments. The system must expose incomplete procedures
instead of hiding them behind confidence scores.

### Novelty ceiling

Novelty is an open-world and time-dependent claim. No closed Atlas can prove
that a result is absent from the literature. At most, OpForge can provide a
reproducible search scope and an explicit external-check status.

### Human interpretation ceiling

Choosing the right formalization, regime, target, and acceptable abstraction can
remain the dominant intellectual task. A precise proof of the wrong formalized
problem is still the wrong result.

## 19. Non-negotiable invariants

1. **No stronger claim than evidence:** observation, candidate, conjecture,
   derivation, numeric support, formal verification, falsification, and unknown
   remain distinct.
2. **No hidden assumptions:** every derived judgment carries its regime and
   premises.
3. **No relation-to-equality promotion:** analogy, correspondence, implication,
   approximation, and metadata similarity cannot become equality edges without
   a proof-producing conversion.
4. **No target coupling:** discovery code cannot receive target IDs, expected
   operators, expected compositions, family names, or pattern types.
5. **No false completeness:** budget termination and unknown constraints are not
   exhaustion.
6. **No unrecorded heuristic deletion:** every heuristic prune is visible in the
   search ledger.
7. **No numerical steering:** numerics cannot generate, rank, or select open
   discovery candidates.
8. **No fake independence:** reproducibility is not independent derivation.
9. **No global contradiction from local mismatch:** contradiction requires
   compatible types and overlapping regimes.
10. **No universal promise:** all capability claims are relative to a declared
    theory, grammar, regime, and resource boundary.

## 20. Review conclusion

The next safe architectural move is not a larger expression enum, a stronger
search heuristic, or a backend selection. It is a small judgment kernel with
indexed operator types, structured regimes, proof-state separation, and
machine-enforced epistemic boundaries.

Layer 16 quotient search should wait until that kernel can demonstrate that its
reductions are sound relative to an explicit theory. Layer 17/18 goal and proof
planning should wait until a problem and proof state have one common proposition
language. Layer 19 verification must consume closed propositions without
feeding open discovery. Layer 20 should be treated as a held-out utility
protocol, not as a promise of broad mathematical capability.

The current benchmark result is therefore useful precisely because it is weak:
it identifies which claims are genuine, which are partial, and which semantic
boundaries still need to be made structural rather than conventional.

## Source basis inspected

- [ARCHITECTURE.md](/Users/user2/Documents/MathOperator/ARCHITECTURE.md)
- [atlas/model.hpp](/Users/user2/Documents/MathOperator/include/opforge/atlas/model.hpp)
- [semantic/closure.hpp](/Users/user2/Documents/MathOperator/include/opforge/semantic/closure.hpp)
- [semantic/closure.cpp](/Users/user2/Documents/MathOperator/src/semantic/closure.cpp)
- [patterns/analyzer.hpp](/Users/user2/Documents/MathOperator/include/opforge/patterns/analyzer.hpp)
- [research/campaign.hpp](/Users/user2/Documents/MathOperator/include/opforge/research/campaign.hpp)
- [scientific_regression_baseline_v1.md](/Users/user2/Documents/MathOperator/reports/scientific_regression_baseline_v1.md)
