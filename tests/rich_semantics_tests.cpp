#include "opforge/atlas/loader.hpp"
#include "opforge/constraints/layer22.hpp"
#include "opforge/semantic/layer23.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

int main() {
  const auto atlas = opforge::atlas::AtlasLoader::load("atlas");
  const auto report = opforge::rich::run_layer23_benchmarks(atlas);
  assert(report.verdict != "LAYER23_FAILED_DUE_TO_UNSOUNDNESS");
  assert(report.leakage.passed);
  assert(report.leakage.opaque_id_robust);
  assert(report.migration.newly_structured >= 0);
  assert(report.theory_metrics.structured_rule_schemas >= 4);
  const auto migrated = opforge::rich::RichTheoryAdapter{}.migrate(atlas);
  const auto semantic_theory = migrated.theory.as_semantic_theory();
  opforge::semantic::Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  opforge::constraints::SemanticConstraint linear;
  linear.key = "linear";
  linear.kind = opforge::constraints::RequirementKind::StructuredProperty;
  linear.refresh_id();
  const auto first_linear = std::find_if(migrated.theory.operator_properties.begin(), migrated.theory.operator_properties.end(),
                                         [](const auto& fact) { return fact.property == opforge::rich::OperatorProperty::Linear; });
  assert(first_linear != migrated.theory.operator_properties.end());
  const auto bridge = opforge::constraints::PropertyEntailment{}.evaluate(
      semantic_theory, context, opforge::semantic::Expression::operator_reference(first_linear->operator_id), linear);
  assert(bridge.status == opforge::constraints::ConstraintStatus::Satisfied);
  assert(report.cases.size() >= 10);
  assert(std::any_of(report.cases.begin(), report.cases.end(), [](const auto& item) {
    return item.category == "RESTRICTION_SPACE_INCLUSION" && item.classification == "STRUCTURAL_RECOVERY";
  }));
  assert(std::any_of(report.cases.begin(), report.cases.end(), [](const auto& item) {
    return item.category == "TENSOR_OPERATOR_CONSTRUCTION" && item.classification == "STRUCTURAL_RECOVERY";
  }));
  assert(std::any_of(report.cases.begin(), report.cases.end(), [](const auto& item) {
    return item.category == "DUAL_ADJOINT_DISTINCTION" && item.classification != "FALSE_POSITIVE";
  }));
  assert(std::all_of(report.scaling.begin(), report.scaling.end(), [](const auto& point) {
    return point.layer23_attempts >= point.layer23_invalid && point.layer23_peak_frontier >= point.layer23_retained;
  }));
  std::cout << "rich semantic tests passed verdict=" << report.verdict
            << " digest=" << report.deterministic_digest << "\n";
}
