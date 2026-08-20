#pragma once

#include "opforge/benchmarks/rediscovery.hpp"
#include "opforge/benchmarks/residual.hpp"
#include "opforge/benchmarks/schema.hpp"
#include "opforge/research/structure_analysis.hpp"
#include "opforge/synthesis/registry.hpp"
#include "opforge/synthesis/schema.hpp"

#include <map>
#include <string>
#include <vector>

namespace opforge::research {

enum class LeadKind { Schema, OperatorFamily, Law, Correction, Closure, Invariant, Bridge };
enum class LeadOutcome { KnownEquivalent, KnownConstruction, TrivialAbstraction, AssumptionArtifact,
                         NumericalArtifact, Invalid, UnderSpecified, Unresolved, StructurallySupported,
                         StronglySupportedDiscoveryLead, SeriousCandidate };
enum class BenchmarkDifficulty { Easy, Medium, Hard };

struct BaselineSnapshot {
  std::string id, atlas_version, digest, construction_registry_digest;
  int operators{0}, spaces{0}, relations{0}, identities{0}, meta_patterns{0}, schemas{0};
  std::vector<std::string> grammar_rules, oracle_configuration, scoring_configuration;
  bool frozen{true};
};

struct LeadDossier {
  std::string id, formal_object, lineage, outcome_reason, numerical_status, counterexample_status;
  LeadKind kind{LeadKind::Schema};
  LeadOutcome outcome{LeadOutcome::Unresolved};
  std::vector<std::string> supporting_patterns, supporting_meta_patterns, assumptions;
  std::vector<std::string> participating_domains, validity_regions, unresolved_dependencies;
  std::vector<std::string> known_equivalents, known_constructions, failed_regimes;
  std::vector<std::string> evidence_history, alternative_explanations, next_experiments;
  std::vector<std::string> outcome_history;
  double compression_gain{0.0}, prediction_power{0.0}, falsification_strength{0.0};
  int counterexample_attempts{0}, numerical_resolutions{0}, representations_checked{0}, rewrites_checked{0};
  bool known_equivalent{false}, trivial{false}, generalized{false};
};

struct BenchmarkV3Result {
  std::string id, benchmark_class, target, outcome, explanation;
  BenchmarkDifficulty difficulty{BenchmarkDifficulty::Easy};
  bool leakage{false}, correction_success{false};
  int exact{0}, semantic{0}, structural{0}, partial{0}, missed{0}, false_positive{0}, misleading{0};
  double precision{0.0}, recall{0.0}, f1{0.0}, false_discovery_rate{0.0};
  std::vector<std::string> hidden_facts, visible_primitives, evidence;
};

struct RediscoveryMetricsV3 {
  int exact{0}, semantic{0}, structural{0}, partial{0}, missed{0}, false_positive{0}, misleading{0}, leakage{0};
  double precision{0.0}, recall{0.0}, f1{0.0}, false_discovery_rate{0.0}, abstraction_accuracy{0.0}, correction_success_rate{0.0};
};

struct StressSummary {
  int leads_tested{0}, oracle_checks{0}, counterexample_attempts{0}, numerical_resolutions{0};
  int representation_checks{0}, rewrite_checks{0}, metamorphic_checks{0}, failed_leads{0};
  double mean_falsification_strength{0.0};
  std::vector<std::string> robustness_findings;
};

struct ActionInformation {
  std::string action_type;
  int count{0};
  double information_gain{0.0};
  std::vector<std::string> reasons;
};

struct MemoryUsefulness {
  int fresh_actions{0}, resumed_actions{0}, repeated_actions{0}, repeated_candidates{0}, repeated_experiments{0};
  double new_information_per_action{0.0};
  std::string conclusion;
};

struct ScientificValidationReport {
  BaselineSnapshot baseline;
  std::vector<LeadDossier> leads;
  std::vector<BenchmarkV3Result> benchmarks;
  RediscoveryMetricsV3 metrics;
  StressSummary stress;
  std::vector<ActionInformation> action_information;
  MemoryUsefulness memory_usefulness;
  std::vector<std::string> competing_hypotheses;
  std::vector<std::string> diagnosis;
  int strong_leads{0}, serious_candidates{0}, eliminated_leads{0}, unresolved_leads{0};
  int geometry_reviewed{0}, geometry_resolved{0}, geometry_unresolved{0};
  std::vector<std::string> under_specified_geometry_review;
};

class ScientificValidator {
public:
  BaselineSnapshot freeze_baseline(const atlas::Atlas&, const synthesis::KnownConstructionRegistry&,
                                   const patterns::MetaPatternReport&, const synthesis::SchemaDiscoveryReport&) const;
  ScientificValidationReport run(const atlas::Atlas&, const BaselineSnapshot&,
                                  const synthesis::SchemaDiscoveryReport&, const StructureAnalysisReport&,
                                  const std::vector<ResidualObject>&, const std::vector<ResidualCluster>&,
                                  const std::vector<std::string>& action_types,
                                  bool numerical_verification_enabled = false) const;
  std::string export_text(const ScientificValidationReport&) const;
};

const char* to_string(LeadKind);
const char* to_string(LeadOutcome);
const char* to_string(BenchmarkDifficulty);

}  // namespace opforge::research
