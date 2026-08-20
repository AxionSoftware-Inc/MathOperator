# OpForge current state

This is the first document a maintainer or future AI should read.

## Status

- Current phase: Phase 0 baseline freeze plus completed Layers 15, 16, 17, 18, and 19.
- Latest completed scope: Phase 0, Scientific Regression Benchmark v1, the
  representation-only Layer 15 core, a parallel Layer 16 quotient-search path,
  and separate Layer 17 goal-directed/bidirectional, Layer 18 proof-planning,
  and Layer 19 verification/evidence paths. The
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
- Layer 20 remains deferred: no practical-utility benchmarking was added.

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
