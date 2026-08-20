#include "opforge/atlas/seed.hpp"
#include "opforge/constraints/layer22.hpp"

#include <cassert>
#include <iostream>

int main() {
  const auto report = opforge::constraints::run_layer22_benchmarks(opforge::atlas::make_vector_calculus_seed());
  assert(report.verdict == "CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED" ||
         report.verdict == "LIMITED_CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED");
  assert(report.leakage.passed);
  assert(report.leakage.opaque_id_robust);
  assert(!report.leakage.numerical_guidance);
  assert(!report.leakage.runtime_llm);
  assert(report.determinism.passed);
  assert(report.determinism.repetitions == 3);

  bool type_only = false;
  bool adjoint = false;
  bool left_inverse = false;
  bool two_sided = false;
  bool commutator = false;
  bool conjugation = false;
  bool indexed = false;
  bool false_property = false;
  bool unknown = false;
  std::size_t opaque = 0;
  for (const auto& item : report.cases) {
    assert(item.metrics.accounting_consistent());
    type_only = type_only || (item.category == "TYPE_ONLY_AMBIGUITY" && item.classification == "TYPE_ONLY_MATCH" && item.candidate_expressions.size() > 1);
    adjoint = adjoint || (item.category == "ADJOINT_PROPERTY" && item.classification == "EXACT_CONSTRAINT_SATISFACTION");
    left_inverse = left_inverse || (item.id == "layer22.inverse.left_inverse" && item.classification == "STRUCTURAL_WITH_OPEN_CONSTRAINTS");
    two_sided = two_sided || (item.id == "layer22.inverse.two_sided_inverse" && item.classification == "STRUCTURAL_WITH_OPEN_CONSTRAINTS");
    commutator = commutator || (item.category == "COMMUTATOR_PROPERTY" && item.classification == "EXACT_CONSTRAINT_SATISFACTION");
    conjugation = conjugation || (item.category == "CONJUGATION_PROPERTY" && item.classification == "EXACT_CONSTRAINT_SATISFACTION");
    indexed = indexed || (item.category == "INDEXED_PARAMETER_CONSTRAINT" && item.classification == "EXACT_CONSTRAINT_SATISFACTION");
    false_property = false_property || (item.category == "FALSE_PROPERTY_NEGATIVE" && item.classification == "NO_MATCH");
    unknown = unknown || (item.category == "UNKNOWN_PROPERTY_CONTROL" && item.classification == "STRUCTURAL_WITH_OPEN_CONSTRAINTS");
    if (item.opaque_id_case && item.classification == "EXACT_CONSTRAINT_SATISFACTION") ++opaque;
  }
  assert(type_only && adjoint && left_inverse && two_sided && commutator && conjugation && indexed && false_property && unknown);
  assert(opaque >= 2);
  assert(report.real_atlas_status == "UNSUPPORTED_CONSTRAINT_LANGUAGE" ||
         report.real_atlas_status == "REAL_ATLAS_STRUCTURAL_PROBE_LIMITED" ||
         report.real_atlas_status == "UNSUPPORTED_FRAGMENT");
  std::cout << "constraint-guided tests passed digest=" << report.deterministic_digest << "\n";
  return 0;
}
