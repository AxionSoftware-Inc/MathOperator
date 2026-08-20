# OpForge takeover audit — Phase 0 epistemic repair

Date: 2026-08-20

## Why this phase exists

The original historical Layer 15–20 definitions are unavailable. The repository nevertheless preserves a coherent implementation history through v0.14. Before extending that history, this phase establishes which stored facts are executable mathematics, which engines are discovery-only, and which historical measurements must not be interpreted as current evidence.

## Critical findings

1. **Legacy `Identity` was not an equality type.** `semantic_identities.json` mixes equalities with analogies, decompositions, correspondences, inverse/adjoint descriptions and other semantic statements. The old loader represented every complete `left/right` pair as an equality-like AST, while closure/rewrite code consumed those records as proof edges. Examples include the convolution theorem and determinant multiplicativity being represented as pairs of operator references rather than their actual formulas.
2. **Historical v0.13 contradictions are therefore not a clean mathematical signal.** The checked-in v0.13 report recorded 38 contradictory closure candidates. After limiting equality closure to statements with a complete executable equality AST, the current bounded closure produces 20 candidates, accepts 10, reports 10 duplicates and 0 contradictions.
3. **Numerical discovery isolation was incomplete.** The main campaign had been gated, but `DeepDiscoveryEngine` still used geometry/numerical truth results while escalating discovery leads. Deep discovery is now structural-only; numerical experiments and refinements remain zero.
4. **Hard top-N candidate truncation was scientifically unsafe.** Tied heuristic scores could make the retained frontier depend on deterministic ID/hash order and erase entire structural regimes. A temporary stratified frontier now preserves structural buckets and explicitly marks the result as truncated. This is a safety measure, not the final million-scale solution.
5. **Reporting conflated absence of evidence with failure.** Numeric diagnostics not run could appear as failed coordinate consistency; under-specified leads could be counted as eliminated. Reports now distinguish `not_run`, unresolved and rejected states.
6. **Deep consensus metrics counted repeated occurrences as independent campaigns.** Consensus now counts unique campaign IDs and campaign-level lineage signatures.
7. **The repository carried machine-specific build artifacts.** Generated build directories are removed/ignored. A missing `<cmath>` include also prevented a clean Linux build and has been repaired.

## Current semantic baseline

Current full Atlas after the Phase 0 loader rules:

- operators: 98
- spaces: 47
- relations: 119
- loaded semantic statements: 67
- machine-executable equalities: 6
- non-executable semantic statements: 61
- partially verified operator records: 98
- formally/symbolically/numerically verified operator records: 0
- disconnected operators: 27
- operators unsupported by the finite-difference backend: 62
- Atlas validation issues: 0
- Atlas audit v2 issues: 0
- Atlas audit v3 issues: 0

The reduction from historical identity counts is not a loss of mathematical knowledge. Semantic statements remain in the Atlas; only their ability to participate in equality closure/rewrite is restricted until their actual proposition AST is represented.

## Current closure baseline

`opforge atlas closure atlas`:

- explicit rules: 9
- generated candidates: 20
- accepted consequences: 10
- duplicates: 10
- pruned: 0
- contradictions: 0
- maximum derivation depth: 1
- derivation DAG acyclic: yes

This result should not be compared numerically to v0.13 as though the engine simply improved from 38 contradictions to zero. The semantics changed: invalid equality edges were removed from the proof graph.

## Search/proof boundary

The default discovery contract is now:

`Atlas -> typed composition/patterns -> meta-pattern/schema/analogy/axiomatic structure -> structural candidate frontier -> closed candidate/proof obligations -> proof-stage verification -> optional numerical confirmation`

Numerical execution is prohibited from influencing candidate generation, ranking, escalation or survival. The current proof-stage gate is conservative and requires completion plus symbolic/formal evidence. Layer 15 must replace this temporary boolean gate with an explicit proposition/proof-obligation lifecycle.

Full-Atlas one-cycle structural baseline:

- patterns discovered: 508
- predicted roles: 32
- parameterized families: 16
- schema completions: 2
- law candidates: 1
- discovery leads: 17
- closure findings: 23
- commutator findings: 24
- invariant hypotheses: 2
- structural gaps: 90
- generated candidate IDs retained in memory: 218
- candidate leads pruned by the current bounded frontier: 133
- rejected candidates: 40
- known constructions: 2
- numerical experiments: 0
- surviving candidates: 0
- serious candidates: 0
- geometry/numeric diagnostics: not run

The zero survivors are expected because generated search leads are not yet property-complete proof candidates.

## Deep discovery baseline

Four full-Atlas deep campaigns now execute with:

- numerical experiments: 0 in every campaign
- numerical refinements: 0 in every campaign
- external-check candidates: 0
- serious candidates: 0
- consensus schemas in the measured run: 2
- each consensus schema occurred in 4 unique campaigns
- independent campaign-level lineage signatures: 1 for each schema

Recurrence therefore demonstrates deterministic convergence onto the same schema representation, not four independent mathematical discoveries.

## Phase 0 invariants now enforced by tests

- descriptive semantic statements such as the convolution theorem are not executable equalities;
- a fully represented Laplacian factorization is executable;
- executable + non-executable statement counts partition the loaded statement set;
- deep discovery performs zero numerical experiments;
- normal structural discovery performs zero numerical experiments;
- requesting proof-stage numerics does not bypass the candidate completion gate;
- geometry diagnostics report `not_run` unless explicitly executed;
- deep consensus campaign counts cannot exceed the number of actual campaigns.

Clean Linux build and regression result: `1/1` CTest passed.

## Reconstructed roadmap (not recovered historical layer definitions)

### Layer 15 — Typed proposition and candidate contract

Create a first-class proposition IR with explicit kinds (equality, composition equality, nilpotence, commutation, inverse law, adjoint law, decomposition, intertwining, implication, approximation/limit, analogy/correspondence), structured validity regimes and proof-obligation DAGs. Legacy `Identity` becomes an import compatibility layer only.

Acceptance: every Atlas statement is classified; only eligible propositions enter rewrite/closure; proposition type checking and canonicalization are deterministic; no free-form statement can silently become a proof edge.

### Layer 16 — Quotient search and completeness accounting

Replace pairwise generation + bounded frontier as the primary scaling mechanism with typed indices, canonical normal forms, alpha/role renaming, algebraic symmetry quotients, known-consequence closure, trivial/degenerate elimination and explicit equivalence classes. Every discarded class must have a machine-readable reason.

Acceptance: exhaustive small fixtures and held-out known constructions show no recall loss relative to brute-force enumeration within the same grammar/depth, while compression is measured separately from correctness.

### Layer 17 — Independent support and proof planning

Define independence of derivations, require closed conjectures, build proof plans from proposition dependencies and separate falsification evidence from proof evidence.

Acceptance: independent lineage accounting is non-inflating; controlled near-miss propositions fail for explicit reasons; withheld true propositions produce replayable proof-obligation plans without numerical evidence.

### Layer 18 — Certificate/checker boundary

Implement backend-neutral proof certificates and regime-aware contradiction checking. Choose Lean/another formal backend only after this interface is stable; the engine must be able to replay its own symbolic certificates deterministically first.

Acceptance: accepted symbolic certificates replay from a frozen Atlas snapshot; contradictions are only claimed under compatible assumption/validity regimes.

### Layer 19 — Real-problem and reproducibility layer

Turn a real mathematical task into typed requirements, run bounded quotient search, retain a complete pruning ledger, and export an evidence bundle with Atlas/config hashes, candidate definition, assumptions, derivation/proof artifacts and optional final numeric verification plan. Add strict hold-out and novelty-hygiene protocols.

### Layer 20 — Practical utility gate

A user supplies a real problem and receives a small auditable candidate set rather than raw combinatorial output. A candidate that passes must carry a reproducible proof/evidence package. Numerical execution may be attached only after the completion/proof gate and can never rescue, rank or discover a candidate.

## Immediate next step

Do **not** enlarge the Atlas or add another analogy heuristic. Design Layer 15 around the proposition/validity/proof-obligation IR, then stress that design against the actual v0.10–v0.14 engines before implementing it broadly. The current stratified frontier should remain explicitly temporary until Layer 16 quotient search exists.
