#include "opforge/atlas/seed.hpp"
#include "opforge/atlas/loader.hpp"
#include "opforge/semantic/layer23.hpp"
#include "opforge/search/layer24.hpp"

#include <cassert>
#include <iostream>

int main() {
  const auto atlas = opforge::atlas::AtlasLoader::load("atlas");
  const auto seed = opforge::atlas::make_vector_calculus_seed();
  const auto report = opforge::search24::run_layer24_benchmarks(atlas, "AtlasLoader::load(atlas)");
  assert(report.verdict == "SCALABLE_CONSTRAINT_DIRECTED_SEARCH_DEMONSTRATED" ||
         report.verdict == "LIMITED_SEARCH_SCALABILITY_IMPROVEMENT_DEMONSTRATED");
  assert(report.leakage.passed);
  assert(report.leakage.opaque_id_robust);
  assert(report.determinism.passed);
  assert(report.determinism.repetitions == 3);
  assert(report.million_scale.hypothetical_raw >= 1000000);
  assert(report.million_scale.materialized < report.million_scale.hypothetical_raw);
  assert(report.million_scale.relative_complete);
  assert(report.numerics_zero && report.runtime_llm_zero);
  assert(report.unrestricted_linear_combinations_disabled);
  assert(report.production_atlas.actual_production_atlas);
  assert(report.production_atlas.atlas_operators == opforge::atlas::AtlasLoader::stats(atlas).operators);
  assert(report.production_atlas.atlas_operators > opforge::atlas::AtlasLoader::stats(seed).operators);
  assert(report.production_atlas.atlas_digest != "");
  assert(report.production_atlas.theory_digest != "");
  assert(report.production_atlas.migration.atlas_facts_before_layer23 > 0);
  assert(report.production_atlas.theory_metrics.structured_rule_schemas > 0);
  assert(report.production_atlas.reference_equivalence_attempted);
  assert(report.production_atlas.reference_equivalence_passed);
  assert(report.production_atlas.deterministic_replay.passed);
  assert(report.production_atlas.deterministic_replay.repetitions == 3);
  assert(report.production_atlas.cache_theory_mutation_detected);
  assert(report.production_atlas.context_isolation_valid);
  assert(report.production_atlas.regime_isolation_valid);
  assert(report.production_atlas.soundness_preserved);
  assert(report.controlled_vector_calculus_seed.metrics.full_theory_operators ==
         opforge::rich::RichTheoryAdapter{}.migrate(seed).theory.semantic_theory.operators.size());
  assert(!report.cases.empty());
  for (const auto& item : report.cases) {
    assert(item.equivalence.passed);
    assert(item.reference.metrics.internally_consistent());
    assert(item.optimized.metrics.internally_consistent());
  }
  assert(report.controls.exhaustive.termination_status == "EXHAUSTED_RELATIVE_SPACE");
  assert(report.controls.exhaustive.relative_complete);
  assert(report.controls.exhaustive.accounting_consistent);
  assert(report.controls.exhaustive.raw_constructions == 12);
  assert(report.controls.exhaustive.retained_representatives == 1);
  assert(report.controls.exhaustive.type_invalid == 8);
  assert(report.controls.budgeted.termination_status == "BUDGET_ENDED");
  assert(!report.controls.budgeted.relative_complete);
  assert(report.controls.budgeted.accounting_consistent);
  assert(report.controls.budgeted.resource_pruned == 9);
  assert(report.controls.unknown_budget.termination_status == "INCOMPLETE_UNKNOWN");
  assert(!report.controls.unknown_budget.relative_complete);
  assert(report.controls.unknown_budget.unknown_deferred > 0);

  auto migration = opforge::rich::RichTheoryAdapter{}.migrate(atlas);
  opforge::search24::Layer24Problem problem;
  problem.theory = migration.theory;
  const auto& operation = problem.theory.semantic_theory.operators.begin()->second;
  problem.goal.target_type = opforge::semantic::TypeRef::operator_type(operation.domain, operation.codomain);
  opforge::rich::RichConstraint form;
  form.key = "constructor_form";
  form.value = "composition";
  problem.goal.constraints.push_back(form);
  opforge::rich::RichConstraint property;
  property.key = "property";
  property.value = "linear";
  problem.goal.constraints.push_back(property);

  opforge::search24::Layer24Policy policy;
  policy.max_depth = 1;
  opforge::search24::SearchScalabilityEngine engine;
  const auto first = engine.run(problem, policy);
  const auto second = engine.run(problem, policy);
  assert(first.plan.digest == second.plan.digest);
  assert(first.canonical_solution_set() == second.canonical_solution_set());
  assert(engine.cache_stats().type_hits > 0 || engine.cache_stats().property_hits > 0);

  auto mutated = problem;
  if (!mutated.theory.operator_properties.empty()) mutated.theory.operator_properties.pop_back();
  mutated.theory.refresh_metrics();
  mutated.theory.refresh_id();
  const auto after_mutation = engine.run(mutated, policy);
  assert(after_mutation.plan.theory_digest != first.plan.theory_digest);
  auto isolated = problem;
  isolated.goal.context.id = "different-context";
  isolated.goal.context.refresh_id();
  const auto after_context = engine.run(isolated, policy);
  assert(after_context.plan.context_digest != first.plan.context_digest);

  std::cout << "search scalability tests passed verdict=" << report.verdict
            << " digest=" << report.deterministic_digest << "\n";
  return 0;
}
