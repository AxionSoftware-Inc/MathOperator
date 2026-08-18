# OpForge — Operator Discovery Engine

OpForge is a C++20 research engine for formal operator mapping, structural relation discovery, rediscovery, and verified candidate synthesis. The Operator Atlas is the central mathematical model, not a passive list.

Initial scope: vector analysis and discrete differential operators, with linear algebra as supporting infrastructure. The architecture reserves boundaries for expression graphs, symbolic/numerical verification, counterexample search, AI hypothesis providers, Python bindings, visualization, and a future Lean adapter.

Build:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Current semantic slice includes a typed vector-calculus seed atlas, versioned JSON export/import, mathematical spaces, typed signatures with input/output regularity, expression AST primitives, structured composition failures, identity/evidence data types, Graphviz export, and human-readable/JSON rediscovery traces. Verification status is recomputed from evidence when records enter the Atlas; callers cannot promote a record merely by setting a status field.

## Atlas Semantic Densification v0.25

The Atlas is the central mathematical model and knowledge graph. It is split into versioned JSON modules rather than one monolithic data file:

- `atlas/vector_calculus.json`, `atlas/differential_forms.json`, `atlas/linear_algebra.json`, `atlas/transforms.json`, and `atlas/discrete.json` provide the typed operator records.
- `atlas/spaces_extended.json` provides form grades, matrix spaces, signal spaces, graph/simplicial spaces, grids, and manifold metadata.
- `atlas/relations.json` and `atlas/semantic_relations.json` encode composition, duality, factorization, decomposition, projection, continuous/discrete analogy, transform correspondence, annihilation, and invariant-preservation relations.
- `atlas/identities.json` and `atlas/semantic_identities.json` encode assumptions, domain constraints, canonical forms, provenance, and verification evidence.

The current loaded Atlas contains 59 operators, 28 spaces, 72 relations, and 46 identities across six domains. `./build/opforge atlas validate atlas` is the consistency gate; it currently reports zero dangling relation or identity references. The loader also reports domain and provenance breakdowns so semantic density can be measured without treating record count as research progress.

The relation ontology is intentionally richer than a formula list. A relation may connect operators to operators or operators to spaces, which allows the Atlas to represent structural bridges such as vector-calculus operators to exterior calculus, continuous operators to discrete analogues, and convolution to Fourier transforms.

The deterministic tests cover `div ∘ grad → Laplacian`, `curl ∘ grad = 0`, `div ∘ curl = 0`, invalid AST/operator references, duplicate operators, dangling relations/identities, space mismatch, regularity failure, JSON round-trip, expanded Atlas consistency, and blind rediscovery regression cases.

The next slice is a fuller seed atlas (curl, 2D rot, Jacobian, Hessian, identities), evidence-derived status transitions, and a schema-backed JSON reader/writer.

Structural Pattern Detection Engine v0.1 is now available in `include/opforge/patterns/analyzer.hpp`. It builds a typed composition graph, discovers composition chains, zero compositions, name-independent differential-complex candidates, abstract operator families, and terminal structural gaps. Every result carries evidence and a deterministic trace; no operator names are special-cased and no candidate operators are generated.

Structural Generalization + Operator Candidate Synthesis v0.1 is available in `include/opforge/synthesis/candidate.hpp`. It uses a bounded typed grammar, canonical AST forms, explicit requirements, explainable score components, triviality/deduplication pruning, deterministic candidate IDs, lineage export, and guarded promotion. Candidates remain outside the Atlas until symbolic/formal verification status is supplied. No AI, random generation, or novelty claim is part of this milestone.

Research Evaluation Core v0.1 is available in `include/opforge/research/evaluation.hpp`. It provides one experiment/evidence model, deterministic typed test cases, property evaluation, active counterexample search, budget exhaustion states, recovery hooks, benchmark comparison hooks, lifecycle states, rejected-candidate records, deterministic sequential/parallel evaluation ordering, and human/JSON reports. The current benchmark result is explicitly `inconclusive` when no numerical executor is available; structural cost is reported only as a proxy and is not treated as empirical performance.

Autonomous Research Engine v0.1 adds `include/opforge/numerics/executor.hpp` and `include/opforge/research/campaign.hpp`. The seed numerical backend executes scalar/vector sampled grids for gradient, divergence, curl, Laplacian, Jacobian, Hessian, identity, zero, and compositions. Campaigns keep Research Memory separate from the Atlas, choose deterministic research actions over multiple cycles, persist checkpoint state, avoid repeated candidate evaluations, and export a reproducible campaign report.

Minimal headless CLI:

```bash
./build/opforge atlas validate
./build/opforge atlas audit atlas
./build/opforge discover --checkpoint /tmp/opforge-campaign.txt
./build/opforge resume --checkpoint /tmp/opforge-campaign.txt
./build/opforge report /tmp/opforge-campaign.txt
```

Numerical benchmark claims are only emitted when both candidate and baseline execute successfully. Otherwise the result remains `unsupported` or `inconclusive`; numerical evidence is never promoted to formal proof.

Historical rediscovery is implemented in `include/opforge/benchmarks/rediscovery.hpp`. Its blind harness masks declared identities and relation keys (`source|kind|target`) before running. The v0.25 cases cover d-squared-zero abstraction, Hodge/Laplacian decomposition, Fourier/convolution correspondence, continuous/discrete analogy, and projection decomposition. Results distinguish exact, structural-equivalent, partial, missed, false-positive, and leakage outcomes; leakage is counted separately and cannot improve precision or recall.

The first open-ended campaign is reproducible through the CLI:

```bash
./build/opforge discover --atlas atlas \
  --snapshot v0.25-open-search-baseline \
  --campaign-id C-open-final \
  --max-cycles 4 --max-actions 40 --max-experiments 80 \
  --checkpoint /tmp/opforge-open-final.checkpoint \
  --report /tmp/opforge-open-final-report.txt
```

The checked-in result is [reports/open_search_v0.25.txt](reports/open_search_v0.25.txt). The campaign ran with AI disabled, target `none`, and a frozen curated Atlas. The run produced 287 patterns, 10 cross-domain matches, 41 gaps, 170 candidates, 42 rejected candidates, 48 false-interest records, 30 numerical experiments, and 4 surviving candidates. These are Atlas-derived structural candidates, not novelty claims.

## Semantic Equivalence, Interestingness, and Oracles v0.3

The v0.3 layer adds a semantic equivalence gate and keeps the Operator Atlas central. `include/opforge/synthesis/registry.hpp` defines a reusable known-construction registry populated from verified Atlas identities and curated constructions. It recognizes exact/canonical identity rewrites and construction families such as `div ∘ grad`, Hessian as `Jacobian ∘ gradient`, curl-curl decomposition, Hodge-Laplacian decomposition, symmetric/skew projection, and Fourier correspondence. This prevents rediscovery of a known construction from being reported as a new operator.

`InterestingnessScore` separates semantic novelty, generalization, recovery, independent relations, MDL-style compression reduction, invariant potential, cross-domain reach, computational utility, non-triviality, and evidence strength. Campaign dossiers print the components and their reasons; the score is an auditable prioritization signal, not a novelty probability.

`include/opforge/research/oracles.hpp` adds regularity, boundary, geometry, dimension, and discretization oracle interfaces. They are assumption-sensitive and distinguish survived regimes, assumption violations, unsupported backends, and future executable counterexamples. Failure patterns retain correction requirements and residual cluster keys so limitations become reusable research memory rather than anonymous rejection messages.

The reproducible v0.3 campaign is:

```bash
./build/opforge discover --atlas atlas \
  --snapshot v0.3-semantic-equivalence \
  --campaign-id C-v0.3-blind \
  --max-cycles 4 --max-actions 40 --max-experiments 80 \
  --checkpoint /tmp/opforge-v0.3.checkpoint \
  --report /tmp/opforge-v0.3-report.txt
```

The checked-in result is [reports/open_search_v0.3.txt](reports/open_search_v0.3.txt): 287 patterns, 169 generated candidates, 43 rejected candidates, 8 known-equivalent constructions, 49 false-interest records, 30 numerical experiments, 21 failure patterns across 15 residual clusters, and 4 surviving leads. Serious candidates: 0. The surviving leads are structurally distinct but assumption-dependent or backend-limited; they are not claims of new mathematics.

## Meta-Pattern Discovery and Residual-Driven Synthesis v0.4

v0.4 promotes structural patterns into first-class `PatternObject` records and adds `MetaPatternAnalyzer` in `include/opforge/patterns/meta.hpp`. It discovers reusable laws such as two-step complexes, continuous/discrete analogies, operator families, and repeated structural roles. Family scores explicitly combine independent domains, concrete realizations, assumption generality, and compression. Missing-role predictions are emitted only when a reusable meta-law justifies them; generic terminal graph gaps are not enough.

`include/opforge/research/residual.hpp` defines typed `ResidualObject` and `ResidualCluster` models. Oracle failures retain domain, object kind, order, locality, dimension, metric, boundary, regularity, discretization, convergence, canonical form, and correction requirements. `include/opforge/synthesis/goal.hpp` adds explicit goals and bounded grammar rules for adjoint, projection/inclusion, weighted combinations, and correction terms. Rules activate only when a meta-pattern or residual requirement justifies them.

The residual benchmark is available through `include/opforge/benchmarks/residual.hpp`; it executes a failure → residual → correction requirement → typed correction → retest loop for a projection regime contrast. The campaign also records meta-patterns, predicted roles, residual clusters, correction attempts, successful repairs, and audits of the four v0.3 research leads.

```bash
./build/opforge discover --atlas atlas \
  --snapshot v0.4-meta-residual \
  --campaign-id C-v0.4-blind \
  --max-cycles 4 --max-actions 60 --max-experiments 80 \
  --checkpoint /tmp/opforge-v0.4.checkpoint \
  --report /tmp/opforge-v0.4-report.txt
```

The checked-in result is [reports/open_search_v0.4.txt](reports/open_search_v0.4.txt): 6 meta-pattern families, 32 justified predicted roles, 15 residual clusters, 59 deduplicated correction attempts, 1 benchmark repair, 4 surviving leads, and 0 serious candidates. AI remained disabled and the Atlas remained frozen.

## Open-Ended Structural Discovery v0.5

v0.5 lifts the search unit above individual AST expressions. `include/opforge/synthesis/schema.hpp` defines `ConstructionSchema`, parameterized families, schema completions, and structural law candidates. Schemas are induced from multiple concrete/meta-pattern realizations and retain typed roles, spaces, constraints, assumptions, identities, realization rules, and evidence.

`include/opforge/research/structure_analysis.hpp` adds algebraic closure findings, typed commutator findings, metadata-backed invariant hypotheses, and candidate validity-region maps. The engine distinguishes `inferred`, `structurally_supported`, `numerically_supported`, `generalized`, `discovery_lead`, and `unresolved`; it does not label any result as new mathematics, a new operator, or a theorem.

The schema compression benchmark hides the higher-level two-step-complex law while concrete realizations remain available. It recovered the law with 7 realizations and compression gain `0.857143`. The open campaign is reproducible:

```bash
./build/opforge discover --atlas atlas \
  --snapshot v0.5-open-ended-schema \
  --campaign-id C-v0.5-open \
  --max-cycles 4 --max-actions 80 --max-experiments 80 \
  --checkpoint /tmp/opforge-v0.5.checkpoint \
  --report /tmp/opforge-v0.5-report.txt
```

The checked-in result is [reports/open_search_v0.5.txt](reports/open_search_v0.5.txt): 20 induced schemas, 11 parameterized families, 4 schema completions, 3 law candidates, 14 discovery leads, 20 closure findings, 24 commutator findings, 2 invariant hypotheses, 7 validity regions, 4 surviving leads, and 0 serious candidates. Ordinary composition generation remained bounded at 169 candidates.

## Scientific Stress Test and Lead Evaluation v0.6

v0.6 freezes a reproducible scientific baseline and evaluates every v0.5 discovery lead through semantic attacks, assumption weakening, edge/dimension/regularity/boundary/geometry contrasts, continuous/discrete checks, counterexample attempts, multiresolution numerical stress, independent-representation checks, rewrite checks, and metamorphic checks. `include/opforge/research/scientific.hpp` defines immutable baseline metadata, `LeadDossier`, reversible outcome history, falsification strength, difficulty-tagged benchmark metrics, action information gain, and scientific diagnosis.

The CLI now accepts campaign modes `blind_rediscovery`, `structural_exploration`, `failure_driven`, `schema_discovery`, and `lead_falsification`. Example:

```bash
./build/opforge discover --atlas atlas \
  --mode lead_falsification \
  --snapshot v0.6-scientific-baseline \
  --campaign-id C-v0.6-lead-falsification \
  --max-cycles 6 --max-actions 140 --max-experiments 200 \
  --checkpoint /tmp/opforge-v0.6-lead.checkpoint \
  --report /tmp/opforge-v0.6-lead-report.txt
```

The validation suite runs four leakage-checked benchmark classes at easy/medium/hard difficulty: hidden operator recovery, hidden higher-level schema, hidden correction law, and hidden structural bridge. The checked-in report is [scientific_validation_v0.6.txt](/Users/macbookpro/Documents/Operator/reports/scientific_validation_v0.6.txt). It evaluates all 14 leads, records 0 leakage events, reports easy/medium/hard misses honestly, retains 0 serious candidates, and confirms that ordinary composition generation remains bounded at 169 candidates.

The current milestone deliberately does not claim novelty, formal Lean proofs, or AI-generated mathematics. It establishes a denser, auditable semantic substrate and a repeatable rediscovery/evaluation protocol for those later layers.
