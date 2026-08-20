# OpForge Layer 19 — Verification, Falsification, Certificates and Scientific Hygiene

Status: implemented as a post-search, deterministic verification and evidence
layer. Layer 19 consumes Layer-18 `ProofPlan` objects. It does not change
candidate generation, candidate ranking, open discovery, forward/backward
frontiers, quotient equivalence, or Layer-15–18 search behavior.

Layer 20 practical-utility benchmarking is not implemented.

## Architecture

The production path is:

```text
ProofPlan
  -> explicit VerificationRequest
  -> declared capability/trust backend
  -> VerificationCertificate
  -> certificate replay/invalidation
  -> evidence firewall
  -> updated ProofPlan + ResultBundle
```

No verifier result is sent back to discovery or reasoning. A verification
request contains the structured obligation ID, target `Judgment`, `Context`,
`ValidityRegime`, substitutions, required evidence, Theory ID/version,
backend/capability, and deterministic configuration. A human-readable target
string alone is never a request.

The public implementation is:

- `include/opforge/verification/layer19.hpp`;
- `src/verification/layer19.cpp`;
- `tests/verification_tests.cpp`; and
- `opforge benchmark verification atlas`.

## Capability and trust model

The internal declarations are explicit:

| Backend | Trust class | Capabilities | Scope |
|---|---|---|---|
| `internal.exact.v1` | `INTERNAL_EXACT_REPLAY` | `EXACT_STRUCTURAL_CHECK`, `EXACT_SYMBOLIC_CHECK`, `CONSTRAINT_CHECK`, `COUNTEREXAMPLE_SEARCH` | Narrow exact Layer-15 semantics and bounded trusted rewrite replay |
| `layer19.numeric.v1` | `NUMERICAL` | `NUMERICAL_SPECIAL_CASE_CHECK`, `NUMERICAL_STRESS_TEST`, `COUNTEREXAMPLE_SEARCH` | Deterministic post-search numerical support and suspicious-counterexample candidates |

No production backend declares `FORMAL_PROOF` or `FORMAL_REFUTATION`.
Consequently:

```text
FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED
```

Capability mismatch is an `INVALID_REQUEST`; a backend name cannot grant a
capability it did not declare. Trust class is stored in every certificate and
is not inferred from the backend name.

## Internal exact verifier

The internal verifier is deliberately not a general-purpose CAS. It can:

- check structured `Definedness` through the Layer-15 type checker;
- check domain/codomain and composition compatibility that Layer 15 can decide;
- check exact literal integer/rational equality and produce an exact
  counterexample for unequal literals; and
- replay a bounded, deterministic expression rewrite path using only trusted
  Layer-15 `RewriteRule` objects and equality facts accepted by
  `rewrite_safety`.

Every rewrite step records source expression, rule ID, substitution, context,
validity regime, and resulting expression. Root and nested expression rewrites
are searched with a declared step bound and deterministic ordering. There is
no textual equivalence, hidden simplifier, analogy conversion, approximation
conversion, or generic relation promotion.

It cannot establish arbitrary quantified theorems, general algebraic
normalization, calculus identities absent from the Theory, approximation,
correspondence, analogy, or partially structured Atlas relations. Those return
`UNSUPPORTED` or `INCONCLUSIVE`, never `false` merely because the narrow
verifier lacks a method.

## Certificate and replay contract

`VerificationCertificate` records:

- certificate and obligation IDs;
- backend/version, capability, and trust class;
- Theory ID/version;
- context/regime digests;
- deterministic input digest;
- result and evidence level;
- replay payload/data and creation metadata;
- exact transformation steps; and
- exact or numerical-counterexample records.

Accepted evidence is attached to the Layer-18 plan as a backend-neutral
certificate envelope. Multiple certificates are retained; a later certificate
does not overwrite earlier structural or numerical evidence.

Replay recomputes the certificate against the current Theory, context, regime,
and deterministic request. It checks the result, payload, replay data, Theory
identity/version, obligation ID, and input digest. Removing or mutating a
trusted rewrite fact invalidates the certificate. A stored `verified` flag is
never trusted without replay.

## Epistemic firewall

The central evidence mapping is conservative:

| Verification result | Allowed meaning |
|---|---|
| `VERIFIED_AT_DECLARED_LEVEL` | Only the declared exact capability/evidence level |
| `REFUTED` | Exact semantic/type/regime refutation in the represented fragment |
| `COUNTEREXAMPLE_FOUND` + `EXACT` | Exact counterexample; can falsify the represented claim |
| `COUNTEREXAMPLE_FOUND` + `NUMERICAL_SUSPICIOUS` | Numerical candidate only; does not falsify the mathematical claim |
| `SUPPORTED_NOT_PROVEN` | `NUMERICALLY_SUPPORTED`; never symbolic/formal proof |
| `INCONCLUSIVE` | Unknown or bounded procedure failure to decide |
| `UNSUPPORTED` | No declared method for the fragment |
| `INVALID_REQUEST` / `BACKEND_FAILURE` | Request/backend failure, not theorem failure |

Numerical evidence can satisfy only the explicitly requested
`NUMERICAL_SUPPORT_ONLY` level. It cannot discharge `SYMBOLIC` or `FORMAL`.
The formal benchmark intentionally remains incomplete when it has structural,
exact internal, and numerical evidence but no genuine formal certificate.

## Numerical verification policy

Numerical computation is reachable only through an explicit Layer-19
`VerificationRequest` using `layer19.numeric.v1`. Its configuration records
operator, resolution, seed, precision, tolerance, domain, discretization, and
boundary policy. The existing numerical executor is not called by Layers
15–18.

Tolerance-based agreement is support only. A numerical discrepancy is labeled
`NUMERICAL_SUSPICIOUS`; it is not an exact counterexample. The only exact
counterexample benchmark uses exact literal semantics (`1 != 2`).

## ResultBundle

`ResultBundle` is a deterministic, backend-neutral artifact containing:

- original problem/source description;
- target `Judgment`;
- structural candidate ID and Layer-17 `SearchScope`;
- Layer-18 `ProofPlan`;
- all certificates and counterexamples;
- numerical evidence;
- unresolved obligations;
- epistemic status;
- conservative `NoveltyStatus` (default `NOT_CHECKED`); and
- Theory version and reproducibility metadata.

`POSSIBLY_NOVEL` is only a state representation. The implementation performs
no broad web/literature novelty search and never emits `NEW MATHEMATICS`.
ResultBundle deterministic identity excludes wall-clock runtime.

## Controlled benchmark results

The command `opforge benchmark verification atlas` currently reports:

| Case | Result | Plan interpretation |
|---|---|---|
| exact-trusted-rewrite | exact certificate with replay steps | complete at required symbolic level |
| exact-typing-definedness | exact structural certificate | complete at required structural level |
| unknown-definedness | `INCONCLUSIVE` | unknown type is not promoted or rejected |
| unsupported-exact-claim | `UNSUPPORTED` | not false; unsupported fragment |
| numeric-support-formal-open | exact internal + numeric certificates | `NUMERICALLY_SUPPORTED`, formal requirement remains open |
| exact-counterexample | `COUNTEREXAMPLE_FOUND`, `EXACT` | plan `FALSIFIED` |
| numeric-suspicious-counterexample | `COUNTEREXAMPLE_FOUND`, `NUMERICAL_SUSPICIOUS` | plan remains incomplete, not falsified |
| real-layer17-layer18-layer19 | actual Layer-17 composition through Layer 18 | 4 required-level exact discharges, 3 unsupported obligations remain |
| open-discovery-structural-fixture | controlled fixture only | not claimed as an open-discovery theorem |

The real pipeline uses the actual Layer-17 composition candidate and its
Layer-18 plan. The resulting bundle is `PROOF_PLAN_GENERATED`, not `PROVED`:
some obligations are exact internally discharged, while missing semantic
condition representations remain unsupported.

The discovery-mode case is explicitly a fixture because the frozen open search
currently reports zero serious candidates. It demonstrates consumption of the
same proof/verification machinery without claiming that open discovery found a
new theorem.

## Numerics firewall

The benchmark runs a numerical Layer-19 request and compares deterministic
snapshots before/after for:

- Layer-17 candidate IDs/order and exported result;
- Layer-16 finite quotient classes; and
- the bounded open-discovery report.

The result is `Numerics firewall: PASS`. Discovery numerical experiments remain
zero (`discovery_numerical_experiments=0`); Layer-19 verification numerical runs
are counted separately (`verification_numerical_experiments >= 2` in the
controlled suite).

## Accounting and performance

Each `VerificationReport` records obligations processed, verifier calls, exact
checks, numerical runs, certificates, replay attempts/failures, discharged,
open, unknown, unsupported, refuted, contradicted, cyclic, and numerical
support counts. The certificate count is bounded by verifier calls and exact
plus numerical calls equal total calls.

Verification runtime is measured in the report's `runtime_ms` field and is not
added to Layer-16/17/18 search runtime or candidate metrics. Runtime is excluded
from deterministic report and ResultBundle IDs.

## Negative controls

The automated suite covers:

- analogy/approximation cannot produce exact certificates;
- incompatible verifier capability is rejected;
- no formal backend can be selected accidentally;
- numerical support cannot discharge formal evidence;
- numerical near-zero/discrepancy is not exact refutation;
- exact literal disagreement is distinguished from numerical suspicion;
- removal of a trusted rewrite fact invalidates replay; and
- Layer-19 numerical execution does not alter Layer-16/17/open-discovery
  snapshots.

## Limitations and deferred work

- No Lean, Coq, Mathematica, SymPy, Z3, or other formal prover is integrated.
- The internal exact verifier is a narrow structured replay engine, not a full
  symbolic mathematics system.
- Numerical support remains model-, discretization-, and tolerance-dependent.
- No broad novelty or literature search is implemented.
- Independence profiles and hold-out utility evaluation remain future work.
- Layer 20 practical-utility benchmarking is explicitly deferred.

The scientifically correct output may remain incomplete or unsupported. Layer
19 does not weaken obligations to improve completion rates.

## Final verification gates

- default Debug build and CTest: **6/6 passed**;
- clean Release configure/build and CTest: **6/6 passed**;
- clean ASan/UBSan configure/build and CTest: **6/6 passed**;
- exact replay, invalidation, capability mismatch, formal-open, numerical
  safety, counterexample, ResultBundle, and firewall tests: passed;
- frozen blind baseline: structural `1/6`, partial `4/6`, miss `1/6`, false
  positives `0`, negative controls `3/3`, leakage `0`, numerical discovery `0`;
- frozen scaling baseline: `12 -> 25`, `50 -> 150`, `98 -> 391`;
- frozen open baseline: generated `62`, pruned `650`, serious `0`, numerical
  discovery `0`;
- quotient finite status distinction and million-scale unknown accounting:
  unchanged;
- Layer-17 and Layer-18 benchmark suites: passed with prior statuses/counts;
- Layer-19 JSON semantic IDs, classifications, counts, statuses and digests:
  identical across three runs; and
- `git diff --check`: passed.

Wall-clock runtime is excluded from scientific identity comparisons.
