#include "opforge/atlas/seed.hpp"
#include "opforge/utility/layer20.hpp"

#include <cassert>
#include <iostream>
#include <set>

int main() {
  const auto first = opforge::utility::run_layer20_benchmarks(opforge::atlas::make_vector_calculus_seed());
  assert(first.determinism.passed);
  assert(first.determinism.repetitions == 3);
  assert(first.leakage.passed());
  assert(first.leakage.discovery_numerical_experiments == 0);
  assert(first.grammar.unrestricted_linear_combinations == false);
  assert(first.practical_utility_verdict == "LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED" ||
         first.practical_utility_verdict == "ARCHITECTURE_WORKS_BUT_PRACTICAL_UTILITY_NOT_YET_DEMONSTRATED");

  const auto second = opforge::utility::run_layer20_benchmarks(opforge::atlas::make_vector_calculus_seed());
  assert(first.deterministic_digest == second.deterministic_digest);
  assert(first.determinism.compared_digests == second.determinism.compared_digests);

  bool synthesis = false;
  bool never_named = false;
  bool missing_primitive = false;
  bool negative_control = false;
  bool budget_distinction = false;
  for (const auto& item : first.cases) {
    if (item.id == "tier-c.missing-operator-synthesis") {
      synthesis = item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION" &&
                  item.structural == opforge::utility::StructuralClassification::ValidAlternative &&
                  !item.result_bundles.empty();
    }
    if (item.id == "tier-d.never-named")
      never_named = item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION" &&
                    item.structural == opforge::utility::StructuralClassification::ValidAlternative;
    if (item.id == "tier-f.missing-primitive")
      missing_primitive = item.scorer_outcome == "UNSUPPORTED" &&
                          item.search == opforge::utility::SearchClassification::UnsupportedLanguage;
    if (item.id == "tier-g.budget-ended-vs-exhausted")
      budget_distinction = item.search == opforge::utility::SearchClassification::BudgetEnded &&
                           !item.accounting.relative_complete;
    if (item.tier == "G")
      negative_control = negative_control || item.structural == opforge::utility::StructuralClassification::Miss;
  }
  assert(synthesis);
  assert(never_named);
  assert(missing_primitive);
  assert(negative_control);
  assert(budget_distinction);
  assert(first.summary.search_budget_ended == 1);
  assert(first.summary.negative_controls == 5);
  assert(first.summary.negative_controls_passed == 5);
  std::cout << "utility tests passed digest=" << first.deterministic_digest << "\n";
  return 0;
}
