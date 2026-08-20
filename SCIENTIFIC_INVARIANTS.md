# OpForge scientific invariants

These are non-negotiable repository invariants. A feature that violates one of
them is not an improvement until the violation is explicitly resolved and the
baseline is re-audited.

## 1. Epistemic invariants

The engine must distinguish these states:

1. `OBSERVATION` — a pattern or measurement was seen in supplied data;
2. `CANDIDATE` — a structurally valid construction was generated;
3. `CONJECTURE` — a closed proposition with explicit variables and validity
   regime was formed;
4. `STRUCTURAL_DERIVATION` — a rule-based derivation exists, but is not a
   symbolic/formal proof;
5. `NUMERICAL_SUPPORT` — a fixed proposition passed a declared numerical test
   suite;
6. `SYMBOLIC_VERIFIED` — a replayable symbolic derivation discharged the
   configured obligations;
7. `FORMAL_VERIFIED` — a trusted formal checker accepted a certificate;
8. `FALSIFIED` — a valid counterexample or failed trusted proof obligation
   refuted the declared proposition;
9. `UNRESOLVED` — evidence is insufficient or a required decision is unknown.

The system must never silently upgrade one state into a stronger state. In
particular:

- a structural pattern is not a conjecture or theorem;
- numerical support is not symbolic or formal verification;
- absence from the Atlas is not novelty;
- a passing test is not proof; and
- an unresolved obligation is not discharged by a confidence score.

## 2. Discovery invariants

- Numerical computation does not generate, rank, select, or promote open-
  discovery candidates.
- Unrestricted arbitrary linear combinations are disabled in open discovery.
  Goal-directed linear combinations require a typed goal and explicit
  justification.
- Benchmark targets, expected operators/compositions, family names, benchmark
  IDs, and scorer state are not visible to blind discovery.
- A deterministic rerun proves reproducibility only. It does not count as an
  independent mathematical derivation.
- Discovery uses a frozen theory/Atlas snapshot. It may not mutate the Atlas
  during a search run.
- A MISS is an acceptable scientific result and must not be converted into a
  pass by a target-specific heuristic.

## 3. Semantic invariants

- Heterogeneous relation kinds do not collapse into equality.
- Analogy, correspondence, implication, approximation, metadata similarity,
  and endpoint-family similarity do not become equality rewrites automatically.
- Equality closure accepts only correctly typed equality judgments with
  compatible context/regime and permitted provenance.
- Contradiction requires compatible types and proven overlapping contexts/
  validity regimes, plus an actual incompatible proposition or trusted
  inequality. Different conclusions alone are not a contradiction.
- `unsupported != false`.
- `not_run != failed`.
- `unresolved != rejected`.
- A missing proof obligation remains visible in every derived result.
- Assumptions may be strengthened in a child context, but they may not be
  silently dropped or weakened without an entailment/conversion proof.

## 4. Search invariants

Search reports must distinguish:

```text
EXHAUSTED_RELATIVE_SPACE
BUDGET_ENDED
TRUNCATED_BY_POLICY
INCOMPLETE_UNKNOWN
UNSUPPORTED_FRAGMENT
FAILED
```

Completeness may be claimed only relative to an explicitly recorded contract:

- grammar `G`;
- search depth and cost bounds `D`;
- resources `R`;
- equivalence theory `E`;
- Theory/Atlas version `Σ`;
- context and validity regime `Γ,R`; and
- allowed rule set `T`.

Lossless reductions and heuristic reductions must be separately counted and
provenance-recorded. A heuristic top-N frontier cannot be reported as an
exhausted search space.

## 5. Provenance and independence invariants

- Every result records source facts, rules, theory version, representation
  version, checker/backend version, and relevant seeds.
- Candidate IDs and canonical forms are reproducible under a frozen contract.
- Provenance overlap is measured before using the word “independent.”
- Different paths through the same premises and rule family are not independent
  evidence.
- Repeated deterministic benchmark outputs are labeled deterministic, not
  independently rediscovered.

## 6. Numerical invariants

Numerics may be used only after the proposition, regime, test generator,
executor version, tolerance, precision, and boundary policy are fixed.

Permitted labels include:

- `NUMERICALLY_FALSIFIED_ON_CASE`;
- `NOT_FALSIFIED_ON_TEST_SUITE`;
- `NUMERICALLY_CONSISTENT_ON_TEST_SUITE`;
- `SPECIAL_CASE_NUMERIC_SUPPORT`; and
- `VALIDATED_NUMERIC_RESULT` for a bounded validated numerical argument.

None of these is silently promoted to a general formal theorem.

## 7. Layer-gate invariant

A layer cannot advance if it silently breaks a demonstrated older capability.
Every gate must preserve:

- target-blind benchmark behavior;
- negative-control behavior;
- semantic equality boundary;
- unsupported/not-run status semantics;
- deterministic counts and candidate identifiers under the frozen contract;
- no numerical discovery; and
- no unrestricted open-search linear combinations.
