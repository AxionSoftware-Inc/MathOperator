# OpForge Layer 18 — Proof Planning and Proof-Obligation DAGs

Status: implemented as a separate, deterministic, backend-neutral proof-planning
surface. Layer 18 does not implement Layer 19 verification backends, numerical
experiments, LLM reasoning, or formal-prover integration.

## 1. Scope and boundary

The production path is:

```text
Layer-17 GoalSearchResult
        |
        v
selected StructuralCandidate
        |
        v
Layer-18 ProofPlanner
        |
        v
ProofPlan = obligations + rule/evidence nodes + dependency DAG
```

Layer 17 remains a structural search. A plan marked
`COMPLETE_AT_REQUIRED_LEVEL` means that every recorded obligation is supported
at the requested evidence capability; it does not mean `PROVED`. In particular,
the real Layer-17 composition case is complete only at `STRUCTURAL` level.

The primary API is in:

- `include/opforge/proof/planning.hpp`
- `src/proof/planning.cpp`
- `tests/proof_planning_tests.cpp`

The CLI command `opforge benchmark proof_plan` exports the controlled plans and
their JSON summaries. The Layer-17 search implementation and its baseline
commands are not called with proof information and were not changed.

## 2. ProofPlan and ProofObligation model

`ProofPlan` records:

- stable plan identity;
- target `Judgment`;
- originating Layer-17 candidate ID, when present;
- `Context`, `ValidityRegime`, and `GoalSearchScope`;
- root obligation IDs;
- all unique obligations and their dependency IDs;
- obligation, rule-application, and evidence nodes;
- `DEPENDS_ON`, `DERIVED_BY`/`ALTERNATIVE`, and `SUPPORTS` edges;
- certificates as opaque backend-neutral envelopes;
- unresolved IDs, cycle paths, accounting, status, and reason.

Each materialized obligation, DAG node, and edge also carries explicit context
and regime IDs (and each obligation carries its requested evidence capability),
so an audit does not have to infer applicability only from a display label.

The Layer-15 `semantic::ProofObligation` was extended without changing its
identity rule. Its ID depends on the label and semantic target, not on lifecycle
status, reason, evidence, or progress. Dependency IDs and the generating origin
are retained as audit fields. Thus changing `OPEN` to a discharge state does not
silently create a new mathematical obligation.

Lifecycle values are distinct:

| Obligation state | Meaning |
|---|---|
| `OPEN` | no accepted discharge is available yet |
| `DISCHARGED_TRUSTED_FACT` | an exact semantic Theory fact passed trust, context, regime, and side-condition gates |
| `DISCHARGED_STRUCTURAL_DERIVATION` | Layer-17 structure, Layer-15 typing/regime evidence, or a proof-safe structural rule supports the obligation only at structural level |
| `DISCHARGED_SYMBOLIC_CERTIFICATE` | an accepted symbolic certificate envelope is attached |
| `DISCHARGED_FORMAL_CERTIFICATE` | an accepted formal certificate envelope is attached; Layer 18 does not create one |
| `NUMERICALLY_SUPPORTED` | support-only evidence; never treated as mathematical proof |
| `BLOCKED_UNKNOWN` | a required semantic decision is unknown |
| `UNSUPPORTED` | the required rule/evidence/provenance contract is unavailable |
| `FALSIFIED` | the target or a required premise is explicitly false |
| `CONTRADICTED` | context/regime or an explicit contradiction prevents the obligation |

`EvidenceLevel` is a compatibility relation, not an invented numeric ordering.
Formal requirements accept only formal evidence; numerical support has its own
support-only capability and cannot satisfy structural, symbolic, or formal
requirements.

## 3. Obligation generation

For a selected Layer-17 candidate, the planner consumes the actual candidate,
retained forward states, retained backward goal states, substitutions, lineage
IDs, rule IDs, context, regime, and unresolved constraints. It generates:

- a root target obligation;
- obligations for forward and backward lineage judgments;
- explicit rule premises and rule conditions;
- type-check obligations for each target operand and substitution term;
- regime compatibility obligations;
- side-condition obligations;
- equality rewrite-safety obligations;
- explicit obligations for candidate constraints and missing rule provenance.

Layer-17 `GoalRule` values are not silently promoted. The caller must pass an
explicit `ProofRule` contract. `proof_rule_from_goal_rule(..., false)` is the
default and produces an unsafe proof-rule record; a search rule without a
reviewed proof contract yields `UNSUPPORTED` if its lineage is needed for the
plan.

Layer-15 decisive results are recorded with provenance. Valid typing and known
regime compatibility become structural discharges; invalid typing becomes
`FALSIFIED`; unknown typing/regime remains `BLOCKED_UNKNOWN`.

## 4. AND/OR DAG semantics

Every premise of one rule application is an AND dependency. Multiple matching
proof-safe rules are retained as separate `ALTERNATIVE` rule nodes. Shared
semantic obligations are keyed by the target's semantic canonical form, which
includes semantic IDs, context, and regime; display text is not used as an
identity key.

The planner does not attempt global proof minimization. It only removes exact
duplicate obligation nodes and reports both generated and unique counts. A
cycle is detected from the active semantic-obligation path. All obligations in
the cycle remain unresolved/blocked and the run is `CYCLIC`.

## 5. Conservative discharge rules

A Theory fact can discharge only when:

1. the Judgment kind matches exactly;
2. expressions match by semantic IDs and typed matching;
3. context and regime are compatible;
4. required side conditions are satisfied;
5. provenance is present and evidence/status is trusted;
6. equality facts pass Layer-15 `rewrite_safety`.

Analogy, correspondence, approximation, generic relation text, display-name
matches, numerical evidence for a formal request, and facts without provenance
do not discharge exact obligations. Partially structured legacy relations remain
generic relations and cannot create proof edges. A Layer-17 candidate may
provide only `DISCHARGED_STRUCTURAL_DERIVATION`, never a formal discharge.

Certificates contain ID, obligation ID, backend/type, evidence capability,
deterministic payload, provenance, and status. Layer 18 stores and replays this
envelope; it does not verify external payloads or claim a mock certificate is a
mathematical proof.

## 6. Controlled benchmark results

All counts below are from `opforge benchmark proof_plan` and were checked by
the automated accounting invariant:

```text
generated = unique + duplicate
unique = discharged + open + unknown + falsified + contradicted + cyclic + unsupported + numeric
```

### Positive/structural plans

| Case | Evidence request | Generated / unique / duplicate | Discharged | Open | Unknown | Falsified | Contradicted | Cyclic | Unsupported | Numeric | Final status |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `trusted-fact` | TRUSTED_FACT | 3 / 3 / 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | `COMPLETE_AT_REQUIRED_LEVEL` |
| `open-premise` | STRUCTURAL | 6 / 5 / 1 | 4 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | `INCOMPLETE_OPEN_OBLIGATIONS` |
| `unknown-regime` | STRUCTURAL | 3 / 3 / 0 | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | `BLOCKED_UNKNOWN` |
| `falsified-target` | STRUCTURAL | 1 / 1 / 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | `FALSIFIED` |
| `contradicted-regime` | STRUCTURAL | 3 / 3 / 0 | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | `CONTRADICTED` |
| `shared-dag` | STRUCTURAL | 7 / 5 / 2 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | `COMPLETE_AT_REQUIRED_LEVEL` |
| `cyclic` | STRUCTURAL | 7 / 5 / 2 | 3 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | `CYCLIC` |
| `layer17-composition` | STRUCTURAL | 13 / 7 / 6 | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | `COMPLETE_AT_REQUIRED_LEVEL` |

The `layer17-composition` row is a real Layer-17 solution: its candidate ID
is `solution_candidate.32f93f1936ecfeee`; the plan contains explicit
composition-rule premises, typing/regime obligations, six duplicate attempts,
and seven unique structural obligations. This is a structural plan, not a
formal theorem.

The three quotient-distinct `positive.multiple` solutions are planned
separately, with candidate IDs:

- `solution_candidate.783de0ba44f5a336` — 7 / 5 / 2, all 5 discharged structurally;
- `solution_candidate.a53343aee79fba57` — 7 / 5 / 2, all 5 discharged structurally;
- `solution_candidate.b27371fcdb6a02b6` — 7 / 5 / 2, all 5 discharged structurally.

Their plan IDs differ, so fundamentally different Layer-17 structural
solutions are not merged by proof planning.

### Indexed-family plan

`indexed` requests `SYMBOLIC` evidence and finishes with 4 unique obligations:
the target is a `Nilpotence` Judgment whose expression contains
`d_(k+1) ∘ d_k` and a typed zero, plus two explicit Layer-15 type obligations
and a regime obligation. The target is discharged by a trusted structured
symbolic fixture fact; the plan preserves the `k` and `k+1` index terms and the
indexed operator family ID `op.d`. No unindexed `d_i` collapse occurs.

Result: 4 / 4 / 0, discharged 4, status
`COMPLETE_AT_REQUIRED_LEVEL` at `SYMBOLIC` evidence. This is not a claim that
Layer 18 ran a symbolic backend; the fixture represents an already available
structured evidence record.

### Negative controls

| Control | Generated / unique / duplicate | Result | Interpretation |
|---|---:|---|---|
| `analogy-not-equality` | 3 / 3 / 0 | `INCOMPLETE_OPEN_OBLIGATIONS` | analogy fact cannot discharge an exact target |
| `unknown-side-condition` | 4 / 4 / 0 | `BLOCKED_UNKNOWN` | unknown geometry side condition remains visible |
| `display-name-only` | 3 / 3 / 0 | `INCOMPLETE_OPEN_OBLIGATIONS` | same display name with different operator ID does not match |
| `numeric-vs-formal` | 3 / 3 / 0 | `INCOMPLETE_OPEN_OBLIGATIONS` | numeric support cannot satisfy FORMAL |
| `missing-provenance` | 3 / 3 / 0 | `INCOMPLETE_OPEN_OBLIGATIONS` | a trusted-looking fact without provenance is not trusted |
| `numeric-support-only` | 3 / 3 / 0 | `COMPLETE_AT_REQUIRED_LEVEL` at NUMERICAL_SUPPORT_ONLY | support-only lifecycle state is represented; reason explicitly says “not a proof” |

No numerical experiment is run by these controls. The last row is an evidence-
model fixture, not numerical discovery.

## 7. Replay and invalidation

`ProofPlanner::replay` retains plan topology and identity while recomputing
evidence-backed states. The permanent regression test creates a complete
trusted-fact plan, removes the fact, and replays it. The plan ID remains
identical, while the dependent target reopens and the final status becomes
`INCOMPLETE_OPEN_OBLIGATIONS`. There is no magical persistence of a prior
discharge.

Identical candidate/theory/context/rules/certificate inputs produce identical
plan IDs, obligation IDs, DAG topology, statuses, accounting, and benchmark
digest. The Layer-18 suite repeats the benchmark report and direct DAG plans to
check this determinism.

## 8. Limitations and deferred work

- Layer 18 does not verify certificates or create formal certificates.
- There is no Lean, Coq, Isabelle, Mathematica, SymPy, Z3, or other backend.
- There is no LLM runtime, numerical discovery, or numerical experiment.
- Structural rule contracts are explicit but are not formal proof terms.
- The current Layer-17 lineage is operator/goal oriented; unsupported lineage
  details are surfaced as explicit unsupported obligations rather than invented.
- No proof-ranking or proof minimization is performed.

These are intentional Layer-19 and later boundaries. The honest output of this
layer is an auditable list of requirements and evidence, not a theorem claim.

## 9. Verification record

Final gates for this implementation:

- default Debug build and CTest: 5/5 passed;
- clean Release configure/build and CTest: 5/5 passed;
- clean ASan/UBSan configure/build and CTest: 5/5 passed;
- Layer-15 semantic, Layer-16 quotient, Layer-17 goal, and Layer-18 proof tests:
  all passed;
- blind baseline: structural 1, partial 4, miss 1, false positives 0,
  leakage 0, numerical experiments 0;
- scaling baseline: 12 -> 25, 50 -> 150, 98 -> 391 legacy raw candidates;
- open search: generated 62, pruned 650, serious 0, numerical experiments 0;
- Layer-17 JSON and Layer-18 JSON exports: three identical runs by candidate
  IDs, classifications, counts, plan IDs, statuses, and digests;
- `git diff --check`: passed.

Runtime milliseconds are intentionally excluded from the deterministic JSON
comparison; human-readable text includes wall-clock timing and therefore is
not byte-identical across runs.

The final state and prior-layer gate details are also maintained in
`CURRENT_STATE.md`.
