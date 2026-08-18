#pragma once

#include "opforge/atlas/geometry.hpp"
#include "opforge/benchmarks/geometry.hpp"
#include "opforge/patterns/meta.hpp"
#include "opforge/research/structure_analysis.hpp"
#include "opforge/synthesis/registry.hpp"
#include "opforge/synthesis/schema.hpp"

#include <string>
#include <vector>

namespace opforge::research {

enum class DeepStrategy { StructureFirst, FailureFirst, SchemaFirst, FalsificationFirst };
enum class DeepLeadStatus { Observation, StructuralLead, SupportedLead, DeepResearchLead, ExternalCheckCandidate, SeriousCandidate, Falsified, KnownEquivalent, UnderSpecified, Stagnant };
enum class LeadClassification { SimpleRecombination, KnownExpressionNewNotation, HigherOrderAbstraction, PredictiveAbstraction, CorrectionGeneralization, NewStructuralHypothesis, Unresolved };

struct DeepBaseline {
  std::string id{"v0.9-deep-discovery-baseline"}, atlas_digest, registry_digest, ontology_digest, numerical_backend{"cpu-reference-v0.8"};
  int operators{0}, spaces{0}, identities{0}, relations{0}, geometry_regimes{0};
  std::vector<std::string> grammar, oracle_configuration, scoring_configuration;
  bool frozen{true};
};
struct LeadEvidence { std::string channel, source, detail; bool independent{false}; };
struct DeepLead {
  std::string id, formal_definition, status_reason, numerical_trust{"not_applicable"}, residual_classification{"unresolved"}, utility_hypothesis, main_weakness;
  DeepLeadStatus status{DeepLeadStatus::Observation}; LeadClassification classification{LeadClassification::Unresolved};
  int depth{0}, stagnation_actions{0}; double score{0};
  std::vector<std::string> genealogy, assumptions, geometry_regimes, boundary_regimes, regularity_regimes, alternative_explanations, next_decisive_tests;
  std::vector<LeadEvidence> evidence;
  std::vector<std::string> counterexample_attempts, prospective_predictions, corrections;
  bool prediction_verified{false}, out_of_sample{false}, independent_convergence{false};
};
struct DeepCampaignReport {
  std::string id; DeepStrategy strategy{DeepStrategy::StructureFirst}; unsigned seed{0}; DeepBaseline baseline;
  std::vector<DeepLead> leads; std::vector<std::string> actions, withheld_facts, decisions; std::string stopping_reason;
  int cycles{0}, actions_executed{0}, experiments{0}, refinements{0}, duplicate_patterns{0}, duplicate_schemas{0}, duplicate_candidates{0}, repeated_experiments{0}, stagnation_events{0};
  int raw_observations{0}, structural_leads{0}, supported_leads{0}, deep_research_leads{0}, external_check_candidates{0}, serious_candidates{0}, falsified_leads{0}, known_equivalent_leads{0}, under_specified_leads{0};
  int prospective_predictions{0}, prediction_successes{0}, out_of_sample_attempts{0}, out_of_sample_recoveries{0}, correction_successes{0};
  double duplicate_rate{0}, stagnation_rate{0}, average_evidence_channels{0}, cpu_runtime_ms{0};
  bool ai_enabled{false}, atlas_frozen{true};
};
struct ConsensusLead { std::string key, formal_definition, consensus_status; int campaigns{0}, independent_lineages{0}; double evidence_overlap{0}; std::vector<std::string> campaign_ids; };
struct DeepDiscoveryReport {
  DeepBaseline baseline; std::vector<DeepCampaignReport> campaigns; std::vector<ConsensusLead> consensus; std::vector<DeepLead> top_dossiers;
  int external_check_candidates{0}, serious_candidates{0}, falsified_leads{0}, under_specified_leads{0};
  double prediction_success_rate{0}, out_of_sample_recovery_rate{0}, correction_success_rate{0}, cross_campaign_recurrence{0}, duplicate_rate{0}, stagnation_rate{0};
  std::string diagnosis, scientific_answer; bool ai_enabled{false}, atlas_frozen{true};
};
struct DeepDiscoveryConfig { int campaigns{4}, max_cycles{8}, max_actions_per_campaign{160}, max_experiments_per_campaign{240}; double max_runtime_ms{30000}; unsigned seed{17}; };

class DeepDiscoveryEngine {
public:
  DeepBaseline freeze_baseline(const atlas::Atlas&) const;
  DeepDiscoveryReport run(const atlas::Atlas&, const DeepDiscoveryConfig& = DeepDiscoveryConfig{}) const;
  std::string export_text(const DeepDiscoveryReport&) const;
  std::string export_json(const DeepDiscoveryReport&) const;
};

const char* to_string(DeepStrategy);
const char* to_string(DeepLeadStatus);
const char* to_string(LeadClassification);

} // namespace opforge::research
