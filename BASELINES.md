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

## Required baseline commands

```bash
./build/opforge benchmark blind_rediscovery atlas
./build/opforge benchmark scaling atlas --medium-operators 50
./build/opforge benchmark open_search atlas
./build/opforge benchmark quotient_search atlas
./build/opforge benchmark goal_search atlas
./build/opforge benchmark proof_plan atlas
./build/opforge benchmark verification atlas
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
