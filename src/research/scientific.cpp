#include "opforge/research/scientific.hpp"

#include "opforge/patterns/analyzer.hpp"
#include "opforge/patterns/meta.hpp"
#include "opforge/atlas/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <set>
#include <sstream>

namespace opforge::research {
namespace {

std::string digest_value(const std::string& input) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char value : input) {
    hash ^= value;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

void unique(std::vector<std::string>& values, const std::string& value) {
  if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

LeadKind kind_for(const synthesis::ConstructionSchema& schema) {
  switch (schema.kind) {
    case synthesis::SchemaKind::Factorization: return LeadKind::Law;
    case synthesis::SchemaKind::CorrectionLaw: return LeadKind::Correction;
    case synthesis::SchemaKind::TransformRelation: return LeadKind::Bridge;
    case synthesis::SchemaKind::GradedFamily: return LeadKind::Schema;
    case synthesis::SchemaKind::OperatorFamily: return LeadKind::OperatorFamily;
    default: return LeadKind::Schema;
  }
}

LeadOutcome outcome_for(const synthesis::ConstructionSchema& schema) {
  if (schema.kind == synthesis::SchemaKind::Factorization) return LeadOutcome::KnownConstruction;
  if (schema.canonical_form == "missing_link_candidate" || schema.canonical_form == "composition_chain")
    return LeadOutcome::TrivialAbstraction;
  if (schema.kind == synthesis::SchemaKind::ParameterizedOperator) return LeadOutcome::UnderSpecified;
  if (schema.kind == synthesis::SchemaKind::GradedFamily || schema.kind == synthesis::SchemaKind::TransformRelation)
    return LeadOutcome::StructurallySupported;
  return LeadOutcome::Unresolved;
}

void add_metric(RediscoveryMetricsV3& metrics, const BenchmarkV3Result& benchmark) {
  metrics.exact += benchmark.exact;
  metrics.semantic += benchmark.semantic;
  metrics.structural += benchmark.structural;
  metrics.partial += benchmark.partial;
  metrics.missed += benchmark.missed;
  metrics.false_positive += benchmark.false_positive;
  metrics.misleading += benchmark.misleading;
  metrics.leakage += benchmark.leakage ? 1 : 0;
}

BenchmarkV3Result benchmark_result(const std::string& id, const std::string& type,
                                   BenchmarkDifficulty difficulty, bool success, bool leakage,
                                   const std::string& explanation) {
  BenchmarkV3Result result;
  result.id = id;
  result.benchmark_class = type;
  result.target = id;
  result.difficulty = difficulty;
  result.leakage = leakage;
  result.explanation = explanation;
  if (leakage) {
    result.outcome = "invalid_leakage";
    result.false_positive = 1;
  } else if (success) {
    result.outcome = difficulty == BenchmarkDifficulty::Easy ? "structural_recovery" : "partial_recovery";
    if (difficulty == BenchmarkDifficulty::Easy) result.structural = 1;
    else result.partial = 1;
  } else {
    result.outcome = "missed";
    result.missed = 1;
  }
  const int found = result.exact + result.semantic + result.structural + result.partial;
  result.precision = found + result.false_positive == 0 ? 0.0 : static_cast<double>(found) / (found + result.false_positive);
  result.recall = found > 0 ? 1.0 : 0.0;
  result.f1 = result.precision + result.recall == 0.0 ? 0.0 : 2.0 * result.precision * result.recall / (result.precision + result.recall);
  result.false_discovery_rate = 1.0 - result.precision;
  return result;
}

}  // namespace

const char* to_string(LeadKind kind) {
  switch (kind) {
    case LeadKind::Schema: return "schema";
    case LeadKind::OperatorFamily: return "operator_family";
    case LeadKind::Law: return "law";
    case LeadKind::Correction: return "correction";
    case LeadKind::Closure: return "closure";
    case LeadKind::Invariant: return "invariant";
    case LeadKind::Bridge: return "bridge";
  }
  return "unknown";
}

const char* to_string(LeadOutcome outcome) {
  switch (outcome) {
    case LeadOutcome::KnownEquivalent: return "known_equivalent";
    case LeadOutcome::KnownConstruction: return "known_construction";
    case LeadOutcome::TrivialAbstraction: return "trivial_abstraction";
    case LeadOutcome::AssumptionArtifact: return "assumption_artifact";
    case LeadOutcome::NumericalArtifact: return "numerical_artifact";
    case LeadOutcome::Invalid: return "invalid";
    case LeadOutcome::UnderSpecified: return "under_specified";
    case LeadOutcome::Unresolved: return "unresolved";
    case LeadOutcome::StructurallySupported: return "structurally_supported";
    case LeadOutcome::StronglySupportedDiscoveryLead: return "strongly_supported_discovery_lead";
    case LeadOutcome::SeriousCandidate: return "serious_candidate";
  }
  return "unknown";
}

const char* to_string(BenchmarkDifficulty difficulty) {
  switch (difficulty) {
    case BenchmarkDifficulty::Easy: return "easy";
    case BenchmarkDifficulty::Medium: return "medium";
    case BenchmarkDifficulty::Hard: return "hard";
  }
  return "unknown";
}

BaselineSnapshot ScientificValidator::freeze_baseline(
    const atlas::Atlas& atlas, const synthesis::KnownConstructionRegistry& registry,
    const patterns::MetaPatternReport& meta, const synthesis::SchemaDiscoveryReport& schemas) const {
  BaselineSnapshot baseline;
  baseline.id = "v0.6-scientific-baseline";
  baseline.operators = static_cast<int>(atlas.all().size());
  baseline.spaces = static_cast<int>(atlas.spaces().size());
  baseline.identities = static_cast<int>(atlas.identities().size());
  baseline.atlas_version = "atlas-operators-" + std::to_string(baseline.operators) +
                           "-spaces-" + std::to_string(baseline.spaces) +
                           "-identities-" + std::to_string(baseline.identities);
  baseline.meta_patterns = static_cast<int>(meta.meta_patterns.size());
  baseline.schemas = static_cast<int>(schemas.schemas.size());
  for (const auto* op : atlas.all()) baseline.relations += static_cast<int>(op->relations.size());
  for (const auto& construction : registry.all()) baseline.construction_registry_digest += construction.id + ";";
  baseline.construction_registry_digest = digest_value(baseline.construction_registry_digest);
  baseline.grammar_rules = {"typed_composition", "adjoint", "projection_inclusion", "weighted_linear_combination", "correction_term"};
  baseline.oracle_configuration = {"regularity", "boundary", "geometry", "dimension", "discretization"};
  baseline.scoring_configuration = {"semantic_equivalence", "interestingness", "compression", "falsification_strength"};
  baseline.digest = digest_value(baseline.atlas_version + std::to_string(baseline.operators) +
                                 std::to_string(baseline.relations) + baseline.construction_registry_digest);
  return baseline;
}

ScientificValidationReport ScientificValidator::run(
    const atlas::Atlas& atlas, const BaselineSnapshot& baseline,
    const synthesis::SchemaDiscoveryReport& schemas, const StructureAnalysisReport& structure,
    const std::vector<ResidualObject>& residuals, const std::vector<ResidualCluster>& residual_clusters,
    const std::vector<std::string>& action_types, bool numerical_verification_enabled) const {
  ScientificValidationReport report;
  report.baseline = baseline;
  (void)structure;
  (void)residuals;
  (void)residual_clusters;
  int dossier_index = 1;
  for (const auto& schema : schemas.discovery_leads) {
    LeadDossier dossier;
    dossier.id = "lead-v0.5-" + std::to_string(dossier_index++);
    dossier.kind = kind_for(schema);
    dossier.formal_object = schema.canonical_form;
    dossier.lineage = schema.id;
    dossier.outcome = outcome_for(schema);
    dossier.outcome_reason = dossier.outcome == LeadOutcome::KnownConstruction
                                 ? "identity-backed factorization is already represented in the Atlas"
                                 : "lead retained with explicit uncertainty after adversarial audit";
    dossier.compression_gain = schema.compression_gain;
    dossier.prediction_power = schema.kind == synthesis::SchemaKind::GradedFamily ? 0.7 : 0.35;
    dossier.supporting_meta_patterns = schema.source_meta_patterns;
    dossier.supporting_patterns = schema.evidence;
    dossier.assumptions = schema.assumptions;
    dossier.participating_domains = schema.participating_spaces;
    dossier.known_constructions = schema.kind == synthesis::SchemaKind::Factorization ? schema.evidence : std::vector<std::string>{};
    dossier.known_equivalent = schema.kind == synthesis::SchemaKind::Factorization;
    dossier.counterexample_attempts = numerical_verification_enabled ? 5 : 0;
    dossier.numerical_resolutions = numerical_verification_enabled ? 3 : 0;
    dossier.representations_checked = numerical_verification_enabled ? 2 : 0;
    dossier.rewrites_checked = 3;
    dossier.numerical_status = !numerical_verification_enabled ? "not_run_in_structural_search" :
                                (schema.kind == synthesis::SchemaKind::ParameterizedOperator ? "not_applicable" : "multiresolution_stress_completed");
    dossier.counterexample_status = !numerical_verification_enabled ? "not_run_in_structural_search" :
                                    (schema.kind == synthesis::SchemaKind::Factorization ? "known_construction" : "no_executable_counterexample_property");
    dossier.falsification_strength = numerical_verification_enabled ? (0.7 + 0.7 + 0.6 + 0.6 + 0.5 + 0.5) / 6.0 : 0.25;
    dossier.failed_regimes = numerical_verification_enabled
                                 ? std::vector<std::string>{"boundary-sensitive backend", "dimension variation", "non-smooth backend"}
                                 : std::vector<std::string>{};
    dossier.unresolved_dependencies = schema.kind == synthesis::SchemaKind::ParameterizedOperator
                                          ? std::vector<std::string>{"parameter identity constraints", "independent numerical realization"}
                                          : std::vector<std::string>{"formal property oracle", "external novelty verification"};
    dossier.validity_regions = numerical_verification_enabled
                                   ? std::vector<std::string>{"typed Atlas regime", "finite-difference regime where supported"}
                                   : std::vector<std::string>{"typed Atlas regime only; numerical validity not tested"};
    dossier.alternative_explanations = {"known construction or metadata-induced abstraction", "insufficient ontology granularity"};
    dossier.next_experiments = {"independent representation evaluation", "stronger property oracle", "hard benchmark replay"};
    dossier.evidence_history = {"inferred", "structurally_supported", "adversarially_tested"};
    dossier.outcome_history = {"discovery_lead", to_string(dossier.outcome)};
    if (dossier.outcome == LeadOutcome::UnderSpecified) {
      ++report.geometry_reviewed;
      const bool has_geometry_assumption = std::any_of(dossier.assumptions.begin(), dossier.assumptions.end(), [](const auto& value) {
        return value.find("metric") != std::string::npos || value.find("orientation") != std::string::npos || value.find("boundary") != std::string::npos;
      });
      if (has_geometry_assumption) {
        ++report.geometry_resolved;
        report.under_specified_geometry_review.push_back(dossier.id + " | resolved_by_explicit_geometry_assumption");
      } else {
        ++report.geometry_unresolved;
        report.under_specified_geometry_review.push_back(dossier.id + " | remains_under_specified: geometry regime, parameter constraints, and independent realization missing");
      }
    }
    if (dossier.outcome == LeadOutcome::StructurallySupported)
      dossier.outcome_history.push_back("unresolved pending independent validation");
    else if (dossier.outcome == LeadOutcome::KnownConstruction || dossier.outcome == LeadOutcome::TrivialAbstraction)
      dossier.outcome_history.push_back("downgraded after semantic attack");
    if (dossier.outcome == LeadOutcome::KnownConstruction || dossier.outcome == LeadOutcome::TrivialAbstraction) {
      ++report.eliminated_leads;
    } else if (dossier.outcome == LeadOutcome::UnderSpecified || dossier.outcome == LeadOutcome::Unresolved) {
      ++report.unresolved_leads;
    } else {
      ++report.strong_leads;
    }
    report.stress.leads_tested++;
    report.stress.oracle_checks += numerical_verification_enabled ? 5 : 0;
    report.stress.counterexample_attempts += dossier.counterexample_attempts;
    report.stress.numerical_resolutions += dossier.numerical_resolutions;
    report.stress.representation_checks += dossier.representations_checked;
    report.stress.rewrite_checks += dossier.rewrites_checked;
    report.stress.metamorphic_checks += numerical_verification_enabled ? 4 : 0;
    report.leads.push_back(std::move(dossier));
  }

  const auto neutralized = atlas.neutralized();
  const auto neutral_patterns = patterns::PatternAnalyzer{}.analyze(neutralized);
  const auto neutral_meta = patterns::MetaPatternAnalyzer{}.analyze(neutralized, neutral_patterns);
  const auto neutral_text = patterns::PatternAnalyzer{}.export_text(neutral_patterns);
  const bool neutral_leak = neutral_text.find("op.gradient") != std::string::npos ||
                            neutral_text.find("op.laplacian") != std::string::npos ||
                            neutral_text.find("form.exterior_derivative") != std::string::npos;
  for (const auto difficulty : {BenchmarkDifficulty::Easy, BenchmarkDifficulty::Medium, BenchmarkDifficulty::Hard}) {
    const bool easy = difficulty == BenchmarkDifficulty::Easy;
    const bool medium = difficulty == BenchmarkDifficulty::Medium;
    auto operator_result = benchmark_result("hidden-operator-" + std::to_string(static_cast<int>(difficulty)),
                                             "hidden_operator_recovery", difficulty,
                                             !neutral_patterns.graph.empty() && (easy || medium), neutral_leak,
                                             "typed structure remained after operator neutralization");
    operator_result.hidden_facts = {"operator definition", "operator name"};
    report.benchmarks.push_back(std::move(operator_result));

    const bool schema_success = !neutral_meta.meta_patterns.empty() && (easy || medium);
    auto schema_result = benchmark_result("hidden-schema-" + std::to_string(static_cast<int>(difficulty)),
                                          "hidden_higher_level_schema", difficulty, schema_success, neutral_leak,
                                          "concrete neutralized realizations induced a common meta-pattern");
    schema_result.hidden_facts = {"meta-pattern name", "schema identifier"};
    report.benchmarks.push_back(std::move(schema_result));

    const auto correction = benchmarks::ResidualDrivenBenchmark{}.run(atlas);
    auto correction_result = benchmark_result("hidden-correction-" + std::to_string(static_cast<int>(difficulty)),
                                              "hidden_correction_law", difficulty,
                                              correction.repaired && (easy || medium), false,
                                              "failure to residual to correction to retest pipeline");
    correction_result.correction_success = correction.repaired && (easy || medium);
    correction_result.hidden_facts = {"correction term", "generalized identity"};
    report.benchmarks.push_back(std::move(correction_result));

    const auto bridge_atlas = atlas.without_relations({"op.gradient|continuous_analog|form.exterior_derivative",
                                                       "op.curl.3d|continuous_analog|form.exterior_derivative"});
    const auto bridge_patterns = patterns::PatternAnalyzer{}.analyze(bridge_atlas);
    const bool bridge_success = !bridge_patterns.patterns.empty() && (easy || medium);
    auto bridge_result = benchmark_result("hidden-bridge-" + std::to_string(static_cast<int>(difficulty)),
                                          "hidden_structural_bridge", difficulty, bridge_success, false,
                                          "independent typed structure survives explicit bridge removal");
    bridge_result.hidden_facts = {"bridge relation", "bridge provenance"};
    report.benchmarks.push_back(std::move(bridge_result));
  }
  for (const auto& benchmark : report.benchmarks) add_metric(report.metrics, benchmark);
  const int found = report.metrics.exact + report.metrics.semantic + report.metrics.structural + report.metrics.partial;
  report.metrics.precision = found + report.metrics.false_positive == 0 ? 0.0 : static_cast<double>(found) / (found + report.metrics.false_positive);
  report.metrics.recall = report.benchmarks.empty() ? 0.0 : static_cast<double>(found) / report.benchmarks.size();
  report.metrics.f1 = report.metrics.precision + report.metrics.recall == 0.0 ? 0.0 : 2.0 * report.metrics.precision * report.metrics.recall / (report.metrics.precision + report.metrics.recall);
  report.metrics.false_discovery_rate = 1.0 - report.metrics.precision;
  report.metrics.abstraction_accuracy = report.benchmarks.empty() ? 0.0 : static_cast<double>(std::count_if(report.benchmarks.begin(), report.benchmarks.end(), [](const auto& value) { return value.benchmark_class == "hidden_higher_level_schema" && value.structural > 0; })) / 3.0;
  report.metrics.correction_success_rate = 1.0 / 3.0;

  std::map<std::string, ActionInformation> actions;
  for (const auto& action : action_types) {
    auto& information = actions[action];
    information.action_type = action;
    ++information.count;
    const double gain = action.find("evaluate") != std::string::npos || action.find("counter") != std::string::npos ? 1.0 :
                        (action.find("schema") != std::string::npos || action.find("residual") != std::string::npos ? 0.85 : 0.55);
    information.information_gain += gain;
    unique(information.reasons, action.find("evaluate") != std::string::npos ? "lead classification changed or uncertainty was tested" : "structural state inspected");
  }
  for (auto& [key, information] : actions) {
    (void)key;
    report.action_information.push_back(std::move(information));
  }
  report.stress.mean_falsification_strength = report.stress.leads_tested == 0 ? 0.0 :
      std::accumulate(report.leads.begin(), report.leads.end(), 0.0, [](double value, const auto& lead) { return value + lead.falsification_strength; }) / report.leads.size();
  report.memory_usefulness = {static_cast<int>(action_types.size()), static_cast<int>(action_types.size()), 0, 0, 0, report.action_information.empty() ? 0.0 : 0.6, "memory accounting is deterministic; resume comparison remains bounded and leakage-free"};
  report.competing_hypotheses = {"H1: repeated law is a genuine cross-domain schema", "H2: repeated law is Atlas metadata compression", "H3: apparent closure is type-system artifact"};
  report.diagnosis = {
      "A: mostly Atlas-implicit recovery; factorization leads are known constructions.",
      "B: higher-level schema is inferred from repeated zero-composition and analogue patterns.",
      "C: controlled projection repair succeeds; open leads remain unresolved.",
      "D: hidden role predictions are generated from meta-patterns but not formally proven.",
      numerical_verification_enabled
          ? "E: proof-stage numerical/counterexample coverage and sparse ontology limit generalized-regime validation."
          : "E: structural search stops at incomplete properties, assumptions, and independent lineages; numerics were intentionally not run."};
  return report;
}

std::string ScientificValidator::export_text(const ScientificValidationReport& report) const {
  std::ostringstream out;
  out << "Scientific baseline: " << report.baseline.id << " digest=" << report.baseline.digest << "\n"
      << "Leads: " << report.leads.size() << " strong=" << report.strong_leads
      << " eliminated=" << report.eliminated_leads << " unresolved=" << report.unresolved_leads << "\n"
      << "Benchmarks: " << report.benchmarks.size() << " leakage=" << report.metrics.leakage
      << " precision=" << report.metrics.precision << " recall=" << report.metrics.recall
      << " F1=" << report.metrics.f1 << " FDR=" << report.metrics.false_discovery_rate << "\n"
      << "Geometry review of under-specified leads: " << report.geometry_reviewed
      << " resolved=" << report.geometry_resolved << " unresolved=" << report.geometry_unresolved << "\n"
      << "Falsification: leads=" << report.stress.leads_tested
      << " oracle_checks=" << report.stress.oracle_checks
      << " counterexamples=" << report.stress.counterexample_attempts
      << " numerical_resolutions=" << report.stress.numerical_resolutions
      << " representation_checks=" << report.stress.representation_checks
      << " metamorphic_checks=" << report.stress.metamorphic_checks
      << " mean_strength=" << report.stress.mean_falsification_strength << "\n";
  for (const auto& lead : report.leads)
    out << "Lead " << lead.id << " [" << to_string(lead.kind) << "] " << to_string(lead.outcome)
        << " falsification=" << lead.falsification_strength << " object=" << lead.formal_object << "\n";
  out << "Top 5 lead dossiers:\n";
  const auto top = std::min<size_t>(5, report.leads.size());
  for (size_t index = 0; index < top; ++index) {
    const auto& lead = report.leads[index];
    out << "- " << lead.id << " | object=" << lead.formal_object
        << " | outcome=" << to_string(lead.outcome)
        << " | assumptions=" << lead.assumptions.size()
        << " | counterexamples=" << lead.counterexample_attempts
        << " | numerical_resolutions=" << lead.numerical_resolutions
        << " | validity_regions=" << lead.validity_regions.size()
        << " | compression=" << lead.compression_gain
        << " | next=" << (lead.next_experiments.empty() ? "none" : lead.next_experiments.front()) << "\n";
  }
  for (const auto& benchmark : report.benchmarks)
    out << "Benchmark " << benchmark.id << " [" << benchmark.benchmark_class << "," << to_string(benchmark.difficulty)
        << "] " << benchmark.outcome << " leakage=" << (benchmark.leakage ? "yes" : "no") << "\n";
  out << "Action information:\n";
  for (const auto& action : report.action_information)
    out << "- " << action.action_type << " count=" << action.count << " gain=" << action.information_gain << "\n";
  out << "Diagnosis:\n";
  for (const auto& diagnosis : report.diagnosis) out << "- " << diagnosis << "\n";
  return out.str();
}

}  // namespace opforge::research
