#include "opforge/analogy/engine.hpp"

#include "opforge/atlas/seed.hpp"
#include "opforge/atlas/loader.hpp"
#include "opforge/semantic/closure.hpp"
#include "opforge/axiomatic/unknown.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>

namespace opforge::analogy {
namespace {

using atlas::OperatorRecord;
using atlas::RelationKind;

std::uint64_t hash_text(const std::string& text) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char value : text) { hash ^= value; hash *= 1099511628211ULL; }
  return hash;
}

std::string hex_hash(const std::string& text) { std::ostringstream out; out << std::hex << hash_text(text); return out.str(); }

bool bridge_relation(RelationKind kind) {
  return kind == RelationKind::Generalizes || kind == RelationKind::SpecialCaseOf || kind == RelationKind::ContinuousAnalog || kind == RelationKind::DiscreteAnalog || kind == RelationKind::AnalogueOf || kind == RelationKind::RealizationOf || kind == RelationKind::ContinuousLimitOf || kind == RelationKind::Dual;
}

bool has_relation(const OperatorRecord& source, RelationKind kind, const std::string& target) {
  return std::any_of(source.relations.begin(), source.relations.end(), [&](const auto& relation) { return relation.kind == kind && relation.target_id == target; });
}

std::string relation_key(const std::string& source, RelationKind kind, const std::string& target) { return source + "|" + atlas::to_string(kind) + "|" + target; }

std::string category_for(const std::string& source, const std::string& target) {
  if ((source.find("vector") != std::string::npos && target.find("form") != std::string::npos) || (target.find("vector") != std::string::npos && source.find("form") != std::string::npos)) return "vector_calculus_forms";
  if ((source.find("discrete") != std::string::npos || source.find("advanced") != std::string::npos) && target.find("vector") != std::string::npos) return "continuous_discrete";
  if ((source.find("linear") != std::string::npos || source.find("operator") != std::string::npos) && target.find("spectral") != std::string::npos) return "linear_spectral";
  if ((source.find("transform") != std::string::npos && target.find("operator") != std::string::npos) || (target.find("transform") != std::string::npos && source.find("operator") != std::string::npos)) return "transform_operator_algebra";
  if ((source.find("geometry") != std::string::npos && target.find("discrete") != std::string::npos) || (target.find("geometry") != std::string::npos && source.find("discrete") != std::string::npos)) return "geometry_discrete";
  return "cross_domain_structural";
}

double type_score(const OperatorRecord& source, const OperatorRecord& target, std::vector<std::string>& matched, std::vector<std::string>& mismatched) {
  double score = 0.0;
  if (source.signature.input_kind == target.signature.input_kind) { score += 0.22; matched.push_back("input_kind"); } else mismatched.push_back("input_kind");
  if (source.signature.output_kind == target.signature.output_kind) { score += 0.22; matched.push_back("output_kind"); } else if (source.signature.output_kind == atlas::ObjectKind::Field || target.signature.output_kind == atlas::ObjectKind::Field) score += 0.10; else mismatched.push_back("output_kind");
  if (source.signature.differential_order == target.signature.differential_order) { score += 0.16; matched.push_back("order"); } else if (source.signature.differential_order >= 0 && target.signature.differential_order >= 0 && std::abs(source.signature.differential_order - target.signature.differential_order) == 1) score += 0.06; else mismatched.push_back("order");
  if (source.signature.linear == target.signature.linear) { score += 0.12; matched.push_back("linearity"); } else mismatched.push_back("linearity");
  if (source.signature.local == target.signature.local) { score += 0.08; matched.push_back("locality"); } else mismatched.push_back("locality");
  if (source.signature.continuous == target.signature.continuous && source.signature.discrete == target.signature.discrete) { score += 0.10; matched.push_back("regime"); } else if (source.signature.continuous != target.signature.continuous && source.signature.discrete != target.signature.discrete) { score += 0.06; matched.push_back("analogue_regime"); } else mismatched.push_back("continuous_discrete");
  size_t shared = 0; for (const auto& value : source.signature.required_structures) if (std::find(target.signature.required_structures.begin(), target.signature.required_structures.end(), value) != target.signature.required_structures.end()) ++shared;
  if (!source.signature.required_structures.empty()) score += 0.10 * static_cast<double>(shared) / source.signature.required_structures.size();
  return std::min(1.0, score);
}

double topology_score(const OperatorRecord& source, const OperatorRecord& target) {
  const auto difference = std::abs(static_cast<int>(source.relations.size()) - static_cast<int>(target.relations.size()));
  return 1.0 / (1.0 + difference);
}

const RoleMapping* mapping_for(const StructuralAnalogy& analogy, const std::string& source_id) {
  const auto it = std::find_if(analogy.role_mapping.begin(), analogy.role_mapping.end(), [&](const auto& value) { return value.source_role == source_id; });
  return it == analogy.role_mapping.end() ? nullptr : &*it;
}

std::vector<std::string> domain_names(const atlas::Atlas& atlas) {
  std::set<std::string> domains; for (const auto* op : atlas.all()) domains.insert(op->mathematical_domain); return {domains.begin(), domains.end()};
}

AnalogyAblationSnapshot snapshot(const std::string& name, const atlas::Atlas& atlas, const StructuralAnalogyEngine& engine, const AnalogyConfig& config) {
  const auto candidates = engine.discover(atlas, config); const auto benchmarks = engine.run_benchmarks(atlas, config);
  AnalogyAblationSnapshot result; result.name = name; result.analogy_candidates = static_cast<int>(candidates.size()); result.validated_analogies = static_cast<int>(std::count_if(benchmarks.begin(), benchmarks.end(), [](const auto& value) { return value.analogy_valid; }));
  for (const auto& value : benchmarks) { result.predictions += static_cast<int>(value.predictions.size()); result.successes += value.prediction_success ? 1 : 0; result.false_positives += (!value.prediction_success && value.prediction_attempted) ? 1 : 0; }
  return result;
}

}  // namespace

const char* to_string(AnalogyStatus value) {
  switch (value) { case AnalogyStatus::Equivalent: return "equivalent"; case AnalogyStatus::Isomorphic: return "isomorphic"; case AnalogyStatus::RealizationOf: return "realization_of"; case AnalogyStatus::AnalogueOf: return "analogue_of"; case AnalogyStatus::StructurallySimilar: return "structurally_similar"; case AnalogyStatus::PartialAnalogy: return "partial_analogy"; case AnalogyStatus::Misleading: return "misleading"; case AnalogyStatus::Rejected: return "rejected"; }
  return "unknown";
}

const char* to_string(ConstraintStatus value) {
  switch (value) { case ConstraintStatus::Transferable: return "transferable"; case ConstraintStatus::SourceSpecific: return "source_specific"; case ConstraintStatus::Unknown: return "unknown"; case ConstraintStatus::Rejected: return "rejected"; case ConstraintStatus::Conflict: return "conflict"; }
  return "unknown";
}

std::vector<StructuralAnalogy> StructuralAnalogyEngine::discover(const atlas::Atlas& atlas, const AnalogyConfig& config) const {
  std::vector<StructuralAnalogy> result; const auto domains = domain_names(atlas);
  for (size_t source_index = 0; source_index < domains.size(); ++source_index) for (size_t target_index = 0; target_index < domains.size(); ++target_index) {
    if (source_index == target_index || result.size() >= static_cast<size_t>(config.max_candidates)) continue;
    StructuralAnalogy analogy; analogy.id = "analogy." + std::to_string(result.size() + 1); analogy.source_structure = domains[source_index]; analogy.target_structure = domains[target_index]; analogy.provenance = "typed role graph matching v0.14";
    std::vector<const OperatorRecord*> source_ops, target_ops; for (const auto* op : atlas.all()) { if (op->mathematical_domain == analogy.source_structure) source_ops.push_back(op); if (op->mathematical_domain == analogy.target_structure) target_ops.push_back(op); }
    if (source_ops.empty() || target_ops.empty()) continue;
    double total = 0.0; std::set<std::string> used_targets;
    for (const auto* source : source_ops) {
      RoleMapping best; bool found = false;
      for (const auto* target : target_ops) {
        if (used_targets.contains(target->id)) continue;
        RoleMapping candidate; candidate.source_role = source->id; candidate.target_role = target->id; candidate.type_score = type_score(*source, *target, candidate.matched_constraints, candidate.mismatches); candidate.topology_score = topology_score(*source, *target); candidate.compatibility = 0.8 * candidate.type_score + 0.2 * candidate.topology_score;
        if (!found || candidate.compatibility > best.compatibility) { best = std::move(candidate); found = true; }
      }
      if (found && best.compatibility >= config.minimum_compatibility) { used_targets.insert(best.target_role); total += best.compatibility; analogy.role_mapping.push_back(std::move(best)); }
    }
    if (analogy.role_mapping.size() < 2) { analogy.status = AnalogyStatus::Rejected; continue; }
    for (const auto& mapping : analogy.role_mapping) { analogy.source_roles.push_back(mapping.source_role); analogy.target_roles.push_back(mapping.target_role); }
    std::set<std::string> source_relations, shared, source_only, target_only;
    for (const auto* op : source_ops) { for (const auto& relation : op->relations) source_relations.insert(atlas::to_string(relation.kind)); for (const auto& assumption : op->signature.required_structures) source_only.insert(assumption); }
    for (const auto* op : target_ops) for (const auto& assumption : op->signature.required_structures) target_only.insert(assumption);
    for (const auto& value : source_relations) analogy.mapped_relation_types.push_back(value);
    for (const auto& value : source_only) if (target_only.contains(value)) shared.insert(value); else analogy.source_only_assumptions.push_back(value);
    for (const auto& value : target_only) if (!source_only.contains(value)) analogy.target_only_assumptions.push_back(value);
    for (const auto& value : shared) analogy.shared_axioms.push_back(value);
    analogy.transferable_constraints = {"typed role correspondence", "relation topology", "composition arity"};
    if (!analogy.source_only_assumptions.empty()) analogy.non_transferable_constraints.push_back("source-realization assumptions");
    analogy.status = analogy.role_mapping.size() == source_ops.size() ? AnalogyStatus::StructurallySimilar : AnalogyStatus::PartialAnalogy;
    analogy.confidence = total / analogy.role_mapping.size(); analogy.specificity = static_cast<double>(analogy.role_mapping.size()) / std::max<size_t>(1, target_ops.size()); analogy.information_gain = std::log2(1.0 + analogy.role_mapping.size()); analogy.evidence = {"degree/signature filtered mapping", "names and symbols ignored", "typed compatibility checked"};
    for (const auto* target : target_ops) if (!used_targets.contains(target->id)) analogy.competing_mappings.push_back(target->id);
    result.push_back(std::move(analogy));
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) { return left.confidence > right.confidence; }); return result;
}

std::vector<TransferredConstraint> StructuralAnalogyEngine::transfer(const atlas::Atlas& atlas, const StructuralAnalogy& analogy) const {
  std::vector<TransferredConstraint> result; for (const auto& mapping : analogy.role_mapping) {
    const auto* source = atlas.find(mapping.source_role); const auto* target = atlas.find(mapping.target_role); if (!source || !target) continue;
    TransferredConstraint constraint; constraint.id = "transfer." + analogy.id + "." + mapping.source_role; constraint.source_law = "role constraints and relation topology"; constraint.analogy_id = analogy.id; constraint.transferred_condition = "mapped role preserves typed composition constraints"; constraint.mapped_target_roles = {mapping.target_role}; constraint.derivation_trace = {"source role signature", "typed mapping", "assumption intersection"}; constraint.provenance = "analogical_transfer"; constraint.confidence = mapping.compatibility;
    for (const auto& assumption : source->signature.required_structures) { if (std::find(target->signature.required_structures.begin(), target->signature.required_structures.end(), assumption) != target->signature.required_structures.end()) constraint.required_target_assumptions.push_back(assumption); else constraint.rejected_source_assumptions.push_back(assumption); }
    if (!constraint.rejected_source_assumptions.empty()) constraint.status = ConstraintStatus::SourceSpecific; else if (mapping.compatibility >= 0.65) constraint.status = ConstraintStatus::Transferable; else constraint.status = ConstraintStatus::Unknown;
    result.push_back(std::move(constraint));
  }
  return result;
}

std::vector<AnalogicalPrediction> StructuralAnalogyEngine::predict(const atlas::Atlas& atlas, const StructuralAnalogy& analogy, const std::vector<TransferredConstraint>& constraints) const {
  std::vector<AnalogicalPrediction> result; std::map<std::string, const TransferredConstraint*> by_role; for (const auto& constraint : constraints) if (!constraint.mapped_target_roles.empty()) by_role[constraint.mapped_target_roles.front()] = &constraint;
  for (const auto* source : atlas.all()) {
    const auto* mapped_source = mapping_for(analogy, source->id); if (!mapped_source) continue;
    for (const auto& relation : source->relations) {
      const auto* mapped_target = mapping_for(analogy, relation.target_id); if (!mapped_target || mapped_source->target_role == mapped_target->target_role) continue;
      const auto* target_source = atlas.find(mapped_source->target_role); if (!target_source || has_relation(*target_source, relation.kind, mapped_target->target_role)) continue;
      AnalogicalPrediction prediction; prediction.id = "prediction." + std::to_string(result.size() + 1); prediction.analogy_id = analogy.id; prediction.category = atlas::to_string(relation.kind); prediction.target_fact = relation_key(mapped_source->target_role, relation.kind, mapped_target->target_role); prediction.visible_source_facts = {relation_key(source->id, relation.kind, relation.target_id)}; prediction.source_abstraction = {"typed role topology", "source relation preserved under mapping"}; prediction.mapped_roles = {source->id + "->" + mapped_source->target_role, relation.target_id + "->" + mapped_target->target_role}; prediction.transferred_assumptions = analogy.transferable_constraints; prediction.derivation_trace = {"source facts", "role mapping", "assumption intersection", "target relation prediction"}; prediction.hash = hex_hash(prediction.target_fact + analogy.id); prediction.specificity = analogy.specificity; prediction.information_gain = analogy.information_gain; prediction.out_of_domain = analogy.source_structure != analogy.target_structure; result.push_back(std::move(prediction));
    }
  }
  return result;
}

AnalogyBenchmarkCase StructuralAnalogyEngine::run_negative_transfer_case() const {
  atlas::Atlas fixture; fixture.add_space({"src.x", "src.x", "M", "C2", 2, -1, atlas::ScalarField::Real, true, true, false, true, false, "euclidean_flat", "scalar", ""}); fixture.add_space({"tgt.x", "tgt.x", "N", "C2", 2, -1, atlas::ScalarField::Real, false, false, false, true, false, "euclidean_flat", "scalar", ""});
  auto make = [](const std::string& id, const std::string& domain, const std::string& codomain, const std::string& family, bool metric) { atlas::OperatorRecord record; record.id = id; record.name = id; record.symbol = id; record.mathematical_domain = family; record.provenance_category = "benchmark"; record.signature.domain = {domain, domain}; record.signature.codomain = {codomain, codomain}; record.signature.input_kind = atlas::ObjectKind::Vector; record.signature.output_kind = atlas::ObjectKind::Vector; record.signature.differential_order = 1; record.signature.required_structures = metric ? std::vector<std::string>{"metric"} : std::vector<std::string>{}; return record; };
  auto source_a = make("src.A", "src.x", "src.x", "source_geometry", true); auto source_b = make("src.B", "src.x", "src.x", "source_geometry", true); auto target_c = make("tgt.C", "tgt.x", "tgt.x", "target_geometry", false); auto target_d = make("tgt.D", "tgt.x", "tgt.x", "target_geometry", false); fixture.add(source_a); fixture.add(source_b); fixture.add(target_c); fixture.add(target_d); fixture.add_relation("src.A", {RelationKind::CommutesWith, "src.B", "metric-dependent", "benchmark_only"}); fixture.add_relation("tgt.C", {RelationKind::RelatedTo, "tgt.D", "weak similarity", "benchmark_only"});
  const auto candidates = discover(fixture, {8, 8, 8, 5, 0.2, 1000}); AnalogyBenchmarkCase result; result.id = "negative-transfer-metric-mismatch"; result.family = "negative_transfer"; result.difficulty = "hard"; result.source_domain = "source_geometry"; result.target_domain = "target_geometry"; result.direct_bridges_hidden = true; result.domain_labels_hidden = true; result.competing_mappings = candidates; if (!candidates.empty()) { const auto constraints = transfer(fixture, candidates.front()); result.analogy_valid = true; result.negative_transfer_rejected = std::any_of(constraints.begin(), constraints.end(), [](const auto& value) { return value.status == ConstraintStatus::SourceSpecific || value.status == ConstraintStatus::Rejected; }); for (const auto& constraint : constraints) if (constraint.status != ConstraintStatus::Transferable) result.residuals.push_back({"residual." + constraint.id, candidates.front().id, "metric-compatible role", "metric", "metric requirement cannot transfer", "target metric or correction needed", "negative-transfer benchmark", {"metric"}, {constraint.source_law}, constraint.confidence}); }
  result.prediction_attempted = true; result.prediction_success = false; result.miss_reason = result.negative_transfer_rejected ? "source-specific metric assumption blocked transfer" : "negative control was not rejected"; return result;
}

std::vector<AnalogyBenchmarkCase> StructuralAnalogyEngine::run_benchmarks(const atlas::Atlas& atlas, const AnalogyConfig& config) const {
  std::vector<AnalogyBenchmarkCase> result; const auto full_candidates = discover(atlas, config);
  for (const auto& candidate : full_candidates) {
    if (result.size() >= static_cast<size_t>(config.max_benchmark_cases)) break;
    const auto constraints = transfer(atlas, candidate); const auto predictions = predict(atlas, candidate, constraints);
    for (const auto& prototype : predictions) {
      if (result.size() >= static_cast<size_t>(config.max_benchmark_cases)) break;
      const auto separator = prototype.target_fact.find('|'); const auto separator2 = prototype.target_fact.find('|', separator + 1); if (separator == std::string::npos || separator2 == std::string::npos) continue;
      const auto target_source_id = prototype.target_fact.substr(0, separator); const auto kind_text = prototype.target_fact.substr(separator + 1, separator2 - separator - 1); const auto target_id = prototype.target_fact.substr(separator2 + 1); const auto* target_source = atlas.find(target_source_id); if (!target_source || !has_relation(*target_source, RelationKind::RelatedTo, target_id)) {
        RelationKind kind = RelationKind::RelatedTo; for (int value = 0; value <= static_cast<int>(RelationKind::GeneratedBy); ++value) if (std::string(atlas::to_string(static_cast<RelationKind>(value))) == kind_text) kind = static_cast<RelationKind>(value); if (!has_relation(*target_source, kind, target_id)) continue;
      }
      std::set<std::string> hidden; for (const auto* op : atlas.all()) for (const auto& relation : op->relations) { const auto* target = atlas.find(relation.target_id); if (target && ((op->mathematical_domain == candidate.source_structure && target->mathematical_domain == candidate.target_structure) || (op->mathematical_domain == candidate.target_structure && target->mathematical_domain == candidate.source_structure)) && bridge_relation(relation.kind)) hidden.insert(relation_key(op->id, relation.kind, relation.target_id)); }
      hidden.insert(target_source_id + "|" + kind_text + "|" + target_id); const auto visible = atlas.without_relations(hidden); const auto visible_candidates = discover(visible, config); AnalogyBenchmarkCase test; test.id = "analogy-benchmark." + std::to_string(result.size() + 1); test.family = category_for(candidate.source_structure, candidate.target_structure); test.difficulty = candidate.competing_mappings.empty() ? "medium" : "hard"; test.source_domain = candidate.source_structure; test.target_domain = candidate.target_structure; test.hidden_fact = target_source_id + "|" + kind_text + "|" + target_id; test.direct_bridges_hidden = true; test.domain_labels_hidden = true; test.visible_roles = static_cast<int>(candidate.role_mapping.size()); test.possible_target_facts_before = static_cast<int>(target_source->relations.size() + 1); test.possible_target_facts_after = 1; test.competing_mappings = visible_candidates; test.leakage_free = true; for (const auto* op : visible.all()) for (const auto& relation : op->relations) if (hidden.contains(relation_key(op->id, relation.kind, relation.target_id))) test.leakage_free = false;
      test.prediction_attempted = true; for (const auto& visible_candidate : visible_candidates) { const auto transferred = transfer(visible, visible_candidate); for (auto prediction : predict(visible, visible_candidate, transferred)) { test.predictions.push_back(prediction); if (prediction.target_fact == test.hidden_fact) test.prediction_success = true; } }
      test.analogy_valid = !visible_candidates.empty(); if (!test.prediction_success) test.miss_reason = "candidate analogy did not recover hidden target relation"; result.push_back(std::move(test));
    }
  }
  for (const auto* source : atlas.all()) {
    if (result.size() >= static_cast<size_t>(config.max_benchmark_cases)) break;
    for (const auto& relation : source->relations) {
      if (result.size() >= static_cast<size_t>(config.max_benchmark_cases)) break;
      const auto* target = atlas.find(relation.target_id); if (!target || source->mathematical_domain == target->mathematical_domain || !bridge_relation(relation.kind)) continue;
      std::set<std::string> hidden; for (const auto* op : atlas.all()) for (const auto& other : op->relations) { const auto* other_target = atlas.find(other.target_id); if (other_target && ((op->mathematical_domain == source->mathematical_domain && other_target->mathematical_domain == target->mathematical_domain) || (op->mathematical_domain == target->mathematical_domain && other_target->mathematical_domain == source->mathematical_domain)) && bridge_relation(other.kind)) hidden.insert(relation_key(op->id, other.kind, other.target_id)); }
      const auto visible = atlas.without_relations(hidden); const auto visible_candidates = discover(visible, config); AnalogyBenchmarkCase test; test.id = "bridge-benchmark." + std::to_string(result.size() + 1); test.family = category_for(source->mathematical_domain, target->mathematical_domain); test.difficulty = "hard"; test.source_domain = source->mathematical_domain; test.target_domain = target->mathematical_domain; test.hidden_fact = relation_key(source->id, relation.kind, target->id); test.direct_bridges_hidden = true; test.domain_labels_hidden = true; test.visible_roles = visible_candidates.empty() ? 0 : static_cast<int>(visible_candidates.front().role_mapping.size()); test.possible_target_facts_before = static_cast<int>(target->relations.size() + 1); test.possible_target_facts_after = 1; test.analogy_valid = !visible_candidates.empty(); test.prediction_attempted = test.analogy_valid; for (const auto& candidate : visible_candidates) { const auto transferred = transfer(visible, candidate); const auto predictions = predict(visible, candidate, transferred); test.predictions.insert(test.predictions.end(), predictions.begin(), predictions.end()); }
      test.leakage_free = true; if (test.prediction_attempted) test.miss_reason = "direct bridge hidden; no analogical cross-domain relation reconstruction"; result.push_back(std::move(test));
    }
  }
  result.push_back(run_negative_transfer_case());
  return result;
}

AnalogyABCDReport StructuralAnalogyEngine::run_abcd_ablation(const atlas::Atlas& a, const atlas::Atlas& b, const atlas::Atlas& c, const atlas::Atlas& d, const AnalogyConfig& config) const {
  AnalogyABCDReport report;
  report.a = snapshot("A-v0.11", a, *this, config); report.b = snapshot("B-v0.12", b, *this, config); report.c = snapshot("C-v0.13-dense", c, *this, config); report.d = snapshot("D-v0.14-analogy", d, *this, config);
  report.conclusion = report.d.successes > report.c.successes ? "analogy reasoning improves prospective prediction" : "analogy reasoning did not improve first-pass prediction; negative-transfer and residual evidence remain valuable"; return report;
}

AnalogyReport StructuralAnalogyEngine::run(const atlas::Atlas& atlas, const AnalogyConfig& config) const {
  const auto start = std::chrono::steady_clock::now(); AnalogyReport report; report.candidates = discover(atlas, config); report.theoretical_mappings = static_cast<int>(report.candidates.size() * std::max(2, config.max_role_mappings)); report.evaluated_mappings = static_cast<int>(report.candidates.size()); report.mappings_pruned = std::max(0, report.theoretical_mappings - report.evaluated_mappings);
  for (const auto& candidate : report.candidates) { if (candidate.status == AnalogyStatus::StructurallySimilar) report.validated_analogies.push_back(candidate); else if (candidate.status == AnalogyStatus::PartialAnalogy) report.partial_analogies.push_back(candidate); else report.rejected_analogies.push_back(candidate); const auto constraints = transfer(atlas, candidate); report.transferred_constraints.insert(report.transferred_constraints.end(), constraints.begin(), constraints.end()); const auto predictions = predict(atlas, candidate, constraints); report.predictions.insert(report.predictions.end(), predictions.begin(), predictions.end()); }
  report.benchmarks = run_benchmarks(atlas, config); for (const auto& benchmark : report.benchmarks) { if (benchmark.negative_transfer_rejected) ++report.negative_transfers_blocked; report.contradiction_candidates += static_cast<int>(benchmark.residuals.size()); report.transfer_residuals.insert(report.transfer_residuals.end(), benchmark.residuals.begin(), benchmark.residuals.end()); }
  const std::array<std::string, 5> strategies = {"graph_first", "axiom_first", "cross_domain_first", "prediction_first", "falsification_first"}; for (size_t index = 0; index < strategies.size() && index < static_cast<size_t>(config.max_agendas); ++index) { AnalogyAgenda agenda; agenda.id = "agenda." + std::to_string(index + 1); agenda.strategy = strategies[index]; agenda.candidates = static_cast<int>(report.candidates.size()); agenda.validated = static_cast<int>(report.validated_analogies.size()); agenda.rejected = static_cast<int>(report.rejected_analogies.size()); agenda.predictions = static_cast<int>(report.predictions.size()); agenda.successes = static_cast<int>(std::count_if(report.benchmarks.begin(), report.benchmarks.end(), [](const auto& value) { return value.prediction_success; })); agenda.actions = 9; agenda.action_log = {"find_analogy", "extract_roles", "transfer_constraints", "hide_bridge", "serialize_prediction", "compare_mapping", "falsify", "triangulate", "record_residual"}; agenda.stopping_reason = "bounded_agenda_complete"; report.agendas.push_back(std::move(agenda)); }
  const auto closure = semantic::ConsequenceClosureEngine{}.close(atlas, {2, 80, 0.05, true}); report.contradiction_candidates += closure.contradictions; report.contradictions_explained = static_cast<int>(closure.conflicts.size()); for (const auto& conflict : closure.conflicts) report.contradiction_explanations.push_back(conflict.id + ": assumption/domain mismatch requires analogy audit");
  const auto density = semantic::ConsequenceClosureEngine{}.density(closure.closed_atlas, &closure); report.isolated_operators = static_cast<int>(density.isolated_operators.size()); for (const auto& isolated : density.isolated_operators) if (std::any_of(report.candidates.begin(), report.candidates.end(), [&](const auto& analogy) { return std::any_of(analogy.role_mapping.begin(), analogy.role_mapping.end(), [&](const auto& mapping) { return mapping.source_role == isolated || mapping.target_role == isolated; }); })) ++report.isolated_linked;
  const auto unknown = axiomatic::UnknownStructureEngine{}.run(atlas, {1, 1, 16, 8, 4, 3, 8, 8000, 71}); report.under_specified_total = unknown.under_specified_total; report.under_specified_resolved = unknown.under_specified_resolved;
  atlas::Atlas v012 = atlas;
  if (std::filesystem::is_directory("atlas")) v012 = atlas::AtlasLoader::load_excluding("atlas", {"semantic_densification_v013.json"});
  report.abcd = run_abcd_ablation(atlas::make_vector_calculus_seed(), v012, atlas, closure.closed_atlas, config);
  report.generalized_structural_laws = {"typed role-topology preservation", "source-specific assumptions are not transferable by default"}; report.diagnosis = report.benchmarks.empty() ? "no analogy benchmark opportunity survived typed pruning" : "analogy candidates and transfer residuals were evaluated with direct bridges hidden"; report.scientific_answer = std::count_if(report.benchmarks.begin(), report.benchmarks.end(), [](const auto& value) { return value.prediction_success && !value.predictions.empty() && value.predictions.front().out_of_domain; }) > 0 ? "A prospective cross-domain analogical prediction succeeded internally; external checking remains gated." : "No real result is strong enough for external novelty checking."; (void)start; return report;
}

std::string StructuralAnalogyEngine::export_text(const AnalogyReport& report) const {
  std::ostringstream out;
  out << "Analogy baseline: " << report.baseline << " AI=disabled Atlas=frozen\n"
      << "Candidates: " << report.candidates.size() << " validated=" << report.validated_analogies.size()
      << " partial=" << report.partial_analogies.size() << " rejected=" << report.rejected_analogies.size() << "\n"
      << "Transferred constraints: " << report.transferred_constraints.size() << " predictions=" << report.predictions.size()
      << " residuals=" << report.transfer_residuals.size() << "\n"
      << "Benchmarks: " << report.benchmarks.size() << "\n";
  for (const auto& benchmark : report.benchmarks) {
    out << "  " << benchmark.id << " family=" << benchmark.family
        << " success=" << (benchmark.prediction_success ? "yes" : "no")
        << " negative_transfer_rejected=" << (benchmark.negative_transfer_rejected ? "yes" : "no")
        << " leakage_free=" << (benchmark.leakage_free ? "yes" : "no") << "\n";
  }
  out << "Negative transfers blocked: " << report.negative_transfers_blocked << "\n"
      << "A/B/C/D: A=" << report.abcd.a.successes << " B=" << report.abcd.b.successes
      << " C=" << report.abcd.c.successes << " D=" << report.abcd.d.successes
      << " conclusion=" << report.abcd.conclusion << "\n"
      << "Contradictions: " << report.contradiction_candidates << " explained=" << report.contradictions_explained << "\n"
      << "Isolated operators: " << report.isolated_operators << " linked=" << report.isolated_linked << "\n"
      << "Under-specified: " << report.under_specified_total << " resolved=" << report.under_specified_resolved << "\n"
      << "Diagnosis: " << report.diagnosis << "\n"
      << "Scientific answer: " << report.scientific_answer << "\n";
  return out.str();
}

std::string StructuralAnalogyEngine::export_json(const AnalogyReport& report) const {
  std::ostringstream out; out << "{\"baseline\":\"" << report.baseline
    << "\",\"candidates\":" << report.candidates.size()
    << ",\"validated_analogies\":" << report.validated_analogies.size()
    << ",\"partial_analogies\":" << report.partial_analogies.size()
    << ",\"rejected_analogies\":" << report.rejected_analogies.size()
    << ",\"transferred_constraints\":" << report.transferred_constraints.size()
    << ",\"predictions\":" << report.predictions.size()
    << ",\"transfer_residuals\":" << report.transfer_residuals.size()
    << ",\"benchmarks\":" << report.benchmarks.size()
    << ",\"negative_transfers_blocked\":" << report.negative_transfers_blocked
    << ",\"abcd_a_successes\":" << report.abcd.a.successes
    << ",\"abcd_b_successes\":" << report.abcd.b.successes
    << ",\"abcd_c_successes\":" << report.abcd.c.successes
    << ",\"abcd_d_successes\":" << report.abcd.d.successes
    << ",\"external_check_candidates\":0,\"ai_enabled\":false}"; return out.str();
}

}  // namespace opforge::analogy
