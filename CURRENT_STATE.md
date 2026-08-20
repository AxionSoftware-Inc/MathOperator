# OpForge current state

This is the first document a maintainer or future AI should read.

## Status

- Current phase: Phase 0 baseline freeze plus completed Layers 15, 16, 17, 18, 19, 20, 21, 22, 23, and the Layer-24 search-scalability evaluation gate.
- Latest completed scope: Phase 0, Scientific Regression Benchmark v1, the
  representation-only Layer 15 core, a parallel Layer 16 quotient-search path,
  and separate Layer 17 goal-directed/bidirectional, Layer 18 proof-planning,
  and Layer 19 verification/evidence paths, followed by the Layer-20 evaluation
  harness, Layer-21 constructor-synthesis harness, and Layer-22
  constraint-guided synthesis harness, Layer-23 rich mathematical semantics
  harness, and Layer-24 search-scalability harness. The
  reconstructed Layers 1–14 remain the existing discovery baseline; none of
  this is a claim of formal mathematical completion.
- Layer 15: **implemented; software, semantic, scientific, and search gates
  passed**.
- Layer 16: **implemented; finite relative-completeness, soundness,
  streaming, ledger-accounting, partial-fact isolation, and baseline gates
  passed**.
- Layer 17: **implemented; typed goal model, explicit safe backward rules,
  AND/OR branches, conservative matcher, frontier meetings, finite exhaustion
  versus budget distinction, negative controls, deterministic replay, and
  Layer-16 integration gates passed**.
- Layer 18: **implemented; deterministic backend-neutral ProofPlan, explicit
  proof-obligation DAGs, AND/OR alternatives, conservative evidence gates,
  cycle detection, replay/invalidation, indexed obligations, negative controls,
  and accounting gates passed**.
- Layer 19: **implemented; capability-gated exact verification, falsification,
  numerical-support isolation, certificates, replay/invalidation, ResultBundle,
  negative controls, and numerics firewall passed**.
- Layer 20: **evaluated; structural utility is limited but demonstrated,
  leakage/determinism/negative-control gates passed, and honest unsupported,
  budget-ended, open-proof, and real-Atlas transfer limitations remain**.
- Layer 21: **evaluated; limited generative synthesis demonstrated** through
  target-blind typed composition, adjoint, inverse-candidate, commutator,
  conjugation, and indexed-family cases. Opaque IDs, negative controls,
  Layer-16 quotient integration, proof-obligation propagation, determinism,
  Release CTest, and ASan/UBSan passed. Formal proof remains unavailable.
- Layer 22: **evaluated; constraint-guided synthesis demonstrated** through
  structured-form, inverse-law, indexed, UNKNOWN, negative, opaque-ID,
  scaling, scorer-isolation, deterministic, and real-Atlas ceiling controls.
  The exact property/proof boundary remains intentionally narrow.
- Layer 23: **evaluated; rich operator semantics demonstrated** through
  explicit space/operator properties, typed rule schemas, restriction/tensor/
  dual/adjoint distinctions, conservative propagation, cross-space
  preservation, partial-fact and numerical firewalls, opaque IDs, and real
  Atlas migration controls. Extension, pullback/pushforward, arbitrary
  coefficient search, formal proof, and physics remain deferred.
- Layer 24: **evaluated; constraint-directed lazy search demonstrated** with a
  deterministic SearchPlan, typed theory indexes, bounded backward/forward
  demand slicing, lazy construction, cache isolation, quotient-at-insertion,
  indexed frontier meetings, explicit UNKNOWN/resource controls, reference
  equivalence, opaque IDs, and million-scale streaming. A true full production
  Atlas integration now runs through `AtlasLoader::load("atlas")`; this remains
  relative to the declared grammar and bounds, not a universal speedup claim.

The complete measured Layer-24 disclosure is in
[layer24_search_scalability_v2.md](reports/layer24_search_scalability_v2.md),
with machine-readable metrics in
[layer24_search_scalability_v2.json](reports/layer24_search_scalability_v2.json).

Layer-24 result:

- verdict: SCALABLE_CONSTRAINT_DIRECTED_SEARCH_DEMONSTRATED;
- all 8 reference/optimized finite cases equivalent;
- finite control: EXHAUSTED_RELATIVE_SPACE with accounting pass;
- budget control: BUDGET_ENDED with relative_complete=false;
- UNKNOWN budget control: 10 UNKNOWN, 8 deferred, INCOMPLETE_UNKNOWN;
- million-scale stress: 1,000,000 raw, 999,999 avoided, 1 materialized, 1
  retained;
- opaque-ID replay and three-run determinism: passed; and
- discovery numerics, runtime LLM calls, and unrestricted linear combinations:
  zero/disabled.
- previous “real Atlas” probe was seed-based and is now labeled as the
  controlled vector-calculus seed: 12 operators, 39 full facts, 11/33 slice,
  19 materialized, 8 retained;
- full production Atlas: 98 operators, 47 spaces, 119 relations, 67 statements
  (6 executable equalities, 61 semantic statements), digest
  `layer24-atlas-snapshot.267014ce981723bb`;
- full Layer-23 migration: 6 previous fully structured, 321 newly structured,
  327 cumulative fully structured, 220 partial, 0 unsupported; and
- full-Atlas depth-1 probe: 8/24/1/4 slice (operators/facts/spaces/rules),
  optimized 64 attempts and 72 materialized, 64 exact retained, full reference
  58016 attempted and 20579 materialized, canonical equivalence 64/64,
  `EXHAUSTED_RELATIVE_SPACE`, full replay 3/3.

## What the system can currently demonstrate

The current target-blind benchmark records:

- genuine structural recovery: 1/6;
- partial structural signal: 4/6;
- miss: 1/6;
- false positives: 0;
- negative controls: 3/3;
- leakage events: 0;
- numerical discovery experiments: 0;
- opaque-ID rediscovery: passed; and
- deterministic reruns: passed.

This is evidence of a bounded structural search capability, not a strong
general mathematical discovery system.

Layer-20 utility result:

- verdict: LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED;
- cases: 15;
- structural exact: 1;
- structural valid alternatives: 6;
- structural partial: 0;
- structural misses: 8;
- false positives: 0;
- negative controls: 5/5;
- target-blind leakage audit: PASS;
- opaque-ID synthesis: PASS;
- deterministic reruns: 3/3;
- open-discovery generated/pruned/serious: 62/650/0;
- discovery numerical experiments: 0; and
- formal verification backend: still unavailable.

Layer-21 synthesis result:

- verdict: LIMITED_GENERATIVE_SYNTHESIS_DEMONSTRATED;
- cases: 12 target-blind controlled cases;
- opaque-ID positive cases: 2;
- open discovery: raw 144, valid 50, invalid 94, unknown 0, retained classes
  62, serious candidates 0;
- discovery numerical experiments: 0; runtime LLM calls: 0; and
- deterministic digest: `layer21_benchmark_digest.4c26807e3ed03a83`.

The full case disclosure is in
[layer21_generative_operator_synthesis.md](reports/layer21_generative_operator_synthesis.md)
and the machine summary is in
[layer21_synthesis_utility.json](reports/layer21_synthesis_utility.json).

## Major known weaknesses

- The current Atlas `Identity` model remains a compatibility representation;
  the new Layer-15 theory adapter preserves that boundary and only explicitly
  executable equality ASTs enter the legacy equality closure.
- The new semantic core provides a bounded typed judgment, indexed-space,
  context, and validity-regime kernel, but it is not yet the implementation
  behind the legacy closure/search path.
- Relation-oriented cases currently show endpoint/family signal, not complete
  relation recovery.
- The quotient path is bounded and relative-complete only for an explicitly
  finite scope; global mathematical completeness is not implemented.
- The quotient path is parallel to legacy closure/search and does not yet
  replace it.
- Structural candidates are not theorems; formal proof and external novelty
  checking are not provided.

## Project-intent documents

Read these together, in this order:

1. [VISION.md](VISION.md)
2. [ARCHITECTURE.md](ARCHITECTURE.md)
3. [SCIENTIFIC_INVARIANTS.md](SCIENTIFIC_INVARIANTS.md)
4. [ROADMAP.md](ROADMAP.md)
5. [DECISIONS.md](DECISIONS.md)
6. [BASELINES.md](BASELINES.md)
7. [OPFORGE_ARCHITECTURE_V1_REVIEW.md](reports/OPFORGE_ARCHITECTURE_V1_REVIEW.md)
8. [layer15_semantic_core.md](reports/layer15_semantic_core.md)
9. [layer16_quotient_search.md](reports/layer16_quotient_search.md)
10. [layer17_bidirectional_reasoning.md](reports/layer17_bidirectional_reasoning.md)
11. [layer18_proof_planning.md](reports/layer18_proof_planning.md)
12. [layer19_verification.md](reports/layer19_verification.md)
13. [layer20_practical_utility.md](reports/layer20_practical_utility.md)
14. [layer21_generative_operator_synthesis.md](reports/layer21_generative_operator_synthesis.md)
15. [layer21_synthesis_utility.json](reports/layer21_synthesis_utility.json)

## Required checks before further work

From the repository root:

```bash
git diff --check
cmake -S . -B build -DOPFORGE_BUILD_TESTS=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/opforge benchmark blind_rediscovery atlas
./build/opforge benchmark scaling atlas --medium-operators 50
./build/opforge benchmark open_search atlas
./build/opforge benchmark quotient_search atlas
./build/opforge benchmark goal_search atlas
./build/opforge benchmark proof_plan atlas
./build/opforge benchmark verification atlas
./build/opforge benchmark utility_gate atlas
./build/opforge benchmark synthesis_utility atlas
```

The blind benchmark must retain its honest MISS and partial outcomes. Any
future Layer 20 work must pass the Layer Gate in `ROADMAP.md` and preserve the
Layer-16 lossless/lossy, Layer-17 goal-status, and Layer-18 proof-boundary
contracts.

Layer 16 acceptance audit result: **LAYER 16 DoD SATISFIED — LAYER 17 SAFE TO
BEGIN**.

Layer 17 acceptance result: **LAYER 17 DoD SATISFIED — LAYER 18 SAFE TO
BEGIN**.

Layer-17 final verification: repository CTest 4/4, clean Release CTest 4/4,
clean ASan/UBSan CTest 4/4, frozen blind/scaling/open/quotient baselines
unchanged, three-run goal and blind deterministic exports identical, and
git diff --check passed. The detailed metrics and limitations are in
[reports/layer17_bidirectional_reasoning.md](reports/layer17_bidirectional_reasoning.md).

Layer-18 acceptance result: **LAYER 18 DoD SATISFIED — LAYER 19 SAFE TO
BEGIN**. This authorizes a separately scoped Layer-19 verification-backend
task only; it does not claim that any structural candidate is formally proved.
The proof-planning suite covers trusted facts, open/unknown/falsified/
contradicted obligations, shared DAGs, cycles, replay/invalidation, indexed
families, evidence-level safety, negative controls, accounting, and deterministic
real Layer-17 candidate conversion. Detailed per-case results are in
[reports/layer18_proof_planning.md](reports/layer18_proof_planning.md).

Layer-19 acceptance result: **LAYER 19 DoD SATISFIED — LAYER 20 SAFE TO BEGIN
AS A SEPARATE TASK**. The exact internal verifier is deliberately narrow and
the production formal backend remains unavailable. Numerical verification is
post-search only and remains separate from discovery numerical experiments.
Detailed certificate, replay, counterexample, ResultBundle, and firewall
results are in [reports/layer19_verification.md](reports/layer19_verification.md).

Layer-19 final verification: repository Debug CTest 6/6, clean Release CTest
6/6, clean ASan/UBSan CTest 6/6, Layer-19 JSON determinism across three runs,
numerics firewall PASS, frozen blind/scaling/open/quotient/goal/proof results
preserved, and `git diff --check` passed.

Layer-20 evaluation result:
LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED. The complete case-by-case
evaluation, target-blind masking audit, opaque-ID rerun, proof/evidence
scorecards, construction-grammar ceiling, forward-discovery comparison, and
deterministic machine output are in
[reports/layer20_practical_utility.md](reports/layer20_practical_utility.md)
and [reports/layer20_practical_utility.json](reports/layer20_practical_utility.json).

## Layer 22 constraint-guided synthesis

Layer 22 is implemented as a separate target-directed constraint layer. Its
verified verdict is **CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED**. The controlled
suite demonstrates type-only ambiguity, adjoint/commutator/conjugation/index
property reduction, left versus two-sided inverse obligations, a false-property
negative, UNKNOWN preservation, and two opaque-ID positive reruns. Leakage,
scorer isolation, numerics/LLM firewalls, deterministic replay, and the real
Atlas semantic-ceiling probe passed.

Measured command result:

- 11 target-blind controlled cases;
- 2 opaque-ID positive cases;
- deterministic replay 3/3, digest
  `layer22_benchmark_digest.d148a171b146b533`;
- scaling type-compatible Layer-21 view / Layer-22 retained classes:
  `39/1`, `132/1`, `279/1` for 3/6/9 operators;
- discovery/synthesis numerics `0`, runtime LLM calls `0`, unrestricted linear
  combinations disabled;
- real Atlas fully structured facts: `6`; self-adjoint probe:
  `UNSUPPORTED_CONSTRAINT_LANGUAGE`; and
- Layer 23 is implemented as a separate rich-semantic path; its measured
  result is recorded below.

The full constraint ceiling and per-case output are in
[reports/layer22_constraint_guided_synthesis.md](reports/layer22_constraint_guided_synthesis.md)
and [reports/layer22_constraint_guided_synthesis.json](reports/layer22_constraint_guided_synthesis.json).

## Layer 23 rich mathematical semantics

Layer 23 adds explicit mathematical-space properties and relations, operator
property facts, typed reusable rule schemas, conservative propagation, and
goal-directed constructors for restriction, tensor, dual-map, adjoint,
composition, product, and controlled scalar combination descriptors. It keeps
Restriction distinct from Extension, dual maps distinct from adjoints, and
tensor/product/direct-sum forms distinct. Extension, pullback/pushforward,
arbitrary coefficient search, and formal theorem proving remain deferred.

The verified result is **RICH_OPERATOR_SEMANTICS_DEMONSTRATED**. The real Atlas
migration measured 6 previously fully structured facts, 321 newly structured
facts, 327 cumulative fully structured facts, 220 partial facts, and 0
unsupported facts. The migrated theory contains 47 spaces, 47 explicit space
property facts, 274 operator property facts, 0 explicit space relations, and 4
trusted rule schemas. Real Atlas probes found 88 linear, 47 space, 0 indexed,
and 10 inverse/commutation facts; partial facts were not promoted.

The target-blind suite has 14 cases, including restriction/tensor/dual-map
positives, missing-prerequisite and dual-versus-adjoint negative controls,
cross-space preservation, partial-fact and numerical firewalls, and 2 opaque-ID
reruns. Scaling records Layer-21 attempts / Layer-23 attempts of `7/21`,
`7/78`, and `7/171` for 3/6/9 operators, with 2 retained classes and no
unknown type decisions in those finite probes. The deterministic digest is
`layer23_benchmark_digest.6b6b46b6e7002750`.

The complete case disclosure, semantic firewall, metrics, limitations, and
machine-readable output are in
[reports/layer23_rich_mathematical_semantics.md](reports/layer23_rich_mathematical_semantics.md)
and [reports/layer23_rich_mathematical_semantics.json](reports/layer23_rich_mathematical_semantics.json).
