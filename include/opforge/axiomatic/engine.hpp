#pragma once

#include "opforge/atlas/model.hpp"

#include <string>
#include <vector>

namespace opforge::axiomatic {

enum class StructureKind {
  VectorSpace, InnerProductSpace, Algebra, AssociativeAlgebra, LieAlgebra,
  GradedAlgebra, ChainComplex, CochainComplex, DifferentialComplex,
  ProjectionIdempotent, AdjointPair, Decomposition, ExactSequence,
  TransformDuality
};

enum class StructureStatus { Rejected, Partial, Supported, DerivedHypothesis };
enum class ConsequenceClass { ExplicitlyStored, TriviallyRestated, StructurallyDerivable,
                              ProspectivelyPredicted, OutOfSampleVerified, Unresolved };

struct StructuralRole {
  std::string id, input_space, output_space, object_kind, grade;
  std::vector<std::string> constraints;
};

struct StructuralAxiom {
  std::string id, statement, scope, status;
  std::vector<std::string> assumptions;
  std::string provenance, implementation_notes;
};

struct MathematicalStructure {
  std::string id, name, provenance;
  StructureKind kind{StructureKind::VectorSpace};
  std::vector<StructuralRole> roles;
  std::vector<StructuralAxiom> axioms;
  std::vector<std::string> required_assumptions, valid_domains, realizations, known_consequences;
};

struct StructureEvidence {
  std::string candidate_id, structure_id, status_reason;
  StructureStatus status{StructureStatus::Partial};
  std::vector<std::string> matched_axioms, missing_axioms, contradictory_evidence;
  std::vector<std::string> participating_realizations, assumptions, essential_assumptions;
  std::vector<std::string> realization_specific_assumptions, alternate_explanations;
  double confidence{0.0}, compression_gain{0.0};
};

struct DerivationNode {
  std::string id, conclusion, structure_id, axiom_id, substitution, status{"structurally_derived"};
  std::vector<std::string> premises, assumptions;
  bool circular{false}, leaked{false};
};

struct DerivationDAG {
  std::vector<DerivationNode> nodes;
  int roots{0}, leaves{0};
  bool acyclic{true}, leakage_free{true};
};

struct StructuralPrediction {
  std::string id, benchmark_id, hidden_target, predicted_conclusion, target_key, reason;
  ConsequenceClass classification{ConsequenceClass::Unresolved};
  std::vector<std::string> premises, assumptions, derivation_nodes;
  bool structure_recognized{false}, valid{false}, leakage_free{true}, out_of_sample{false};
};

struct StructuralGap {
  std::string id, structure_id, missing_role, expected_consequence, justification;
  std::vector<std::string> assumptions, evidence;
  bool synthesis_allowed{false};
};

struct StructureRecognitionReport {
  std::vector<MathematicalStructure> library;
  std::vector<StructureEvidence> recognized, partial, rejected;
  int false_structures_rejected{0};
};

struct DeductionReport {
  DerivationDAG dag;
  std::vector<StructuralPrediction> predictions;
  std::vector<StructuralGap> gaps;
  int explained_facts{0}, generated_predictions{0}, validated_predictions{0}, falsified_predictions{0}, unresolved_predictions{0};
};

struct PredictiveBenchmark {
  std::string id, family, difficulty, hidden_fact, miss_reason;
  int visible_facts{0};
  bool structure_recognized{false}, prediction_attempted{false}, prediction_success{false};
  bool out_of_sample_verified{false}, false_structure_rejected{false}, leakage_free{true};
  std::vector<StructuralPrediction> predictions;
};

struct PredictiveBenchmarkReport {
  std::vector<PredictiveBenchmark> benchmarks;
  int structures_recognized{0}, predictions{0}, successful_predictions{0}, out_of_sample_recoveries{0};
  int false_structures_rejected{0}, failed_predictions{0};
};

struct AblationReport {
  int benchmarks{0};
  int pattern_only_predictions{0}, pattern_only_successes{0};
  int axiomatic_predictions{0}, axiomatic_successes{0};
  double pattern_only_precision{0.0}, axiomatic_precision{0.0};
  std::string conclusion;
};

struct AxiomaticCampaignConfig {
  int campaigns{4}, max_cycles{6}, max_actions_per_campaign{120};
  double max_runtime_ms{30000};
  unsigned seed{31};
};

struct AxiomaticCampaign {
  std::string id, strategy, stopping_reason;
  int cycles{0}, actions{0}, recognized{0}, partial{0}, rejected{0}, predictions{0}, successes{0};
  int structural_gaps{0}, upgraded_leads{0}, still_under_specified{0};
  std::vector<std::string> action_log, decisions;
  std::vector<StructureEvidence> structures;
  std::vector<StructuralPrediction> predictions_detail;
};

struct AxiomaticReport {
  std::string baseline{"v0.10-axiomatic-predictive-baseline"}, diagnosis, scientific_answer;
  std::vector<MathematicalStructure> structures;
  StructureRecognitionReport recognition;
  DeductionReport deduction;
  PredictiveBenchmarkReport benchmarks;
  AblationReport ablation;
  std::vector<AxiomaticCampaign> campaigns;
  int axioms_instantiated{0}, under_specified_total{0}, under_specified_resolved{0}, under_specified_upgraded{0};
  bool ai_enabled{false}, atlas_frozen{true};
};

class StructureLibrary {
public:
  std::vector<MathematicalStructure> initial() const;
};

class AxiomaticEngine {
public:
  StructureRecognitionReport recognize(const atlas::Atlas&) const;
  DeductionReport derive(const atlas::Atlas&, const StructureRecognitionReport&) const;
  PredictiveBenchmarkReport run_predictive_benchmarks(const atlas::Atlas&) const;
  AblationReport run_ablation(const atlas::Atlas&, const PredictiveBenchmarkReport&) const;
  AxiomaticReport run(const atlas::Atlas&, const AxiomaticCampaignConfig& = {}) const;
  std::string export_text(const AxiomaticReport&) const;
  std::string export_json(const AxiomaticReport&) const;
};

const char* to_string(StructureKind);
const char* to_string(StructureStatus);
const char* to_string(ConsequenceClass);

}  // namespace opforge::axiomatic
