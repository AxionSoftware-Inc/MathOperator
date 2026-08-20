# OpForge Layer 23 — Rich Mathematical Semantics and Construction Grammar v2

## Final verdict

`RICH_OPERATOR_SEMANTICS_DEMONSTRATED`

Layer 23 adds a separate rich semantic view over the Layer-15 `Theory` and
`Expression` model. It does not replace the historical Layer-20, Layer-21, or
Layer-22 paths, and it does not implement Layer 24 search redesign or Layer 25
formal proving.

The benchmark command was:

```text
./build/opforge benchmark rich_semantics atlas
```

Deterministic digest: `layer23_benchmark_digest.6b6b46b6e7002750`.

## Implemented semantic boundary

The new module is in `include/opforge/semantic/layer23.hpp` and
`src/semantic/layer23.cpp`.

It provides:

- explicit space properties: vector, inner-product, Hilbert-like, normed,
  dual, subspace, product, tensor-product, direct-sum, graded/indexed,
  function, finite/infinite dimensional, and scalar-field markers;
- explicit space relations: equality, inclusion, embedding, isomorphism,
  dual-of, product-of, tensor-product-of, graded-next, indexed, restriction,
  and extension;
- structured operator property facts with the distinct states
  `DECLARED_PROPERTY_FACT`, `DERIVED_PROPERTY`, `OPEN_PROPERTY_CANDIDATE`, and
  `UNKNOWN_PROPERTY`;
- typed rule schemas with metavariables, premises, conclusions, context,
  validity regime, side conditions, provenance, and trust level;
- goal-directed `Restriction`, `Tensor`, `DualMap`, and `Adjoint` constructors;
- conservative generic propagation for linearity and invertibility through
  typed composition;
- a cross-space bridge that transports only properties covered by an explicit
  preservation rule;
- exact symbolic scalar descriptors, with arbitrary coefficient search
  deliberately deferred.

`Extension` is deferred because inclusion does not define a unique extension.
Pullback/pushforward are deferred because the current Atlas lacks a sufficient
structured smooth-map/domain relation. Formal theorem proving remains absent.

## Conservative real-Atlas migration

The adapter distinguishes explicit JSON metadata from parser defaults. A
default `linear=true`, `continuous=true`, or `dimension=-1` is not promoted as
a declared theorem or space fact.

| Metric | Measured value |
|---|---:|
| pre-Layer-23 fully structured facts | 6 |
| newly structured facts | 321 |
| cumulative fully structured adapter facts | 327 |
| remaining partial facts/statements | 220 |
| unsupported statements | 0 |
| structured spaces | 47 |
| structured space-property facts | 47 |
| structured space relations | 0 |
| structured operator-property facts | 274 |
| trusted rule schemas | 4 |

The 321 new facts are 47 explicit space-property facts plus 274 explicit
operator-property facts. The 4 generic rules are reported separately. Real
Atlas probes found 88 explicit linear facts, 47 structured space facts, no
indexed-space facts, and 10 inverse/commutation-related property facts.

No operator name, display name, alias, or analogy was used to promote a fact.
Metric metadata is retained as partial metadata when it does not establish the
full algebra required for an inner-product space. Analogy, correspondence,
relatedness, continuous/discrete analogy, and transform correspondence remain
non-proof relations.

## Target-blind controlled cases

Expected expressions and hidden targets were scorer-only fixture data. The
solver received only the rich theory, context, goal type, visible operands,
constraints, and policy.

| Case | Hidden target | Visible prerequisites | Classification | Result |
|---|---|---|---|---|
| `layer23.restriction.valid` | `Restriction(A,U)` | `A: V -> W`; explicit `U ⊆ V` | `STRUCTURAL_RECOVERY` | exact |
| `layer23.restriction.missing-inclusion` | `Restriction(A,U)` | `A: V -> W`; no inclusion fact | `STRUCTURAL_WITH_OPEN_CONSTRAINTS` | inclusion UNKNOWN |
| `layer23.restriction.opaque` | `Restriction(op_017,space_031)` | opaque IDs; explicit inclusion | `STRUCTURAL_RECOVERY` | exact |
| `layer23.tensor.valid` | `Tensor(A,B)` | tensor-capable `V1,V2,W1,W2`; typed `A,B` | `STRUCTURAL_RECOVERY` | exact |
| `layer23.tensor.opaque` | `Tensor(op_017,op_044)` | opaque IDs; same tensor structure | `STRUCTURAL_RECOVERY` | exact |
| `layer23.dual-map.valid` | `DualMap(A)` | `DualOf(V,V*)`, `DualOf(W,W*)` | `STRUCTURAL_RECOVERY` | exact |
| `layer23.dual-adjoint.negative` | `Adjoint(A)` | dual spaces only; no inner products | `STRUCTURAL_WITH_OPEN_CONSTRAINTS` | adjoint UNKNOWN |
| `layer23.propagation.linear-composition` | `Compose(B,A)` | explicit `LINEAR(A)`, `LINEAR(B)` | `STRUCTURAL_RECOVERY` | derived by trusted rule |
| `layer23.propagation.missing-linear` | `Compose(B,A)` | `LINEAR(B)` removed | `STRUCTURAL_WITH_OPEN_CONSTRAINTS` | derived property UNKNOWN |
| `layer23.cross-space.linear-preservation` | transported linear property | explicit isomorphism plus preservation schema | `STRUCTURAL_RECOVERY` | schema-supported |
| `layer23.cross-space.no-idempotent-law` | transported idempotence | isomorphism only; no preservation schema | `UNKNOWN` | no transfer |
| `layer23.partial-fact-firewall` | promoted analogy/equality | partial relation only | `NO_FALSE_POSITIVE` | not promoted |
| `layer23.numerical-closeness-negative` | exact property from approximation | no numerical input to solver | `NO_FALSE_POSITIVE` | numerics not consumed |
| `layer23.unknown-regime` | regime-dependent result | regime not established | `UNKNOWN` | UNKNOWN preserved |

The negative restriction case is intentionally not a pass: it retains a typed
construction only with an explicit open inclusion obligation. It is not exact
recovery.

## Property and rule safety

`LINEAR(A) + LINEAR(B) + composable(B,A)` derives
`LINEAR(Compose(B,A))` through a stable rule ID and provenance chain.
Removing `LINEAR(B)` changes the result to UNKNOWN. The same trusted fragment
supports invertibility composition; self-adjointness, commutation, and generic
property transport are not inferred from type.

`DualMap(A)`, `Adjoint(A)`, and inverse candidates remain distinct. A dual-space
relation does not establish an adjoint. An isomorphism does not preserve a
property unless an explicit preservation rule names that property.

Layer-22 integration was verified through `RichTheory::as_semantic_theory()`:
Layer-22 `PropertyEntailment` consumes Layer-23 declared/derived property facts
as structured generic facts while rejecting missing facts as UNKNOWN.

## Scaling comparison

Layer-21 values are actual Layer-21 goal-directed type-only synthesis counts
for the same controlled target. The grammars are not identical, so these are
burden measurements, not a claim of equal search spaces.

| Operators | Layer-21 attempts | Layer-23 attempts | Property checks | Invalid | UNKNOWN | Retained | Peak frontier |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 3 | 7 | 21 | 2 | 19 | 0 | 2 | 2 |
| 6 | 7 | 78 | 2 | 76 | 0 | 2 | 2 |
| 9 | 7 | 171 | 2 | 169 | 0 | 2 | 2 |

Layer 23 measures the semantic burden and retains a small goal-directed
grammar. Search scalability redesign is explicitly deferred to Layer 24.

## Leakage, determinism, and firewalls

- opaque-ID positive tests: 2/2 pass;
- hidden target in solver input: false;
- expected expression in solver input: false;
- display-name dependency: false;
- analogy-as-equality promotion: false;
- partial-fact promotion: false;
- numerical guidance: 0;
- runtime LLM calls: 0;
- repeated deterministic benchmark digest: identical across 3 runs;
- formal verification backend: not implemented.

Open discovery does not enable the rich constructors. The new constructors are
goal-directed only; the historical open-discovery policy remains unchanged.

## Gate status

The Layer-23-specific build and test passed, including the Layer-22 bridge
regression. Final gates passed: Debug CTest 10/10, clean Release CTest 10/10,
ASan/UBSan CTest 10/10, Layer-21 regression digest
`layer21_benchmark_digest.4c26807e3ed03a83`, Layer-22 Atlas regression digest
`layer22_benchmark_digest.d148a171b146b533`, rich benchmark determinism 3/3,
and `git diff --check`. The blind rediscovery baseline was rerun after the
Layer-23 build; the legacy open-discovery policy and numerical firewall remain
unchanged.

Layer 24 and Layer 25 were not implemented.
