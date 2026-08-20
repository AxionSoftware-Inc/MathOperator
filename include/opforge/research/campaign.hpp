#pragma once
#include "opforge/benchmarks/residual.hpp"
#include "opforge/benchmarks/schema.hpp"
#include "opforge/benchmarks/geometry.hpp"
#include "opforge/numerics/executor.hpp"
#include "opforge/research/evaluation.hpp"
#include "opforge/research/oracles.hpp"
#include "opforge/synthesis/goal.hpp"
#include "opforge/synthesis/schema.hpp"
#include "opforge/research/structure_analysis.hpp"
#include "opforge/research/scientific.hpp"
#include "opforge/research/deep.hpp"
#include "opforge/axiomatic/engine.hpp"
#include "opforge/axiomatic/unknown.hpp"
#include "opforge/synthesis/candidate.hpp"
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

namespace opforge::research {
enum class CampaignMode { Rediscovery, BlindRediscovery, StructuralExploration, FailureDriven, SchemaDiscovery, LeadFalsification, ProblemDriven, DeepOpenDiscovery, AxiomaticOpenDiscovery, UnknownStructureDiscovery };
enum class ActionType { InspectPattern, GeneralizePattern, InspectGap, InspectMetaPattern, PredictMissingRole, InduceSchema, TestSchema, CompleteSchema, SearchClosure, HypothesizeInvariant, TestCommutator, MapValidityRegion, GeneralizeViaCorrection, AnalyzeResidual, SynthesizeCorrection, TestGeneralizedIdentity, SynthesizeCandidate, EvaluateCandidate, DeepenCandidate, SearchCounterexample, RunBenchmark, ComparePatterns, RevisitInconclusive, InspectGeometry, TestBoundary, TestRegularity, SearchDecisiveCounterexample };
struct ResearchAction { std::string id, target, evidence_target; ActionType type{ActionType::InspectPattern}; double expected_value{0}, cost{0}, priority{0}; std::vector<std::string> dependencies; };
struct CampaignBudget { int max_cycles{10}, max_actions{100}, max_experiments{500}; double max_runtime_ms{10000}; };
struct CampaignConfig {
  CampaignMode mode{CampaignMode::StructuralExploration};
  CampaignBudget budget{};
  std::string campaign_id{"C-open-001"}, atlas_snapshot{"v0.14-structural-analogy-baseline"}, target{"none"};
  bool ai_enabled{false}, freeze_atlas{true};
  // Search is structural-only by default. Numeric work is an explicit proof
  // stage and is still gated by ready_for_numerical_verification().
  bool enable_numerical_verification{false};
  bool run_numeric_diagnostics{false};
  patterns::PatternBudget pattern_budget{};
  size_t max_candidate_leads{256};
};
struct CampaignState {
  std::string id, version{"0.1"}, atlas_snapshot;
  CampaignMode mode{CampaignMode::StructuralExploration};
  int cycle{0}, actions_executed{0};
  std::vector<std::string> active_queue;
  double budget_used{0};
  bool stopped{false};
  bool search_contract_initialized{false}, numerical_verification_enabled{false}, numeric_diagnostics_enabled{false};
  size_t max_candidate_leads{256};
  patterns::PatternBudget pattern_budget{};
};
struct ResearchMemory { std::string version{"0.1"}; std::vector<std::string> discovered_patterns, abstractions, gaps, generated_candidates, rejected_candidates, counterexamples, experiments, reports, decisions; std::vector<std::string> failed_constructions, successful_constructions, benchmark_history, false_interest_cases, unresolved_questions; bool has(const std::string& hash) const; void remember(std::vector<std::string>& bucket,const std::string& hash); bool save(const std::string& path) const; static ResearchMemory load(const std::string& path); };
struct CandidateDossier { synthesis::OperatorCandidate candidate; EvaluationReport evaluation; std::vector<OracleResult> oracle_results; std::string category, expected_value, weakness, next_test; };
struct CampaignReport { CampaignState state; ResearchMemory memory; std::vector<EvaluationReport> evaluations; std::vector<ResearchAction> actions; std::vector<std::string> rediscovered, unresolved, failed_directions, cross_domain_patterns, structural_gaps, false_interest_cases, lead_audits; synthesis::CandidateReport candidates; std::vector<synthesis::OperatorCandidate> surviving_candidates, serious_candidates; std::vector<CandidateDossier> top_candidate_dossiers; std::vector<FailurePattern> failure_patterns; std::vector<patterns::MetaPattern> meta_patterns; std::vector<patterns::PatternPrediction> predicted_roles; std::vector<ResidualObject> residual_objects; std::vector<ResidualCluster> residual_cluster_details; std::vector<synthesis::GoalDirectedCandidate> correction_candidates; synthesis::SchemaDiscoveryReport schema_discovery; StructureAnalysisReport structure_analysis; benchmarks::ResidualDrivenBenchmarkReport residual_benchmark; benchmarks::SchemaCompressionBenchmarkReport schema_benchmark; benchmarks::GeometryBenchmarkReport geometry_benchmark; ScientificValidationReport scientific; DeepDiscoveryReport deep_discovery; axiomatic::AxiomaticReport axiomatic_discovery; axiomatic::UnknownDiscoveryReport unknown_discovery; std::string atlas_snapshot, target{"none"}, epistemic_status; bool ai_enabled{false},atlas_frozen{true}, geometry_diagnostics_ran{false}; int numerical_experiments{0}, counterexamples_found{0}, known_equivalent{0}, known_constructions{0}, residual_clusters{0}, correction_attempts{0}, successful_repairs{0}, pruned_candidates{0}; };
class ResearchOrchestrator {
public:
  CampaignReport run(const atlas::Atlas&, CampaignMode, const CampaignBudget&, ResearchMemory memory={}, CampaignState state={}, const std::string& checkpoint="") const;
  CampaignReport run(const atlas::Atlas&, const CampaignConfig&, ResearchMemory memory={}, CampaignState state={}, const std::string& checkpoint="") const;
  bool save_checkpoint(const std::string& path,const CampaignState&,const ResearchMemory&) const;
  std::pair<CampaignState,ResearchMemory> load_checkpoint(const std::string& path) const;
  std::string report_text(const CampaignReport&) const;
  std::string report_json(const CampaignReport&) const;
};
const char* to_string(CampaignMode); const char* to_string(ActionType);
}
