#include "opforge/search/layer24.hpp"

#include <algorithm>
#include <sstream>

namespace opforge::search24 {
namespace {

std::string token(const std::string& value) { return std::to_string(value.size()) + ":" + value; }

std::string list(const std::string& tag, std::vector<std::string> values, bool sort_values = false) {
  if (sort_values) std::sort(values.begin(), values.end());
  std::ostringstream out;
  out << tag << "[";
  for (const auto& value : values) out << token(value);
  out << "]";
  return out.str();
}

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (const char c : value) {
    switch (c) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << c; break;
    }
  }
  return out.str();
}

}  // namespace

std::string export_text(const Layer24BenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 24 Search Scalability v2 — Constraint-Directed Lazy Mathematical Search\n"
      << "Verdict: " << report.verdict << "\n"
      << "Cases: " << report.cases.size() << " equivalence=" << (std::all_of(report.cases.begin(), report.cases.end(), [](const auto& item) { return item.equivalence.passed; }) ? "PASS" : "FAIL") << "\n"
      << "Leakage: " << (report.leakage.passed ? "PASS" : "FAIL") << " opaque_id=" << (report.leakage.opaque_id_robust ? "PASS" : "FAIL")
      << " numerics=" << (report.numerics_zero ? "0" : "USED") << " runtime_llm=" << (report.runtime_llm_zero ? "0" : "USED") << "\n";
  for (const auto& item : report.cases) {
    out << item.id << " category=" << item.category << " classification=" << item.classification
        << " reference_materialized=" << item.reference.metrics.materialized_expressions
        << " optimized_materialized=" << item.optimized.metrics.materialized_expressions
        << " ref_slice=" << item.reference.metrics.slice_operators << " opt_slice=" << item.optimized.metrics.slice_operators
        << " output_skips=" << item.optimized.metrics.schema_skipped_by_output_demand
        << " operand_skips=" << item.optimized.metrics.operands_avoided_by_type_index
        << " retained=" << item.optimized.metrics.retained_exact << " unknown=" << item.optimized.metrics.unknown_retained
        << " termination=" << item.optimized.metrics.termination_status << " equivalent=" << (item.equivalence.passed ? "yes" : "no")
        << " ref_exact=" << item.equivalence.reference_exact.size() << " opt_exact=" << item.equivalence.optimized_exact.size()
        << " ref_unknown=" << item.equivalence.reference_unknown.size() << " opt_unknown=" << item.equivalence.optimized_unknown.size() << "\n"
        << "  reference_output=" << list("output", item.reference_search_output, true) << "\n"
        << "  optimized_output=" << list("output", item.optimized_search_output, true) << "\n";
  }
  for (const auto& point : report.distractor_scaling)
    out << "Distractors=" << point.distractors << " full_ops=" << point.full_operators << " slice_ops=" << point.slice_operators
        << " baseline_attempted=" << point.baseline_attempted << " optimized_attempted=" << point.optimized_attempted
        << " baseline_materialized=" << point.baseline_materialized << " optimized_materialized=" << point.optimized_materialized
        << " operand_skips=" << point.optimized_operand_skips << " retained=" << point.retained << " status=" << point.termination_status << "\n";
  out << "Million-scale hypothetical_raw=" << report.million_scale.hypothetical_raw << " avoided=" << report.million_scale.schema_operand_avoided
      << " materialized=" << report.million_scale.materialized << " retained=" << report.million_scale.canonical_retained
      << " unknown=" << report.million_scale.unknown << " resource_pruned=" << report.million_scale.resource_pruned
      << " peak=" << report.million_scale.peak_state_count << " status=" << report.million_scale.termination_status << "\n"
      << "Finite exhaustive raw=" << report.controls.exhaustive.raw_constructions << " reps=" << report.controls.exhaustive.retained_representatives
      << " type_invalid=" << report.controls.exhaustive.type_invalid << " merges=" << report.controls.exhaustive.exact_canonical_merges
      << " resource_pruned=" << report.controls.exhaustive.resource_pruned << " status=" << report.controls.exhaustive.termination_status
      << " accounting=" << (report.controls.exhaustive.accounting_consistent ? "PASS" : "FAIL") << "\n"
      << "Finite budget raw=" << report.controls.budgeted.raw_constructions << " reps=" << report.controls.budgeted.retained_representatives
      << " type_invalid=" << report.controls.budgeted.type_invalid << " resource_pruned=" << report.controls.budgeted.resource_pruned
      << " status=" << report.controls.budgeted.termination_status << " relative_complete=" << (report.controls.budgeted.relative_complete ? "true" : "false")
      << " accounting=" << (report.controls.budgeted.accounting_consistent ? "PASS" : "FAIL") << "\n"
      << "UNKNOWN budget raw=" << report.controls.unknown_budget.raw_constructions << " retained=" << report.controls.unknown_budget.retained_representatives
      << " unknown=" << report.controls.unknown_budget.unknown_states << " deferred=" << report.controls.unknown_budget.unknown_deferred
      << " status=" << report.controls.unknown_budget.termination_status
      << " relative_complete=" << (report.controls.unknown_budget.relative_complete ? "true" : "false") << "\n"
      << "Controlled vector-calculus seed full_ops=" << report.controlled_vector_calculus_seed.metrics.full_theory_operators
      << " slice_ops=" << report.controlled_vector_calculus_seed.metrics.slice_operators
      << " full_facts=" << report.controlled_vector_calculus_seed.metrics.full_theory_facts
      << " materialized=" << report.controlled_vector_calculus_seed.metrics.materialized_expressions
      << " retained=" << report.controlled_vector_calculus_seed.metrics.retained_exact
      << " status=" << report.controlled_vector_calculus_seed.metrics.termination_status << "\n"
      << "Full production Atlas source=" << report.production_atlas.source_label
      << " actual=" << (report.production_atlas.actual_production_atlas ? "true" : "false")
      << " atlas_ops=" << report.production_atlas.atlas_operators << " atlas_spaces=" << report.production_atlas.atlas_spaces
      << " atlas_relations=" << report.production_atlas.atlas_relations << " atlas_statements=" << report.production_atlas.atlas_statements
      << " atlas_executable_equalities=" << report.production_atlas.atlas_executable_equalities
      << " atlas_semantic_statements=" << report.production_atlas.atlas_semantic_statements
      << " atlas_digest=" << report.production_atlas.atlas_digest << "\n"
      << "Layer23 full migration before_fully_structured=" << report.production_atlas.migration.pre_layer23_fully_structured
      << " newly_structured=" << report.production_atlas.migration.newly_structured
      << " fully_structured=" << report.production_atlas.migration.fully_structured
      << " partial=" << report.production_atlas.migration.remaining_partial
      << " unsupported=" << report.production_atlas.migration.unsupported
      << " space_properties=" << report.production_atlas.theory_metrics.structured_space_property_facts
      << " space_relations=" << report.production_atlas.theory_metrics.structured_space_relations
      << " operator_properties=" << report.production_atlas.theory_metrics.structured_operator_property_facts
      << " trusted_rules=" << report.production_atlas.theory_metrics.structured_rule_schemas
      << " theory_version=" << report.production_atlas.theory_version
      << " theory_digest=" << report.production_atlas.theory_digest << "\n"
      << "Full Atlas goal target_type=" << report.production_atlas.goal_target_type
      << " constraints=" << list("constraints", report.production_atlas.goal_constraints, true) << "\n"
      << "Full Atlas search full_ops=" << report.real_atlas.metrics.full_theory_operators
      << " full_facts=" << report.real_atlas.metrics.full_theory_facts
      << " slice_ops=" << report.real_atlas.metrics.slice_operators << " slice_facts=" << report.real_atlas.metrics.slice_facts
      << " slice_spaces=" << report.production_atlas.slice_spaces << " slice_rules=" << report.production_atlas.slice_rules
      << " schemas_considered=" << report.real_atlas.metrics.schema_families_considered
      << " schemas_skipped_output=" << report.real_atlas.metrics.schema_skipped_by_output_demand
      << " schemas_skipped_property=" << report.real_atlas.metrics.schema_skipped_by_property_demand
      << " reference_attempted=" << report.production_atlas.reference_attempted
      << " reference_materialized=" << report.production_atlas.reference_materialized
      << " optimized_attempted=" << report.real_atlas.metrics.optimized_attempted
      << " materialized=" << report.real_atlas.metrics.materialized_expressions
      << " canonical_retained=" << report.real_atlas.metrics.retained_exact + report.real_atlas.metrics.unknown_retained
      << " exact_retained=" << report.real_atlas.metrics.retained_exact
      << " unknown_retained=" << report.real_atlas.metrics.unknown_retained
      << " peak_frontier=" << report.real_atlas.metrics.peak_frontier
      << " meeting_attempts=" << report.real_atlas.metrics.frontier_meeting_attempts
      << " termination=" << report.real_atlas.metrics.termination_status
      << " relative_complete=" << (report.real_atlas.metrics.relative_complete ? "true" : "false") << "\n"
      << "Full Atlas reference method=" << report.production_atlas.reference_method
      << " passed=" << (report.production_atlas.reference_equivalence_passed ? "true" : "false")
      << " ref_exact=" << report.production_atlas.reference_equivalence.reference_exact.size()
      << " opt_exact=" << report.production_atlas.reference_equivalence.optimized_exact.size()
      << " ref_unknown=" << report.production_atlas.reference_equivalence.reference_unknown.size()
      << " opt_unknown=" << report.production_atlas.reference_equivalence.optimized_unknown.size() << "\n"
      << "Full Atlas relevance target_type=" << report.production_atlas.target_type_dependencies
      << " target_property=" << report.production_atlas.target_property_dependencies
      << " constructor=" << report.production_atlas.constructor_dependencies
      << " trusted_rule=" << report.production_atlas.trusted_rule_dependencies
      << " space_relation=" << report.production_atlas.space_relation_dependencies
      << " context=" << report.production_atlas.context_dependencies << "\n"
      << "Full Atlas replay=" << (report.production_atlas.deterministic_replay.passed ? "PASS" : "FAIL")
      << " digest=" << report.production_atlas.deterministic_replay.reference_digest << "\n"
      << "Full Atlas cache theory_mutation=" << (report.production_atlas.cache_theory_mutation_detected ? "PASS" : "FAIL")
      << " context_isolation=" << (report.production_atlas.context_isolation_valid ? "PASS" : "FAIL")
      << " regime_isolation=" << (report.production_atlas.regime_isolation_valid ? "PASS" : "FAIL") << "\n"
      << "Real Atlas full_ops=" << report.real_atlas.metrics.full_theory_operators << " slice_ops=" << report.real_atlas.metrics.slice_operators
      << " materialized=" << report.real_atlas.metrics.materialized_expressions << " retained=" << report.real_atlas.metrics.retained_exact
      << " unknown=" << report.real_atlas.metrics.unknown_retained << " status=" << report.real_atlas.metrics.termination_status << "\n"
      << "Determinism: " << (report.determinism.passed ? "PASS" : "FAIL") << " digest=" << report.deterministic_digest << "\n";
  out << "Slice inclusion audit:\n";
  for (const auto& entry : report.production_atlas.slice_inclusion_audit) out << "  " << entry << "\n";
  out << "Slice exclusion audit:\n";
  for (const auto& entry : report.production_atlas.exclusion_audit) out << "  " << entry << "\n";
  return out.str();
}

std::string export_json(const Layer24BenchmarkReport& report) {
  std::ostringstream out;
  const auto string_array = [&](const std::vector<std::string>& values) {
    std::ostringstream value;
    value << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) value << ",";
      value << "\"" << json_escape(values[i]) << "\"";
    }
    value << "]";
    return value.str();
  };
  out << "{\"verdict\":\"" << json_escape(report.verdict) << "\",\"deterministic_digest\":\"" << json_escape(report.deterministic_digest)
      << "\",\"determinism\":{\"repetitions\":" << report.determinism.repetitions << ",\"passed\":" << (report.determinism.passed ? "true" : "false") << ",\"digests\":[";
  for (std::size_t i = 0; i < report.determinism.digests.size(); ++i) { if (i) out << ","; out << "\"" << json_escape(report.determinism.digests[i]) << "\""; }
  out << "]},\"leakage\":{\"passed\":" << (report.leakage.passed ? "true" : "false") << ",\"opaque_id_robust\":" << (report.leakage.opaque_id_robust ? "true" : "false")
      << ",\"target_in_solver\":" << (report.leakage.target_in_solver ? "true" : "false") << ",\"expected_in_solver\":" << (report.leakage.expected_in_solver ? "true" : "false")
      << ",\"benchmark_id_in_solver\":" << (report.leakage.benchmark_id_in_solver ? "true" : "false") << ",\"operator_name_dependency\":" << (report.leakage.operator_name_dependency ? "true" : "false")
      << ",\"numerical_guidance\":" << (report.leakage.numerical_guidance ? "true" : "false") << ",\"runtime_llm\":" << (report.leakage.runtime_llm ? "true" : "false") << "},\"cases\":[";
  for (std::size_t i = 0; i < report.cases.size(); ++i) {
    if (i) out << ",";
    const auto& item = report.cases[i];
    out << "{\"id\":\"" << json_escape(item.id) << "\",\"category\":\"" << json_escape(item.category) << "\",\"classification\":\"" << json_escape(item.classification)
        << "\",\"target_blind\":" << (item.target_blind ? "true" : "false") << ",\"leakage_free\":" << (item.leakage_free ? "true" : "false")
        << ",\"opaque_id\":" << (item.opaque_id_case ? "true" : "false") << ",\"reference_attempted\":" << item.reference.metrics.raw_schema_attempts
        << ",\"optimized_attempted\":" << item.optimized.metrics.raw_schema_attempts << ",\"reference_materialized\":" << item.reference.metrics.materialized_expressions
        << ",\"optimized_materialized\":" << item.optimized.metrics.materialized_expressions << ",\"schema_output_skips\":" << item.optimized.metrics.schema_skipped_by_output_demand
        << ",\"operand_type_skips\":" << item.optimized.metrics.operands_avoided_by_type_index << ",\"retained_exact\":" << item.optimized.metrics.retained_exact
        << ",\"retained_unknown\":" << item.optimized.metrics.unknown_retained << ",\"termination\":\"" << item.optimized.metrics.termination_status
        << "\",\"equivalence\":" << (item.equivalence.passed ? "true" : "false")
        << ",\"reference_output\":[";
    for (std::size_t j = 0; j < item.reference_search_output.size(); ++j) {
      if (j) out << ",";
      out << "\"" << json_escape(item.reference_search_output[j]) << "\"";
    }
    out << "],\"optimized_output\":[";
    for (std::size_t j = 0; j < item.optimized_search_output.size(); ++j) {
      if (j) out << ",";
      out << "\"" << json_escape(item.optimized_search_output[j]) << "\"";
    }
    out << "]}";
  }
  out << "],\"distractor_scaling\":[";
  for (std::size_t i = 0; i < report.distractor_scaling.size(); ++i) {
    if (i) out << ",";
    const auto& point = report.distractor_scaling[i];
    out << "{\"distractors\":" << point.distractors << ",\"full_operators\":" << point.full_operators << ",\"slice_operators\":" << point.slice_operators
        << ",\"baseline_attempted\":" << point.baseline_attempted << ",\"optimized_attempted\":" << point.optimized_attempted
        << ",\"baseline_materialized\":" << point.baseline_materialized << ",\"optimized_materialized\":" << point.optimized_materialized
        << ",\"optimized_operand_skips\":" << point.optimized_operand_skips << ",\"retained\":" << point.retained << ",\"termination\":\"" << json_escape(point.termination_status) << "\"}";
  }
  out << "],\"million_scale\":{\"hypothetical_raw\":" << report.million_scale.hypothetical_raw << ",\"schema_operand_avoided\":" << report.million_scale.schema_operand_avoided
      << ",\"materialized\":" << report.million_scale.materialized << ",\"canonical_retained\":" << report.million_scale.canonical_retained << ",\"unknown\":" << report.million_scale.unknown
      << ",\"resource_pruned\":" << report.million_scale.resource_pruned << ",\"peak_state_count\":" << report.million_scale.peak_state_count
      << ",\"termination\":\"" << json_escape(report.million_scale.termination_status) << "\",\"relative_complete\":" << (report.million_scale.relative_complete ? "true" : "false") << "}"
      << ",\"controlled_vector_calculus_seed\":{\"full_operators\":" << report.controlled_vector_calculus_seed.metrics.full_theory_operators
      << ",\"slice_operators\":" << report.controlled_vector_calculus_seed.metrics.slice_operators
      << ",\"full_facts\":" << report.controlled_vector_calculus_seed.metrics.full_theory_facts
      << ",\"materialized\":" << report.controlled_vector_calculus_seed.metrics.materialized_expressions
      << ",\"retained\":" << report.controlled_vector_calculus_seed.metrics.retained_exact
      << ",\"termination\":\"" << json_escape(report.controlled_vector_calculus_seed.metrics.termination_status) << "\"}"
      << ",\"real_atlas\":{\"full_operators\":" << report.real_atlas.metrics.full_theory_operators << ",\"slice_operators\":" << report.real_atlas.metrics.slice_operators
      << ",\"full_facts\":" << report.real_atlas.metrics.full_theory_facts << ",\"slice_facts\":" << report.real_atlas.metrics.slice_facts
      << ",\"materialized\":" << report.real_atlas.metrics.materialized_expressions << ",\"retained\":" << report.real_atlas.metrics.retained_exact << ",\"unknown\":" << report.real_atlas.metrics.unknown_retained
      << ",\"termination\":\"" << json_escape(report.real_atlas.metrics.termination_status) << "\"}"
      << ",\"production_atlas\":{\"actual_production_atlas\":" << (report.production_atlas.actual_production_atlas ? "true" : "false")
      << ",\"source_label\":\"" << json_escape(report.production_atlas.source_label)
      << "\",\"atlas_version\":\"" << json_escape(report.production_atlas.atlas_version)
      << "\",\"atlas_digest\":\"" << json_escape(report.production_atlas.atlas_digest)
      << "\",\"atlas_counts\":{\"operators\":" << report.production_atlas.atlas_operators
      << ",\"spaces\":" << report.production_atlas.atlas_spaces << ",\"relations\":" << report.production_atlas.atlas_relations
      << ",\"statements\":" << report.production_atlas.atlas_statements
      << ",\"executable_equalities\":" << report.production_atlas.atlas_executable_equalities
      << ",\"semantic_statements\":" << report.production_atlas.atlas_semantic_statements << "}"
      << ",\"layer23_migration\":{\"before_fully_structured\":" << report.production_atlas.migration.pre_layer23_fully_structured
      << ",\"newly_structured\":" << report.production_atlas.migration.newly_structured
      << ",\"fully_structured\":" << report.production_atlas.migration.fully_structured
      << ",\"partial\":" << report.production_atlas.migration.remaining_partial
      << ",\"unsupported\":" << report.production_atlas.migration.unsupported
      << ",\"atlas_facts_before_layer23\":" << report.production_atlas.migration.atlas_facts_before_layer23 << "}"
      << ",\"layer23_theory\":{\"spaces\":" << report.production_atlas.theory_metrics.spaces_total
      << ",\"space_properties\":" << report.production_atlas.theory_metrics.structured_space_property_facts
      << ",\"space_relations\":" << report.production_atlas.theory_metrics.structured_space_relations
      << ",\"operator_properties\":" << report.production_atlas.theory_metrics.structured_operator_property_facts
      << ",\"trusted_rules\":" << report.production_atlas.theory_metrics.structured_rule_schemas
      << ",\"fully_structured\":" << report.production_atlas.theory_metrics.fully_structured_facts
      << ",\"partial\":" << report.production_atlas.migration.remaining_partial << "}"
      << ",\"theory_version\":\"" << json_escape(report.production_atlas.theory_version)
      << "\",\"theory_digest\":\"" << json_escape(report.production_atlas.theory_digest)
      << "\",\"goal_target_type\":\"" << json_escape(report.production_atlas.goal_target_type)
      << "\",\"goal_constraints\":" << string_array(report.production_atlas.goal_constraints)
      << ",\"full_theory\":{\"operators\":" << report.production_atlas.full_theory_operators
      << ",\"facts\":" << report.production_atlas.full_theory_facts << ",\"spaces\":" << report.production_atlas.full_spaces
      << ",\"rules\":" << report.production_atlas.full_rules << "}"
      << ",\"relevance_slice\":{\"operators\":" << report.production_atlas.slice_operators
      << ",\"facts\":" << report.production_atlas.slice_facts << ",\"spaces\":" << report.production_atlas.slice_spaces
      << ",\"rules\":" << report.production_atlas.slice_rules
      << ",\"target_type_dependencies\":" << report.production_atlas.target_type_dependencies
      << ",\"target_property_dependencies\":" << report.production_atlas.target_property_dependencies
      << ",\"constructor_dependencies\":" << report.production_atlas.constructor_dependencies
      << ",\"trusted_rule_dependencies\":" << report.production_atlas.trusted_rule_dependencies
      << ",\"space_relation_dependencies\":" << report.production_atlas.space_relation_dependencies
      << ",\"context_dependencies\":" << report.production_atlas.context_dependencies
      << ",\"inclusion_audit\":" << string_array(report.production_atlas.slice_inclusion_audit)
      << ",\"exclusion_audit\":" << string_array(report.production_atlas.exclusion_audit) << "}"
      << ",\"search\":{\"schemas_considered\":" << report.real_atlas.metrics.schema_families_considered
      << ",\"schemas_skipped_by_output_demand\":" << report.real_atlas.metrics.schema_skipped_by_output_demand
      << ",\"schemas_skipped_by_property_demand\":" << report.real_atlas.metrics.schema_skipped_by_property_demand
      << ",\"reference_attempted\":" << report.production_atlas.reference_attempted
      << ",\"reference_materialized\":" << report.production_atlas.reference_materialized
      << ",\"reference_peak_frontier\":" << report.production_atlas.reference_peak_frontier
      << ",\"optimized_attempted\":" << report.real_atlas.metrics.optimized_attempted
      << ",\"materialized\":" << report.real_atlas.metrics.materialized_expressions
      << ",\"canonical_retained\":" << report.real_atlas.metrics.retained_exact + report.real_atlas.metrics.unknown_retained
      << ",\"exact_retained\":" << report.real_atlas.metrics.retained_exact
      << ",\"unknown_retained\":" << report.real_atlas.metrics.unknown_retained
      << ",\"peak_frontier\":" << report.real_atlas.metrics.peak_frontier
      << ",\"meeting_attempts\":" << report.real_atlas.metrics.frontier_meeting_attempts
      << ",\"termination\":\"" << json_escape(report.real_atlas.metrics.termination_status)
      << "\",\"relative_complete\":" << (report.real_atlas.metrics.relative_complete ? "true" : "false") << "}"
      << ",\"reference\":{\"method\":\"" << json_escape(report.production_atlas.reference_method)
      << "\",\"attempted\":" << (report.production_atlas.reference_equivalence_attempted ? "true" : "false")
      << ",\"passed\":" << (report.production_atlas.reference_equivalence_passed ? "true" : "false")
      << ",\"scope\":\"" << json_escape(report.production_atlas.reference_scope)
      << "\",\"reference_exact\":" << string_array(report.production_atlas.reference_equivalence.reference_exact)
      << ",\"optimized_exact\":" << string_array(report.production_atlas.reference_equivalence.optimized_exact)
      << ",\"reference_unknown\":" << string_array(report.production_atlas.reference_equivalence.reference_unknown)
      << ",\"optimized_unknown\":" << string_array(report.production_atlas.reference_equivalence.optimized_unknown) << "}"
      << ",\"deterministic_replay\":{\"repetitions\":" << report.production_atlas.deterministic_replay.repetitions
      << ",\"passed\":" << (report.production_atlas.deterministic_replay.passed ? "true" : "false")
      << ",\"reference_digest\":\"" << json_escape(report.production_atlas.deterministic_replay.reference_digest)
      << "\",\"digests\":" << string_array(report.production_atlas.deterministic_replay.digests) << "}"
      << ",\"cache\":{\"theory_mutation_detected\":" << (report.production_atlas.cache_theory_mutation_detected ? "true" : "false")
      << ",\"context_isolation_valid\":" << (report.production_atlas.context_isolation_valid ? "true" : "false")
      << ",\"regime_isolation_valid\":" << (report.production_atlas.regime_isolation_valid ? "true" : "false")
      << ",\"baseline_plan_digest\":\"" << json_escape(report.production_atlas.cache_baseline_plan_digest)
      << "\",\"mutated_plan_digest\":\"" << json_escape(report.production_atlas.cache_mutated_plan_digest)
      << "\",\"context_plan_digest\":\"" << json_escape(report.production_atlas.cache_context_plan_digest)
      << "\",\"regime_plan_digest\":\"" << json_escape(report.production_atlas.cache_regime_plan_digest) << "}" 
      << ",\"soundness_preserved\":" << (report.production_atlas.soundness_preserved ? "true" : "false") << "},\"controls\":";
  const auto control_json = [&](const Layer24FiniteControl& control) {
    out << "{\"raw_constructions\":" << control.raw_constructions
        << ",\"retained_representatives\":" << control.retained_representatives
        << ",\"exact_canonical_merges\":" << control.exact_canonical_merges
        << ",\"proven_equivalent_merges\":" << control.proven_equivalent_merges
        << ",\"type_invalid\":" << control.type_invalid
        << ",\"known_consequences\":" << control.known_consequences
        << ",\"other_lossless_terminal\":" << control.other_lossless_terminal
        << ",\"unknown_states\":" << control.unknown_states
        << ",\"unknown_deferred\":" << control.unknown_deferred
        << ",\"resource_pruned\":" << control.resource_pruned
        << ",\"engine_raw_attempts\":" << control.engine_raw_attempts
        << ",\"engine_candidate_representatives\":" << control.engine_candidate_representatives
        << ",\"termination\":\"" << json_escape(control.termination_status)
        << "\",\"termination_reason\":\"" << json_escape(control.termination_reason)
        << "\",\"relative_complete\":" << (control.relative_complete ? "true" : "false")
        << ",\"accounting_consistent\":" << (control.accounting_consistent ? "true" : "false") << "}";
  };
  out << "{\"exhaustive\":";
  control_json(report.controls.exhaustive);
  out << ",\"budgeted\":";
  control_json(report.controls.budgeted);
  out << ",\"unknown_budget\":";
  control_json(report.controls.unknown_budget);
  out << "},\"historical_regression_passed\":" << (report.historical_regression_passed ? "true" : "false")
      << ",\"open_discovery_unchanged\":" << (report.open_discovery_unchanged ? "true" : "false") << ",\"numerics_zero\":" << (report.numerics_zero ? "true" : "false")
      << ",\"runtime_llm_zero\":" << (report.runtime_llm_zero ? "true" : "false") << ",\"unrestricted_linear_combinations_disabled\":" << (report.unrestricted_linear_combinations_disabled ? "true" : "false") << "}";
  return out.str();
}

}  // namespace opforge::search24
