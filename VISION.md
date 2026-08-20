# OpForge vision

## What OpForge is

OpForge is a C++ mathematical structural reasoning and discovery engine. Its
long-term design has two complementary modes:

1. **Forward mathematical discovery:** start with known operators and
   structures, generate legal consequences, quotient equivalent constructions,
   form explicit conjectures, and expose proof obligations.
2. **Goal-directed mathematical solving:** represent an in-scope problem as a
   mathematical context and target judgment, reason backward from the target
   and forward from known mathematics, and return auditable candidate
   derivations or an explicit failure state.

## Near-term scope

The near-term domain is operator-centric mathematics: typed spaces, operators,
compositions, differential complexes, decompositions, transformations,
continuous/discrete analogies, assumptions, and validity regimes.

OpForge does not promise universal mathematics, autonomous theorem discovery, or
replacement of human mathematical judgment.

## Long-term target

OpForge should represent mathematical theories, contexts, judgments, proof
states, and validity regimes; search structurally under an explicit grammar and
equivalence contract; preserve derivation provenance; generate explicit proof
obligations; and produce a small auditable set of mathematical candidates or a
reproducible explanation of failure.

Every result must expose what is known, what was inferred, which assumptions
apply, which obligations remain, how search was bounded, and what evidence
supports the reported epistemic status.

## Non-goals and prohibitions

- LLM output is not mathematical proof.
- Numerical experiments do not guide open discovery, generate candidates, rank
  candidates, or promote candidates.
- Unrestricted arbitrary linear-combination enumeration is not allowed in open
  discovery.
- Heuristic top-N ranking is not mathematically complete search and must never
  be presented as such.
- Absence from the Atlas or from an internet/literature search is not proof of
  novelty.
- Passing tests is not proof of a theorem.
- A structural pattern is not automatically an equality, rewrite rule, or
  verified result.
- A deterministic rerun is reproducibility evidence, not independent
  mathematical derivation.
- OpForge must not claim to solve arbitrary mathematics merely because a problem
  can be serialized.

## Success condition

Success is not a large candidate count or a nonzero novelty claim. Success is a
bounded, reproducible, scientifically honest result whose assumptions,
validity regime, derivation, pruning, proof obligations, evidence, and
unresolved items can be audited by another researcher.
