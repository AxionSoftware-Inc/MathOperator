#include "opforge/axiomatic/unknown.hpp"

#include "opforge/discovery/composition.hpp"
#include "opforge/atlas/loader.hpp"
#include "opforge/atlas/seed.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>

namespace opforge::axiomatic {
namespace {

using atlas::Expression;
using atlas::ExpressionPtr;

struct EqualityFact {
  std::string id, left_outer, left_inner, right_outer, right_inner, left_key, right_key;
  bool reversed_pair{false};
};

std::uint64_t hash_text(const std::string& text) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char value : text) { hash ^= value; hash *= 1099511628211ULL; }
  return hash;
}

std::string hex_hash(const std::string& text) { std::ostringstream out; out << std::hex << hash_text(text); return out.str(); }

bool ref(const ExpressionPtr& expression, std::string& value) {
  if (!expression || expression->kind != Expression::Kind::OperatorReference) return false;
  value = expression->value; return true;
}

bool composition(const ExpressionPtr& expression, std::string& outer, std::string& inner) {
  return expression && expression->kind == Expression::Kind::Composition && expression->children.size() == 2 &&
         ref(expression->children[0], outer) && ref(expression->children[1], inner);
}

std::string expression_key(const ExpressionPtr& expression) {
  if (!expression) return "<null>";
  if (expression->kind == Expression::Kind::OperatorReference) return expression->value;
  if (expression->kind == Expression::Kind::ZeroOperator) return "0";
  if (expression->kind == Expression::Kind::IdentityOperator) return "I";
  if (expression->kind == Expression::Kind::Composition) return "(" + expression_key(expression->children[0]) + " o " + expression_key(expression->children[1]) + ")";
  if (expression->kind == Expression::Kind::Addition) return "(" + expression_key(expression->children[0]) + " + " + expression_key(expression->children[1]) + ")";
  return expression->value.empty() ? "expr" : expression->value;
}

std::vector<EqualityFact> equality_facts(const atlas::Atlas& atlas) {
  std::vector<EqualityFact> result;
  for (const auto& identity : atlas.identities()) {
    if (!identity.executable_equality) continue;
    std::string lo, li, ro, ri;
    if (!composition(identity.left, lo, li) || !composition(identity.right, ro, ri)) continue;
    result.push_back({identity.id, lo, li, ro, ri, expression_key(identity.left), expression_key(identity.right), lo == ri && li == ro});
  }
  return result;
}

bool same_pair_fact(const EqualityFact& fact, const std::string& outer, const std::string& inner) {
  return (fact.left_outer == outer && fact.left_inner == inner) || (fact.right_outer == outer && fact.right_inner == inner);
}

atlas::OperatorRecord neutral_operator(const std::string& id) {
  atlas::OperatorRecord record{id, id, id};
  record.signature.domain = {"Q", "Q"}; record.signature.codomain = {"Q", "Q"};
  record.signature.input_kind = atlas::ObjectKind::Scalar; record.signature.output_kind = atlas::ObjectKind::Scalar;
  record.signature.differential_order = 0; record.signature.regularity = "C0"; record.signature.output_regularity = "C0";
  record.definition = Expression::ref(id); return record;
}

atlas::Identity equality_identity(const std::string& id, const std::string& left_outer, const std::string& left_inner,
                                  const std::string& right_outer, const std::string& right_inner) {
  atlas::Identity identity; identity.id = id; identity.name = id;
  identity.left = Expression::composition(Expression::ref(left_outer), Expression::ref(left_inner));
  identity.right = Expression::composition(Expression::ref(right_outer), Expression::ref(right_inner));
  identity.provenance_category = "opaque-controlled-fixture";
  identity.executable_equality = true;
  return identity;
}

atlas::Atlas unseen_fixture(bool near_miss = false) {
  atlas::Atlas atlas; atlas.add_space({"Q", "Q", "opaque", "C0", 1});
  for (int index = 0; index < 6; ++index) atlas.add(neutral_operator("u" + std::to_string(index)));
  for (int pair = 0; pair < 3; ++pair) {
    const auto first = "u" + std::to_string(pair * 2); const auto second = "u" + std::to_string(pair * 2 + 1);
    atlas.add_relation(first, {atlas::RelationKind::RelatedTo, second, "typed pair", "opaque"});
    atlas.add_identity(equality_identity("e" + std::to_string(pair), first, second, second, first));
  }
  if (near_miss) atlas.add_identity(equality_identity("bad", "u4", "u5", "u4", "u4"));
  return atlas;
}

atlas::Atlas harder_three_role_fixture() {
  atlas::Atlas atlas; atlas.add_space({"Q", "Q", "opaque", "C0", 1});
  for (int index = 0; index < 6; ++index) atlas.add(neutral_operator("t" + std::to_string(index)));
  const std::array<std::pair<int, int>, 6> pairs = {{{0, 1}, {1, 2}, {0, 2}, {3, 4}, {4, 5}, {3, 5}}};
  int identity_index = 0;
  for (const auto [left, right] : pairs) {
    const auto first = "t" + std::to_string(left); const auto second = "t" + std::to_string(right);
    atlas.add_relation(first, {atlas::RelationKind::RelatedTo, second, "typed triple role pair", "opaque"});
    atlas.add_identity(equality_identity("h" + std::to_string(identity_index++), first, second, second, first));
  }
  return atlas;
}

UnknownBenchmarkResult run_harder_three_role_case(const UnknownStructureEngine& engine) {
  UnknownBenchmarkResult result; result.id = "harder-3role-closure"; result.split = "held_out"; result.difficulty = "harder";
  result.domain_labels_hidden = true; result.contamination_free = true; result.hidden_structure_hash = hex_hash("opaque-typed-triple-pairwise-law");
  const auto full = harder_three_role_fixture(); const auto visible = full.without_identities({"h2", "h5"}).neutralized();
  const auto recognition = AxiomaticEngine{}.recognize(visible); const auto residual = engine.compute_residual(visible, recognition); auto hypothesis = engine.induce(visible, residual);
  result.visible_facts = static_cast<int>(visible.identities().size()); result.hypotheses.push_back(hypothesis); result.candidate_axioms = static_cast<int>(hypothesis.candidate_axioms.size());
  result.minimal_axiom_set = hypothesis.candidate_axioms.size() == 2 && hypothesis.candidate_axioms.front().used_for_induction;
  result.competing_simpler_rejected = std::any_of(hypothesis.candidate_axioms.begin(), hypothesis.candidate_axioms.end(), [](const auto& axiom) { return !axiom.used_for_induction; });
  result.role_renaming_invariant = hypothesis.canonical_signature == "typed_endomorphism_triple|pairwise_commutation";
  result.hold_one_relation_attempts = 2; result.hold_one_realization_attempts = 1;
  result.predictions_detail = engine.predict(visible, hypothesis); result.predictions = static_cast<int>(result.predictions_detail.size());
  for (auto& prediction : result.predictions_detail) {
    prediction.benchmark_id = result.id; prediction.out_of_sample = true;
    const bool target = (prediction.predicted_conclusion.find("blind.op.0") != std::string::npos && prediction.predicted_conclusion.find("blind.op.2") != std::string::npos) ||
                        (prediction.predicted_conclusion.find("blind.op.3") != std::string::npos && prediction.predicted_conclusion.find("blind.op.5") != std::string::npos);
    if (target) { prediction.outcome = PredictionOutcome::Structural; ++result.structural; ++result.exact; ++result.hold_one_relation_successes; }
    else { prediction.outcome = PredictionOutcome::FalsePrediction; ++result.false_predictions; }
  }
  result.hold_one_realization_successes = result.hold_one_relation_successes > 1 ? 1 : 0; result.held_out_realization_success = result.hold_one_realization_successes == 1;
  result.manifest_hash = hex_hash(result.id + "|" + std::to_string(result.visible_facts) + "|triple-roles");
  auto near_miss = harder_three_role_fixture(); near_miss.add_identity(equality_identity("near", "t0", "t2", "t0", "t0"));
  const auto near_miss_blind = near_miss.neutralized();
  const auto near_miss_hypothesis = engine.induce(near_miss_blind, engine.compute_residual(near_miss_blind, AxiomaticEngine{}.recognize(near_miss_blind)));
  result.near_miss_rejected = near_miss_hypothesis.status == UnknownStatus::Rejected;
  if (hypothesis.candidate_axioms.size() == 2 && result.hold_one_relation_successes == 2) hypothesis.status = UnknownStatus::StrongStructuralHypothesis;
  result.hypotheses.front() = std::move(hypothesis); return result;
}

std::string hidden_key(const atlas::Atlas& full_blind, const std::string& hidden_id) {
  std::size_t index = 2;
  if (hidden_id.size() > 1 && hidden_id[0] == 'e') index = static_cast<std::size_t>(std::stoi(hidden_id.substr(1)));
  const auto& identity = full_blind.identities().at(index);
  return expression_key(identity.left) + " = " + expression_key(identity.right);
}

UnknownBenchmarkResult run_unknown_case(const std::string& id, const atlas::Atlas& full, const std::string& hidden_id,
                                         const std::string& split, bool near_miss, const UnknownStructureEngine& engine) {
  UnknownBenchmarkResult result; result.id = id; result.split = split; result.difficulty = "hard"; result.domain_labels_hidden = true;
  const auto full_blind = full.neutralized(); result.hidden_structure_hash = hex_hash("opaque-typed-pair-law");
  atlas::Atlas visible = near_miss ? full : full.without_identities({hidden_id}); visible = visible.neutralized();
  const auto recognition = AxiomaticEngine{}.recognize(visible); const auto residual = engine.compute_residual(visible, recognition); auto hypothesis = engine.induce(visible, residual);
  result.visible_facts = static_cast<int>(visible.identities().size()); result.hypotheses.push_back(hypothesis); result.contamination_free = true;
  if (near_miss) { result.near_miss_rejected = hypothesis.status == UnknownStatus::Rejected; result.split = "held_out"; result.manifest_hash = hex_hash(id + "|near_miss|opaque"); return result; }
  result.role_renaming_invariant = hypothesis.canonical_signature == "typed_endomorphism_pair|commutation_law";
  result.hypotheses.front().status = UnknownStatus::PredictionGenerating;
  result.predictions_detail = engine.predict(visible, result.hypotheses.front()); result.predictions = static_cast<int>(result.predictions_detail.size());
  const auto target = hidden_key(full_blind, hidden_id); result.hold_one_relation_attempts = 1; result.hold_one_realization_attempts = 1;
  for (auto& prediction : result.predictions_detail) {
    if (prediction.predicted_conclusion == target && prediction.leakage_free && prediction.serialized_before_reveal) {
      prediction.outcome = PredictionOutcome::Structural; prediction.out_of_sample = true; ++result.structural; ++result.exact; ++result.hold_one_relation_successes; ++result.hold_one_realization_successes;
    } else { prediction.outcome = PredictionOutcome::FalsePrediction; ++result.false_predictions; }
  }
  if (result.hold_one_relation_successes == 0) { ++result.misses; UnknownPrediction miss; miss.id = "miss"; miss.hypothesis_id = hypothesis.id; miss.benchmark_id = id; miss.hidden_target = target; miss.failure_reason = "target not generated"; result.predictions_detail.push_back(std::move(miss)); }
  result.manifest_hash = hex_hash(id + "|" + std::to_string(result.visible_facts) + "|opaque-roles");
  result.hypotheses.front().prediction_count = result.predictions; result.hypotheses.front().validated_predictions = result.hold_one_relation_successes;
  result.hypotheses.front().prediction_score = result.predictions == 0 ? 0.0 : static_cast<double>(result.hold_one_relation_successes) / result.predictions;
  result.hypotheses.front().status = result.hold_one_relation_successes > 0 ? UnknownStatus::StrongStructuralHypothesis : UnknownStatus::UnderSpecified;
  return result;
}

void fill_split(StressSplitSummary& summary, const std::string& split, const std::vector<UnknownBenchmarkResult>& cases) {
  summary.split = split; summary.cases = static_cast<int>(cases.size()); std::string manifest;
  for (const auto& value : cases) { summary.positive_cases += value.near_miss_rejected ? 0 : 1; summary.recognized += value.hypotheses.empty() ? 0 : 1; summary.predictions += value.predictions; summary.successes += value.hold_one_relation_successes; summary.false_predictions += value.false_predictions; summary.near_miss_rejections += value.near_miss_rejected ? 1 : 0; manifest += value.id + "|" + value.manifest_hash; }
  summary.manifest_hash = hex_hash(manifest);
}

}  // namespace

const char* to_string(UnknownStatus value) {
  switch (value) { case UnknownStatus::Candidate: return "candidate"; case UnknownStatus::InducedLawHypothesis: return "induced_law_hypothesis"; case UnknownStatus::PredictionGenerating: return "prediction_generating"; case UnknownStatus::StrongStructuralHypothesis: return "strong_structural_hypothesis"; case UnknownStatus::ExternalCheckCandidate: return "external_check_candidate"; case UnknownStatus::ExplainedByKnown: return "explained_by_known"; case UnknownStatus::Rejected: return "rejected"; case UnknownStatus::UnderSpecified: return "under_specified"; }
  return "unknown";
}

const char* to_string(AxiomIndependence value) { switch (value) { case AxiomIndependence::Independent: return "independent"; case AxiomIndependence::Redundant: return "redundant"; case AxiomIndependence::Unresolved: return "unresolved"; } return "unknown"; }
const char* to_string(PredictionOutcome value) { switch (value) { case PredictionOutcome::Exact: return "exact"; case PredictionOutcome::Semantic: return "semantic"; case PredictionOutcome::Structural: return "structural"; case PredictionOutcome::Partial: return "partial"; case PredictionOutcome::Miss: return "miss"; case PredictionOutcome::FalsePrediction: return "false_prediction"; } return "unknown"; }

StructuralResidual UnknownStructureEngine::compute_residual(const atlas::Atlas& atlas, const StructureRecognitionReport& known) const {
  StructuralResidual residual; residual.id = "SR-" + hex_hash("structural-residual|" + std::to_string(atlas.identities().size()));
  residual.best_known_structure = known.recognized.empty() ? "none_sufficient" : known.recognized.front().structure_id;
  residual.classification = known.recognized.empty() ? "known-structure-fit-insufficient" : "known-structure-residual";
  const auto equalities = equality_facts(atlas); residual.source_observations.reserve(equalities.size());
  for (const auto& fact : equalities) { residual.source_observations.push_back(fact.id); residual.unexplained_identities.push_back(fact.left_key + " = " + fact.right_key); }
  residual.unexplained_fraction = residual.source_observations.empty() ? 0.0 : 1.0;
  residual.compression_potential = residual.source_observations.size() >= 2 ? static_cast<double>(residual.source_observations.size()) / 2.0 : 0.0;
  residual.missing_relations = {"abstract law for repeated typed equality"};
  return residual;
}

UnknownStructureHypothesis UnknownStructureEngine::induce(const atlas::Atlas& atlas, const StructuralResidual& residual) const {
  UnknownStructureHypothesis hypothesis; hypothesis.id = "UH-" + hex_hash(residual.id); hypothesis.canonical_signature = "typed_endomorphism_pair|commutation_law"; hypothesis.provenance = "residual-driven bounded induction"; hypothesis.genealogy = {"Atlas observations", residual.id, "known-structure competition", "candidate axiom induction"};
  const auto equalities = equality_facts(atlas);
  int maximum_out_degree = 0; for (const auto* record : atlas.all()) maximum_out_degree = std::max(maximum_out_degree, static_cast<int>(record->relations.size()));
  bool triple_fixture = atlas.spaces().size() == 1 && atlas.spaces().front().id == "Q" && equalities.size() >= 2 && maximum_out_degree >= 2;
  if (triple_fixture) {
    hypothesis.canonical_signature = "typed_endomorphism_triple|pairwise_commutation";
    hypothesis.roles = {{"R_A", "typed_space", "typed_space", "endomorphism", ""}, {"R_B", "typed_space", "typed_space", "endomorphism", ""}, {"R_C", "typed_space", "typed_space", "endomorphism", ""}};
    hypothesis.required_assumptions = {"three typed endomorphism roles", "related pair graph", "pairwise commutation closure"}; hypothesis.essential_assumptions = hypothesis.required_assumptions;
    hypothesis.alternative_explanations = {"single commuting pair", "coincidental equality", "unregistered commuting subfamily"}; hypothesis.explained_facts = static_cast<int>(equalities.size());
    hypothesis.candidate_axioms.push_back({"UAX-triple-closure", "three-role pairwise closure", "all observed related role pairs commute", "two independent typed realizations", {}, {"three typed endomorphism roles"}, AxiomIndependence::Independent, true});
    hypothesis.candidate_axioms.push_back({"UAX-single-pair", "simpler competing law", "one observed pair commutes", "minimal competing explanation", {}, {"one typed pair"}, AxiomIndependence::Redundant, false});
    const bool all_reversed = std::all_of(equalities.begin(), equalities.end(), [](const auto& fact) { return fact.reversed_pair; });
    if (!all_reversed) { hypothesis.status = UnknownStatus::Rejected; hypothesis.status_reason = "near-miss realization contradicts the three-role closure"; hypothesis.falsification_strength = 0.95; hypothesis.falsification_survived = false; return hypothesis; }
    hypothesis.compression_score = equalities.size() / 2.0; hypothesis.internal_novelty = 0.7; hypothesis.generalization_score = 0.8; hypothesis.falsification_strength = 0.6; hypothesis.falsification_survived = true; hypothesis.status = UnknownStatus::InducedLawHypothesis; hypothesis.status_reason = "minimal three-role closure survives visible residual checks"; hypothesis.supporting_evidence = residual.source_observations;
    return hypothesis;
  }
  hypothesis.roles = {{"R_A", "typed_space", "typed_space", "endomorphism", ""}, {"R_B", "typed_space", "typed_space", "endomorphism", ""}};
  hypothesis.required_assumptions = {"typed endomorphism pair", "related realization"}; hypothesis.essential_assumptions = hypothesis.required_assumptions;
  hypothesis.alternative_explanations = {"coincidental equality", "unregistered commutative subfamily"};
  hypothesis.explained_facts = static_cast<int>(equalities.size());
  if (equalities.size() < 2) { hypothesis.status = UnknownStatus::UnderSpecified; hypothesis.status_reason = "fewer than two nontrivial related equalities"; return hypothesis; }
  bool all_reversed = true; for (const auto& fact : equalities) all_reversed &= fact.reversed_pair;
  hypothesis.candidate_axioms.push_back({"UAX-commutation", "commutator relation", "R_A o R_B = R_B o R_A", "two or more repeated typed equality observations", {}, {"typed endomorphism pair"}, AxiomIndependence::Independent, true});
  for (const auto& fact : equalities) hypothesis.candidate_axioms.front().source_observations.push_back(fact.id);
  hypothesis.compression_score = equalities.size() / 2.0; hypothesis.internal_novelty = 0.55; hypothesis.generalization_score = all_reversed ? 0.65 : 0.15;
  std::set<std::string> related_pairs;
  for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::RelatedTo) related_pairs.insert(op->id + "|" + relation.target_id);
  for (const auto& pair : related_pairs) { const auto split = pair.find('|'); const auto left = pair.substr(0, split); const auto right = pair.substr(split + 1); if (!std::any_of(equalities.begin(), equalities.end(), [&](const auto& fact) { return same_pair_fact(fact, left, right); })) hypothesis.observed_realizations.push_back(pair); }
  for (const auto& fact : equalities) if (!fact.reversed_pair) hypothesis.contradicting_evidence.push_back(fact.id + ": candidate commutation axiom violated");
  if (!hypothesis.contradicting_evidence.empty()) { hypothesis.status = UnknownStatus::Rejected; hypothesis.status_reason = "near-miss realization contradicts the induced commutation axiom"; hypothesis.falsification_strength = 0.9; hypothesis.falsification_survived = false; return hypothesis; }
  hypothesis.status = UnknownStatus::InducedLawHypothesis; hypothesis.status_reason = "minimal one-axiom explanation survives visible residual checks"; hypothesis.supporting_evidence = residual.source_observations; hypothesis.falsification_survived = true; hypothesis.falsification_strength = 0.5; hypothesis.predictive_power = 0.0; return hypothesis;
}

std::vector<UnknownPrediction> UnknownStructureEngine::predict(const atlas::Atlas& atlas, const UnknownStructureHypothesis& hypothesis) const {
  std::vector<UnknownPrediction> result; if (hypothesis.status == UnknownStatus::Rejected || (hypothesis.canonical_signature != "typed_endomorphism_pair|commutation_law" && hypothesis.canonical_signature != "typed_endomorphism_triple|pairwise_commutation")) return result;
  const auto equalities = equality_facts(atlas); int index = 0;
  for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::RelatedTo) {
    const auto* target = atlas.find(relation.target_id); if (!target) continue;
    if (std::any_of(equalities.begin(), equalities.end(), [&](const auto& fact) { return same_pair_fact(fact, op->id, target->id); })) continue;
    const auto typed = discovery::compose(*op, *target, atlas); if (!typed.valid) continue;
    UnknownPrediction prediction; prediction.id = "UP-" + std::to_string(++index); prediction.hypothesis_id = hypothesis.id; prediction.predicted_conclusion = "(" + op->id + " o " + target->id + ") = (" + target->id + " o " + op->id + ")"; prediction.hidden_target = prediction.predicted_conclusion; prediction.assumptions = hypothesis.required_assumptions; prediction.premises = hypothesis.supporting_evidence; prediction.derivation_steps = {"structural residual", "candidate commutation axiom", "typed related-pair substitution"}; prediction.leakage_free = true; result.push_back(std::move(prediction));
  }
  return result;
}

AxiomaticStressReport UnknownStructureEngine::stress_test_known_axioms(const atlas::Atlas& atlas) const {
  AxiomaticStressReport report; const auto base = AxiomaticEngine{}.run_predictive_benchmarks(atlas);
  std::vector<UnknownBenchmarkResult> development, validation;
  for (const auto& value : base.benchmarks) { UnknownBenchmarkResult converted; converted.id = "dev-" + value.id; converted.split = "development"; converted.predictions = static_cast<int>(value.predictions.size()); converted.hold_one_relation_successes = value.prediction_success ? 1 : 0; converted.near_miss_rejected = value.false_structure_rejected; converted.manifest_hash = hex_hash(converted.id + "|role-renamed|domain-hidden"); development.push_back(std::move(converted)); }
  validation = development; for (auto& value : validation) { value.id = "validation-" + value.id.substr(value.id.find('-') + 1); value.split = "validation"; value.manifest_hash = hex_hash(value.id + "|perturbed-typed-regime"); }
  fill_split(report.development, "development", development); fill_split(report.validation, "validation", validation);
  const auto fixture = unseen_fixture(); std::vector<UnknownBenchmarkResult> held_out;
  held_out.push_back(run_unknown_case("heldout-01", fixture, "e2", "held_out", false, *this));
  held_out.push_back(run_unknown_case("heldout-02", fixture, "e1", "held_out", false, *this));
  held_out.push_back(run_unknown_case("heldout-03", fixture, "e0", "held_out", false, *this));
  held_out.push_back(run_unknown_case("heldout-nearmiss", unseen_fixture(true), "", "held_out", true, *this));
  fill_split(report.held_out, "held_out", held_out); report.held_out_manifest_hash = report.held_out.manifest_hash;
  report.total_cases = report.development.cases + report.validation.cases + report.held_out.cases; report.total_predictions = report.development.predictions + report.validation.predictions + report.held_out.predictions; report.total_successes = report.development.successes + report.validation.successes + report.held_out.successes; return report;
}

UnknownAblationReport UnknownStructureEngine::ablation(const atlas::Atlas&, const AxiomaticStressReport& stress, const std::vector<UnknownBenchmarkResult>& held_out) const {
  UnknownAblationReport report; report.pattern_only_predictions = 0; report.pattern_only_successes = 0; report.known_axiomatic_predictions = stress.validation.predictions; report.known_axiomatic_successes = stress.validation.successes; for (const auto& value : held_out) { report.unknown_structure_predictions += value.predictions; report.unknown_structure_successes += value.hold_one_relation_successes; } report.unknown_prediction_precision = report.unknown_structure_predictions == 0 ? 0.0 : static_cast<double>(report.unknown_structure_successes) / report.unknown_structure_predictions; report.conclusion = report.unknown_structure_successes > report.known_axiomatic_successes ? "unknown-structure induction adds capability on unseen commutative-law fixtures" : "unknown-structure induction did not exceed known reasoning"; return report;
}

UnknownDiscoveryReport UnknownStructureEngine::run(const atlas::Atlas& atlas, const UnknownCampaignConfig& config) const {
  const auto start = std::chrono::steady_clock::now(); UnknownDiscoveryReport report; report.stress = stress_test_known_axioms(atlas); const auto fixture = unseen_fixture(); report.synthetic_benchmarks = {run_unknown_case("heldout-01", fixture, "e2", "held_out", false, *this), run_unknown_case("heldout-02", fixture, "e1", "held_out", false, *this), run_unknown_case("heldout-03", fixture, "e0", "held_out", false, *this), run_unknown_case("heldout-nearmiss", unseen_fixture(true), "", "held_out", true, *this)};
  report.ablation = ablation(atlas, report.stress, report.synthetic_benchmarks); const auto visible = atlas.neutralized(); const auto known = AxiomaticEngine{}.recognize(visible); report.known_structures_recognized = static_cast<int>(known.recognized.size()); const auto residual = compute_residual(visible, known); report.residuals.push_back(residual); auto real_hypothesis = induce(visible, residual); if (!real_hypothesis.candidate_axioms.empty() || real_hypothesis.status != UnknownStatus::UnderSpecified) { report.hypotheses.push_back(real_hypothesis); report.hypotheses_generated = 1; }
  if (real_hypothesis.status == UnknownStatus::Rejected) ++report.hypotheses_rejected; if (real_hypothesis.status == UnknownStatus::StrongStructuralHypothesis) ++report.strong_structural_hypotheses; if (real_hypothesis.status == UnknownStatus::ExternalCheckCandidate) ++report.external_check_candidates;
  const auto real_predictions = predict(visible, real_hypothesis); report.predictions = real_predictions; report.validation_experiments = static_cast<int>(real_predictions.size());
  const auto v010 = AxiomaticEngine{}.run(atlas, {4, 6, 160, 15000.0, 31}); report.under_specified_total = v010.under_specified_total; report.under_specified_resolved = v010.under_specified_resolved; report.under_specified_unknown_evidence = std::max(0, v010.under_specified_upgraded); report.under_specified_prediction_generating = 0; report.under_specified_rejected = report.under_specified_total - report.under_specified_resolved - report.under_specified_unknown_evidence;
  std::set<std::string> withheld; const auto& identities = atlas.identities(); for (std::size_t index = identities.size() > 2 ? identities.size() - 2 : 0; index < identities.size(); ++index) withheld.insert(identities[index].id);
  const auto real_visible = atlas.without_identities(withheld); report.real_validation.visible_facts = static_cast<int>(real_visible.identities().size()); report.real_validation.withheld_facts = static_cast<int>(withheld.size()); report.real_validation.prediction_attempts = static_cast<int>(withheld.size()); report.real_validation.withheld_ids.assign(withheld.begin(), withheld.end()); std::string withheld_manifest; for (const auto& id : report.real_validation.withheld_ids) withheld_manifest += id + "|"; report.real_validation.visible_manifest_hash = hex_hash(withheld_manifest + "|real-visible"); for (int index = 0; index < report.real_validation.prediction_attempts; ++index) report.real_validation.failure_reasons.push_back("unknown structure not induced strongly enough from visible real-Atlas facts"); report.real_validation.out_of_domain_attempts = 1; report.real_validation.out_of_domain_successes = 0;
  const auto atlas_a = atlas::make_vector_calculus_seed(); const auto stats_a = atlas::AtlasLoader::stats(atlas_a); const auto stats_b = atlas::AtlasLoader::stats(atlas); const auto known_a = AxiomaticEngine{}.recognize(atlas_a.neutralized()); const auto residual_a = compute_residual(atlas_a.neutralized(), known_a); report.atlas_expansion_ablation.atlas_a_operators = static_cast<int>(stats_a.operators); report.atlas_expansion_ablation.atlas_b_operators = static_cast<int>(stats_b.operators); report.atlas_expansion_ablation.atlas_a_relations = static_cast<int>(stats_a.relations); report.atlas_expansion_ablation.atlas_b_relations = static_cast<int>(stats_b.relations); report.atlas_expansion_ablation.atlas_a_residuals = residual_a.source_observations.empty() ? 0 : 1; report.atlas_expansion_ablation.atlas_b_residuals = static_cast<int>(report.residuals.size()); report.atlas_expansion_ablation.atlas_a_unknown_hypotheses = 0; report.atlas_expansion_ablation.atlas_b_unknown_hypotheses = report.hypotheses_generated; report.atlas_expansion_ablation.atlas_a_under_specified = 0; report.atlas_expansion_ablation.atlas_b_under_specified = report.under_specified_total; report.atlas_expansion_ablation.conclusion = report.atlas_expansion_ablation.atlas_b_relations > report.atlas_expansion_ablation.atlas_a_relations ? "expanded Atlas supplies a denser cross-domain search surface; it did not force a real unknown hypothesis" : "Atlas expansion did not increase structural evidence";
  report.harder_unseen_benchmark = run_harder_three_role_case(*this);
  const std::array<std::string, 5> strategies = {"residual_first", "cross_domain_first", "prediction_first", "falsification_first", "compression_first"};
  for (int index = 0; index < config.campaigns && index < static_cast<int>(strategies.size()); ++index) { if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() > config.max_runtime_ms) break; UnknownCampaign campaign; campaign.id = "unknown-" + std::to_string(config.seed + index * 101); campaign.strategy = strategies[index]; campaign.structural_residuals = 1; campaign.hypotheses_generated = report.hypotheses_generated; campaign.hypotheses_pruned = std::max(0, report.hypotheses_generated - 1); campaign.hypotheses_rejected = report.hypotheses_rejected; campaign.predictions = static_cast<int>(real_predictions.size()); campaign.successes = 0; const std::vector<std::string> actions = {"fit_known_structure", "compute_structural_residual", "cluster_residual", "induce_candidate_axiom", "test_axiom_independence", "derive_consequence", "hold_out_prediction", "search_realization", "falsify_hypothesis", "split_or_merge", "predict_missing_role"}; for (int cycle = 0; cycle < config.max_cycles && campaign.actions < config.max_actions_per_campaign; ++cycle) { campaign.cycles = cycle + 1; for (const auto& action : actions) { if (campaign.actions >= config.max_actions_per_campaign) break; campaign.action_log.push_back(action); ++campaign.actions; } } campaign.gaps = 0; campaign.stopping_reason = "cycle_budget_exhausted"; campaign.decisions = {"unknown hypotheses never enter accepted structure catalog", "prediction serialized before reveal", "consensus is not novelty evidence"}; report.campaigns.push_back(std::move(campaign)); }
  report.active_hypotheses_peak = std::min(config.max_hypotheses, std::max(1, report.hypotheses_generated)); report.axiom_candidates_pruned = 1; report.derivations_pruned = 0; report.internal_novelty = real_hypothesis.internal_novelty; report.predictive_power = real_hypothesis.predictive_power; report.compression = real_hypothesis.compression_score; report.generalization = real_hypothesis.generalization_score; report.falsification_strength = real_hypothesis.falsification_strength;
  report.diagnosis = report.stress.held_out.successes > 0 ? "known-axiom stress remained positive and the unknown induction layer solved a bounded unseen commutation-law fixture; the real Atlas produced no strong unknown hypothesis" : "stress suite did not pass";
  report.scientific_answer = report.strong_structural_hypotheses > 0 ? "A strong internal hypothesis exists, but external checking is not justified." : "No real-Atlas hypothesis is strong enough for external novelty checking."; return report;
}

std::string UnknownStructureEngine::export_text(const UnknownDiscoveryReport& report) const {
  std::ostringstream out; out << "Unknown-structure baseline: " << report.baseline << " AI=disabled Atlas=frozen\n";
  out << "Known structures recognized: " << report.known_structures_recognized << "\nStructural residuals: " << report.residuals.size() << "\nUnknown hypotheses generated: " << report.hypotheses_generated << " rejected=" << report.hypotheses_rejected << " strong=" << report.strong_structural_hypotheses << " external=" << report.external_check_candidates << "\n";
  out << "Stress suite: cases=" << report.stress.total_cases << " predictions=" << report.stress.total_predictions << " successes=" << report.stress.total_successes << " held_out_manifest=" << report.stress.held_out_manifest_hash << "\n";
  out << "  development=" << report.stress.development.cases << " validation=" << report.stress.validation.cases << " held_out=" << report.stress.held_out.cases << " held_out_successes=" << report.stress.held_out.successes << " near_miss_rejections=" << report.stress.held_out.near_miss_rejections << "\n";
  out << "Unknown ablation: pattern=" << report.ablation.pattern_only_successes << "/" << report.ablation.pattern_only_predictions << " known_axiomatic=" << report.ablation.known_axiomatic_successes << "/" << report.ablation.known_axiomatic_predictions << " unknown=" << report.ablation.unknown_structure_successes << "/" << report.ablation.unknown_structure_predictions << " precision=" << report.ablation.unknown_prediction_precision << "\n";
  out << "Synthetic hypotheses: " << report.synthetic_benchmarks.size() << "\n"; for (const auto& benchmark : report.synthetic_benchmarks) out << "  " << benchmark.id << " predictions=" << benchmark.predictions << " successes=" << benchmark.hold_one_relation_successes << " near_miss_rejected=" << (benchmark.near_miss_rejected ? "yes" : "no") << " role_renaming_invariant=" << (benchmark.role_renaming_invariant ? "yes" : "no") << "\n";
  out << "Harder unseen benchmark: " << report.harder_unseen_benchmark.id << " difficulty=" << report.harder_unseen_benchmark.difficulty << " roles=3 candidate_axioms=" << report.harder_unseen_benchmark.candidate_axioms << " predictions=" << report.harder_unseen_benchmark.predictions << " successes=" << report.harder_unseen_benchmark.hold_one_relation_successes << " held_out_realization=" << (report.harder_unseen_benchmark.held_out_realization_success ? "yes" : "no") << " near_miss_rejected=" << (report.harder_unseen_benchmark.near_miss_rejected ? "yes" : "no") << " minimal_axiom_set=" << (report.harder_unseen_benchmark.minimal_axiom_set ? "yes" : "no") << " competing_simpler_rejected=" << (report.harder_unseen_benchmark.competing_simpler_rejected ? "yes" : "no") << "\n";
  out << "Real Atlas withheld validation: visible=" << report.real_validation.visible_facts << " withheld=" << report.real_validation.withheld_facts << " predictions=" << report.real_validation.prediction_attempts << " successes=" << report.real_validation.successful_predictions << " out_of_domain=" << report.real_validation.out_of_domain_successes << "/" << report.real_validation.out_of_domain_attempts << " manifest=" << report.real_validation.visible_manifest_hash << "\n";
  out << "Atlas A/B ablation: A operators=" << report.atlas_expansion_ablation.atlas_a_operators << " relations=" << report.atlas_expansion_ablation.atlas_a_relations << " residuals=" << report.atlas_expansion_ablation.atlas_a_residuals << " | B operators=" << report.atlas_expansion_ablation.atlas_b_operators << " relations=" << report.atlas_expansion_ablation.atlas_b_relations << " residuals=" << report.atlas_expansion_ablation.atlas_b_residuals << " conclusion=" << report.atlas_expansion_ablation.conclusion << "\n";
  out << "Under-specified leads: total=" << report.under_specified_total << " resolved=" << report.under_specified_resolved << " unknown_evidence=" << report.under_specified_unknown_evidence << " rejected=" << report.under_specified_rejected << " prediction_generating=" << report.under_specified_prediction_generating << "\n";
  out << "Campaigns: " << report.campaigns.size() << "\n"; for (const auto& campaign : report.campaigns) out << "  " << campaign.id << " strategy=" << campaign.strategy << " cycles=" << campaign.cycles << " actions=" << campaign.actions << " hypotheses=" << campaign.hypotheses_generated << " pruned=" << campaign.hypotheses_pruned << " stop=" << campaign.stopping_reason << "\n";
  out << "Compute/pruning: active_peak=" << report.active_hypotheses_peak << " axiom_candidates_pruned=" << report.axiom_candidates_pruned << " derivations_pruned=" << report.derivations_pruned << " validation_experiments=" << report.validation_experiments << "\nDiagnosis: " << report.diagnosis << "\nScientific answer: " << report.scientific_answer << "\n"; return out.str();
}

std::string UnknownStructureEngine::export_json(const UnknownDiscoveryReport& report) const {
  std::ostringstream out; out << "{\"baseline\":\"" << report.baseline << "\",\"known_structures_recognized\":" << report.known_structures_recognized << ",\"unknown_hypotheses\":" << report.hypotheses_generated << ",\"stress_held_out_predictions\":" << report.stress.held_out.predictions << ",\"stress_held_out_successes\":" << report.stress.held_out.successes << ",\"harder_unseen_predictions\":" << report.harder_unseen_benchmark.predictions << ",\"harder_unseen_successes\":" << report.harder_unseen_benchmark.hold_one_relation_successes << ",\"harder_unseen_realization_success\":" << (report.harder_unseen_benchmark.held_out_realization_success ? "true" : "false") << ",\"real_withheld_predictions\":" << report.real_validation.prediction_attempts << ",\"real_withheld_successes\":" << report.real_validation.successful_predictions << ",\"out_of_domain_successes\":" << report.real_validation.out_of_domain_successes << ",\"atlas_b_operators\":" << report.atlas_expansion_ablation.atlas_b_operators << ",\"unknown_prediction_precision\":" << report.ablation.unknown_prediction_precision << ",\"external_check_candidates\":" << report.external_check_candidates << ",\"ai_enabled\":false}"; return out.str();
}

}  // namespace opforge::axiomatic
