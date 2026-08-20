# OpForge frozen baselines

These are scientific and engineering baselines, not performance targets. A
future change may improve them, but unexplained regressions block layer
advancement. Baselines must be interpreted with the architecture and invariants
documents, not as claims of general mathematical capability.

## Phase 0 semantic baseline

Current loaded Atlas:

- operators: 98;
- mathematical spaces: 47;
- typed relations: 119;
- semantic mathematical statements: 67;
- machine-executable equality statements: 6;
- non-executable semantic statements: 61.

Phase-0 equality closure:

- generated: 20;
- accepted: 10;
- duplicates: 10;
- contradictions: 0.

The six executable equalities are the only identity records allowed into the
current equality closure/rewrite boundary. Semantic relation statements remain
Atlas evidence until a typed proposition/proof model can represent them safely.

Numerical discovery invariant:

- open structural discovery performs zero numerical discovery experiments;
- numerical execution is an explicit downstream proof/diagnostic path; and
- incomplete candidates cannot enter that path.

## Scientific Regression Benchmark v1

The current target-blind positive suite reports:

- genuine structural recovery: 1/6;
- partial structural signal: 4/6;
- miss: 1/6;
- false positives: 0;
- negative controls passed: 3/3;
- benchmark leakage events: 0;
- numerical discovery experiments: 0;
- opaque-ID robustness: passed; and
- deterministic reruns: passed.

The strongest opaque-ID discovery is represented externally as a composition of
opaque operators. Partial results are not counted as full recall. Relation cases
currently test endpoint/family signal rather than complete relation-kind
recovery.

The complete case-by-case disclosure, masking audit, ablations, metric
definitions, and final verdicts are in
[scientific_regression_baseline_v1.md](reports/scientific_regression_baseline_v1.md).

## Scaling baseline

The direct structural scaling run reports:

| Operators | Compatible structural candidates |
|---:|---:|
| 12 | 25 |
| 50 | 150 |
| 98 | 391 |

These are raw/direct structural-stage quantities. They are not the same as the
multi-cycle open-search retained candidate count. In the current open campaign:

- generated candidate IDs retained after bounded multi-stage processing: 62;
- cumulative frontier candidates pruned across two cycles: 650;
- serious candidates: 0; and
- numerical experiments: 0.

The counts measure different stages and must not be cosmetically normalized.
Definitions and the `391` versus `62`/`650` explanation are frozen in the
scientific regression report.

## Layer-24 search scalability baseline

Layer 24 is a separate goal-directed path; the historical open-discovery
baseline above is unchanged. Verified result:

- verdict: `SCALABLE_CONSTRAINT_DIRECTED_SEARCH_DEMONSTRATED`;
- 8/8 finite reference/optimized cases have identical canonical exact and
  fully explored UNKNOWN sets;
- shallow constrained materialization reductions include commutator 10→8,
  conjugation 10→8, restriction 12→6, tensor 42→3, indexed 9→7, and UNKNOWN
  explosion 124→22; multi-step is 95→93 with conservative internal retention;
- finite exhaustive control: raw 12, one retained constructor representative,
  three primitive terminals, eight type-invalid, zero resource-pruned,
  `EXHAUSTED_RELATIVE_SPACE`, accounting pass;
- finite budget control: same raw 12, nine resource-pruned,
  `BUDGET_ENDED`, `relative_complete=false`;
- UNKNOWN budget control: 10 UNKNOWN states, 8 deferred,
  `INCOMPLETE_UNKNOWN`;
- distractor scaling (synthetic fixture): full theory 16→1006 operators,
  relevant slice fixed at 4, optimized materialization fixed at 8;
- million-scale stress: 1,000,000 raw, 999,999 avoided, 1 materialized, 1
  retained, 0 UNKNOWN, `EXHAUSTED_RELATIVE_SPACE` for the declared synthetic
  relative space; and
- controlled vector-calculus seed probe: 12 operators, 39 full facts,
  11-operator/33-fact slice, 19 materialized, 8 retained, 0 UNKNOWN;
- full production Atlas integration through `AtlasLoader::load("atlas")`: 98
  operators, 47 spaces, 119 relations, 67 statements (6 executable equalities,
  61 semantic statements), Atlas digest
  `layer24-atlas-snapshot.267014ce981723bb`;
- full Layer-23 migration: 6 previous fully structured, 321 newly structured,
  327 cumulative fully structured, 220 partial, 0 unsupported; 47 space
  properties, 0 structured space relations, 274 operator properties, and 4
  trusted rules;
- full-Atlas probe: 8 operators / 24 facts / 1 space / 4 rules in the slice;
  optimized 64 attempted, 72 materialized, 64 exact retained, 0 UNKNOWN;
  full reference 58016 attempted, 20579 materialized, 64/64 canonical
  equivalence, `EXHAUSTED_RELATIVE_SPACE`; and
- full-Atlas three-run replay, Theory-mutation cache invalidation, Context
  isolation, and Regime isolation: passed.

Full case outputs, scorer isolation, ledger controls, and machine metrics are
in `reports/layer24_search_scalability_v2.md` and
`reports/layer24_search_scalability_v2.json`.

## Layer 15 regression verification

Layer 15 was added as a separate semantic-core library surface. The frozen
discovery path was rerun after the change and retained the exact blind,
scaling, open-search, opaque-ID, deterministic, leakage, and numerical-isolation
results above. Verification used:

- clean configure/build in `/tmp/opforge-layer15-clean-v1`;
- CTest: 2/2 passed in the clean build;
- AddressSanitizer/UndefinedBehaviorSanitizer configure/build in
  `/tmp/opforge-layer15-asan-v1`; and
- sanitizer CTest: 2/2 passed.

The pre-existing compiler warnings are aggregate-initializer warnings in older
Atlas/discovery/axiomatic sources; Layer-15 sources introduced no warning in
the clean or sanitizer build. Full migration counts and the exact baseline
comparison are in [layer15_semantic_core.md](reports/layer15_semantic_core.md).

## Layer 16 quotient-search baseline

The Layer-16 path is measured separately from the legacy scaling/open-search
path. It uses an ordered-pair construction grammar over the migrated typed
operator declarations:

| Operators | Legacy compatible/raw reference | Layer-16 raw | Type-valid | Type-invalid | Classes | Runtime ms |
|---:|---:|---:|---:|---:|---:|---:|
| 12 | 25 | 144 | 58 | 86 | 58 | 14.178 |
| 50 | 150 | 2,500 | 217 | 2,283 | 217 | 187.060 |
| 98 | 391 | 9,604 | 488 | 9,116 | 488 | 699.433 |

The stages are intentionally not conflated: the legacy counts include Atlas
structure/regularity checks, while Layer 16 currently uses the Layer-15
typed domain/codomain fragment. The deterministic finite benchmark reports
`EXHAUSTED_RELATIVE_SPACE`; its budgeted rerun reports `BUDGET_ENDED`.

The streamed synthetic benchmark processed 1,000,000 raw constructions with
5 retained classes, 999,995 lossless reductions, 0 lossy reductions, and
`INCOMPLETE_UNKNOWN` because unknown candidates were retained rather than
rejected. It retained no per-candidate ledger records in stress mode.

The implementation and full ledger counts are in
[layer16_quotient_search.md](reports/layer16_quotient_search.md).

## Layer 17 goal-directed baseline

Layer 17 is measured by the separate command:

```bash
./build/opforge benchmark goal_search atlas
```

The controlled positive suite reports four single structural solutions and one
multiple-solution case:

| Case | Result | Relative complete |
|---|---|---|
| composition | `SOLVED_STRUCTURALLY` | yes |
| trusted identity | `SOLVED_STRUCTURALLY` | yes |
| indexed family | `SOLVED_STRUCTURALLY` | yes |
| multistep | `SOLVED_STRUCTURALLY` | yes |
| multiple solution | `MULTIPLE_STRUCTURAL_SOLUTIONS` | yes |

Negative controls remain explicit: impossible type and incompatible regime are
`INVALID_PROBLEM`; missing prerequisite and approximation-vs-equality are
`NO_SOLUTION_IN_RELATIVE_SPACE` with relative completeness; the unknown-type
case is `UNDER_SPECIFIED`. A separate unknown-side-condition regression ends
`INCOMPLETE_UNKNOWN` without a solution.

The finite three-atom goal grammar reports three constructions, three retained
classes, three solutions, and `EXHAUSTED_RELATIVE_SPACE`. The same grammar with
budget one reports one construction, two budget-pruned items,
`BUDGET_ENDED`, and `relative_complete=false`. Forward construction counts,
quotient classes, meetings, decompositions, and goal ledger counts are exported
separately; they are not relabelled as one recall number.

The controlled performance comparison reports 21 retained forward states from
42 forward-only constructions versus 6 retained states from 6 constructions on
the bidirectional path, with 3 backward states and fewer meetings in the latter
run. This is a bounded target-oriented comparison, not a universal complexity
claim.

Layer 17 does not add any quotient using the 180 partially structured Layer-15
facts. The Layer-16 partial-fact-removal test remains a required dependency and
passes. The open discovery baseline remains unchanged: structural 1/6, partial
4/6, miss 1/6, false positives 0, negative controls 3/3, leakage 0, and
numerical discovery 0.

The full design, per-case metrics, finite/budget details, target-blind boundary,
and limitations are in
[layer17_bidirectional_reasoning.md](reports/layer17_bidirectional_reasoning.md).

## Layer 18 proof-planning baseline

Layer 18 is measured separately with:

```bash
./build/opforge benchmark proof_plan atlas
```

It does not alter discovery counts. The real Layer-17 composition candidate
produces a proof plan with 13 generated obligation attempts, 7 unique
obligations, 6 lossless duplicate joins, 7 structural discharges, 11 DAG
nodes, and 14 edges. The plan status is
`COMPLETE_AT_REQUIRED_LEVEL` at `STRUCTURAL`; it is not `PROVED`.

Controlled proof-plan status baseline:

| Case | Status | Generated / unique / duplicate |
|---|---|---:|
| trusted fact | `COMPLETE_AT_REQUIRED_LEVEL` | 3 / 3 / 0 |
| missing premise | `INCOMPLETE_OPEN_OBLIGATIONS` | 6 / 5 / 1 |
| unknown regime | `BLOCKED_UNKNOWN` | 3 / 3 / 0 |
| falsified target | `FALSIFIED` | 1 / 1 / 0 |
| contradicted regime | `CONTRADICTED` | 3 / 3 / 0 |
| shared DAG | `COMPLETE_AT_REQUIRED_LEVEL` | 7 / 5 / 2 |
| cyclic dependency | `CYCLIC` | 7 / 5 / 2 |
| indexed nilpotence | symbolic-level complete | 4 / 4 / 0 |

Every Layer-18 plan satisfies `generated = unique + duplicate` and the
lossless terminal-category accounting invariant. Replay after removing a
trusted fact keeps the plan ID but reopens the dependent obligation. The three
Layer-17 multiple-solution candidates retain three separate proof-plan IDs.

The proof-planning report is
[layer18_proof_planning.md](reports/layer18_proof_planning.md). These are
audit/status baselines, not theorem or performance claims.

## Layer 19 verification baseline

Layer 19 is measured separately with:

```bash
./build/opforge benchmark verification atlas
./build/opforge benchmark synthesis_utility atlas
```

The capability baseline is:

- internal exact backend: structured type/definedness, exact literals, and
  bounded trusted rewrite replay;
- numerical backend: explicit post-search support and suspicious-counterexample
  candidates only;
- formal proof/refutation capabilities: unavailable;
- formal status: `FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED`.

Controlled results include exact trusted rewrite and exact typing certificates,
`UNSUPPORTED` for an unsupported exact fragment, exact literal falsification,
`NUMERICALLY_SUPPORTED` with a formal requirement deliberately left open, and a
numerical suspicious counterexample that is not marked as exact refutation.
Certificate replay succeeds on unchanged Theory and invalidates after removal
of the trusted rewrite dependency. Multiple certificates remain attached to a
single obligation. ResultBundle IDs and Layer-19 semantic digests are
deterministic with wall-clock timing excluded.

The Layer-19 numerics firewall compares Layer-16 finite quotient classes,
Layer-17 exported candidate results, and the bounded open-discovery report
before and after a numerical verification request. It passes. This does not
change the frozen discovery numerical experiment count, which remains zero:
`discovery_numerical_experiments=0`. Verification requests are counted
separately as `verification_numerical_experiments`.

## Layer 20 practical utility baseline

Layer 20 is measured separately with:

    ./build/opforge benchmark utility_gate atlas

The frozen Layer-20 result is:

- 15 cases;
- structural exact/valid-alternative/partial/miss/false-positive:
  1/6/0/8/0;
- proof complete/partial/open/unsupported/falsified: 0/0/11/1/3;
- search exhausted/budget-ended/incomplete-or-invalid/unsupported-language:
  9/1/3/2;
- negative controls: 5/5;
- target-blind leakage audit: PASS;
- opaque-ID synthesis: PASS;
- deterministic replay: 3/3;
- legacy open discovery generated/pruned/serious: 62/650/0;
- target-free hidden forward fixture: raw 6, retained classes 4, lossless
  reductions 2, unresolved 0, status EXHAUSTED_RELATIVE_SPACE;
- discovery numerical experiments: 0; and
- formal backend: NOT YET IMPLEMENTED.

The budget-ended synthesis control has relative_complete=false and is never
counted as relative exhaustion. The real-Atlas transfer case is explicitly
NOT_RUN_REAL_ATLAS_LIMITATION; the controlled typed bridge probe is not called
a transfer theorem. The full case-by-case result and audit fields are in
reports/layer20_practical_utility.md and reports/layer20_practical_utility.json.

## Layer 21 generative synthesis baseline

Layer 21 is measured separately with:

```bash
./build/opforge benchmark synthesis_utility atlas
```

The historical Layer-20 section above is frozen. Layer 21 adds a generic
constructor grammar and does not alter the earlier counts.

Measured Layer-21 result:

- verdict: `LIMITED_GENERATIVE_SYNTHESIS_DEMONSTRATED`;
- 12 target-blind controlled cases;
- implemented families: composition, adjoint, left/right/two-sided inverse
  candidates, commutator, conjugation, and indexed instantiation;
- opaque-ID positives: 2;
- open policy enables composition and indexed instantiation only;
- open raw/valid/invalid/unknown: 144/50/94/0;
- open quotient merges/retained/serious/budget-pruned: 0/62/0/0;
- two UNKNOWN-precondition controls remain open and are not counted as valid
  theorems;
- tensor/product missing-constructor control remains `UNSUPPORTED_LANGUAGE`;
- target-blind leakage PASS, false positives 0, discovery numerics 0, runtime
  LLM calls 0, and unrestricted linear combinations disabled;
- deterministic replay: 3/3, digest
  `layer21_benchmark_digest.4c26807e3ed03a83`;
- Release CTest 8/8; ASan/UBSan CTest 8/8; and
- formal backend: `NOT YET IMPLEMENTED`.

The detailed case disclosure and exact canonical candidates are in
[reports/layer21_generative_operator_synthesis.md](reports/layer21_generative_operator_synthesis.md)
and [reports/layer21_synthesis_utility.json](reports/layer21_synthesis_utility.json).

## Layer 22 constraint-guided synthesis baseline

Layer 22 is measured separately with:

```bash
./build/opforge benchmark constraint_synthesis atlas
```

The historical Layer-20 and Layer-21 sections above remain frozen. The
Layer-22 result is:

- verdict: `CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED`;
- 11 target-blind controlled cases and 2 opaque-ID positives;
- type-only ambiguity retained 4 candidates;
- constrained adjoint, commutator, conjugation, and indexed cases retained 1
  exact structural-form candidate each;
- left-inverse retained 2 open candidates, while two-sided inverse retained 1
  open candidate; neither inverse law was claimed proven;
- false-property negative: 0 retained candidates; UNKNOWN property: 22 open
  candidates and 48 proof obligations;
- scaling Layer-21 type-only compatible / Layer-22 retained classes:
  `39/1`, `132/1`, `279/1` for 3/6/9 operators;
- leakage/scorer isolation PASS; opaque-ID PASS; numerics 0; runtime LLM 0;
- real Atlas probe: 6 fully structured facts,
  `UNSUPPORTED_CONSTRAINT_LANGUAGE`; and
- deterministic replay 3/3, digest
  `layer22_benchmark_digest.d148a171b146b533`.

Layer-22 raw attempt counts include pre-expansion applicability checks and
post-expansion type checks, so they are not required to equal Layer-21
generated-candidate counts. The detailed definitions and per-case outputs are
in [reports/layer22_constraint_guided_synthesis.md](reports/layer22_constraint_guided_synthesis.md)
and [reports/layer22_constraint_guided_synthesis.json](reports/layer22_constraint_guided_synthesis.json).

## Layer 23 rich mathematical semantics baseline

Layer 23 is measured separately with:

```bash
./build/opforge benchmark rich_semantics atlas
```

The verified result is `RICH_OPERATOR_SEMANTICS_DEMONSTRATED`. The migration
ledger reports 6 previously fully structured facts, 321 newly structured facts,
327 cumulative fully structured facts, 220 partial facts, and 0 unsupported
facts. The rich theory contains 47 spaces, 47 space-property facts, 274
operator-property facts, 0 explicit space relations, and 4 trusted rule
schemas. Real Atlas probes report 88 linear, 47 space, 0 indexed, and 10
inverse/commutation facts.

The target-blind suite has 14 cases and 2 opaque-ID positive reruns. Its
controls keep missing inclusion, dual-versus-adjoint, partial facts, numerical
closeness, and unknown regimes from becoming false positives. Scaling records
Layer-21 attempts / Layer-23 attempts `7/21`, `7/78`, and `7/171` for 3/6/9
operators; each retains 2 classes with peak retained frontier 2 and 0 unknown
type decisions. Leakage and opaque-ID checks pass; discovery numerics and
runtime LLM calls are 0; deterministic digest is
`layer23_benchmark_digest.6b6b46b6e7002750`.

Layer-23 metrics do not rewrite frozen Layer-20/21/22 baselines. Extension,
pullback/pushforward, arbitrary coefficient search, formal theorem proving,
and physics remain deferred.

## Required baseline commands

```bash
./build/opforge benchmark blind_rediscovery atlas
./build/opforge benchmark scaling atlas --medium-operators 50
./build/opforge benchmark open_search atlas
./build/opforge benchmark quotient_search atlas
./build/opforge benchmark goal_search atlas
./build/opforge benchmark proof_plan atlas
./build/opforge benchmark verification atlas
./build/opforge benchmark search_scalability atlas
```

For a baseline rerun, record:

- command and configuration;
- Atlas/theory snapshot hash;
- classification and leakage results;
- candidate IDs/digests;
- raw/generated/canonical/pruned counts;
- numerical experiment count;
- deterministic comparison result; and
- termination/completeness status.

## Regression policy

A new layer must not advance if it silently changes a demonstrated baseline.
Any change in the following requires an explanation and updated audit:

- executable equality count or semantic/equality boundary;
- blind classification, leakage, or negative controls;
- opaque-ID behavior;
- deterministic candidate IDs or counts;
- scaling-stage definitions;
- numerical discovery count; or
- honest partial/MISS/unsupported/not-run statuses.

An improvement is not automatically valid if it converts an honest failure into
a pass through target coupling or a new heuristic.

## Temporary integrity probe disposition

`/tmp/opforge-integrity-probe.cpp` was an audit-only helper used to print
case-by-case masking details, run three blind repetitions, and perform
dependency ablations. It is intentionally outside the repository and must not
be added to the project checkpoint.

Persistent checks already live in the blind benchmark/test boundary: leakage,
negative controls, deterministic exports, opaque candidate-ID digests, scaling
counts, and numerical isolation. The probe's target-aware disclosure and
ablation printing remain report-generation tooling rather than production
discovery logic.
