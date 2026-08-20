#pragma once

#include "opforge/semantic/layer23.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace opforge::search24 {

using rich::RichExpressionPtr;
using rich::RichStatus;
using rich::RichTypeResult;
using semantic::Context;
using semantic::SemanticId;
using semantic::TypeRef;
using semantic::ValidityRegime;

enum class SearchMode { ReferenceExhaustive, OptimizedLazy };
enum class Layer24Termination { ExhaustedRelativeSpace, BudgetEnded, IncompleteUnknown, Failed };

const char* to_string(SearchMode);
const char* to_string(Layer24Termination);

struct Layer24Schema {
  SemanticId id;
  std::string name;
  rich::RichConstructorFamily family{rich::RichConstructorFamily::Composition};
  std::size_t arity{0};
  std::vector<std::string> output_forms;
  std::vector<std::string> guaranteed_properties;
  std::vector<std::string> required_properties;
  std::vector<std::string> side_conditions;
  std::size_t construction_cost{1};
  std::size_t depth_cost{1};
  bool usable_in_reference{true};
  bool usable_in_optimized{true};

  void refresh_id();
  std::string canonical() const;
};

struct Layer24ResourceLimits {
  std::size_t raw_schema_attempts{0};
  std::size_t materialized_expressions{0};
  std::size_t canonical_states{0};
  std::size_t frontier_size{0};
  std::size_t depth{0};
  std::size_t cost{0};
  std::size_t meeting_attempts{0};
  std::size_t unknown_states{0};
  std::size_t time_ms{0};

  std::string canonical() const;
};

struct TheoryIndex {
  std::string theory_digest;
  std::map<std::string, std::vector<SemanticId>> operators_by_domain;
  std::map<std::string, std::vector<SemanticId>> operators_by_codomain;
  std::map<std::string, std::vector<SemanticId>> operators_by_pair;
  std::map<std::string, std::vector<SemanticId>> operators_by_property;
  std::map<std::string, std::vector<SemanticId>> spaces_by_property;
  std::map<std::string, std::vector<SemanticId>> relations_by_key;
  std::map<std::string, std::vector<SemanticId>> indexed_members_by_family;
  std::size_t operator_count{0};
  std::size_t structured_fact_count{0};

  std::string canonical() const;
  std::string digest() const;
};

TheoryIndex build_index(const rich::RichTheory&);

struct Layer24Problem {
  rich::RichTheory theory;
  rich::RichProblem goal;
  std::vector<Layer24Schema> schemas;
  std::string equivalence_theory_id{"layer16-trusted-canonical-v1"};
  std::string contract_id{"layer24-relative-space-v1"};

  std::string canonical() const;
};

struct SearchPlan {
  std::string theory_id;
  std::string theory_version;
  std::string theory_digest;
  std::string context_digest;
  std::string regime_digest;
  std::string target_type;
  std::vector<std::string> required_forms;
  std::vector<std::string> required_properties;
  std::vector<std::string> required_relations;
  std::vector<SemanticId> relevant_operators;
  std::vector<SemanticId> relevant_spaces;
  std::vector<SemanticId> relevant_facts;
  std::vector<Layer24Schema> schemas_considered;
  std::vector<SemanticId> schemas_avoided;
  std::vector<std::string> backward_demands;
  std::vector<std::string> impossible_families;
  TheoryIndex index;
  Layer24ResourceLimits limits;
  std::string equivalence_theory_id;
  std::string digest;

  std::string canonical() const;
};

struct Layer24Policy {
  std::size_t max_depth{1};
  std::size_t max_cost{8};
  Layer24ResourceLimits limits;
  bool retain_unknown{true};
  bool defer_unknown{false};
  bool use_relevance_slice{true};
  bool use_output_demand{true};
  bool use_property_demand{true};
  bool use_indexed_meetings{true};
  bool retain_provenance{true};
  std::uint64_t deterministic_seed{24};

  std::string canonical() const;
};

struct Layer24LedgerRecord {
  std::string candidate_id;
  std::string reason;
  std::string detail;

  std::string canonical() const;
};

struct Layer24Ledger {
  std::map<std::string, std::size_t> counts;
  std::vector<Layer24LedgerRecord> records;
  std::string record_digest;

  void record(const std::string&, const std::string&, const std::string&, bool retain = true);
  std::size_t count(const std::string&) const;
  std::string canonical() const;
};

struct Layer24Candidate {
  SemanticId id;
  SemanticId schema_id;
  std::string family;
  RichExpressionPtr expression;
  RichTypeResult type;
  RichStatus property_status{RichStatus::Satisfied};
  bool form_satisfied{false};
  bool retained{false};
  bool unknown{false};
  std::size_t depth{0};
  std::size_t cost{0};
  std::size_t provenance_paths{1};
  std::vector<rich::RichProofObligation> obligations;

  void refresh_id();
  std::string canonical() const;
};

struct Layer24Metrics {
  std::size_t schema_families_total{0};
  std::size_t schema_families_considered{0};
  std::size_t schema_skipped_by_output_demand{0};
  std::size_t schema_skipped_by_property_demand{0};
  std::size_t operands_total{0};
  std::size_t operands_avoided_by_type_index{0};
  std::size_t operands_avoided_by_property_index{0};
  std::size_t raw_schema_attempts{0};
  std::size_t baseline_attempted{0};
  std::size_t optimized_attempted{0};
  std::size_t materialized_expressions{0};
  std::size_t expression_memo_hits{0};
  std::size_t type_invalid{0};
  std::size_t regime_invalid{0};
  std::size_t property_pruned{0};
  std::size_t index_pruned{0};
  std::size_t substitution_conflicts{0};
  std::size_t canonical_duplicate_merges{0};
  std::size_t certified_equivalence_merges{0};
  std::size_t dominated_states{0};
  std::size_t exact_valid_states{0};
  std::size_t unknown_states{0};
  std::size_t unknown_retained{0};
  std::size_t unknown_deferred{0};
  std::size_t resource_pruned{0};
  std::size_t retained_exact{0};
  std::size_t retained_unknown{0};
  std::size_t peak_frontier{0};
  std::size_t naive_potential_meetings{0};
  std::size_t frontier_meeting_attempts{0};
  std::size_t frontier_meeting_successes{0};
  std::size_t proof_obligations{0};
  std::size_t open_obligations{0};
  std::size_t full_theory_operators{0};
  std::size_t slice_operators{0};
  std::size_t full_theory_facts{0};
  std::size_t slice_facts{0};
  std::size_t cache_type_hits{0};
  std::size_t cache_type_misses{0};
  std::size_t cache_property_hits{0};
  std::size_t cache_property_misses{0};
  double planning_ms{0.0};
  double index_build_ms{0.0};
  double search_ms{0.0};
  double quotient_ms{0.0};
  double proof_plan_ms{0.0};
  std::string termination_status;
  std::string termination_reason;
  bool relative_complete{false};

  bool internally_consistent() const;
  std::string canonical() const;
};

struct Layer24Result {
  Layer24Problem problem;
  Layer24Policy policy;
  SearchPlan plan;
  SearchMode mode{SearchMode::OptimizedLazy};
  Layer24Termination termination{Layer24Termination::Failed};
  Layer24Metrics metrics;
  Layer24Ledger ledger;
  std::vector<Layer24Candidate> candidates;
  std::vector<std::string> retained_ids;
  std::vector<std::string> unknown_ids;
  std::string status_reason;

  bool relative_complete() const { return metrics.relative_complete; }
  std::vector<std::string> canonical_solution_set() const;
  std::vector<std::string> canonical_unknown_set() const;
  std::string canonical() const;
};

struct Layer24CacheStats {
  std::size_t type_hits{0};
  std::size_t type_misses{0};
  std::size_t property_hits{0};
  std::size_t property_misses{0};
  std::size_t applicability_hits{0};
  std::size_t applicability_misses{0};
  std::size_t invalidation_events{0};

  std::string canonical() const;
};

struct ReferenceEquivalenceResult {
  bool passed{false};
  std::vector<std::string> reference_exact;
  std::vector<std::string> optimized_exact;
  std::vector<std::string> reference_unknown;
  std::vector<std::string> optimized_unknown;
  std::string reason;

  std::string canonical() const;
};

class SearchScalabilityEngine {
public:
  SearchPlan compile_plan(const Layer24Problem&, const Layer24Policy&) const;
  Layer24Result run(const Layer24Problem&, const Layer24Policy&, SearchMode = SearchMode::OptimizedLazy) const;
  Layer24CacheStats cache_stats() const;
  void clear_cache() const;

private:
  struct Cache;
  mutable std::shared_ptr<Cache> cache_;
};

struct Layer24BenchmarkCase {
  std::string id;
  std::string category;
  std::string hidden_target;
  std::string expected_expression;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  std::string classification;
  std::string scorer_outcome;
  Layer24Result reference;
  Layer24Result optimized;
  ReferenceEquivalenceResult equivalence;
  std::vector<std::string> reference_search_output;
  std::vector<std::string> optimized_search_output;
  bool target_blind{true};
  bool leakage_free{false};
  bool opaque_id_case{false};

  std::string canonical() const;
};

struct Layer24DistractorPoint {
  std::size_t distractors{0};
  std::size_t full_operators{0};
  std::size_t slice_operators{0};
  std::size_t baseline_attempted{0};
  std::size_t optimized_attempted{0};
  std::size_t baseline_materialized{0};
  std::size_t optimized_materialized{0};
  std::size_t optimized_operand_skips{0};
  std::size_t retained{0};
  std::string termination_status;

  std::string canonical() const;
};

struct Layer24StressResult {
  std::size_t hypothetical_raw{0};
  std::size_t schema_operand_avoided{0};
  std::size_t materialized{0};
  std::size_t canonical_retained{0};
  std::size_t unknown{0};
  std::size_t resource_pruned{0};
  std::size_t peak_state_count{0};
  std::string termination_status;
  bool relative_complete{false};

  std::string canonical() const;
};

struct Layer24LeakageAudit {
  bool passed{false};
  bool target_in_solver{false};
  bool expected_in_solver{false};
  bool benchmark_id_in_solver{false};
  bool operator_name_dependency{false};
  bool partial_fact_pruning{false};
  bool numerical_guidance{false};
  bool runtime_llm{false};
  bool unrestricted_linear_combinations{false};
  bool opaque_id_robust{false};
  std::vector<std::string> notes;

  std::string canonical() const;
};

struct Layer24Determinism {
  std::size_t repetitions{3};
  bool passed{false};
  std::vector<std::string> digests;
  std::string reference_digest;

  std::string canonical() const;
};

struct Layer24ProductionAtlasReport {
  bool actual_production_atlas{false};
  std::string source_label;
  std::string atlas_version;
  std::string atlas_digest;
  std::size_t atlas_operators{0};
  std::size_t atlas_spaces{0};
  std::size_t atlas_relations{0};
  std::size_t atlas_statements{0};
  std::size_t atlas_executable_equalities{0};
  std::size_t atlas_semantic_statements{0};

  rich::RichMigrationReport migration;
  rich::RichTheoryMetrics theory_metrics;
  std::string theory_version;
  std::string theory_digest;

  std::string goal_target_type;
  std::vector<std::string> goal_constraints;
  std::size_t full_theory_operators{0};
  std::size_t full_theory_facts{0};
  std::size_t full_spaces{0};
  std::size_t full_rules{0};
  std::size_t slice_operators{0};
  std::size_t slice_facts{0};
  std::size_t slice_spaces{0};
  std::size_t slice_rules{0};
  std::size_t target_type_dependencies{0};
  std::size_t target_property_dependencies{0};
  std::size_t constructor_dependencies{0};
  std::size_t trusted_rule_dependencies{0};
  std::size_t space_relation_dependencies{0};
  std::size_t context_dependencies{0};
  std::vector<std::string> slice_inclusion_audit;
  std::vector<std::string> exclusion_audit;

  std::string reference_method;
  bool reference_equivalence_attempted{false};
  bool reference_equivalence_passed{false};
  ReferenceEquivalenceResult reference_equivalence;
  std::size_t reference_attempted{0};
  std::size_t reference_materialized{0};
  std::size_t reference_unknown_retained{0};
  std::size_t reference_peak_frontier{0};
  std::string reference_scope;

  Layer24Determinism deterministic_replay;
  bool cache_theory_mutation_detected{false};
  bool context_isolation_valid{false};
  bool regime_isolation_valid{false};
  std::string cache_baseline_plan_digest;
  std::string cache_mutated_plan_digest;
  std::string cache_context_plan_digest;
  std::string cache_regime_plan_digest;
  bool soundness_preserved{true};

  std::string canonical() const;
};

struct Layer24FiniteControl {
  std::size_t raw_constructions{0};
  std::size_t retained_representatives{0};
  std::size_t exact_canonical_merges{0};
  std::size_t proven_equivalent_merges{0};
  std::size_t type_invalid{0};
  std::size_t known_consequences{0};
  std::size_t other_lossless_terminal{0};
  std::size_t unknown_states{0};
  std::size_t unknown_deferred{0};
  std::size_t resource_pruned{0};
  std::size_t engine_raw_attempts{0};
  std::size_t engine_candidate_representatives{0};
  std::string termination_status;
  std::string termination_reason;
  bool relative_complete{false};
  bool accounting_consistent{false};

  std::string canonical() const;
};

struct Layer24ControlReport {
  Layer24FiniteControl exhaustive;
  Layer24FiniteControl budgeted;
  Layer24FiniteControl unknown_budget;

  std::string canonical() const;
};

struct Layer24BenchmarkReport {
  std::vector<Layer24BenchmarkCase> cases;
  std::vector<Layer24DistractorPoint> distractor_scaling;
  Layer24StressResult million_scale;
  Layer24Result controlled_vector_calculus_seed;
  Layer24Result real_atlas;
  Layer24ProductionAtlasReport production_atlas;
  Layer24LeakageAudit leakage;
  Layer24Determinism determinism;
  Layer24ControlReport controls;
  bool historical_regression_passed{false};
  bool open_discovery_unchanged{true};
  bool numerics_zero{true};
  bool runtime_llm_zero{true};
  bool unrestricted_linear_combinations_disabled{true};
  std::vector<std::string> top_bottlenecks;
  std::string verdict;
  std::string deterministic_digest;

  std::string canonical() const;
};

Layer24BenchmarkReport run_layer24_benchmarks(const atlas::Atlas&, const std::string& source_label = "caller-supplied Atlas");
Layer24ControlReport run_layer24_controls();
std::string export_text(const Layer24BenchmarkReport&);
std::string export_json(const Layer24BenchmarkReport&);

}  // namespace opforge::search24
