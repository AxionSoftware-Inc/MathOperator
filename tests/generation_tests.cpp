#include "opforge/atlas/seed.hpp"
#include "opforge/generation/layer21.hpp"

#include <cassert>
#include <iostream>

int main() {
  const auto report = opforge::generation::run_layer21_benchmarks(opforge::atlas::make_vector_calculus_seed());
  assert(report.verdict == "LIMITED_GENERATIVE_SYNTHESIS_DEMONSTRATED" ||
         report.verdict == "CONSTRUCTION_FRAMEWORK_IMPLEMENTED_BUT_UTILITY_NOT_DEMONSTRATED");
  assert(report.leakage.passed);
  assert(report.leakage.opaque_id_robust);
  assert(report.leakage.discovery_numerical_experiments == 0);
  assert(!report.open_discovery.unrestricted_linear_combinations);
  assert(report.determinism.passed);
  assert(report.determinism.repetitions == 3);
  assert(report.cases.size() >= 9);

  bool adjoint = false;
  bool inverse = false;
  bool inverse_unknown = false;
  bool commutator = false;
  bool conjugation = false;
  bool indexed = false;
  bool missing = false;
  bool invalid_commutator = false;
  bool adjoint_unknown_guard = false;
  std::size_t opaque_successes = 0;
  for (const auto& item : report.cases) {
    adjoint = adjoint || (item.family == "ADJOINT" && item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION");
    inverse = inverse || (item.family == "INVERSE_CANDIDATE" && item.scorer_outcome == "SYNTHESIZED_INVERSE_CANDIDATE");
    inverse_unknown = inverse_unknown || (item.category == "reverse-type-negative-control" &&
                                           item.precondition_classification == "UNKNOWN" &&
                                           item.scorer_outcome == "CANDIDATE_BUT_NOT_PROVEN_INVERTIBILITY");
    commutator = commutator || (item.family == "COMMUTATOR" && item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION");
    conjugation = conjugation || (item.family == "CONJUGATION" && item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION");
    indexed = indexed || (item.family == "INDEXED_INSTANTIATION" && item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION");
    missing = missing || (item.family == "TENSOR_PRODUCT" && item.search_classification == "UNSUPPORTED_LANGUAGE");
    invalid_commutator = invalid_commutator || (item.id == "layer21.commutator.invalid-type" && item.scorer_outcome == "NO_FALSE_POSITIVE");
    adjoint_unknown_guard = adjoint_unknown_guard ||
                           (item.id == "layer21.adjoint.missing-structure" &&
                            item.precondition_classification == "UNKNOWN" &&
                            item.scorer_outcome == "CANDIDATE_BUT_OPEN_PRECONDITION" &&
                            item.structural_classification == "VALID_ALTERNATIVE_WITH_OPEN_PRECONDITION");
    if (item.opaque_id_case && item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION") ++opaque_successes;
  }
  assert(adjoint && inverse && inverse_unknown && commutator && conjugation && indexed && missing && invalid_commutator &&
         adjoint_unknown_guard);
  assert(opaque_successes >= 2);
  std::cout << "generation tests passed digest=" << report.deterministic_digest << "\n";
  return 0;
}
