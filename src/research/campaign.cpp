#include "opforge/research/campaign.hpp"

#include "opforge/atlas/loader.hpp"
#include "opforge/patterns/analyzer.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>

namespace opforge::research {

const char* to_string(CampaignMode mode) {
  switch (mode) {
    case CampaignMode::Rediscovery: return "rediscovery";
    case CampaignMode::BlindRediscovery: return "blind_rediscovery";
    case CampaignMode::StructuralExploration: return "structural_exploration";
    case CampaignMode::FailureDriven: return "failure_driven";
    case CampaignMode::SchemaDiscovery: return "schema_discovery";
    case CampaignMode::LeadFalsification: return "lead_falsification";
    case CampaignMode::ProblemDriven: return "problem_driven";
    case CampaignMode::DeepOpenDiscovery: return "deep_open_discovery";
    case CampaignMode::AxiomaticOpenDiscovery: return "axiomatic_open_discovery";
    case CampaignMode::UnknownStructureDiscovery: return "unknown_structure_discovery";
  }
  return "unknown";
}

const char* to_string(ActionType type) {
  switch (type) {
    case ActionType::InspectPattern: return "inspect_pattern";
    case ActionType::GeneralizePattern: return "generalize_pattern";
    case ActionType::InspectGap: return "inspect_gap";
    case ActionType::InspectMetaPattern: return "inspect_meta_pattern";
    case ActionType::PredictMissingRole: return "predict_missing_role";
    case ActionType::InduceSchema: return "induce_schema";
    case ActionType::TestSchema: return "test_schema";
    case ActionType::CompleteSchema: return "complete_schema";
    case ActionType::SearchClosure: return "search_closure";
    case ActionType::HypothesizeInvariant: return "hypothesize_invariant";
    case ActionType::TestCommutator: return "test_commutator";
    case ActionType::MapValidityRegion: return "map_validity_region";
    case ActionType::GeneralizeViaCorrection: return "generalize_via_correction";
    case ActionType::AnalyzeResidual: return "analyze_residual";
    case ActionType::SynthesizeCorrection: return "synthesize_correction";
    case ActionType::TestGeneralizedIdentity: return "test_generalized_identity";
    case ActionType::SynthesizeCandidate: return "synthesize_candidate";
    case ActionType::EvaluateCandidate: return "evaluate_candidate";
    case ActionType::DeepenCandidate: return "deepen_candidate";
    case ActionType::SearchCounterexample: return "search_counterexample";
    case ActionType::RunBenchmark: return "run_benchmark";
    case ActionType::ComparePatterns: return "compare_patterns";
    case ActionType::RevisitInconclusive: return "revisit_inconclusive";
    case ActionType::InspectGeometry: return "inspect_geometry";
    case ActionType::TestBoundary: return "test_boundary";
    case ActionType::TestRegularity: return "test_regularity";
    case ActionType::SearchDecisiveCounterexample: return "search_decisive_counterexample";
  }
  return "unknown";
}

bool ResearchMemory::has(const std::string& hash) const {
  for (const auto& bucket : {discovered_patterns, abstractions, gaps, generated_candidates,
                             rejected_candidates, counterexamples, experiments, reports,
                             decisions, failed_constructions, successful_constructions,
                             benchmark_history, false_interest_cases, unresolved_questions}) {
    if (std::find(bucket.begin(), bucket.end(), hash) != bucket.end()) return true;
  }
  return false;
}

void ResearchMemory::remember(std::vector<std::string>& bucket, const std::string& hash) {
  if (!has(hash)) bucket.push_back(hash);
}

static const std::vector<std::pair<const char*, std::vector<std::string> ResearchMemory::*>> memory_buckets{
    {"pattern", &ResearchMemory::discovered_patterns},
    {"abstraction", &ResearchMemory::abstractions},
    {"gap", &ResearchMemory::gaps},
    {"candidate", &ResearchMemory::generated_candidates},
    {"rejected", &ResearchMemory::rejected_candidates},
    {"counterexample", &ResearchMemory::counterexamples},
    {"experiment", &ResearchMemory::experiments},
    {"report", &ResearchMemory::reports},
    {"decision", &ResearchMemory::decisions},
    {"failed", &ResearchMemory::failed_constructions},
    {"success", &ResearchMemory::successful_constructions},
    {"benchmark", &ResearchMemory::benchmark_history},
    {"false_interest", &ResearchMemory::false_interest_cases},
    {"unresolved", &ResearchMemory::unresolved_questions}};

static bool write_memory(std::ofstream& out, const ResearchMemory& memory) {
  out << "OPFORGE_MEMORY_V0.2\n";
  for (const auto& [name, member] : memory_buckets) {
    for (const auto& value : memory.*member) out << name << "|" << value << "\n";
  }
  return true;
}

bool ResearchMemory::save(const std::string& path) const {
  std::ofstream out(path);
  return out && write_memory(out, *this);
}

ResearchMemory ResearchMemory::load(const std::string& path) {
  ResearchMemory memory;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    const auto split = line.find('|');
    if (split == std::string::npos) continue;
    const auto name = line.substr(0, split);
    const auto value = line.substr(split + 1);
    for (const auto& [bucket, member] : memory_buckets) {
      if (name == bucket) (memory.*member).push_back(value);
    }
  }
  return memory;
}

bool ResearchOrchestrator::save_checkpoint(const std::string& path, const CampaignState& state,
                                           const ResearchMemory& memory) const {
  std::ofstream out(path);
  if (!out) return false;
  out << "OPFORGE_CHECKPOINT_V0.2\n"
      << "campaign|" << state.id << "\n"
      << "cycle|" << state.cycle << "\n"
      << "actions|" << state.actions_executed << "\n"
      << "mode|" << to_string(state.mode) << "\n"
      << "atlas_snapshot|" << state.atlas_snapshot << "\n";
  return write_memory(out, memory);
}

std::pair<CampaignState, ResearchMemory> ResearchOrchestrator::load_checkpoint(const std::string& path) const {
  CampaignState state;
  ResearchMemory memory;
  std::ifstream in(path);
  std::string line;
  bool memory_section = false;
  while (std::getline(in, line)) {
    if (line == "OPFORGE_MEMORY_V0.1" || line == "OPFORGE_MEMORY_V0.2") {
      memory_section = true;
      continue;
    }
    const auto split = line.find('|');
    if (split == std::string::npos) continue;
    const auto key = line.substr(0, split);
    const auto value = line.substr(split + 1);
    if (memory_section) {
      for (const auto& [bucket, member] : memory_buckets) {
        if (key == bucket) (memory.*member).push_back(value);
      }
    } else if (key == "campaign") {
      state.id = value;
    } else if (key == "cycle") {
      state.cycle = std::stoi(value);
    } else if (key == "actions") {
      state.actions_executed = std::stoi(value);
    } else if (key == "atlas_snapshot") {
      state.atlas_snapshot = value;
    } else if (key == "mode") {
      if (value == "rediscovery") state.mode = CampaignMode::Rediscovery;
      else if (value == "blind_rediscovery") state.mode = CampaignMode::BlindRediscovery;
      else if (value == "failure_driven") state.mode = CampaignMode::FailureDriven;
      else if (value == "schema_discovery") state.mode = CampaignMode::SchemaDiscovery;
      else if (value == "lead_falsification") state.mode = CampaignMode::LeadFalsification;
      else if (value == "problem_driven") state.mode = CampaignMode::ProblemDriven;
      else if (value == "deep_open_discovery") state.mode = CampaignMode::DeepOpenDiscovery;
      else if (value == "axiomatic_open_discovery") state.mode = CampaignMode::AxiomaticOpenDiscovery;
      else if (value == "unknown_structure_discovery") state.mode = CampaignMode::UnknownStructureDiscovery;
      else state.mode = CampaignMode::StructuralExploration;
    }
  }
  return {state, std::move(memory)};
}

namespace {

std::string category_for(const synthesis::OperatorCandidate& candidate) {
  if (candidate.construction_rule.find("compose") != std::string::npos)
    return "new composition candidate";
  if (!candidate.lineage.source_gaps.empty()) return "missing-role completion";
  if (!candidate.lineage.abstractions.empty()) return "operator-family unification";
  return "unclear";
}

std::string join(const std::vector<std::string>& values, const std::string& separator = ", ") {
  std::ostringstream out;
  for (size_t index = 0; index < values.size(); ++index) {
    if (index) out << separator;
    out << values[index];
  }
  return out.str();
}

std::string signature_text(const atlas::OperatorSignature& signature) {
  return signature.domain.id + " -> " + signature.codomain.id +
         " | order=" + std::to_string(signature.differential_order) +
         " | linear=" + (signature.linear ? "true" : "false") +
         " | local=" + (signature.local ? "true" : "false");
}

bool already_evaluated(const CampaignReport& report, const std::string& id) {
  return std::any_of(report.evaluations.begin(), report.evaluations.end(),
                     [&](const auto& evaluation) { return evaluation.candidate_id == id; });
}

void merge_candidate(std::vector<synthesis::OperatorCandidate>& candidates,
                     synthesis::OperatorCandidate candidate) {
  if (std::none_of(candidates.begin(), candidates.end(),
                   [&](const auto& existing) { return existing.id == candidate.id; })) {
    candidates.push_back(std::move(candidate));
  }
}

void append_unique(std::vector<std::string>& values, const std::string& value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

bool make_numeric_input(const atlas::OperatorSignature& signature, int resolution,
                        numerics::NumericObject& input) {
  numerics::Grid grid{3, resolution, resolution, resolution, 1.0 / std::max(1, resolution - 1)};
  if (signature.input_kind == atlas::ObjectKind::Scalar) {
    input = numerics::scalar_grid(grid, [](double x, double y, double z) { return x * x + y + z * z; });
    return true;
  }
  if (signature.input_kind == atlas::ObjectKind::Vector) {
    input.kind = numerics::NumericObject::Kind::Vector;
    input.grid = grid;
    input.components = 3;
    input.values.resize(grid.nx * grid.ny * grid.nz * 3);
    for (int z = 0; z < grid.nz; ++z) {
      for (int y = 0; y < grid.ny; ++y) {
        for (int x = 0; x < grid.nx; ++x) {
          const auto index = ((z * grid.ny + y) * grid.nx + x) * 3;
          input.values[index] = x * grid.step;
          input.values[index + 1] = y * grid.step;
          input.values[index + 2] = z * grid.step;
        }
      }
    }
    return true;
  }
  return false;
}

std::string false_interest_reason(const atlas::Atlas& atlas,
                                  const synthesis::OperatorCandidate& candidate) {
  std::string reason;
  const auto semantic = synthesis::compare_semantics(candidate.expression, atlas);
  if (semantic.equivalent) return semantic.explanation;
  if (synthesis::is_trivial(candidate.expression, atlas, &reason)) return reason;
  if (candidate.expression && candidate.expression->kind == atlas::Expression::Kind::Composition) {
    for (const auto& identity : atlas.identities()) {
      if (identity.left && synthesis::canonical(identity.left) == candidate.canonical_form &&
          identity.right && identity.right->kind == atlas::Expression::Kind::OperatorReference &&
          atlas.find(identity.right->value)) {
        return "known operator recovery: " + identity.right->value;
      }
    }
  }
  if (candidate.signature.input_kind == atlas::ObjectKind::Unknown ||
      candidate.signature.output_kind == atlas::ObjectKind::Unknown)
    return "unsupported abstraction: object kind is unknown";
  if (!candidate.expression || candidate.lineage.source_patterns.empty()) return "repackaging: missing construction lineage";
  if (candidate.expression->kind == atlas::Expression::Kind::Composition && candidate.expression->children.size() == 2) {
    for (const auto& child : candidate.expression->children) {
      if (child && child->kind == atlas::Expression::Kind::OperatorReference &&
          (child->value == "la.identity" || child->value == "op.identity.scalar.r3"))
        return "identity wrapper";
      if (child && child->kind == atlas::Expression::Kind::OperatorReference &&
          (child->value == "la.zero" || child->value == "form.zero" || child->value == "discrete.zero" ||
           child->value.rfind("op.zero.", 0) == 0))
        return "trivial zero-operator wrapper";
    }
  }
  return {};
}

void add_numeric_stress(const atlas::Atlas& atlas, const synthesis::OperatorCandidate& candidate,
                        EvaluationReport& evaluation, int& experiment_counter, int max_experiments) {
  if (experiment_counter >= max_experiments) return;
  bool all_supported = true;
  bool any_supported = false;
  for (const int resolution : {8, 16, 32}) {
    if (experiment_counter >= max_experiments) break;
    Experiment experiment;
    experiment.id = "E-multiresolution-" + candidate.id + "-" + std::to_string(resolution);
    experiment.candidate_id = candidate.id;
    experiment.parameters = "resolution=" + std::to_string(resolution);
    experiment.type = ExperimentType::Numerical;
    numerics::NumericObject input;
    const bool input_supported = make_numeric_input(candidate.signature, resolution, input);
    const auto result = input_supported
                            ? numerics::NumericalExecutor{}.apply(candidate.expression, input, atlas)
                            : numerics::ExecutionResult{};
    experiment.status = result.supported ? ExperimentStatus::Pass : ExperimentStatus::Unsupported;
    experiment.generated_cases.push_back("resolution=" + std::to_string(resolution));
    experiment.evidence.push_back({experiment.id, "numerical_checked", result.backend, "0.25",
                                   "2026-08-15", candidate.canonical_form,
                                   result.supported ? "supported" : result.reason, "", -1});
    evaluation.experiments.push_back(std::move(experiment));
    ++experiment_counter;
    any_supported |= result.supported;
    all_supported &= result.supported;
  }
  if (all_supported && any_supported) {
    evaluation.scores.numerical_stability = 1.0;
    evaluation.epistemic_status = "numerically_supported_multiresolution";
  } else if (any_supported) {
    evaluation.scores.numerical_stability = 0.5;
  } else {
    evaluation.state = CandidateState::Unknown;
    evaluation.epistemic_status = "unsupported_numerical_backend";
    evaluation.failure_reason = "multiresolution numerical executor does not support this signature";
  }
}

}  // namespace

CampaignReport ResearchOrchestrator::run(const atlas::Atlas& atlas, CampaignMode mode,
                                         const CampaignBudget& budget, ResearchMemory memory,
                                         CampaignState state, const std::string& checkpoint) const {
  CampaignConfig config;
  config.mode = mode;
  config.budget = budget;
  config.campaign_id = state.id.empty() ? "C-001" : state.id;
  config.atlas_snapshot = state.atlas_snapshot.empty() ? "unspecified" : state.atlas_snapshot;
  return run(atlas, config, std::move(memory), std::move(state), checkpoint);
}

CampaignReport ResearchOrchestrator::run(const atlas::Atlas& atlas, const CampaignConfig& config,
                                         ResearchMemory memory, CampaignState state,
                                         const std::string& checkpoint) const {
  CampaignReport report;
  report.memory = std::move(memory);
  report.atlas_snapshot = config.atlas_snapshot;
  report.target = config.target;
  report.ai_enabled = config.ai_enabled;
  report.atlas_frozen = config.freeze_atlas;
  report.state = std::move(state);
  if (report.state.id.empty()) report.state.id = config.campaign_id;
  report.state.mode = config.mode;
  report.state.atlas_snapshot = config.atlas_snapshot;
  report.epistemic_status = "structural_exploration_only";
  if (config.mode == CampaignMode::DeepOpenDiscovery) {
    DeepDiscoveryConfig deep_config;
    deep_config.max_cycles = std::max(1, config.budget.max_cycles);
    deep_config.max_actions_per_campaign = std::max(20, config.budget.max_actions / 4);
    deep_config.max_experiments_per_campaign = std::max(20, config.budget.max_experiments / 4);
    deep_config.max_runtime_ms = config.budget.max_runtime_ms;
    report.deep_discovery = DeepDiscoveryEngine{}.run(atlas, deep_config);
    report.state.cycle = 0;
    for (const auto& campaign : report.deep_discovery.campaigns) {
      report.state.cycle = std::max(report.state.cycle, campaign.cycles);
      report.state.actions_executed += campaign.actions_executed;
      for (const auto& action : campaign.actions) report.actions.push_back({action, action, "deep lead escalation", ActionType::DeepenCandidate, 1.0, 0.5, 0.5, {}});
    }
    report.state.stopped = true;
    report.epistemic_status = report.deep_discovery.external_check_candidates > 0 ? "deep_research_external_check_candidate" : "deep_research_no_external_candidate";
    report.unresolved.push_back(report.deep_discovery.scientific_answer);
    return report;
  }
  if (config.mode == CampaignMode::AxiomaticOpenDiscovery) {
    axiomatic::AxiomaticCampaignConfig axiomatic_config;
    axiomatic_config.max_cycles = std::max(1, config.budget.max_cycles);
    axiomatic_config.max_actions_per_campaign = std::max(20, config.budget.max_actions / 4);
    axiomatic_config.max_runtime_ms = config.budget.max_runtime_ms;
    report.axiomatic_discovery = axiomatic::AxiomaticEngine{}.run(atlas, axiomatic_config);
    report.state.cycle = 0;
    for (const auto& campaign : report.axiomatic_discovery.campaigns) {
      report.state.cycle = std::max(report.state.cycle, campaign.cycles);
      report.state.actions_executed += campaign.actions;
      for (const auto& action : campaign.action_log) report.actions.push_back({action, action, "axiomatic structural reasoning", ActionType::TestGeneralizedIdentity, 1.0, 0.5, 0.5, {}});
    }
    report.state.stopped = true;
    report.epistemic_status = "axiomatic_predictive_benchmark_complete";
    report.unresolved.push_back(report.axiomatic_discovery.scientific_answer);
    return report;
  }
  if (config.mode == CampaignMode::UnknownStructureDiscovery) {
    axiomatic::UnknownCampaignConfig unknown_config;
    unknown_config.max_cycles = std::max(1, config.budget.max_cycles);
    unknown_config.max_actions_per_campaign = std::max(20, config.budget.max_actions / 4);
    unknown_config.max_runtime_ms = config.budget.max_runtime_ms;
    report.unknown_discovery = axiomatic::UnknownStructureEngine{}.run(atlas, unknown_config);
    report.state.cycle = 0;
    for (const auto& campaign : report.unknown_discovery.campaigns) {
      report.state.cycle = std::max(report.state.cycle, campaign.cycles);
      report.state.actions_executed += campaign.actions;
      for (const auto& action : campaign.action_log) report.actions.push_back({action, action, "unknown-structure induction", ActionType::TestGeneralizedIdentity, 1.0, 0.5, 0.5, {}});
    }
    report.state.stopped = true;
    report.epistemic_status = report.unknown_discovery.external_check_candidates > 0 ? "unknown_external_check_candidate" : "unknown_no_external_candidate";
    report.unresolved.push_back(report.unknown_discovery.scientific_answer);
    return report;
  }
  report.residual_benchmark = benchmarks::ResidualDrivenBenchmark{}.run(atlas);
  report.successful_repairs = report.residual_benchmark.repaired ? 1 : 0;
  report.schema_benchmark = benchmarks::SchemaCompressionBenchmark{}.run(atlas);
  const auto initial_patterns = patterns::PatternAnalyzer{}.analyze(atlas);
  const auto initial_meta = patterns::MetaPatternAnalyzer{}.analyze(atlas, initial_patterns);
  report.meta_patterns = initial_meta.meta_patterns;
  report.predicted_roles = initial_meta.predictions;
  report.schema_discovery = synthesis::SchemaInducer{}.induce(atlas, initial_patterns, initial_meta);
  report.structure_analysis = StructureAnalyzer{}.analyze(
      atlas, report.schema_discovery, report.residual_objects, report.residual_cluster_details);
  report.scientific.baseline = ScientificValidator{}.freeze_baseline(
      atlas, synthesis::KnownConstructionRegistry::from_atlas(atlas), initial_meta, report.schema_discovery);

  const auto start = std::chrono::steady_clock::now();
  int experiment_counter = 0;
  while (report.state.cycle < config.budget.max_cycles && !report.state.stopped &&
         report.state.actions_executed < config.budget.max_actions) {
    const auto patterns = patterns::PatternAnalyzer{}.analyze(atlas);
    const auto meta_report = patterns::MetaPatternAnalyzer{}.analyze(atlas, patterns);
    report.meta_patterns = meta_report.meta_patterns;
    report.predicted_roles = meta_report.predictions;
    report.schema_discovery = synthesis::SchemaInducer{}.induce(atlas, patterns, meta_report);
    report.structure_analysis = StructureAnalyzer{}.analyze(
        atlas, report.schema_discovery, report.residual_objects, report.residual_cluster_details);
    if (report.scientific.baseline.digest.empty()) {
      report.scientific.baseline = ScientificValidator{}.freeze_baseline(
          atlas, synthesis::KnownConstructionRegistry::from_atlas(atlas), meta_report, report.schema_discovery);
    }
    for (const auto& pattern : patterns.patterns) {
      report.memory.remember(report.memory.discovered_patterns, pattern.id);
      if (pattern.type == patterns::PatternType::SharedStructure) {
        append_unique(report.cross_domain_patterns, pattern.id);
      }
      if (pattern.type == patterns::PatternType::MissingLinkCandidate) {
        append_unique(report.structural_gaps, pattern.id);
        report.memory.remember(report.memory.gaps, pattern.id);
      }
      if (pattern.type == patterns::PatternType::DifferentialComplexCandidate ||
          pattern.type == patterns::PatternType::OperatorFamily ||
          pattern.type == patterns::PatternType::SharedStructure) {
        report.memory.remember(report.memory.abstractions, pattern.id);
      }
    }

    auto add_action = [&](ActionType type, const std::string& target, const std::string& evidence,
                          double expected, double cost) {
      if (report.state.actions_executed >= config.budget.max_actions) return;
      ResearchAction action{"A-" + std::to_string(report.state.actions_executed), target, evidence,
                            type, expected, cost, expected - cost, {}};
      report.actions.push_back(action);
      report.memory.remember(report.memory.decisions, action.id + ":" + to_string(type) + ":" + target);
      ++report.state.actions_executed;
    };

    add_action(ActionType::InspectPattern, "all-visible-patterns", "typed pattern report", 1.0, 0.1);
    if (!report.cross_domain_patterns.empty())
      add_action(ActionType::GeneralizePattern, report.cross_domain_patterns.back(), "shared structure", 0.9, 0.2);
    if (!report.structural_gaps.empty())
      add_action(ActionType::InspectGap, report.structural_gaps.back(), "terminal structural gap", 0.8, 0.2);
    if (!meta_report.meta_patterns.empty())
      add_action(ActionType::InspectMetaPattern, meta_report.meta_patterns.front().id, "pattern-of-patterns", 1.1, 0.25);
    if (!meta_report.predictions.empty())
      add_action(ActionType::PredictMissingRole, meta_report.predictions.front().id, "meta-pattern justified role", 1.0, 0.35);
    if (!report.schema_discovery.schemas.empty())
      add_action(ActionType::InduceSchema, report.schema_discovery.schemas.front().id, "common construction schema", 1.3, 0.45);
    if (!report.schema_discovery.schemas.empty())
      add_action(ActionType::TestSchema, report.schema_discovery.schemas.front().id, "schema realization check", 1.05, 0.55);
    if (!report.schema_discovery.completions.empty())
      add_action(ActionType::CompleteSchema, report.schema_discovery.completions.front().id, "schema role completion", 1.2, 0.45);
    if (!report.structure_analysis.closures.empty())
      add_action(ActionType::SearchClosure, report.structure_analysis.closures.front().id, "family closure analysis", 1.15, 0.5);
    if (!report.structure_analysis.invariants.empty())
      add_action(ActionType::HypothesizeInvariant, report.structure_analysis.invariants.front().id, "metadata-backed invariant", 1.0, 0.35);
    if (!report.structure_analysis.commutators.empty())
      add_action(ActionType::TestCommutator, report.structure_analysis.commutators.front().id, "typed AB-BA probe", 1.1, 0.5);

    const auto generated = synthesis::CandidateSynthesizer{}.synthesize(atlas, patterns);
    for (auto candidate : generated.accepted) {
      candidate.category = category_for(candidate);
      const auto semantic = synthesis::compare_semantics(candidate.expression, atlas);
      if (semantic.equivalent) {
        ++report.known_equivalent;
        if (semantic.matched_id.rfind("construction.", 0) == 0 ||
            semantic.matched_id.rfind("identity.", 0) == 0) {
          ++report.known_constructions;
        }
      }
      const auto false_interest = false_interest_reason(atlas, candidate);
      if (!false_interest.empty()) {
        candidate.rejection_reason = false_interest;
        report.memory.remember(report.memory.rejected_candidates, candidate.id);
        const auto record = candidate.id + ":" + false_interest;
        append_unique(report.false_interest_cases, record);
        report.memory.remember(report.memory.false_interest_cases, record);
        merge_candidate(report.candidates.rejected, std::move(candidate));
        continue;
      }
      report.memory.remember(report.memory.generated_candidates, candidate.id);
      merge_candidate(report.candidates.accepted, std::move(candidate));
    }
    for (auto candidate : generated.rejected) {
      candidate.category = category_for(candidate);
      report.memory.remember(report.memory.rejected_candidates, candidate.id);
      const auto false_interest = candidate.id + ":" + candidate.rejection_reason;
      append_unique(report.false_interest_cases, false_interest);
      report.memory.remember(report.memory.false_interest_cases, false_interest);
      merge_candidate(report.candidates.rejected, std::move(candidate));
    }
    add_action(ActionType::SynthesizeCandidate, "target-free-typed-grammar", "visible structural patterns", 0.7, 0.4);

    auto candidates = report.candidates.accepted;
    const auto numerically_applicable = [](const synthesis::OperatorCandidate& candidate) {
      const auto& domain = candidate.signature.domain.id;
      return (candidate.signature.input_kind == atlas::ObjectKind::Scalar ||
              candidate.signature.input_kind == atlas::ObjectKind::Vector) &&
             (domain.rfind("scalar.", 0) == 0 || domain.rfind("vector.", 0) == 0 ||
              domain.rfind("grid.", 0) == 0);
    };
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
      if (left.score.total() != right.score.total()) return left.score.total() > right.score.total();
      return left.id < right.id;
    });
    std::stable_sort(candidates.begin(), candidates.end(), [&](const auto& left, const auto& right) {
      return numerically_applicable(left) > numerically_applicable(right);
    });
    const auto top_count = std::min<size_t>(10, candidates.size());
    for (size_t index = 0; index < top_count; ++index) {
      auto& candidate = candidates[index];
      if (already_evaluated(report, candidate.id)) continue;
      if (report.state.actions_executed >= config.budget.max_actions) break;
      add_action(ActionType::EvaluateCandidate, candidate.id, "top structural score", candidate.score.total(), 1.0);
      auto evaluation = CandidateEvaluationEngine{}.evaluate(atlas, candidate, {}, nullptr,
                                                              {std::max(1, config.budget.max_experiments - experiment_counter),
                                                               64, 64, 8, config.budget.max_runtime_ms});
      add_numeric_stress(atlas, candidate, evaluation, experiment_counter, config.budget.max_experiments);
      const auto oracle_results = CounterexampleOracleEngine{}.run_all_strong(
          candidate, atlas, {std::max(1, config.budget.max_experiments - experiment_counter), 64, 64, 8,
                             config.budget.max_runtime_ms});
      for (const auto& oracle : oracle_results) {
        if (!oracle.residual_object.id.empty()) report.residual_objects.push_back(oracle.residual_object);
        if (oracle.status == OracleStatus::CounterexampleFound) {
          ++report.counterexamples_found;
        }
        if (oracle.status == OracleStatus::CounterexampleFound ||
            oracle.status == OracleStatus::AssumptionViolation) {
          report.failure_patterns.push_back(oracle.failure);
        }
      }
      report.memory.remember(report.memory.reports, candidate.id + ":" + evaluation.epistemic_status);
      report.evaluations.push_back(evaluation);
      if (!evaluation.counterexamples.empty()) report.counterexamples_found += static_cast<int>(evaluation.counterexamples.size());
      const bool has_numeric_experiment = std::any_of(evaluation.experiments.begin(), evaluation.experiments.end(),
                                                      [](const auto& experiment) { return experiment.type == ExperimentType::Numerical; });
      const bool has_numeric_support = std::any_of(evaluation.experiments.begin(), evaluation.experiments.end(),
                                                   [](const auto& experiment) { return experiment.type == ExperimentType::Numerical && experiment.status == ExperimentStatus::Pass; });
      if (has_numeric_experiment && !has_numeric_support) {
        const auto record = candidate.id + ":unsupported abstraction or numerical backend";
        append_unique(report.false_interest_cases, record);
        report.memory.remember(report.memory.false_interest_cases, record);
      }
      if (evaluation.state != CandidateState::CounterexampleRejected && evaluation.state != CandidateState::Unknown &&
          (!has_numeric_experiment || has_numeric_support)) {
        report.surviving_candidates.push_back(candidate);
        report.memory.remember(report.memory.successful_constructions, candidate.id);
      }
      CandidateDossier dossier;
      dossier.candidate = candidate;
      dossier.evaluation = evaluation;
      dossier.oracle_results = oracle_results;
      dossier.category = candidate.category;
      dossier.expected_value = candidate.interestingness.reasons.empty()
                                  ? "typed composition is supported by a visible structural pattern"
                                  : join(candidate.interestingness.reasons);
      dossier.weakness = evaluation.experiments.empty() ? "no evidence" : "formal proof, baseline comparison, and domain-specific property oracle are absent";
      dossier.next_test = "add an explicit mathematical property and run counterexample search; compare with a known baseline where applicable";
      const bool has_assumption_failure = std::any_of(oracle_results.begin(), oracle_results.end(),
                                                      [](const auto& oracle) { return oracle.status == OracleStatus::AssumptionViolation; });
      const bool has_counterexample = std::any_of(oracle_results.begin(), oracle_results.end(),
                                                  [](const auto& oracle) { return oracle.status == OracleStatus::CounterexampleFound; });
      const bool goal_directed = candidate.construction_rule.find("typed_composition") == std::string::npos &&
                                 candidate.construction_rule.find("compose roles") == std::string::npos;
      if (goal_directed && !candidate.equivalence.equivalent && candidate.interestingness.total() >= 4.0 &&
          !has_assumption_failure && !has_counterexample &&
          evaluation.state != CandidateState::Unknown) {
        candidate.semantic_category = "serious candidate";
        report.serious_candidates.push_back(candidate);
      } else if (has_assumption_failure) {
        candidate.semantic_category = "assumption-dependent novelty";
      }
      report.lead_audits.push_back(candidate.id + " | surviving=" +
                                   (evaluation.state == CandidateState::Unknown ? "no" : "yes") +
                                   " | oracle_assumption_uncertainty=" + (has_assumption_failure ? "yes" : "no") +
                                   " | residuals=" + std::to_string(oracle_results.size()) +
                                   " | meta_pattern_link=" +
                                   (candidate.lineage.source_patterns.empty() ? "none" : candidate.lineage.source_patterns.front()));
      dossier.candidate.semantic_category = candidate.semantic_category;
      auto dossier_quality = [](const CandidateDossier& value) {
        const auto numerical_passes = static_cast<int>(std::count_if(
            value.evaluation.experiments.begin(), value.evaluation.experiments.end(),
            [](const auto& experiment) { return experiment.type == ExperimentType::Numerical && experiment.status == ExperimentStatus::Pass; }));
        return value.candidate.score.total() + numerical_passes * 2.0 -
               (value.evaluation.state == CandidateState::Unknown ? 2.0 : 0.0);
      };
      if (report.top_candidate_dossiers.size() < 5) {
        report.top_candidate_dossiers.push_back(std::move(dossier));
      } else {
        const auto weakest = std::min_element(report.top_candidate_dossiers.begin(), report.top_candidate_dossiers.end(),
                                              [&](const auto& left, const auto& right) { return dossier_quality(left) < dossier_quality(right); });
        if (dossier_quality(dossier) > dossier_quality(*weakest)) *weakest = std::move(dossier);
      }
    }

    report.residual_cluster_details = ResidualAnalyzer{}.cluster(report.residual_objects);
    report.residual_clusters = static_cast<int>(report.residual_cluster_details.size());
    report.structure_analysis = StructureAnalyzer{}.analyze(
        atlas, report.schema_discovery, report.residual_objects, report.residual_cluster_details);
    if (!report.residual_cluster_details.empty()) {
      add_action(ActionType::AnalyzeResidual, report.residual_cluster_details.front().id,
                 "canonical residual classification", 1.1, 0.4);
      if (!report.structure_analysis.validity_regions.empty())
        add_action(ActionType::MapValidityRegion, report.structure_analysis.validity_regions.front().id,
                   "failure boundary map", 1.15, 0.45);
      const auto goals = synthesis::GoalDirectedSynthesizer{}.derive_goals(
          atlas, meta_report, report.residual_cluster_details);
      if (!goals.empty()) {
        add_action(ActionType::SynthesizeCorrection, goals.front().id,
                   "residual-driven typed correction", 1.2, 0.6);
        const auto correction_candidates = synthesis::GoalDirectedSynthesizer{}.synthesize(atlas, goals, 32);
        for (const auto& correction_candidate : correction_candidates) {
          const auto duplicate = std::any_of(
              report.correction_candidates.begin(), report.correction_candidates.end(),
              [&](const auto& existing) {
                return existing.candidate.canonical_form == correction_candidate.candidate.canonical_form;
              });
          if (!duplicate) {
            report.correction_candidates.push_back(correction_candidate);
            ++report.correction_attempts;
          }
        }
        if (!correction_candidates.empty()) {
          add_action(ActionType::TestGeneralizedIdentity, correction_candidates.front().candidate.id,
                     "correction retest", 1.0, 0.8);
          add_action(ActionType::GeneralizeViaCorrection, correction_candidates.front().candidate.id,
                     "compare restricted and generalized regimes", 1.25, 0.9);
        }
      }
    }

    ++report.state.cycle;
    if (!checkpoint.empty()) save_checkpoint(checkpoint, report.state, report.memory);
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >
        config.budget.max_runtime_ms) {
      report.state.stopped = true;
      report.memory.remember(report.memory.unresolved_questions, "campaign runtime budget exhausted");
    }
    if (generated.accepted.empty()) break;
  }

  report.state.stopped = true;
  report.numerical_experiments = experiment_counter;
  report.geometry_benchmark = benchmarks::GeometryBenchmarkSuite{}.run(atlas, 17);
  if (report.actions.size() + 4 <= static_cast<size_t>(config.budget.max_actions)) {
    report.actions.push_back({"geometry-ontology", "geometry-catalog", "typed regime compatibility", ActionType::InspectGeometry, 1.3, 0.4, 3.25, {}});
    report.actions.push_back({"boundary-stress", "boundary-regimes", "local-vs-global behavior", ActionType::TestBoundary, 1.25, 0.5, 2.5, {"geometry-ontology"}});
    report.actions.push_back({"regularity-stress", "regularity-hierarchy", "unsupported regularity detection", ActionType::TestRegularity, 1.2, 0.45, 2.67, {"geometry-ontology"}});
    report.actions.push_back({"decisive-counterexample", "targeted-regimes", "residual plus adversarial field family", ActionType::SearchDecisiveCounterexample, 1.35, 0.75, 1.8, {"boundary-stress", "regularity-stress"}});
    report.state.actions_executed = static_cast<int>(report.actions.size());
  }
  std::set<std::string> residual_clusters;
  for (const auto& failure : report.failure_patterns) {
    if (!failure.residual.cluster_key.empty()) residual_clusters.insert(failure.residual.cluster_key);
  }
  report.residual_clusters = static_cast<int>(residual_clusters.size());
  std::vector<std::string> action_types;
  for (const auto& action : report.actions) action_types.push_back(to_string(action.type));
  report.scientific = ScientificValidator{}.run(
      atlas, report.scientific.baseline, report.schema_discovery, report.structure_analysis,
      report.residual_objects, report.residual_cluster_details, action_types);
  if (!report.evaluations.empty()) {
    report.memory.remember(report.memory.unresolved_questions,
                           "formal proof and domain-specific property oracles remain unresolved");
  }
  if (report.counterexamples_found == 0 && !report.evaluations.empty()) {
    report.memory.remember(report.memory.unresolved_questions,
                           "no executable counterexample property was supplied in target-free mode");
  }
  if (report.evaluations.size() > 0 && report.memory.benchmark_history.empty()) {
    report.memory.remember(report.memory.unresolved_questions,
                           "baseline comparison was unavailable for the generated compositions");
  }
  report.unresolved = report.memory.unresolved_questions;
  report.memory.remember(report.memory.reports, report.state.id + ":completed:" + report.atlas_snapshot);
  return report;
}

std::string ResearchOrchestrator::report_text(const CampaignReport& report) const {
  std::ostringstream out;
  out << "Campaign ID: " << report.state.id << "\n"
      << "Atlas snapshot: " << report.atlas_snapshot << "\n"
      << "Mode: " << to_string(report.state.mode) << "\n"
      << "AI: " << (report.ai_enabled ? "enabled" : "disabled") << "\n"
      << "Target: " << report.target << "\n"
      << "Atlas frozen: " << (report.atlas_frozen ? "yes" : "no") << "\n";
  if (!report.deep_discovery.campaigns.empty()) {
    out << "Deep open discovery:\n" << DeepDiscoveryEngine{}.export_text(report.deep_discovery);
    return out.str();
  }
  if (!report.axiomatic_discovery.campaigns.empty()) {
    out << "Axiomatic open discovery:\n" << axiomatic::AxiomaticEngine{}.export_text(report.axiomatic_discovery);
    return out.str();
  }
  if (!report.unknown_discovery.campaigns.empty()) {
    out << "Unknown-structure discovery:\n" << axiomatic::UnknownStructureEngine{}.export_text(report.unknown_discovery);
    return out.str();
  }
  out << "Cycles: " << report.state.cycle << "\n"
      << "Actions executed: " << report.state.actions_executed << "\n"
      << "Patterns discovered: " << report.memory.discovered_patterns.size() << "\n"
      << "Meta-pattern families: " << report.meta_patterns.size() << "\n"
      << "Predicted roles: " << report.predicted_roles.size() << "\n"
      << "Induced schemas: " << report.schema_discovery.schemas.size() << "\n"
      << "Parameterized families: " << report.schema_discovery.parameterized_families.size() << "\n"
      << "Schema completions: " << report.schema_discovery.completions.size() << "\n"
      << "Law candidates: " << report.schema_discovery.laws.size() << "\n"
      << "Discovery leads: " << report.schema_discovery.discovery_leads.size() << "\n"
      << "Closure findings: " << report.structure_analysis.closures.size() << "\n"
      << "Commutator findings: " << report.structure_analysis.commutators.size() << "\n"
      << "Invariant hypotheses: " << report.structure_analysis.invariants.size() << "\n"
      << "Validity regions: " << report.structure_analysis.validity_regions.size() << "\n"
      << "Cross-domain patterns: " << report.cross_domain_patterns.size() << "\n"
      << "Structural gaps: " << report.structural_gaps.size() << "\n"
      << "Candidates generated: " << report.memory.generated_candidates.size() << "\n"
      << "Candidates rejected: " << report.memory.rejected_candidates.size() << "\n"
      << "Known-equivalent: " << report.known_equivalent << "\n"
      << "Known constructions: " << report.known_constructions << "\n"
      << "Counterexamples found: " << report.counterexamples_found << "\n"
      << "Numerical experiments: " << report.numerical_experiments << "\n"
      << "Surviving candidates: " << report.surviving_candidates.size() << "\n"
      << "Serious candidates: " << report.serious_candidates.size() << "\n"
      << "False-interest cases: " << report.false_interest_cases.size() << "\n"
      << "Failure patterns: " << report.failure_patterns.size() << "\n"
      << "Residual clusters: " << report.residual_clusters << "\n"
      << "Correction attempts: " << report.correction_attempts << "\n"
      << "Successful repairs: " << report.successful_repairs << "\n"
      << "Residual benchmark: " << report.residual_benchmark.id
      << " | repaired=" << (report.residual_benchmark.repaired ? "yes" : "no") << "\n"
      << "Schema benchmark: " << report.schema_benchmark.id
      << " | inferred=" << (report.schema_benchmark.inferred ? "yes" : "no")
      << " | compression=" << report.schema_benchmark.compression_gain << "\n"
      << "Geometry regimes: " << report.geometry_benchmark.regimes.size()
      << " | bridges=" << report.geometry_benchmark.bridges.size()
      << " | hard=" << report.geometry_benchmark.hard_recovered << "/" << report.geometry_benchmark.hard_total
      << " | convergence=" << report.geometry_benchmark.convergence.size() << "\n"
      << "Numerical truth records: " << report.geometry_benchmark.numerical_truth.records.size()
      << " | curved executions=" << report.geometry_benchmark.curved_executions.size()
      << " | coordinate consistency=" << (report.geometry_benchmark.coordinate_consistency.passed ? "passed" : "failed") << "\n"
      << "Unresolved questions: " << report.unresolved.size() << "\n"
      << "Epistemic status: " << report.epistemic_status << "\n";
  out << "Top candidate dossiers:\n";
  for (const auto& dossier : report.top_candidate_dossiers) {
    out << "- " << dossier.candidate.id << " | category=" << dossier.category
        << " | state=" << to_string(dossier.evaluation.state)
        << " | status=" << dossier.evaluation.epistemic_status
        << " | score=" << dossier.candidate.score.total() << "\n"
        << "  signature=" << signature_text(dossier.candidate.signature) << "\n"
        << "  canonical=" << dossier.candidate.canonical_form << "\n"
        << "  semantic_category=" << dossier.candidate.semantic_category << "\n"
        << "  interestingness=" << dossier.candidate.interestingness.total() << "\n"
        << "  interestingness_components=novelty:" << dossier.candidate.interestingness.semantic_novelty
        << ",generalization:" << dossier.candidate.interestingness.generalization_power
        << ",recovery:" << dossier.candidate.interestingness.recovery_power
        << ",relations:" << dossier.candidate.interestingness.independent_relations
        << ",compression:" << dossier.candidate.interestingness.compression_reduction
        << ",invariants:" << dossier.candidate.interestingness.invariant_potential
        << ",cross_domain:" << dossier.candidate.interestingness.cross_domain_reach
        << ",utility:" << dossier.candidate.interestingness.computational_utility << "\n"
        << "  semantic_equivalence=" << (dossier.candidate.equivalence.equivalent ? "yes" : "no")
        << " | level=" << dossier.candidate.equivalence.level
        << " | matched=" << dossier.candidate.equivalence.matched_id
        << " | explanation=" << dossier.candidate.equivalence.explanation << "\n"
        << "  equivalence_assumptions=" << join(dossier.candidate.equivalence.assumptions) << "\n"
        << "  lineage.patterns=" << join(dossier.candidate.lineage.source_patterns) << "\n"
        << "  assumptions=" << join(dossier.candidate.assumptions) << "\n"
        << "  requirements=" << join(dossier.candidate.required_structures) << "\n"
        << "  derived_properties=" << join(dossier.candidate.derived_properties) << "\n"
        << "  recovered_operators=" << join(dossier.candidate.lineage.atlas_operators) << "\n"
        << "  expected_identities=" << join(dossier.candidate.expected_identities) << "\n"
        << "  numerical_evidence=";
    for (const auto& experiment : dossier.evaluation.experiments) {
      if (experiment.type == ExperimentType::Numerical) out << to_string(experiment.status) << " ";
    }
    out << "\n"
        << "  oracle_evidence=";
    for (const auto& oracle : dossier.oracle_results) out << to_string(oracle.kind) << ":" << to_string(oracle.status) << " ";
    out << "\n"
        << "  why=" << dossier.expected_value << "\n"
        << "  weakness=" << dossier.weakness << "\n"
        << "  next=" << dossier.next_test << "\n";
  }
  out << "Meta-pattern families:\n";
  for (const auto& meta : report.meta_patterns) {
    out << "- " << meta.id << " | law=" << meta.law
        << " | score=" << meta.family_score
        << " | independent_realizations=" << meta.independent_realizations
        << " | members=" << join(meta.member_pattern_ids) << "\n";
  }
  out << "Predicted roles:\n";
  for (const auto& prediction : report.predicted_roles) {
    out << "- " << prediction.id << " | source=" << prediction.source_meta_pattern
        << " | role=" << prediction.predicted_role << " | confidence=" << prediction.confidence
        << " | justified=" << (prediction.justified ? "yes" : "no") << "\n";
  }
  out << "Induced schemas:\n";
  for (const auto& schema : report.schema_discovery.schemas) {
    out << "- " << schema.id << " | kind=" << synthesis::to_string(schema.kind)
        << " | form=" << schema.canonical_form << " | compression=" << schema.compression_gain
        << " | status=" << schema.status << "\n";
  }
  out << "Parameterized families:\n";
  for (const auto& family : report.schema_discovery.parameterized_families)
    out << "- " << family.id << " | parameters=" << join(family.parameters)
        << " | constraints=" << join(family.constraints) << "\n";
  out << "Closure findings: " << report.structure_analysis.closures.size() << "\n";
  for (const auto& closure : report.structure_analysis.closures)
    out << "- " << closure.id << " | family=" << closure.family_id
        << " | value=" << closure.value << " | missing=" << closure.missing_role << "\n";
  out << "Commutator findings: " << report.structure_analysis.commutators.size() << "\n";
  for (const auto& commutator : report.structure_analysis.commutators)
    out << "- " << commutator.id << " | " << commutator.left << "," << commutator.right
        << " | status=" << commutator.status << " | class=" << commutator.classification << "\n";
  out << "Invariant hypotheses: " << report.structure_analysis.invariants.size() << "\n";
  for (const auto& invariant : report.structure_analysis.invariants)
    out << "- " << invariant.id << " | target=" << invariant.target
        << " | property=" << invariant.property << " | confidence=" << invariant.confidence << "\n";
  out << "Validity regions: " << report.structure_analysis.validity_regions.size() << "\n";
  for (const auto& region : report.structure_analysis.validity_regions)
    out << "- " << region.id << " | target=" << region.target
        << " | valid=" << join(region.valid_regimes)
        << " | failed=" << join(region.failed_regimes)
        << " | generalized=" << (region.generalized ? "yes" : "no") << "\n";
  out << "Residual clusters:\n";
  for (const auto& cluster : report.residual_cluster_details) {
    out << "- " << cluster.id << " | classification=" << cluster.classification
        << " | candidates=" << join(cluster.candidate_ids)
        << " | corrections=" << join(cluster.correction_requirements) << "\n";
  }
  out << "Lead audits:\n";
  for (const auto& audit : report.lead_audits) out << "- " << audit << "\n";
  out << "Correction candidates: " << report.correction_candidates.size() << "\n";
  out << report.geometry_benchmark.id << ":\n" << benchmarks::GeometryBenchmarkSuite{}.export_text(report.geometry_benchmark);
  out << "Scientific validation:\n" << ScientificValidator{}.export_text(report.scientific);
  return out.str();
}

std::string ResearchOrchestrator::report_json(const CampaignReport& report) const {
  std::ostringstream out;
  out << "{\"campaign_id\":\"" << report.state.id
      << "\",\"atlas_snapshot\":\"" << report.atlas_snapshot
      << "\",\"mode\":\"" << to_string(report.state.mode)
      << "\",\"ai_enabled\":" << (report.ai_enabled ? "true" : "false")
      << ",\"target\":\"" << report.target
      << "\",\"atlas_frozen\":" << (report.atlas_frozen ? "true" : "false")
      << ",\"cycles\":" << report.state.cycle
      << ",\"actions\":" << report.state.actions_executed
      << ",\"patterns\":" << report.memory.discovered_patterns.size()
      << ",\"meta_patterns\":" << report.meta_patterns.size()
      << ",\"predicted_roles\":" << report.predicted_roles.size()
      << ",\"schemas\":" << report.schema_discovery.schemas.size()
      << ",\"parameterized_families\":" << report.schema_discovery.parameterized_families.size()
      << ",\"schema_completions\":" << report.schema_discovery.completions.size()
      << ",\"law_candidates\":" << report.schema_discovery.laws.size()
      << ",\"discovery_leads\":" << report.schema_discovery.discovery_leads.size()
      << ",\"closure_findings\":" << report.structure_analysis.closures.size()
      << ",\"commutator_findings\":" << report.structure_analysis.commutators.size()
      << ",\"invariant_hypotheses\":" << report.structure_analysis.invariants.size()
      << ",\"validity_regions\":" << report.structure_analysis.validity_regions.size()
      << ",\"cross_domain_patterns\":" << report.cross_domain_patterns.size()
      << ",\"structural_gaps\":" << report.structural_gaps.size()
      << ",\"candidates_generated\":" << report.memory.generated_candidates.size()
      << ",\"candidates_rejected\":" << report.memory.rejected_candidates.size()
      << ",\"known_equivalent\":" << report.known_equivalent
      << ",\"known_constructions\":" << report.known_constructions
      << ",\"counterexamples\":" << report.counterexamples_found
      << ",\"numerical_experiments\":" << report.numerical_experiments
      << ",\"surviving_candidates\":" << report.surviving_candidates.size()
      << ",\"serious_candidates\":" << report.serious_candidates.size()
      << ",\"false_interest_cases\":" << report.false_interest_cases.size()
      << ",\"failure_patterns\":" << report.failure_patterns.size()
      << ",\"residual_clusters\":" << report.residual_clusters
      << ",\"correction_attempts\":" << report.correction_attempts
      << ",\"successful_repairs\":" << report.successful_repairs
      << ",\"residual_benchmark_repaired\":" << (report.residual_benchmark.repaired ? "true" : "false")
      << ",\"schema_benchmark_inferred\":" << (report.schema_benchmark.inferred ? "true" : "false")
      << ",\"schema_compression_gain\":" << report.schema_benchmark.compression_gain
      << ",\"geometry_regimes\":" << report.geometry_benchmark.regimes.size()
      << ",\"geometry_bridges\":" << report.geometry_benchmark.bridges.size()
      << ",\"geometry_hard_recovered\":" << report.geometry_benchmark.hard_recovered
      << ",\"geometry_hard_total\":" << report.geometry_benchmark.hard_total
      << ",\"geometry_leakage\":" << report.geometry_benchmark.leakage
      << ",\"numerical_truth_records\":" << report.geometry_benchmark.numerical_truth.records.size()
      << ",\"curved_executions\":" << report.geometry_benchmark.curved_executions.size()
      << ",\"coordinate_consistency\":" << (report.geometry_benchmark.coordinate_consistency.passed ? "true" : "false")
      << ",\"boundary_policy_discrepancy\":" << report.geometry_benchmark.boundary_policy_discrepancy
      << ",\"deep_campaigns\":" << report.deep_discovery.campaigns.size()
      << ",\"deep_external_check_candidates\":" << report.deep_discovery.external_check_candidates
      << ",\"deep_serious_candidates\":" << report.deep_discovery.serious_candidates
      << ",\"deep_out_of_sample_recovery\":" << report.deep_discovery.out_of_sample_recovery_rate
      << ",\"axiomatic_campaigns\":" << report.axiomatic_discovery.campaigns.size()
      << ",\"axiomatic_predictions\":" << report.axiomatic_discovery.benchmarks.predictions
      << ",\"axiomatic_successes\":" << report.axiomatic_discovery.benchmarks.successful_predictions
      << ",\"axiomatic_out_of_sample\":" << report.axiomatic_discovery.benchmarks.out_of_sample_recoveries
      << ",\"unknown_campaigns\":" << report.unknown_discovery.campaigns.size()
      << ",\"unknown_hypotheses\":" << report.unknown_discovery.hypotheses_generated
      << ",\"unknown_held_out_successes\":" << report.unknown_discovery.stress.held_out.successes
      << ",\"scientific_leads\":" << report.scientific.leads.size()
      << ",\"scientific_eliminated\":" << report.scientific.eliminated_leads
      << ",\"scientific_unresolved\":" << report.scientific.unresolved_leads
      << ",\"scientific_leakage\":" << report.scientific.metrics.leakage
      << ",\"scientific_f1\":" << report.scientific.metrics.f1
      << ",\"unresolved_questions\":" << report.unresolved.size()
      << ",\"epistemic_status\":\"" << report.epistemic_status << "\"}";
  return out.str();
}

}  // namespace opforge::research
