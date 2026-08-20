# OpForge architecture decisions

These lightweight Architecture Decision Records freeze intent without selecting
implementation libraries prematurely. A decision may be revisited only under
its recorded condition, with baseline and invariant impact documented.

## ADR-001 — Use Theory + Context + Judgment + ProofState as the core

**Decision:** The mathematical kernel is `Theory Σ -> Context Γ -> Judgment ->
ProofState`, supported by typed terms, validity regimes, evidence, provenance,
and search scope.

**Reason:** A Proposition/Expression AST describes syntax but does not capture
theory, scope, assumptions, regime, proof obligations, or epistemic status.
The current overloaded `Identity` boundary would otherwise reappear in a richer
AST.

**Alternatives considered:** Proposition-only IR; a general theorem prover
kernel immediately; untyped JSON records; a universal dependent-type theory.

**Consequences:** Layer 15 must separate terms from propositions and make
assumptions/regimes first-class. A bounded indexed/dependent-lite language is
required for current operator mathematics.

**Revisit condition:** Revisit only if the operator-centric litmus suite proves
that the judgment/context model cannot represent a required in-scope case.

## ADR-002 — Keep relation kinds distinct from equality

**Decision:** Analogy, correspondence, implication, approximation, factorization,
decomposition, inclusion, and generic relations remain explicit judgment kinds.

**Reason:** They have different logical effects and side conditions. Treating
them as equality edges caused the Phase-0 epistemic failure.

**Alternatives considered:** One generic `Identity(left,right)` record;
relation-kind-specific implicit rewrite rules; semantic text parsing.

**Consequences:** Only typed, regime-compatible equality judgments with trusted
provenance may enter equality closure or rewriting. Relation-to-equality
conversion requires an explicit proof-producing rule.

**Revisit condition:** Never for the semantic boundary. New relation kinds may be
added only with an explicit rewrite/derivation policy.

## ADR-003 — Exclude numerics from open discovery

**Decision:** Numerical computation cannot generate, rank, select, or promote
open-discovery candidates.

**Reason:** Numerical tests can falsify a fixed proposition or support special
cases, but they are not proof and can create data-dependent search bias.

**Alternatives considered:** Numerical ranking of structural candidates;
numerical candidate generation; mixed symbolic/numeric beam search.

**Consequences:** Numerics are an isolated downstream consumer with explicit
labels such as `NUMERICALLY_FALSIFIED_ON_CASE` and
`NOT_FALSIFIED_ON_TEST_SUITE`.

**Revisit condition:** Only if a future scientific protocol demonstrates a
separate, auditable use that cannot influence open discovery; the invariant
still remains for the open-discovery mode.

## ADR-004 — Search completeness is relative, not global

**Decision:** Completeness claims must name grammar `G`, bounds `D`, resources
`R`, equivalence theory `E`, Theory/Atlas version, context/regime, and rules.

**Reason:** Global mathematical completeness is impossible for the intended
scope, and bounded search currently has distinct raw, retained, and pruned
stages.

**Alternatives considered:** “Complete” as a global engine property; top-N
search reported as complete; candidate count as coverage.

**Consequences:** Reports distinguish `EXHAUSTED_RELATIVE_SPACE` from
`BUDGET_ENDED`, policy truncation, unsupported fragments, and unknown decisions.

**Revisit condition:** Never as a global claim. The relative contract may be
extended with a stronger finite grammar certificate.

## ADR-005 — Determinism is not independent evidence

**Decision:** Repeated deterministic runs count as reproducibility only.

**Reason:** Same facts, rules, canonicalizer, and proof method do not become
independent because execution is repeated.

**Alternatives considered:** Count different seeds, campaign IDs, or traversal
orders as independent; count distinct candidate paths automatically.

**Consequences:** Independent support requires a provenance overlap profile and
declared source/rule/representation diversity level.

**Revisit condition:** The independence scale may be refined when separate proof
methods or trusted checkers are available.

## ADR-006 — Do not enumerate unrestricted arbitrary linear combinations

**Decision:** Open discovery uses a bounded typed grammar and does not enumerate
arbitrary sums or scalar combinations. Goal-directed combinations require an
explicit typed goal, justification, and constraints.

**Reason:** Unrestricted combinations create combinatorial explosion and make
candidate counts meaningless without mathematical motivation.

**Alternatives considered:** Enumerate all linear combinations then rank;
random coefficients; numerical residual-driven open generation.

**Consequences:** Linear combinations remain an explicit goal-directed feature,
not an open-search default. Search reports must show the grammar boundary.

**Revisit condition:** A new combination grammar may be added only with a finite
scope, typed constraints, proof/utility objective, and completeness accounting.

## ADR-007 — Postpone proof-backend selection

**Decision:** Do not select Lean, Mathematica, SymPy, Z3, or another backend as
an architectural prerequisite.

**Reason:** The mathematical kernel, regime language, proof obligations, and
evidence contract must be defined before choosing an external checker. Tool
availability is not proof of semantic fit.

**Alternatives considered:** Immediate Lean-first design; symbolic CAS-first
design; solver-first constraints; Python interoperability as the core.

**Consequences:** Layer 18 defines replayable proof plans and Layer 19 defines a
backend adapter boundary. Backend choice is evaluated against explicit
capabilities and certificates later.

**Revisit condition:** After Layer 15–18 litmus tests freeze the fragment,
certificate interface, and required side-condition logic.

## ADR-008 — Keep the near-term domain operator-centric

**Decision:** The first semantic kernel covers typed operators, spaces,
compositions, indexed families, regimes, decompositions, transformations,
approximations, and related propositions. It does not claim universal
mathematics.

**Reason:** The audited Atlas and benchmarks are operator-centric. A universal
IR would add complexity before the project has a sound in-scope foundation.

**Alternatives considered:** General set theory; full category theory; arbitrary
analysis/PDE; universal dependent type theory; unrestricted theorem language.

**Consequences:** Extension points remain in symbols, indexed types, predicates,
rules, and regimes, but unsupported fragments report `UNSUPPORTED_FRAGMENT`.

**Revisit condition:** Only after an in-scope operator problem requires an
extension and the Layer Gate remains satisfiable.

## ADR-009 — Keep the integrity probe outside the repository

**Decision:** `/tmp/opforge-integrity-probe.cpp` remains an audit helper outside
the project checkpoint. It is not production architecture.

**Reason:** The probe prints case-by-case masking disclosures and dependency
ablations using private benchmark target metadata. Those are report-generation
and audit activities, not runtime discovery behavior. The important persistent
checks—leakage, deterministic exports, candidate-ID digests, scaling counts,
negative controls, and numerical isolation—are represented by the benchmark
harness and regression tests.

**Alternatives considered:** Add the probe as a production executable; copy its
target-aware disclosure logic into the discovery engine; leave all checks only
in `/tmp`.

**Consequences:** The temporary source is intentionally not part of the
repository or checkpoint. If recurring audits need automation, they must be
added as a separate scorer/audit tool with no access from discovery code.

**Revisit condition:** Revisit if a future release needs reproducible automated
case disclosure or ablation in CI; then promote only the audit-side logic with
an explicit target/scorer boundary.

## ADR-010 — Freeze architecture before Layer 15 implementation

**Decision:** Documentation and litmus-test contracts were frozen before source
implementation of Layer 15.

**Reason:** The current scientific result is intentionally weak and the project
must not let benchmark-score pressure determine the semantic model.

**Alternatives considered:** Improve rediscovery first; implement a large IR
speculatively; increase Atlas size; optimize candidate counts.

**Consequences:** The Layer-15 implementation was subsequently added under
ADR-011 and passed the Layer Gate while preserving honest partial/MISS
outcomes. The next task is a separately reviewed Layer-16 specification.

**Revisit condition:** After the architecture documents and required litmus
tests are accepted, with an explicit decision record for any change.

## ADR-011 — Keep Layer 15 parallel to legacy discovery during migration

**Decision:** Implement the semantic core as `opforge::semantic` theory,
context, judgment, proof-state, and migration APIs without rewiring the legacy
closure/search engine in the same change.

**Reason:** Layer 15 is representation infrastructure. Reusing the new types
inside discovery would change candidate generation, rewrite eligibility, or
baseline counts before a separately reviewed migration gate.

**Consequences:** The Atlas can be migrated and inspected without semantic
collapse; the old six-equality closure boundary and all scientific baselines
remain unchanged. Regime-aware closure integration requires its own tests and
decision.

**Revisit condition:** A later closure/search migration with replayable
provenance, baseline comparison, and an explicit Layer Gate.

## ADR-012 — Keep proof-obligation identity independent of lifecycle status

**Decision:** A `ProofObligation` ID identifies its target, label, and
provenance, while unresolved/discharged/unsupported/falsified is serialized in
`ProofState` rather than changing the obligation identity.

## ADR-013 — Keep Layer 17 goal reasoning separate from legacy discovery

**Decision:** Implement goal-directed reasoning as `opforge::reasoning` over
Layer-15 semantic `Theory`, `Context`, `Judgment`, and `ValidityRegime`, and
reuse Layer-16 quotient search for forward construction reduction. Do not
rewire the legacy discovery path in the same change.

**Reason:** Goal-directed search necessarily consumes a target judgment, while
the open discovery baseline must remain target-blind. A separate API makes the
boundary auditable and preserves the frozen structural/partial/MISS results.

**Backward rule policy:** Only explicit non-heuristic `BACKWARD`/`BOTH` rules
with premises may create predecessor goals. A forward implication is never
reversed implicitly. Typed matching returns `MATCH`, `NO_MATCH`, or `UNKNOWN`,
and unknown remains unresolved.

**Consequences:** Layer 17 reports relative search status, explicit goal and
meeting ledgers, finite exhaustion versus budget, and structural candidates;
it does not produce formal proofs, call an LLM, use numerics, or enumerate
unrestricted linear combinations. Layer 18 proof planning remains a separate
post-search path, governed by ADR-015.

**Revisit condition:** Revisit only with a Layer-18/legacy-migration proposal
that includes replayable provenance, target-boundary tests, and frozen-baseline
comparison.

**Reason:** The same obligation must be traceable across proof progress. A
status transition is lifecycle evidence, not a new mathematical obligation.

**Consequences:** Obligation-level joins and replay remain stable, while
`ProofState` IDs change when progress changes because state serialization
includes lifecycle statuses.

**Revisit condition:** Only if a future proof backend requires immutable
versioned obligation objects, with compatibility retained at the state layer.

## ADR-013 — Add Layer 16 as a parallel quotient-search path

**Decision:** Implement principled quotient search behind a new
`opforge::search::QuotientSearchEngine` API. Do not replace the legacy closure,
structural campaign, or scientific benchmark path in the same change.

**Reason:** The new path needs a distinct completeness contract, typed
equivalence certificates, streaming memory behavior, and lossless/lossy
accounting. Replacing the old path would make baseline comparison ambiguous.

**Consequences:** Search scopes, deterministic equivalence classes, explicit
symmetry rules, type/regime propagation, known-consequence checks, and a
machine-readable pruning ledger are available for future forward or backward
frontiers. The old 1/6 structural baseline remains independently measurable.

**Revisit condition:** A later migration with apples-to-apples stage metrics,
relative-completeness evidence, and a new baseline decision.

## ADR-014 — Stream raw quotient-search constructions

**Decision:** Accept a callback-based `ConstructionSource` and retain class
representatives plus optional audit members rather than all raw constructions.
Large runs use ledger counts/digests or an external ledger sink.

**Reason:** Million-scale stress must demonstrate streaming rather than hide
memory growth behind a later deduplication pass.

**Consequences:** Small audits can retain complete member certificates; stress
runs retain deterministic aggregate accounting and class summaries. Full
per-member replay is an explicit memory/configuration choice.

**Revisit condition:** If a future distributed/replay backend supplies an
external durable ledger without changing reduction semantics.

## ADR-015 — Keep Layer 18 proof planning backend-neutral and post-search

**Decision:** Implement Layer 18 as `opforge::proof::ProofPlanner`, consuming
actual Layer-17 structural candidates only after search. Represent proof
obligations, rule applications, evidence envelopes, shared DAG dependencies,
cycles, lifecycle states, provenance, and replay without selecting a formal
backend.

**Reason:** A Layer-17 structural solution is not a proof. The project needs an
auditable boundary that exposes every prerequisite without allowing search
scores, numerical support, display names, or legacy partial relations to
become proof evidence.

**Consequences:** Proof-safe rule contracts are explicit; `GoalRule` values are
not silently promoted. Structural discharge remains structural, numerical
support cannot satisfy formal evidence, missing facts reopen on replay, and
cycles remain unresolved. Legacy discovery behavior and all frozen baselines
remain unchanged. Layer 19 owns certificate verification and external
backends.

**Revisit condition:** Only with a separately reviewed Layer-19 backend and
certificate contract that preserves plan identity, provenance, evidence-level
separation, and replay invalidation.

## ADR-016 — Keep Layer 19 verification post-search and capability-gated

**Decision:** Implement Layer 19 as `opforge::verification::VerificationOrchestrator`
over explicit Layer-18 proof obligations. Verifiers declare capabilities and
trust classes; requests carry structured judgments, contexts, regimes,
substitutions, Theory versions, and deterministic configuration. Certificates
are replayable and invalidated when semantic dependencies change.

**Reason:** Verification evidence must strengthen only the evidence actually
produced. A numerical pass is not symbolic or formal proof, a numerical
suspicion is not an exact counterexample, and a missing backend must remain
unavailable rather than being represented by a mock certificate.

**Consequences:** The narrow internal exact verifier handles only structured
Layer-15 type/definedness, exact literal counterexamples, and trusted bounded
rewrite replay. Numerical verification is available only through Layer-19
requests and cannot feed candidate generation, ranking, frontiers, or quotient
equivalence. `ResultBundle` is the deterministic handoff artifact for Layer 20;
novelty remains an explicit external status.

**Revisit condition:** A separately reviewed formal or symbolic backend may be
added only with declared capabilities, certificate validation/replay,
tamper/invalidation tests, and preserved Layer-15–18 baselines.
