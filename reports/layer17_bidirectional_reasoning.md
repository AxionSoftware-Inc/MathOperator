# OpForge Layer 17 — Goal-Directed / Bidirectional Reasoning

Status: implemented as a separate deterministic `opforge::reasoning` path.
Layer 18 proof planning was not implemented.

The implementation is deliberately bounded and operator-centric. A structural
solution candidate is not a theorem and is never promoted to formal proof.

## Implementation boundary

Layer 17 consumes the Layer-15 semantic objects directly:

```text
Theory Σ + Context Γ + ValidityRegime R + target Judgment φ
        + GoalSearchScope + SearchPolicy
        -> forward quotient states
        -> backward GoalState AND/OR branches
        -> typed frontier meetings
        -> SolutionCandidate
```

`Problem` is only a container for the theory snapshot, context, semantic target,
scope, policy, safe rules, and optional explicit forward seeds. It does not add a
second mathematical expression or proposition language. The CLI benchmark
command is:

```bash
./build/opforge benchmark goal_search atlas
```

The benchmark fixture knows expected categories only outside
`GoalSearchEngine::run`. The engine receives no expected operator, expected
composition, benchmark answer, scorer callback, or target-specific branch.

## Backward rule semantics

`GoalRule` records an ID, direction, soundness class, pattern context,
conclusion, premises, conditions, regime, and provenance. Only rules marked
`BACKWARD` or `BOTH`, with non-heuristic soundness and explicit premises, may
decompose a goal. A forward implication is therefore not reversed implicitly.

The search uses OR over alternative safe rules and AND over the generated
subgoals of one rule. Every child inherits the parent context and the rule's
constraint/provenance information. Conditions are three-valued:
`SATISFIED`, `VIOLATED`, or `UNKNOWN`. An unknown condition produces an
unresolved branch; it cannot close the goal.

Goal states retain the semantic target, context/regime, parent, rule used,
generated children, constraints, provenance, and lifecycle status:
`OPEN`, `SATISFIED`, `DECOMPOSED`, `BLOCKED_UNKNOWN`, `UNSUPPORTED`,
`CONTRADICTED`, and `BUDGET_ENDED`.

## Typed matching and frontier meetings

The matcher is conservative first-order/dependent-lite matching. It checks:

- semantic expression kind and operator ID;
- declared type/domain/codomain;
- context and validity-regime compatibility;
- indexed terms and offsets;
- parameters and expression shape; and
- declared context variables.

It returns `MATCH`, `NO_MATCH`, or `UNKNOWN`. A context variable binds only to a
type-compatible expression; unknown variable or candidate types remain
`UNKNOWN`. There is no unrestricted higher-order unification and no
base-name-only matching. `d_k` and `d_(k+1)` remain distinct.

A meeting records the goal ID, forward-state ID, match/substitution, context,
regime, and reason. `SolutionCandidate` retains forward and backward lineages,
substitutions, regime, scope, unresolved conditions, and structural epistemic
status.

Equality/Equivalence facts are admitted to the forward fact frontier only when
Layer-15 safety, typing, regime, side conditions, and trusted evidence permit
it. Approximation, analogy, correspondence, and generic relations are never
converted into exact equality or equivalence.

## Controlled positive cases

Counts below are from one clean CLI run. Runtime is observational and may vary;
IDs, classifications, and count fields are deterministic. “Forward states” is
the number of eligible Theory facts plus retained Layer-16 quotient classes.
“Constructions considered” is the raw construction input to the quotient at the
final forward depth. Thus invalid constructions are not forward states.

| Case | Forward states | Constructions | Retained classes | Type invalid/unknown | Quotient merges (exact/canonical/proven/symmetry/consequence) | Backward states | Meetings attempted/successful/rejected | Decompositions | Result |
|---|---:|---:|---:|---:|---|---:|---|---:|---|
| `positive.composition` | 4 | 6 | 4 | 2/0 | 0/0/0/0/0 | 3 | 6/2/4 | 1 | `SOLVED_STRUCTURALLY`, relative complete |
| `positive.identity` | 2 | 2 | 1 | 0/0 | 0/0/1/0/0 | 1 | 2/1/1 | 0 | `SOLVED_STRUCTURALLY`, relative complete |
| `positive.indexed` | 3 | 6 | 3 | 3/0 | 0/0/0/0/0 | 1 | 11/1/10 | 0 | `SOLVED_STRUCTURALLY`, relative complete |
| `positive.multistep` | 6 | 12 | 6 | 6/0 | 0/0/0/0/0 | 5 | 15/3/12 | 2 | `SOLVED_STRUCTURALLY`, relative complete |
| `positive.multiple` | 3 | 3 | 3 | 0/0 | 0/0/0/0/0 | 1 | 3/3/0 | 0 | `MULTIPLE_STRUCTURAL_SOLUTIONS`, relative complete |

The identity case demonstrates a trusted Layer-15 equality entering the
quotient as one proven-equivalent merge. The multistep case uses explicit safe
composition rules; it does not infer a reverse implication. The multiple case
retains all three quotient-distinct structural candidates.

The indexed regression also supplies an explicit safe rule with premises
`d_(k+1)` and `d_k`; the backward decomposition produces those exact indexed
subgoals and solves without collapsing their indices. The table's indexed
benchmark count uses its direct forward construction path, while the separate
adversarial test verifies the backward path.

## Negative controls

| Case | Control | Result | Why |
|---|---|---|---|
| `negative.impossible-type` | composition has incompatible domain/codomain | `INVALID_PROBLEM` | target term fails semantic type checking |
| `negative.incompatible-regime` | target is curved while context is Euclidean | `INVALID_PROBLEM` | target regime is incompatible with the problem context |
| `negative.missing-prerequisite` | only `op.A` is an explicit seed for target `op.B ∘ op.A` | `NO_SOLUTION_IN_RELATIVE_SPACE`, relative complete | safe backward decomposition exposes missing `op.B`; no automatic atom completion is used when explicit seeds are supplied |
| `negative.under-specified` | target literal has unknown type | `UNDER_SPECIFIED` | unknown typing is preserved before search |
| `negative.near-match` | Atlas fact is `Approximation(A,B)`, target is exact `Equality(A,B)` | `NO_SOLUTION_IN_RELATIVE_SPACE`, relative complete | judgment kinds do not unify and approximation is not an equality rewrite |

Additional tests cover analogy/correspondence isolation through the same
non-equality path, forward-only rules not being reversed, incompatible rule
regimes, and unknown rule side conditions. The unknown-side-condition test
ends `INCOMPLETE_UNKNOWN` with no solution.

## Finite exhaustive versus budgeted search

The finite fixture is an independently enumerable zero-depth atom grammar over
three known operators `{op.A, op.B, op.C}`. It has no candidate budget, no
lossy frontier/resource limit, exhaustive policy, and a known independent
solution set of three operator classes.

| Run | Raw constructions considered | Retained classes | Solutions | Budget pruned | Quotient termination | Goal result |
|---|---:|---:|---:|---:|---|---|
| finite exhaustive | 3 | 3 | 3 | 0 | `EXHAUSTED_RELATIVE_SPACE` | `MULTIPLE_STRUCTURAL_SOLUTIONS`, `relative_complete=yes` |
| same grammar, budget 1 | 1 | 1 | 1 | 2 | `BUDGET_ENDED` | `BUDGET_ENDED`, `relative_complete=no` |

The budgeted run is not labelled relatively exhaustive even though it happens
to find one solution before the budget stops the run. Every budget-pruned
construction is separately counted in the goal ledger.

The finite expected solution-set digest is generated by the external benchmark
fixture from `{op.A,op.B,op.C}`. The search engine never receives that digest or
the expected set.

## Performance / explosion comparison

This is one controlled target-oriented comparison, not a universal complexity
claim. It uses the same typed theory and target, with six operators and the
composition grammar.

| Path | Forward states | Constructions considered | Retained classes | Backward states | Meetings | Peak forward / backward |
|---|---:|---:|---:|---:|---:|---:|
| forward-only, no backward rules | 21 | 42 | 21 | 0 | 45 | 21 / 0 |
| bidirectional | 6 | 6 | 6 | 3 | 18 | 6 / 2 |

The forward-only path explores all raw depth-1 compositions in the bounded
scope. The bidirectional path uses the target's typed composition structure and
safe rules. Runtime is exported for both paths in the CLI; the structural
counts above are the comparison, not a claim that runtime improvement is
universal.

## Ledger and termination accounting

The goal ledger records forward facts, raw quotient invalid/unknown outcomes,
quotient merges, meeting attempts, successful/rejected meetings, decompositions,
budget pruning, unresolved goals, unsupported rules, and no-match outcomes.

For non-unknown positive cases:

```text
frontier meetings attempted
  = successful meetings + rejected meetings
```

Unknown matcher/constraint events are additional explicit ledger entries and
are not counted as rejected meetings. Quotient raw-construction accounting is
delegated to the Layer-16 ledger; Layer 17 imports its type-invalid,
type-unknown, merge, lossy, unresolved, retained-class, peak-frontier, and
termination fields without relabelling them.

The Layer-16 finite and million-scale accounting tests remain enabled. In
particular, the existing million-scale result remains `1,000,000 raw`,
`999,995 lossless reductions`, `5 retained classes`,
`INCOMPLETE_UNKNOWN`. Its type accounting is `997,000 valid / 1,000 invalid /
2,000 type-unknown`; the ledger has exactly `2 UNKNOWN` unresolved terminal
representatives, because the other `1,998` unknown-typed constructions are
canonical duplicates of those classes. The two UNKNOWN representatives are
neither rejected nor treated as proven equivalent, which is why
`INCOMPLETE_UNKNOWN` is retained.

## Partial Layer-15 fact isolation

Layer 17 does not add any quotient rule for the 180 partially structured
Layer-15 facts. Its forward fact gate admits only typed, regime-compatible,
trusted Equality/Equivalence facts for exact semantic use; relation kinds remain
relations. The Layer-16 regression that removes all 180 partial facts and
compares quotient metrics remains part of `opforge_quotient_tests` and passes.

## Determinism and target-blindness checks

`opforge_bidirectional_tests` runs the controlled suite repeatedly and compares
result canonical forms and solution IDs. The search orders map/vector-derived
objects deterministically and sorts OR branches by semantic IDs. Runtime is not
part of the canonical result identity.

The scorer/fixture layer owns expected case labels and the finite expected set.
The engine API accepts only semantic problem inputs. No target-specific
benchmark ID, expected operator, expected composition, family label, or scorer
callback is present in `GoalSearchEngine::run`.

## Deferred work and limitations

- This is bounded structural reasoning, not general theorem proving.
- Indexed matching is conservative; only the supported dependent-lite index
  forms are handled.
- The current benchmark composition rule generator is explicit and typed; it
  does not reverse arbitrary implications or infer missing mathematics.
- No numerical experiment, LLM, proof backend, unrestricted linear-combination
  enumeration, or Layer-18 proof-obligation planner is in the runtime path.
- The legacy discovery path and its frozen scientific baseline remain
  unchanged and parallel to this goal-directed path.

## Verification performed

The Layer-17 test target covers positive/negative goals, finite exhaustion versus
budget, deterministic replay, typed/indexed matching, UNKNOWN metavariables,
backward-rule soundness, constraint propagation, approximation/analogy/
correspondence isolation, and performance direction.

Final gates completed:

- repository Debug build: passed;
- repository CTest: 4/4 passed;
- clean Release configure/build/CTest: 4/4 passed;
- clean ASan/UBSan configure/build/CTest: 4/4 passed;
- post-change clean and ASan/UBSan Layer-17 test target: passed;
- frozen blind/scaling/open/quotient baselines: passed with unchanged values;
- three identical Layer-17 goal benchmark JSON reruns: identical;
- three identical blind benchmark JSON reruns: identical; and
- git diff --check: passed.

The clean builds retain pre-existing aggregate-initializer warnings in older
Atlas/discovery/axiomatic files; the Layer-17 source itself introduces no new
compiler warning.
