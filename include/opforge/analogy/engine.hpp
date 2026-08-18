#pragma once

#include "opforge/atlas/model.hpp"

#include <map>
#include <string>
#include <vector>

namespace opforge::analogy {

enum class AnalogyStatus { Equivalent, Isomorphic, RealizationOf, AnalogueOf, StructurallySimilar, PartialAnalogy, Misleading, Rejected };
enum class ConstraintStatus { Transferable, SourceSpecific, Unknown, Rejected, Conflict };

struct RoleMapping {
  std::string source_role, target_role;
  double type_score{0.0}, topology_score{0.0}, compatibility{0.0};
  std::vector<std::string> matched_constraints, mismatches;
};

struct StructuralAnalogy {
  std::string id, source_structure, target_structure, provenance;
  AnalogyStatus status{AnalogyStatus::PartialAnalogy};
  std::vector<RoleMapping> role_mapping;
  std::vector<std::string> source_roles, target_roles, mapped_relation_types, shared_axioms;
  std::vector<std::string> source_only_assumptions, target_only_assumptions;
  std::vector<std::string> transferable_constraints, non_transferable_constraints, evidence, competing_mappings;
  double confidence{0.0}, specificity{0.0}, information_gain{0.0};
};

struct TransferredConstraint {
  std::string id, source_law, analogy_id, transferred_condition, provenance;
  std::vector<std::string> mapped_target_roles, required_target_assumptions, rejected_source_assumptions, derivation_trace;
  ConstraintStatus status{ConstraintStatus::Unknown};
  double confidence{0.0};
};

struct TransferResidual {
  std::string id, analogy_id, missing_role, mismatched_assumption, mismatched_relation, target_correction, provenance;
  std::vector<std::string> target_only_requirements, source_evidence;
  double confidence{0.0};
};

struct AnalogicalPrediction {
  std::string id, analogy_id, category, target_fact, status, hash;
  std::vector<std::string> visible_source_facts, source_abstraction, mapped_roles, transferred_assumptions, derivation_trace;
  bool serialized_before_reveal{true}, leakage_free{true}, out_of_domain{false}, success{false};
  double specificity{0.0}, information_gain{0.0};
};

struct AnalogyBenchmarkCase {
  std::string id, family, difficulty, source_domain, target_domain, hidden_fact, miss_reason;
  int visible_roles{0}, possible_target_facts_before{0}, possible_target_facts_after{0};
  bool direct_bridges_hidden{true}, domain_labels_hidden{true}, leakage_free{true};
  bool analogy_valid{false}, negative_transfer_rejected{false}, prediction_attempted{false}, prediction_success{false}, triangulated{false};
  std::vector<StructuralAnalogy> competing_mappings;
  std::vector<AnalogicalPrediction> predictions;
  std::vector<TransferResidual> residuals;
};

struct AnalogyAgenda {
  std::string id, strategy, stopping_reason;
  int candidates{0}, validated{0}, rejected{0}, predictions{0}, successes{0}, actions{0};
  std::vector<std::string> action_log;
};

struct AnalogyAblationSnapshot {
  std::string name;
  int analogy_candidates{0}, validated_analogies{0}, predictions{0}, successes{0}, false_positives{0}, contradictions{0}, under_specified{0};
};

struct AnalogyABCDReport {
  AnalogyAblationSnapshot a, b, c, d;
  std::string conclusion;
};

struct AnalogyReport {
  std::string baseline{"v0.14-structural-analogy-baseline"}, diagnosis, scientific_answer;
  std::vector<StructuralAnalogy> candidates, validated_analogies, partial_analogies, rejected_analogies;
  std::vector<TransferredConstraint> transferred_constraints;
  std::vector<TransferResidual> transfer_residuals;
  std::vector<AnalogicalPrediction> predictions;
  std::vector<AnalogyBenchmarkCase> benchmarks;
  std::vector<AnalogyAgenda> agendas;
  std::vector<std::string> generalized_structural_laws, contradiction_explanations, isolated_link_candidates;
  AnalogyABCDReport abcd;
  int theoretical_mappings{0}, evaluated_mappings{0}, mappings_pruned{0}, negative_transfers_blocked{0};
  int contradiction_candidates{0}, contradictions_explained{0}, isolated_operators{0}, isolated_linked{0};
  int under_specified_total{0}, under_specified_resolved{0};
  bool ai_enabled{false}, atlas_frozen{true};
};

struct AnalogyConfig {
  int max_candidates{48}, max_role_mappings{8}, max_benchmark_cases{12}, max_agendas{5};
  double minimum_compatibility{0.35}, max_runtime_ms{30000};
  unsigned seed{113};
};

class StructuralAnalogyEngine {
public:
  std::vector<StructuralAnalogy> discover(const atlas::Atlas&, const AnalogyConfig& = {}) const;
  std::vector<TransferredConstraint> transfer(const atlas::Atlas&, const StructuralAnalogy&) const;
  std::vector<AnalogicalPrediction> predict(const atlas::Atlas&, const StructuralAnalogy&, const std::vector<TransferredConstraint>&) const;
  AnalogyBenchmarkCase run_negative_transfer_case() const;
  std::vector<AnalogyBenchmarkCase> run_benchmarks(const atlas::Atlas&, const AnalogyConfig& = {}) const;
  AnalogyABCDReport run_abcd_ablation(const atlas::Atlas&, const atlas::Atlas&, const atlas::Atlas&, const atlas::Atlas&, const AnalogyConfig& = {}) const;
  AnalogyReport run(const atlas::Atlas&, const AnalogyConfig& = {}) const;
  std::string export_text(const AnalogyReport&) const;
  std::string export_json(const AnalogyReport&) const;
};

const char* to_string(AnalogyStatus);
const char* to_string(ConstraintStatus);

}  // namespace opforge::analogy
