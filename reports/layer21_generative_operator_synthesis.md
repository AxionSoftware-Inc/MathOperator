# Layer 21 — Generative Operator Synthesis v1 integrity and utility report

Evaluation date: 2026-08-20. The report is generated from the deterministic
Layer-21 harness over the frozen vector-calculus seed and controlled synthetic
Theories. Layer 20 remains historical and is not rewritten.

## Verdict

`LIMITED_GENERATIVE_SYNTHESIS_DEMONSTRATED`

The framework genuinely constructs expressions from typed primitives using
constructor schemas beyond plain composition. The result is deliberately
limited: all new-family proof plans remain `OPEN`, the formal backend is not
implemented, and the open-discovery policy enables only composition and
indexed instantiation. This is construction-level utility, not theorem
proving or a claim of global mathematical discovery.

## What was implemented

The first-class `ConstructorSchema` API records a stable ID, family, arity,
input requirements, output derivation, context/regime requirements,
parameter constraints, side conditions, cost/depth, provenance, open versus
goal-directed availability, and the policy for UNKNOWN prerequisites.

The eight catalog entries in this run were:

| Schema | Open discovery | Goal-directed | UNKNOWN candidates |
|---|---:|---:|---:|
| typed composition | yes | yes | no |
| adjoint candidate | no | yes | yes |
| left inverse candidate | no | yes | yes |
| right inverse candidate | no | yes | yes |
| two-sided inverse candidate | no | yes | yes |
| explicit commutator | no | yes | yes |
| conjugation candidate | no | yes | yes |
| indexed family instantiation | yes | yes | no |

Generated expressions retain deterministic IDs, schema IDs, child IDs,
indices, context/regime provenance, obligations, depth, and cost. They remain
`GENERATED_EXPRESSION` objects and are not inserted into Atlas as primitives.
Every retained construction is passed into the Layer-16 quotient engine with
an explicit Layer-21 grammar scope. Goal-directed output-type matching is
performed before child expansion for composition, adjoint, inverse,
commutator, and conjugation.

Anti-commutator was deferred because the current semantic core does not have
a general, sound coefficient/addition proof contract. Restriction/extension
was deferred because structured subspace/inclusion semantics are insufficient.
Tensor product is an intentional unsupported control. No unrestricted linear
combination enumeration was enabled.

## Target-blind case disclosure

The external scorer holds `hidden_target`, `expected_expression`, and expected
outcome. The synthesizer receives only the Theory, Context, target Judgment/type,
and the exported grammar policy. The table gives the exact text classification
line emitted by the run. The full canonical candidate strings are emitted by
the CLI JSON export; the checked-in JSON is the compact deterministic summary.

### 1. Composition holdout

- Benchmark: `layer21.composition.holdout`
- Hidden target: `op.C`
- Removed: `operator.id=op.C`, `operator.name=op.C`,
  `operator.provenance=layer21-controlled-fixture`; the target node is absent
  from the Theory.
- Visible prerequisites: `op.A: Scalar -> Vector`,
  `op.B: Vector -> Scalar`, typed composition, target type `Scalar -> Scalar`.
- Exact search output:

  ```text
  layer21.composition.holdout ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_VALID_EXPRESSION
    candidates=1 raw=1 valid=1 invalid=0 unknown=0 quotient_merges=0 retained=3 obligations=0 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `Compose(op.B, op.A)`; it is a generated composition,
  not the removed Atlas node.
- Classification: structural recovery; the scorer matched the generated
  expression and the search exhausted the stated depth/cost grammar.
- Verdict: `VALID BLIND BENCHMARK`.

### 2. Opaque composition holdout

- Benchmark: `layer21.composition.opaque`
- Hidden target: `op_999`
- Removed: `operator.id=op_999`, `operator.name=op_999`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `op_017: Scalar -> Vector`,
  `op_044: Vector -> Scalar`, target type `Scalar -> Scalar`; semantic names
  are replaced by deterministic opaque IDs.
- Exact search output:

  ```text
  layer21.composition.opaque ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_VALID_EXPRESSION
    candidates=1 raw=1 valid=1 invalid=0 unknown=0 quotient_merges=0 retained=3 obligations=0 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `Compose(op_044, op_017)`.
- Classification: structural recovery; the same typed construction was found
  without English mathematical names.
- Verdict: `VALID BLIND BENCHMARK`.

### 3. Adjoint holdout

- Benchmark: `layer21.adjoint.holdout`
- Hidden target: `op.adjoint`
- Removed: `operator.id=op.adjoint`, `operator.name=op.adjoint`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `op.forward: V -> W`, explicit inner-product
  structure on both `V` and `W`, target type `W -> V`.
- Exact search output:

  ```text
  layer21.adjoint.holdout ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_VALID_EXPRESSION
    candidates=4 raw=4 valid=4 invalid=0 unknown=3 quotient_merges=0 retained=5 obligations=5 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `Adjoint(op.forward)`. The four candidates include the
  adjoint and three reverse-type inverse candidates; the scorer selects only
  the expected constructor family after discovery.
- Generated obligation: space structure for the adjoint; the verification
  result remains `OPEN`.
- Classification: structural recovery with an open proof obligation.
- Verdict: `VALID BLIND BENCHMARK`.

### 4. Opaque adjoint holdout

- Benchmark: `layer21.adjoint.opaque`
- Hidden target: `op_999`
- Removed: `operator.id=op_999`, `operator.name=op_999`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `op_017: V -> W`, inner-product structure on `V` and
  `W`, target type `W -> V`.
- Exact search output:

  ```text
  layer21.adjoint.opaque ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_VALID_EXPRESSION
    candidates=4 raw=4 valid=4 invalid=0 unknown=3 quotient_merges=0 retained=5 obligations=5 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `Adjoint(op_017)`.
- Classification: structural recovery under opaque identifiers.
- Verdict: `VALID BLIND BENCHMARK`.

### 5. Adjoint missing-structure negative control

- Benchmark: `layer21.adjoint.missing-structure`
- Hidden target: `op.adjoint`
- Removed: the same standalone target operator metadata as the adjoint
  holdout; no inner-product assumptions are supplied.
- Visible prerequisites: `op.forward: V -> W`, reverse target type `W -> V`,
  but no represented inner-product/Hilbert-like structure.
- Exact search output:

  ```text
  layer21.adjoint.missing-structure ... structural=VALID_ALTERNATIVE_WITH_OPEN_PRECONDITION precondition=UNKNOWN proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=CANDIDATE_BUT_OPEN_PRECONDITION
    candidates=4 raw=4 valid=4 invalid=0 unknown=4 quotient_merges=0 retained=5 obligations=5 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `Adjoint(op.forward)` is present only as an unresolved
  candidate; it is not marked valid or proved.
- Classification: partial/open-precondition control. Reverse type alone did
  not become `VALID`.
- Verdict: `VALID BUT WEAK/PARTIAL`.

### 6. Structured inverse candidate

- Benchmark: `layer21.inverse.structured`
- Hidden target: `op.inverse`
- Removed: `operator.id=op.inverse`, `operator.name=op.inverse`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `op.forward: V -> W`, explicit invertibility
  structure on the forward expression, target type `W -> V`.
- Exact search output:

  ```text
  layer21.inverse.structured ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_INVERSE_CANDIDATE
    candidates=4 raw=4 valid=4 invalid=0 unknown=1 quotient_merges=0 retained=5 obligations=5 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `InverseCandidate(op.forward, TWO_SIDED_INVERSE_CANDIDATE)`.
- Generated obligations: the left law `B ∘ A = I_V` and right law
  `A ∘ B = I_W`; neither obligation is discharged by the constructor itself.
- Classification: structural recovery of an inverse candidate, not proof of
  invertibility.
- Verdict: `VALID BLIND BENCHMARK`.

### 7. Reverse-type inverse negative control

- Benchmark: `layer21.inverse.unknown-precondition`
- Hidden target: `op.inverse`
- Removed: `operator.id=op.inverse`, `operator.name=op.inverse`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `op.forward: V -> W`, target type `W -> V`, no
  invertibility assumption.
- Exact search output:

  ```text
  layer21.inverse.unknown-precondition ... structural=VALID_ALTERNATIVE_WITH_OPEN_PRECONDITION precondition=UNKNOWN proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=CANDIDATE_BUT_NOT_PROVEN_INVERTIBILITY
    candidates=4 raw=4 valid=4 invalid=0 unknown=4 quotient_merges=0 retained=5 obligations=5 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: the two-sided inverse candidate is retained with two
  open inverse-law obligations; the scorer explicitly rejects interpreting it
  as a proved inverse.
- Classification: partial recovery / honest negative control.
- Verdict: `VALID BUT WEAK/PARTIAL`.

### 8. Commutator holdout

- Benchmark: `layer21.commutator.holdout`
- Hidden target: `op.commutator`
- Removed: `operator.id=op.commutator`, `operator.name=op.commutator`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `op.A: V -> V`, `op.B: V -> V`, explicit additive/
  linear structure on `V`, target type `V -> V`.
- Exact search output:

  ```text
  layer21.commutator.holdout ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_VALID_EXPRESSION
    candidates=20 raw=20 valid=20 invalid=0 unknown=12 quotient_merges=0 retained=22 obligations=26 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `[op.A, op.B]` as the explicit commutator constructor;
  the other 19 typed candidates remain visible in the audit output.
- Generated obligations: additive structure and definedness of the ordered
  compositions. No arbitrary coefficient enumeration was used.
- Classification: structural recovery with open proof obligations.
- Verdict: `VALID BLIND BENCHMARK`.

### 9. Incompatible commutator negative control

- Benchmark: `layer21.commutator.invalid-type`
- Hidden target: `op.commutator`
- Removed: `operator.id=op.commutator`, `operator.name=op.commutator`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: two `V -> W` operators, target type `V -> V`, no
  compatible endomorphism pair.
- Exact search output:

  ```text
  layer21.commutator.invalid-type ... structural=MISS precondition=NONE proof=UNSUPPORTED search=EXHAUSTED_RELATIVE_SPACE scorer=NO_FALSE_POSITIVE
    candidates=0 raw=0 valid=0 invalid=0 unknown=0 quotient_merges=0 retained=2 obligations=0 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Classification: miss/negative-control pass. Goal output typing pruned the
  commutator schema before child expansion; no invalid commutator was emitted.
- Verdict: `VALID BLIND BENCHMARK`.

### 10. Conjugation holdout

- Benchmark: `layer21.conjugation.holdout`
- Hidden target: `op.conjugated`
- Removed: `operator.id=op.conjugated`, `operator.name=op.conjugated`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `op.T: V -> W`, `op.A: W -> W`, explicit invertibility
  structure for `op.T`, target type `V -> V`.
- Exact search output:

  ```text
  layer21.conjugation.holdout ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_VALID_EXPRESSION
    candidates=1 raw=1 valid=1 invalid=0 unknown=0 quotient_merges=0 retained=3 obligations=2 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `Conjugation(op.T, op.A)`, representing the typed
  transport form `T⁻¹ ∘ A ∘ T`.
- Generated obligations: invertibility of `T` and transport compatibility;
  the output is not called a verified transport theorem.
- Classification: structural recovery with open transport obligations.
- Verdict: `VALID BLIND BENCHMARK`.

### 11. Indexed family holdout

- Benchmark: `layer21.indexed-family`
- Hidden target: `op.hidden`
- Removed: `operator.id=op.hidden`, `operator.name=op.hidden`,
  `operator.provenance=layer21-controlled-fixture`.
- Visible prerequisites: `d_k: X_k -> X_(k+1)`, declared index family `d`,
  `d_(k+1): X_(k+1) -> X_(k+2)`, target type `X_k -> X_(k+2)`.
- Exact search output:

  ```text
  layer21.indexed-family ... structural=VALID_ALTERNATIVE precondition=VALID proof=OPEN search=EXHAUSTED_RELATIVE_SPACE scorer=SYNTHESIZED_VALID_EXPRESSION
    candidates=1 raw=4 valid=4 invalid=0 unknown=0 quotient_merges=0 retained=4 obligations=2 termination=EXHAUSTED_RELATIVE_SPACE
  ```

- Selected candidate: `Compose(d_(k+1), d_k)`.
- Classification: structural recovery with index terms retained in expression
  identity; base-name matching alone is not used.
- Verdict: `VALID BLIND BENCHMARK`.

### 12. Missing-constructor control

- Benchmark: `layer21.missing-tensor-product`
- Hidden target: `tensor_product(op.A,op.B)`; no standalone target operator is
  injected.
- Removed: no operator node; the tensor/product constructor is outside the
  Layer-21 v1 catalog.
- Visible prerequisites: `op.A: V -> W`, `op.B: W -> V`; target requests a
  disabled constructor family.
- Exact search output:

  ```text
  layer21.missing-tensor-product ... structural=MISS precondition=UNSUPPORTED proof=UNSUPPORTED search=UNSUPPORTED_LANGUAGE scorer=UNSUPPORTED_LANGUAGE
    candidates=0 raw=0 valid=0 invalid=0 unknown=0 quotient_merges=0 retained=0 obligations=0 termination=UNSUPPORTED_LANGUAGE
  ```

- Classification: honest unsupported-language result; no fabricated tensor
  candidate was added to make the benchmark pass.
- Verdict: `VALID BLIND BENCHMARK`.

## Leakage and scorer isolation audit

All 12 cases were target-blind at the synthesizer boundary. The following
checks were run against the canonical `Problem` supplied to the solver:

| Potential leak | Result |
|---|---|
| hidden target ID/name | false |
| expected expression | false |
| expected constructor family as scorer data | false |
| benchmark ID or fixture label | false |
| aliases/descriptions/semantic statement text | false |
| relation IDs/family names/metadata/source references | false |
| target-specific code branch | false |
| scorer data/callback in discovery | false |
| runtime LLM calls | false |
| numerical discovery experiments | 0 |

The controlled fixture removes the target operator, name, and provenance before
the Theory is refreshed. The expected expression exists only in the fixture
and external scorer. Opaque composition and opaque adjoint both succeeded,
so the opaque-ID control has two positive cases. The machine report records
`leakage.passed=true` and `opaque_id_robust=true`.

## Open-discovery safety

Open discovery used the exact policy
`OPEN_DISCOVERY_GRAMMAR`, depth 1, cost 4, candidate budget 256, with only
typed composition and indexed instantiation enabled. The real seed had no
indexed family instances in the selected open fixture.

| Schema family | Enabled | Raw | VALID | INVALID | UNKNOWN | Quotient merges | Retained classes | Serious | Budget-pruned |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| composition | yes | 144 | 50 | 94 | 0 | 0 | 62 | 0 | 0 |
| adjoint | no | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| inverse left/right/two-sided | no | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| commutator | no | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| conjugation | no | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| indexed instantiation | yes | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

The aggregate accounting is `raw=144 = valid 50 + invalid 94`, with no
UNKNOWN or budget terminal category. `retained_classes=62` includes the
primitive representatives and is therefore not a second count of generated
applications. Quotient merges are zero because the retained expressions in
this run are structurally distinct under the active Layer-16 equivalence
theory; the quotient path was still executed and its ledger is auditable.

## Scaling comparison

The composition-only and full Layer-21 measurements use the same synthetic
all-`V -> V` primitive theories, depth 1, and deterministic enumeration. The
Layer-21 unknown column is the aggregate of unresolved constructor
preconditions. Runtime is measured but excluded from the deterministic digest.

| Primitives | Composition raw | Composition retained | Layer21 raw | Layer21 type-invalid | Layer21 UNKNOWN | Layer21 quotient merges | Layer21 retained | Peak frontier | Composition ms | Layer21 ms |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 3 | 9 | 12 | 39 | 0 | 30 | 0 | 42 | 42 | 19.2782 | 114.838 |
| 6 | 36 | 42 | 132 | 0 | 96 | 0 | 138 | 138 | 79.0938 | 418.77 |
| 9 | 81 | 90 | 279 | 0 | 198 | 0 | 288 | 288 | 185.312 | 938.515 |

Composition-only counts represent primitive atoms plus typed compositions.
Layer21 counts include composition, adjoint, three inverse variants,
commutator, and conjugation attempts in the goal-directed grammar. The paths
are not identical algorithms, so the table is a stage comparison, not a
claim that the numbers are directly interchangeable or universally scalable.
The growth and peak frontier are reported without heuristic candidate hiding.

## Proof and verification boundary

Constructor-specific obligations were attached to the Layer-18 `ProofPlan`
and verified through the Layer-19 orchestrator after discovery. The measured
holdout proof statuses are:

- composition: no constructor-specific proof obligation for the selected
  expression; indexed family instantiations contribute two inherited index
  parameter obligations to the selected composite;
- adjoint: space-structure obligation, `OPEN` when the structure is present
  as a prerequisite but no defining theorem is available;
- inverse: left/right inverse-law obligations, `OPEN`; the candidate cannot
  discharge its own law;
- commutator: additive-structure and ordered-composition obligations, `OPEN`;
- conjugation: transform invertibility and transport compatibility, `OPEN`;
- missing structure/unknown inverse: `UNKNOWN` precondition plus `OPEN`
  obligations; and
- missing tensor: `UNSUPPORTED_LANGUAGE`, not an invented proof.

`EXHAUSTED_RELATIVE_SPACE` in the UNKNOWN cases means that the finite
constructor grammar/depth/cost stream was fully enumerated. It does not mean
that the UNKNOWN prerequisite or its mathematical obligation was proved.

`FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED` remains unchanged.

## Historical Layer-20 comparison

Layer 20 remains frozen at `LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED` with
15 cases and structural exact/valid-alternative/partial/miss/false-positive
counts `1/6/0/8/0`. Its open-discovery legacy count remains 62 generated,
650 pruned, and 0 serious. Layer21 adds a separate constructor grammar and
controlled synthesis report; it does not rewrite those counts or claim that
Layer21 existed during the Layer-20 evaluation.

## Gates and remaining bottlenecks

- Debug CTest: 8/8 passed.
- Release build: passed.
- Release CTest: 8/8 passed.
- ASan/UBSan CTest: 8/8 passed.
- Layer21 internal determinism: 3/3 identical,
  digest `layer21_benchmark_digest.4c26807e3ed03a83`.
- Opaque-ID positives: 2/2 required cases passed.
- Discovery numerics: 0.
- Runtime LLM calls: 0.
- Unrestricted linear combinations: disabled.
- Frozen blind baseline: 0 exact, 1 structural, 4 partial, 1 miss,
  0 false positives; unchanged.
- `git diff --check`: run after documentation edits.

Top three remaining bottlenecks are `FORMAL_VERIFICATION`,
`SEARCH_SCALABILITY`, and `SPACE_STRUCTURE_CONSTRAINTS`. A physics Theory
Pack is technically safe to begin as a controlled schema/data experiment only:
the constructor API is domain-agnostic, but no physics theorem, transport
claim, or production physical interpretation is implied.

No Layer 22 work was started.
