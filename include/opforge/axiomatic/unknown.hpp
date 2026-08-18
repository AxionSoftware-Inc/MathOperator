#pragma once

#include "opforge/axiomatic/engine.hpp"

#include <string>
#include <vector>

namespace opforge::axiomatic {

enum class UnknownStatus { Candidate, InducedLawHypothesis, PredictionGenerating,
                           StrongStructuralHypothesis, ExternalCheckCandidate,
                           ExplainedByKnown, Rejected, UnderSpecified };
enum class AxiomIndependence { Independent, Redundant, Unresolved };
enum class PredictionOutcome { Exact, Semantic, Structural, Partial, Miss, FalsePrediction };

struct StructuralResidual {
  std::string id, best_known_structure, classification;
  std::vector<std::string> unexplained_roles, unexplained_identities, violated_axioms;
  std::vector<std::string> extra_relations, missing_relations, assumption_mismatches, source_observations;
  double unexplained_fraction{0.0}, compression_potential{0.0};
};

struct UnknownRole {
  std::string abstract_id, input_type, output_type, object_kind, grade;
  std::vector<std::string> constraints;
};

struct CandidateUnknownAxiom {
  std::string id, shape, statement, provenance;
  std::vector<std::string> source_observations, assumptions;
  AxiomIndependence independence{AxiomIndependence::Unresolved};
  bool used_for_induction{true};
};

struct UnknownStructureHypothesis {
  std::string id, canonical_signature, provenance, status_reason;
  UnknownStatus status{UnknownStatus::Candidate};
  std::vector<UnknownRole> roles;
  std::vector<CandidateUnknownAxiom> candidate_axioms;
  std::vector<std::string> observed_realizations, required_assumptions, supporting_evidence, contradicting_evidence;
  std::vector<std::string> genealogy, alternative_explanations, essential_assumptions, realization_specific_assumptions;
  int explained_facts{0}, prediction_count{0}, validated_predictions{0}, falsified_predictions{0};
  double compression_score{0.0}, prediction_score{0.0}, internal_novelty{0.0}, predictive_power{0.0};
  double generalization_score{0.0}, falsification_strength{0.0}, utility_potential{0.0};
  bool falsification_survived{false}, known_equivalent{false};
};

struct PredictedRealization {
  std::string id, hypothesis_id, target_domain, missing_role, expected_consequence;
  std::vector<UnknownRole> expected_roles, assumptions;
  bool atlas_match{false}, partial_match{false};
};

struct UnknownPrediction {
  std::string id, hypothesis_id, benchmark_id, hidden_target, predicted_conclusion, failure_reason;
  PredictionOutcome outcome{PredictionOutcome::Miss};
  std::vector<std::string> premises, derivation_steps, assumptions;
  bool prospective{true}, serialized_before_reveal{true}, leakage_free{true}, out_of_sample{false};
};

struct UnknownBenchmarkResult {
  std::string id, split, difficulty, manifest_hash, hidden_structure_hash;
  int visible_facts{0}, predictions{0}, exact{0}, semantic{0}, structural{0}, partial{0}, misses{0}, false_predictions{0};
  int hold_one_relation_attempts{0}, hold_one_relation_successes{0};
  int hold_one_realization_attempts{0}, hold_one_realization_successes{0};
  int candidate_axioms{0};
  bool minimal_axiom_set{false}, competing_simpler_rejected{false}, held_out_realization_success{false};
  bool role_renaming_invariant{false}, domain_labels_hidden{true}, contamination_free{true}, near_miss_rejected{false};
  std::vector<UnknownStructureHypothesis> hypotheses;
  std::vector<UnknownPrediction> predictions_detail;
};

struct StressSplitSummary {
  std::string split, manifest_hash;
  int cases{0}, positive_cases{0}, recognized{0}, predictions{0}, successes{0}, false_predictions{0}, near_miss_rejections{0};
};

struct AxiomaticStressReport {
  StressSplitSummary development, validation, held_out;
  int total_cases{0}, total_predictions{0}, total_successes{0};
  std::string held_out_manifest_hash;
};

struct UnknownAblationReport {
  int pattern_only_predictions{0}, pattern_only_successes{0};
  int known_axiomatic_predictions{0}, known_axiomatic_successes{0};
  int unknown_structure_predictions{0}, unknown_structure_successes{0};
  double unknown_prediction_precision{0.0};
  std::string conclusion;
};

struct RealAtlasValidation {
  std::string visible_manifest_hash;
  int visible_facts{0}, withheld_facts{0}, prediction_attempts{0}, successful_predictions{0};
  int out_of_domain_attempts{0}, out_of_domain_successes{0};
  std::vector<std::string> withheld_ids, failure_reasons;
};

struct AtlasExpansionAblation {
  int atlas_a_operators{0}, atlas_b_operators{0}, atlas_a_relations{0}, atlas_b_relations{0};
  int atlas_a_residuals{0}, atlas_b_residuals{0}, atlas_a_unknown_hypotheses{0}, atlas_b_unknown_hypotheses{0};
  int atlas_a_under_specified{0}, atlas_b_under_specified{0};
  std::string conclusion;
};

struct UnknownCampaignConfig {
  int campaigns{5}, max_cycles{7}, max_actions_per_campaign{120}, max_hypotheses{12};
  int max_axiom_candidates{8}, max_derivation_depth{4}, max_validation_experiments{32};
  double max_runtime_ms{30000};
  unsigned seed{47};
};

struct UnknownCampaign {
  std::string id, strategy, stopping_reason;
  int cycles{0}, actions{0}, hypotheses_generated{0}, hypotheses_pruned{0}, hypotheses_rejected{0};
  int predictions{0}, successes{0}, structural_residuals{0}, gaps{0}, stagnation_events{0};
  std::vector<std::string> action_log, decisions;
  std::vector<UnknownStructureHypothesis> hypotheses;
};

struct UnknownDiscoveryReport {
  std::string baseline{"v0.11-unknown-structure-baseline"}, diagnosis, scientific_answer;
  std::vector<StructuralResidual> residuals;
  std::vector<UnknownStructureHypothesis> hypotheses, merged_hypotheses, split_hypotheses;
  std::vector<PredictedRealization> predicted_realizations;
  std::vector<UnknownPrediction> predictions;
  std::vector<UnknownBenchmarkResult> synthetic_benchmarks;
  UnknownBenchmarkResult harder_unseen_benchmark;
  std::vector<UnknownCampaign> campaigns;
  AxiomaticStressReport stress;
  UnknownAblationReport ablation;
  RealAtlasValidation real_validation;
  AtlasExpansionAblation atlas_expansion_ablation;
  int known_structures_recognized{0}, hypotheses_generated{0}, hypotheses_merged{0}, hypotheses_split{0};
  int hypotheses_rejected{0}, strong_structural_hypotheses{0}, external_check_candidates{0};
  int under_specified_total{0}, under_specified_resolved{0}, under_specified_rejected{0};
  int under_specified_unknown_evidence{0}, under_specified_prediction_generating{0};
  int active_hypotheses_peak{0}, axiom_candidates_pruned{0}, derivations_pruned{0}, validation_experiments{0};
  double internal_novelty{0.0}, predictive_power{0.0}, compression{0.0}, generalization{0.0}, falsification_strength{0.0};
  bool ai_enabled{false}, atlas_frozen{true};
};

class UnknownStructureEngine {
public:
  StructuralResidual compute_residual(const atlas::Atlas&, const StructureRecognitionReport&) const;
  UnknownStructureHypothesis induce(const atlas::Atlas&, const StructuralResidual&) const;
  std::vector<UnknownPrediction> predict(const atlas::Atlas&, const UnknownStructureHypothesis&) const;
  AxiomaticStressReport stress_test_known_axioms(const atlas::Atlas&) const;
  UnknownAblationReport ablation(const atlas::Atlas&, const AxiomaticStressReport&, const std::vector<UnknownBenchmarkResult>&) const;
  UnknownDiscoveryReport run(const atlas::Atlas&, const UnknownCampaignConfig& = {}) const;
  std::string export_text(const UnknownDiscoveryReport&) const;
  std::string export_json(const UnknownDiscoveryReport&) const;
};

const char* to_string(UnknownStatus);
const char* to_string(AxiomIndependence);
const char* to_string(PredictionOutcome);

}  // namespace opforge::axiomatic
