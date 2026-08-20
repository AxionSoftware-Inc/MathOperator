# OpForge scientific regression baseline v1

Date: 2026-08-20  
Repository state: Phase-0-repaired OpForge; Layer 15 intentionally not implemented.

## Purpose and epistemic rule

This report freezes the engine's current rediscovery and search behavior before
further architecture changes. The benchmark is designed to preserve honest MISS
results. It does not optimize for recall, candidate count, or a nonzero novelty
claim.

The current rule is:

```text
mask target -> structural discovery -> external scoring
```

The discovery engine receives only the masked Atlas. The external scorer is the
only component that knows the hidden target specification.

## Audit of historical rediscovery evidence

The repository's older mechanisms are retained, but they are not used as the
current scientific baseline.

### Legacy fixed-target logic

- `src/atlas/validation.cpp` contains `RediscoveryEngine::discover`, which
  directly names `op.gradient`, `op.divergence`, and `op.laplacian` and returns a
  Laplacian result when that known pattern is present.
- `src/discovery/composition.cpp` contains dedicated
  `rediscover_div_grad`, `rediscover_curl_grad`, and `rediscover_div_curl`
  functions with target-specific operator and identity IDs.
- `src/benchmarks/rediscovery.cpp` previously branched on strings such as
  `laplacian_rediscovery` and `zero_composition`. Its zero-composition branch
  used `NumericalExecutor` on a finite-difference probe.
- Existing tests intentionally preserve these functions as compatibility and
  historical/demo coverage. They are not deleted and their results are not
  counted as blind rediscovery evidence.

### Historical open-search reports

The checked-in v0.25 open-search report recorded 287 patterns, 170 generated
candidates, 30 numerical experiments, and 4 surviving candidates. The v0.3 and
v0.5 reports similarly recorded 30 numerical experiments and 4 surviving leads.
Those runs are useful historical implementation snapshots, but numerical
experiments were part of the discovery-era workflow and the candidates were
Atlas-derived. They are not target-blind structural hold-out measurements.

The v0.13 closure report also recorded 38 contradictory candidates. Phase 0
reclassified overloaded semantic `Identity` records so only explicit executable
equality ASTs enter equality closure. The current bounded closure therefore has
different semantics and must not be compared as a simple numerical improvement.

## New target-blind harness

Implemented in `include/opforge/benchmarks/rediscovery.hpp` and
`src/benchmarks/rediscovery.cpp` as `BlindRediscoveryHarness`.

For every fixture it deterministically:

1. removes declared target operators when requested;
2. removes target identities and all identity expressions referring to hidden
   operators;
3. removes declared relation keys and every relation between the hidden target
   endpoints;
4. clears descriptive names, aliases, formulas, evidence, assumptions and
   provenance from the masked copy;
5. remaps visible operator and identity IDs to neutral `blind.*` IDs;
6. runs `PatternAnalyzer` and the normal structural `CandidateSynthesizer`;
7. lets a generic external scorer compare structural output with the hidden
   target specification.

The harness reports `engine_knows_targets=false`. It does not call the numerical
executor. A target-specific branch is not present in the discovery path; target
specifications exist only in the fixture/scorer layer.

## Blind rediscovery results

Command:

```bash
./build/opforge benchmark blind_rediscovery atlas
```

| Case | Result | Interpretation |
|---|---|---|
| Laplacian-like composition | structural-equivalent | `div ∘ grad` was recovered from visible typed composition after the Laplacian operator and identities were hidden |
| curl-gradient zero | partial | the typed composition survived, but the hidden annihilation property was not recovered |
| exterior nilpotence | missed | no target composition or zero property survived the duplicate-identity mask |
| projection decomposition | partial | endpoint family remained visible; the hidden decomposition relation was not recovered |
| Fourier correspondence | partial | endpoint structure remained visible; the hidden transform relation was not recovered |
| continuous/discrete analogy | partial | endpoint structure remained visible; the hidden bridge relation was not recovered |

Negative controls:

- type-incompatible `gradient ∘ gradient`: passed;
- near-miss `divergence ∘ divergence`: passed;
- missing-prerequisite case with divergence removed: passed.

Measured scorecard:

```text
exact=0
structural=1
partial=4
missed=1
false_positive=0
negative_control_pass=3
invalid_benchmarks=0
leakage_events=0
precision=1
exact_rate=0
structural_rate=0.166667
full_structural_recovery_rate=0.166667
partial_rate=0.666667
miss_rate=0.166667
false_positive_rate=0
negative_control_pass_rate=1
useful_signal_rate_including_partial=0.833333
numerical_experiments=0
```

The four partial results are deliberately weak: they record visible typed
endpoint/composition structure, not recovery of the complete hidden theorem or
relation. The nilpotence MISS is retained as a valid limitation. The old
compatibility field named `recall` is now equal to the full structural recovery
rate (`0.166667`) and is not used as the headline metric.

Aggregate masked-search accounting was:

```text
graph candidates=3850
compatible edges=3503
retained patterns=4544
generated candidates=3503
canonical classes=3503
canonical duplicates=0
rejected candidates=16
```

## Scientific integrity audit

### Case-by-case disclosure

The following details come from a benchmark-only disclosure probe over the full
98-operator Atlas. The probe does not alter the production Atlas.

Common metadata masking is deterministic. On retained operators it clears
`name`, `symbol`, `mathematical_domain`, provenance, aliases, parameters,
invariants, theorems, applications, limitations, sources, coordinate
definitions, numerical-stability text, complexity text, evidence and
verification status. On retained identities it clears the name, assumptions,
dimension/regularity constraints, counterexamples, sources, required structures,
applicable domains, canonical form, provenance, geometric text and evidence.
Relation condition/evidence text is cleared. Typed signatures, spaces, relation
kinds and remapped expression ASTs remain because they are the allowed
structural input.

#### `blind_laplacian_composition`

- Hidden target: `div ∘ grad -> Laplacian`; target operator removed:
  `op.laplacian`.
- Removed identities: `atlas.identity.laplacian`,
  `id.v013.discrete_laplacian_limit`, `id.bridge.laplacian_hodge`,
  `id.bridge.continuous_discrete_laplacian`.
- Removed relations:
  `discrete.combinatorial_laplacian|related_to|op.laplacian`,
  `discrete.laplacian|continuous_limit_of|op.laplacian`,
  `geom.laplace_beltrami|continuous_analog|op.laplacian`,
  `op.divergence|factorization|op.laplacian`,
  `op.gradient|factorization|op.laplacian`,
  `op.laplacian|generalizes|form.de_rham_laplacian`,
  `op.laplacian|discretized_by|discrete.laplacian`,
  `op.laplacian|decomposition|form.de_rham_laplacian`,
  `op.laplacian|continuous_analog|discrete.laplacian`.
- Visible prerequisites: `op.gradient: scalar.r3 -> vector.r3, order=1`;
  `op.divergence: vector.r3 -> scalar.r3, order=1`.
- Exact discovery output:

  ```text
  compose(blind.op.69,blind.op.70)
  ```

  External mapping is `blind.op.69=op.divergence` and
  `blind.op.70=op.gradient`. The output was classified structural-equivalent
  because the hidden named Laplacian was absent and only its typed composition
  remained. Verdict: **VALID BLIND BENCHMARK**.

#### `blind_curl_gradient_zero`

- Hidden target: `curl ∘ grad = 0`; removed identity:
  `id.vector.curl_grad_zero`.
- Removed operators and relations: none.
- Visible prerequisites: `op.gradient: scalar.r3 -> vector.r3, order=1`;
  `op.curl.3d: vector.r3 -> vector.r3, order=1`; the zero operator remains
  visible.
- Exact discovery output:

  ```text
  compose(blind.op.68,blind.op.70)
  ```

  This is `curl ∘ grad` after external mapping, but no zero-composition pattern
  was produced. It is partial, not an exact rediscovery. Verdict: **VALID BUT
  WEAK/PARTIAL**.

#### `blind_exterior_nilpotence`

- Hidden target: `d ∘ d = 0`.
- Removed identity: loaded duplicate `atlas.identity.exterior_square`.
  The declared `id.forms.d_squared` does not exist as a loaded identity in the
  current 67-statement Atlas; it is therefore a no-op declaration in this
  fixture, not evidence consumed by discovery.
- Removed operators and relations: none.
- Visible prerequisites: `form.exterior_derivative:
  form.m.k -> form.m.k1, order=1` remains, but the same generic signature is
  not composable with itself because the outer operator expects `form.m.k`, not
  `form.m.k1`.
- Exact discovery output: none. Classification: MISS.
- Verdict: **VALID BUT WEAK/PARTIAL**. The mask is blind, but this MISS mainly
  exposes a representation limitation of the current typed signature rather
  than a clean test of mathematical nilpotence.

#### `blind_projection_decomposition`

- Hidden target: symmetric/skew projection decomposition.
- Removed identities: `id.v013.symmetric_skew_reconstruction`,
  `id.linear.symmetric_skew`.
- Removed relations:
  `la.symmetric_projection|related_to|la.skew_projection` and
  `la.symmetric_projection|decomposition|la.skew_projection`.
- Visible prerequisites: `la.symmetric_projection:
  matrix.n -> matrix.symmetric.n, order=0`; `la.skew_projection:
  matrix.n -> matrix.skew.n, order=0`.
- Exact discovery output:

  ```text
  family(blind.op.64,blind.op.62)
  ```

  The endpoint family survives (`blind.op.64` and `blind.op.62` map to the two
  projections), but the decomposition relation does not. Verdict: **VALID BUT
  WEAK/PARTIAL**.

#### `blind_fourier_correspondence`

- Hidden target: convolution/Fourier transform correspondence.
- Removed identities: `id.transform.convolution_theorem`,
  `id.bridge.convolution_fourier`.
- Removed relations:
  `transform.convolution|related_to|transform.fourier` and
  `transform.convolution|transform_correspondence|transform.fourier`.
- Visible prerequisites: `transform.convolution:
  signal.time -> signal.time, order=0`; `transform.fourier:
  signal.time -> frequency.space, order=0`.
- Exact discovery output:

  ```text
  family(blind.op.83,blind.op.85)
  ```

  This is only an endpoint-family observation; the hidden relation is not
  recovered. The opaque IDs map externally to convolution and Fourier.
  Verdict: **VALID BUT WEAK/PARTIAL**.

#### `blind_continuous_discrete_analogy`

- Hidden target: continuous/discrete gradient analogy.
- Removed identity: `id.discrete.continuous_gradient`.
- Removed relations:
  `op.gradient|discretized_by|discrete.gradient` and
  `op.gradient|discrete_analog|discrete.gradient`.
- Visible prerequisites: `op.gradient:
  scalar.r3 -> vector.r3, order=1`; `discrete.gradient:
  grid.scalar.r3 -> grid.vector.r3, order=1`.
- Exact discovery output:

  ```text
  family(blind.op.70,blind.op.17)
  ```

  The typed endpoints remain, but no continuous/discrete relation is
  recovered. Verdict: **VALID BUT WEAK/PARTIAL**.

#### Negative controls

- `negative_type_incompatible`: target-shaped forbidden `gradient ∘ gradient`;
  no items removed; the visible prerequisite is `op.gradient:
  scalar.r3 -> vector.r3, order=1`. Reusing it as the outer operator is
  type-incompatible. Exact search output: none; negative-control pass.
- `negative_near_miss`: forbidden `divergence ∘ divergence`; no items removed;
  the visible prerequisite is `op.divergence: vector.r3 -> scalar.r3, order=1`;
  it cannot feed another divergence. Exact search output: none;
  negative-control pass.
- `negative_missing_prerequisite`: forbidden `div ∘ grad` with
  `op.divergence` removed. Removed identities:
  `atlas.identity.laplacian`, `id.vector.div_curl_zero`,
  `id.discrete.continuous_divergence`, `id.bridge.div_codifferential`.
  Removed relations:
  `discrete.graph_divergence|discrete_analog|op.divergence`,
  `op.divergence|related_to|form.codifferential`,
  `op.divergence|discretized_by|discrete.divergence`,
  `op.divergence|continuous_analog|form.codifferential`,
  `op.divergence|discrete_analog|discrete.divergence`,
  `op.divergence|factorization|op.laplacian`. No target-shaped output;
  the only visible prerequisite is `op.gradient: scalar.r3 -> vector.r3,
  order=1`; `op.divergence` is absent. Exact search output: none;
  negative-control pass. Verdict for all three: **VALID BLIND BENCHMARK**.

### Final verdict by case

| Case | Final verdict |
|---|---|
| `blind_laplacian_composition` | **VALID BLIND BENCHMARK** |
| `blind_curl_gradient_zero` | **VALID BUT WEAK/PARTIAL** |
| `blind_exterior_nilpotence` | **VALID BUT WEAK/PARTIAL** — the MISS is representation-limited |
| `blind_projection_decomposition` | **VALID BUT WEAK/PARTIAL** |
| `blind_fourier_correspondence` | **VALID BUT WEAK/PARTIAL** |
| `blind_continuous_discrete_analogy` | **VALID BUT WEAK/PARTIAL** |
| `negative_type_incompatible` | **VALID BLIND BENCHMARK** |
| `negative_near_miss` | **VALID BLIND BENCHMARK** |
| `negative_missing_prerequisite` | **VALID BLIND BENCHMARK** |

The three relation cases are blind and leakage-free, but their current scorer
only checks whether the two endpoint operators form a family pattern. It does
not yet require the hidden `relation_kind` to be rediscovered. Their `partial`
labels therefore mean endpoint-structure signal, not relation recovery.

### Leakage audit

The new discovery path consumes only `fixture.atlas` after masking. It does not
receive the `BlindTarget`, benchmark ID, expected operator, expected composition,
expected family, or expected pattern type. The scorer is called only after
`PatternAnalyzer` and `CandidateSynthesizer` return.

The masked fixture removes operator names, aliases, formulas, sources, evidence,
identity prose, relation prose and original IDs. Visible space IDs such as
`scalar.r3`, relation kinds, typed signatures and non-target expression ASTs
remain by design as structural primitives. They do not encode the hidden answer
directly. No benchmark ID, family name, target-specific branch, or expected
answer is passed to the analyzer or synthesizer. The strongest case is consumed as
`blind.op.69`/`blind.op.70`, not as divergence/gradient.

Static audit found the old target-coupled helpers still present in
`RediscoveryEngine`, `rediscover_div_grad`, `rediscover_curl_grad`,
`rediscover_div_curl`, and the legacy `HistoricalRediscovery` ID branches. They
are not called by `BlindRediscoveryHarness`; they remain explicitly marked
legacy/demo. The new harness contains no benchmark-ID switch in its discovery
path. Verdict: **VALID BLIND BENCHMARK** for the new suite; legacy helpers are
**INVALID DUE TO TARGET COUPLING** as evidence.

### Opaque-ID robustness

The blind harness itself constructs a benchmark-only transformed Atlas. All
retained operators become deterministic `blind.op.N` IDs and `operator_N` names;
identities become `blind.identity.N`. The original Atlas files are not modified.
For the strongest case, the structural output was
`compose(blind.op.69,blind.op.70)`, and the external scorer alone mapped those
IDs back to divergence and gradient. This is a genuine opaque-ID run, not a
second name-based scorer invocation.

### Dependency ablation

Removing one prerequisite at a time produced:

| Case | Removed prerequisite | Result |
|---|---|---|
| Laplacian composition | `op.gradient` | miss |
| Laplacian composition | `op.divergence` | miss |
| curl-gradient | `op.gradient` | miss |
| curl-gradient | `op.curl.3d` | miss |
| curl-gradient | `op.zero.scalar_to_vector.r3` | still partial |
| projection decomposition | either endpoint projection | miss |
| Fourier correspondence | convolution or Fourier | miss |
| continuous/discrete analogy | continuous or discrete gradient | miss |

The zero-operator ablation is informative: the current partial result does not
depend on the zero operator because it only claims typed composition, not the
annihilation law. This confirms that the partial label is not being upgraded to
the target theorem.

### Determinism

The complete blind suite was run three times with identical Atlas and config.
All three full CLI outputs produced identical classifications, counts, and
per-case candidate-ID digests:

```text
exact=0 structural=1 partial=4 missed=1 false_positive=0
negative_control_pass=3 leakage_events=0 numerical_experiments=0
generated_candidates=3503 canonical_classes=3503
```

Candidate-ID digests from the run were:

```text
blind_laplacian_composition=d0edf7bae2aa7316
blind_curl_gradient_zero=506c7f6adc875613
blind_exterior_nilpotence=506c7f6adc875613
blind_projection_decomposition=506c7f6adc875613
blind_fourier_correspondence=506c7f6adc875613
blind_continuous_discrete_analogy=506c7f6adc875613
negative_type_incompatible=506c7f6adc875613
negative_near_miss=506c7f6adc875613
negative_missing_prerequisite=f782385d22c44dcc
```

The digest is a stable FNV-1a digest of the sorted accepted/rejected candidate
IDs; it is an audit checksum, not a scientific metric. The existing regression
test also compares repeated JSON exports byte-for-byte.

The structural analyzer sorts retained graph edges, candidate IDs use the
deterministic canonical-expression hash, and the regression test repeats the
exported report comparison. No timing value is used in classification.

## Metric definitions and the open-search/scaling difference

- `compatible_edges`: valid typed inner/outer operator pairs retained in the
  structural graph. `graph_candidates` is the number of pairs inspected before
  type/space/regularity rejection.
- Scaling `raw_candidate_count`: composition-pattern candidates considered by
  the direct one-cycle structural synthesizer before its canonical `seen` set.
- `canonical_classes`: unique canonical expressions retained as accepted or
  rejected candidates; `canonical_duplicates` counts raw candidates collapsed
  by that canonicalization.
- Open-search `Candidates generated=62`: unique candidate IDs remembered by the
  multi-cycle campaign after per-cycle frontier bounding, false-interest
  filtering and cross-cycle merging. It is not the raw one-cycle candidate
  pool.
- `pruned=650`: the sum of candidates discarded by the bounded frontier across
  the two open-search cycles. It can exceed final generated IDs because it is a
  cumulative discard count, while `62` is a final unique retained-memory count.
- `serious candidates`: candidates passing the current serious-candidate gate:
  non-equivalent, sufficiently interesting, no assumption failure or
  counterexample, and not unknown after evaluation. It is not a count of all
  structural candidates.

Therefore full scaling reports 391 raw structural candidates on 98 operators,
while the open campaign reports 62 retained candidate IDs after two cycles and
650 cumulative frontier prunes. These numbers measure different stages and are
not forced to match.

## Negative-control and invariant coverage

Automated tests now check:

- blind leakage is zero for all valid fixtures;
- blind numerical experiments are zero;
- semantic statements do not become equality proof edges;
- repeated blind execution produces identical exported counts/results;
- deep repeated campaigns are not counted as independent evidence;
- ordinary open structural synthesis emits no unrestricted addition or scalar-
  multiplication candidates;
- goal-directed weighted linear combinations remain available only through an
  explicit justified synthesis goal;
- unsupported property execution remains `unsupported`, not `false`;
- unrun geometry diagnostics remain `not_run`, with JSON coordinate consistency
  represented as `null`, not `failed`;
- zero serious/novel candidates is an accepted result.

The candidate report now also exposes raw candidate and canonical-duplicate
counts so future quotient-search work can measure compression rather than hide
it inside a `set`.

## Full open-search regression

Command:

```bash
./build/opforge benchmark open_search atlas
```

Configuration: full 98-operator Atlas, structural mode, two cycles, 40 action
budget, 64 retained candidate-lead budget, numerical verification disabled.

Measured result:

```text
patterns discovered=508
candidates generated=62
candidates pruned by budget=650
candidates rejected=4
known-equivalent=0
known constructions=0
numerical experiments=0
surviving candidates=0
serious candidates=0
false-interest cases=4
geometry/numeric diagnostics=not_run
epistemic status=structural_exploration_only
```

`0 serious novel candidates` is the expected valid negative outcome at this
stage. The run does not turn structural leads into discoveries and does not use
numerics to rank, reject, rescue, or promote them.

## Search-explosion scaling regression

Command:

```bash
./build/opforge benchmark scaling atlas --medium-operators 50
```

The medium case is a new deterministic approximately-50-operator subset. It is
not claimed to reconstruct the historical 51-operator Atlas.

| Fixture | Operators | Spaces | Compatible edges | Raw candidates | Canonical classes | Duplicates | Rejected | Pruned | Frontier cap | Runtime (ms) | Numeric |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| small seed | 12 | 5 | 25 | 25 | 25 | 0 | 3 | 0 | 64 | 22.09 | 0 |
| new approximately-50 subset | 50 | 47 | 150 | 150 | 150 | 0 | 0 | 86 | 64 | 134.05 | 0 |
| full Atlas | 98 | 47 | 391 | 391 | 391 | 0 | 2 | 325 | 64 | 563.87 | 0 |

Runtime is a local wall-clock measurement and is not a mathematical invariant.
The meaningful frozen quantities are the deterministic counts and the explicit
frontier/pruning ledger. Both medium and full runs are marked truncated because
the current bounded campaign frontier prunes leads. This is temporary control,
not a completeness claim and not the final Layer 16 quotient search.

## Linear-combination explosion control

The ordinary `CandidateSynthesizer::synthesize(atlas, patterns)` path creates
typed composition candidates only. Regression tests inspect accepted candidates
and reject any unrestricted `Addition` or `ScalarMultiplication` expression.

`GoalDirectedSynthesizer` still exposes `WeightedLinearCombination` for a
justified missing-role goal, and `CorrectionTerm` for residual-driven repair.
This preserves legitimate goal-directed correction synthesis without allowing
open discovery to enumerate arbitrary linear combinations.

## Verification

Completed checks:

- clean target-blind suite: pass; MISS is accepted;
- negative controls: 3/3 passed;
- open-search regression: pass, numerical experiments 0;
- scaling regression: 3/3 runs completed, numerical experiments 0;
- CTest: `1/1` passed;
- `git diff --check`: clean.

The repository still emits pre-existing aggregate-initializer warnings from the
axiomatic fixture source under `-Wall -Wextra -Wpedantic`; they are unrelated to
the benchmark changes and do not fail the build.

## Known limitations

- Structural equivalence is currently typed-graph equivalence, not a proof.
- Partial endpoint-family matches are intentionally not counted as full relation
  recovery.
- Canonical duplicate counts are zero for the current pairwise composition
  grammar; future richer grammars should retain duplicate accounting before
  pruning.
- The current approximately-50 subset is a new benchmark fixture, not historical
  data.
- The harness does not implement Layer 15 Proposition IR, formal certificates,
  or complete quotient search.
- No result in this report proves new mathematics or makes a novelty claim.

## Changed files

- `include/opforge/benchmarks/rediscovery.hpp` and
  `src/benchmarks/rediscovery.cpp`: blind scorecard metric separation,
  opaque-ID discovery reporting, and candidate-ID audit digests.
- `tests/atlas_tests.cpp`: deterministic blind-export and digest assertions.
- `reports/scientific_regression_baseline_v1.md`: this integrity audit.

No production discovery behavior was changed for this audit. Layer 15 was not
implemented.
