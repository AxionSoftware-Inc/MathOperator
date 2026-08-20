#pragma once

#include "opforge/atlas/model.hpp"
#include "opforge/verification/layer19.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace opforge::utility {

enum class StructuralClassification { Exact, ValidAlternative, Partial, Miss, FalsePositive };
enum class ProofClassification { CompleteAtRequiredLevel, Partial, Open, Falsified, Unsupported };
enum class SearchClassification {
  ExhaustedRelativeSpace,
  SolutionFoundInIncompleteSearch,
  BudgetEnded,
  IncompleteUnknown,
  UnsupportedLanguage,
  UnderSpecified,
  InvalidProblem
};

const char* to_string(StructuralClassification);
const char* to_string(ProofClassification);
const char* to_string(SearchClassification);

struct UtilitySearchAccounting {
  std::size_t forward_constructions_considered{0};
  std::size_t forward_states{0};
  std::size_t backward_states{0};
  std::size_t quotient_reductions{0};
  std::size_t quotient_lossless_reductions{0};
  std::size_t meetings_attempted{0};
  std::size_t solution_candidates{0};
  std::size_t budget_pruned{0};
  std::size_t unresolved{0};
  std::size_t type_invalid{0};
  std::size_t type_unknown{0};
  std::size_t retained_frontier{0};
  std::string termination_status;
  bool relative_complete{false};
  double search_runtime_ms{0.0};
  double proof_plan_runtime_ms{0.0};
  double verification_runtime_ms{0.0};

  std::string identity_canonical() const;
};

struct UtilityBundleAudit {
  std::string candidate_expression;
  std::string candidate_type;
  std::string candidate_domain;
  std::string candidate_codomain;
  std::string context;
  std::vector<std::string> assumptions;
  std::string validity_regime;
  std::vector<std::string> forward_lineage;
  std::vector<std::string> backward_lineage;
  std::vector<std::string> quotient_provenance;
  std::string proof_plan_id;
  std::size_t total_obligations{0};
  std::size_t exact_discharged{0};
  std::size_t structural_discharged{0};
  std::size_t numerically_supported{0};
  std::size_t unsupported{0};
  std::size_t open{0};
  std::size_t falsified{0};
  bool formal_verification_available{false};
  std::string final_evidence_status;
  verification::NoveltyStatus novelty{verification::NoveltyStatus::NotChecked};
  std::string theory_version;

  std::string identity_canonical() const;
};

struct UtilityCaseResult {
  std::string id;
  std::string tier;
  std::string category;
  std::string hidden_target;
  std::string expected_expression;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  std::string problem_id;
  std::string target_id;
  StructuralClassification structural{StructuralClassification::Miss};
  ProofClassification proof{ProofClassification::Unsupported};
  SearchClassification search{SearchClassification::IncompleteUnknown};
  std::string scorer_outcome;
  std::string notes;
  bool executed{true};
  bool target_blind{true};
  bool leakage_free{true};
  bool opaque_id_case{false};
  std::string search_status_reason;
  std::vector<std::string> candidate_expressions;
  std::vector<verification::ResultBundle> result_bundles;
  std::vector<UtilityBundleAudit> bundle_audits;
  UtilitySearchAccounting accounting;

  std::string identity_canonical() const;
};

struct UtilityOutcomeSummary {
  std::size_t exact{0};
  std::size_t valid_alternative{0};
  std::size_t partial{0};
  std::size_t miss{0};
  std::size_t false_positive{0};
  std::size_t proof_complete{0};
  std::size_t proof_partial{0};
  std::size_t proof_open{0};
  std::size_t proof_unsupported{0};
  std::size_t proof_falsified{0};
  std::size_t search_exhausted{0};
  std::size_t search_budget_ended{0};
  std::size_t search_incomplete_unknown{0};
  std::size_t search_unsupported_language{0};
  std::size_t negative_controls{0};
  std::size_t negative_controls_passed{0};

  std::string identity_canonical() const;
};

struct ForwardDiscoveryResult {
  std::size_t legacy_raw_or_generated{0};
  std::size_t legacy_pruned{0};
  std::size_t legacy_serious_candidates{0};
  std::size_t legacy_numerical_experiments{0};
  std::size_t hidden_fixture_raw{0};
  std::size_t hidden_fixture_lossless_reductions{0};
  std::size_t hidden_fixture_unresolved{0};
  std::size_t hidden_fixture_retained_classes{0};
  std::string hidden_fixture_candidate;
  std::string hidden_fixture_status;
  bool hidden_fixture_reconstructed{false};
  bool relative_complete{false};
  std::string notes;

  std::string identity_canonical() const;
};

struct AtlasDependenceScorecard {
  std::string known_operator_selection;
  std::string held_out_fact_derivation;
  std::string held_out_operator_reconstruction;
  std::string never_named_expression_synthesis;
  std::string cross_space_transfer;
  std::string unsupported_missing_primitive;

  std::string identity_canonical() const;
};

struct ConstructionGrammarCoverage {
  std::vector<std::string> supported;
  std::vector<std::string> not_generated_or_missing;
  bool unrestricted_linear_combinations{false};

  std::string identity_canonical() const;
};

struct LeakageAudit {
  bool benchmark_id_in_solver_input{false};
  bool hidden_operator_id_or_name_in_solver_input{false};
  bool alias_or_description_leakage{false};
  bool expected_expression_in_solver_input{false};
  bool relation_or_metadata_leakage{false};
  bool scorer_data_in_solver_input{false};
  bool target_specific_branch_found{false};
  bool opaque_id_robust{false};
  bool runtime_llm_calls{false};
  std::size_t discovery_numerical_experiments{0};
  std::vector<std::string> audit_notes;

  bool passed() const;
  std::string identity_canonical() const;
};

struct DeterminismAudit {
  std::size_t repetitions{3};
  bool passed{false};
  std::string reference_digest;
  std::vector<std::string> compared_digests;
  std::string notes;
};

struct Layer20BenchmarkReport {
  std::vector<UtilityCaseResult> cases;
  UtilityOutcomeSummary summary;
  ForwardDiscoveryResult forward_discovery;
  AtlasDependenceScorecard atlas_dependence;
  ConstructionGrammarCoverage grammar;
  LeakageAudit leakage;
  DeterminismAudit determinism;
  std::string formal_backend_status{"FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED"};
  std::string practical_utility_verdict;
  std::vector<std::string> top_bottlenecks;
  std::string deterministic_digest;

  std::string identity_canonical() const;
};

Layer20BenchmarkReport run_layer20_benchmarks(const atlas::Atlas&);
std::string export_text(const Layer20BenchmarkReport&);
std::string export_json(const Layer20BenchmarkReport&);

}  // namespace opforge::utility
