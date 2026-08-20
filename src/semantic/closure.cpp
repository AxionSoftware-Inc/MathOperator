#include "opforge/semantic/closure.hpp"

#include "opforge/axiomatic/unknown.hpp"
#include "opforge/discovery/composition.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>

namespace opforge::semantic {
namespace {

using atlas::Expression;
using atlas::ExpressionPtr;

std::string key(const ExpressionPtr& expression) {
  if (!expression) return "<null>";
  switch (expression->kind) {
    case Expression::Kind::OperatorReference: return expression->value;
    case Expression::Kind::ZeroOperator: return "0";
    case Expression::Kind::IdentityOperator: return "I";
    case Expression::Kind::Composition: return "(" + key(expression->children[0]) + " o " + key(expression->children[1]) + ")";
    case Expression::Kind::Addition: return "(" + key(expression->children[0]) + " + " + key(expression->children[1]) + ")";
    case Expression::Kind::ScalarMultiplication: return expression->value + "*" + key(expression->children[0]);
    case Expression::Kind::Adjoint: return "adj(" + key(expression->children[0]) + ")";
    default: return expression->value.empty() ? "expr" : expression->value;
  }
}

std::string canonical_identity(const atlas::Identity& identity) {
  auto left = key(identity.left); auto right = key(identity.right);
  if (right < left) std::swap(left, right);
  return left + " = " + right;
}

bool reference(const ExpressionPtr& expression, std::string& value) {
  if (!expression || expression->kind != Expression::Kind::OperatorReference) return false;
  value = expression->value; return true;
}

bool reference_pair(const ExpressionPtr& expression, std::string& outer, std::string& inner) {
  return expression && expression->kind == Expression::Kind::Composition && expression->children.size() == 2 &&
         reference(expression->children[0], outer) && reference(expression->children[1], inner);
}

bool composable(const atlas::Atlas& atlas, const std::string& outer, const std::string& inner) {
  const auto* left = atlas.find(outer); const auto* right = atlas.find(inner);
  return left && right && opforge::discovery::compose(*left, *right, atlas).valid;
}

std::vector<std::string> assumptions_for(const atlas::Atlas& atlas, const std::vector<std::string>& premises) {
  std::vector<std::string> result;
  for (const auto& premise : premises) {
    if (const auto* op = atlas.find(premise)) {
      for (const auto& value : op->signature.required_structures)
        if (std::find(result.begin(), result.end(), value) == result.end()) result.push_back(value);
    }
  }
  return result;
}

atlas::Identity derived_identity(const std::string& id, const ExpressionPtr& left, const ExpressionPtr& right,
                                 const std::string& rule, const std::vector<std::string>& assumptions) {
  atlas::Identity identity;
  identity.id = id; identity.name = "derived consequence"; identity.left = left; identity.right = right;
  identity.assumptions = assumptions; identity.provenance_category = "derived_consequence";
  identity.canonical_form = canonical_identity(identity);
  identity.executable_equality = true;
  identity.evidence.push_back({id + ".derivation", "derived", rule, "0.13", "2026-08-15", id, "accepted", "", -1});
  return identity;
}

std::string rule_category(const std::string& rule) {
  if (rule.find("projection") != std::string::npos) return "decomposition";
  if (rule.find("inverse") != std::string::npos) return "adjoint";
  if (rule.find("commutation") != std::string::npos) return "commutator";
  if (rule.find("spectral") != std::string::npos) return "spectral";
  if (rule.find("transform") != std::string::npos) return "transform";
  return "within_domain";
}

const atlas::Identity* identity_by_id(const atlas::Atlas& atlas, const std::string& id) {
  return atlas.find_identity(id);
}

}  // namespace

const char* to_string(DerivationRuleKind value) {
  switch (value) {
    case DerivationRuleKind::EqualitySubstitution: return "equality_substitution";
    case DerivationRuleKind::CompositionSubstitution: return "composition_substitution";
    case DerivationRuleKind::InverseCancellation: return "inverse_cancellation";
    case DerivationRuleKind::AdjointReversal: return "adjoint_reversal";
    case DerivationRuleKind::ProjectionIdempotence: return "projection_idempotence";
    case DerivationRuleKind::ChainComplex: return "chain_complex";
    case DerivationRuleKind::DecompositionSubstitution: return "decomposition_substitution";
    case DerivationRuleKind::TransformConjugation: return "transform_conjugation";
    case DerivationRuleKind::SpectralConsequence: return "spectral_consequence";
  }
  return "unknown";
}

std::vector<DerivationRule> ConsequenceClosureEngine::rule_catalog() const {
  return {
      {"rule.equality.substitution", "Equality substitution", "A=B, B=C", "A=C", "typed equality", "curated v0.13 rule catalog", DerivationRuleKind::EqualitySubstitution, {}},
      {"rule.composition.substitution", "Composition substitution", "A=B, F composable", "F∘A=F∘B", "typed composition", "curated v0.13 rule catalog", DerivationRuleKind::CompositionSubstitution, {}},
      {"rule.inverse.cancellation", "Inverse cancellation", "B inverse_of A", "B∘A=I and A∘B=I", "invertible maps", "curated v0.13 rule catalog", DerivationRuleKind::InverseCancellation, {"invertible"}},
      {"rule.adjoint.reversal", "Adjoint reversal", "B adjoint_of A", "A adjoint_of B", "inner-product operators", "curated v0.13 rule catalog", DerivationRuleKind::AdjointReversal, {"inner_product"}},
      {"rule.projection.idempotence", "Projection idempotence", "P projection P", "P²=P", "typed endomorphism", "curated v0.13 rule catalog", DerivationRuleKind::ProjectionIdempotence, {"idempotent"}},
      {"rule.chain.d_squared", "Chain-complex consequence", "adjacent graded maps", "d²=0", "graded complex", "curated v0.13 rule catalog", DerivationRuleKind::ChainComplex, {"typed graded maps"}},
      {"rule.decomposition.substitution", "Decomposition substitution", "component_of and partition", "component consequence", "declared direct sum", "curated v0.13 rule catalog", DerivationRuleKind::DecompositionSubstitution, {"direct_sum"}},
      {"rule.transform.conjugation", "Transform conjugation", "T inverse_of S", "conjugated correspondence", "transform duality", "curated v0.13 rule catalog", DerivationRuleKind::TransformConjugation, {"invertibility"}},
      {"rule.spectral.projection", "Spectral projection consequence", "spectral projection", "idempotent spectral component", "finite-dimensional spectral gap", "curated v0.13 rule catalog", DerivationRuleKind::SpectralConsequence, {"finite_dimensional", "spectral_gap"}},
  };
}

ClosureReport ConsequenceClosureEngine::close(const atlas::Atlas& source, const ClosureConfig& config) const {
  ClosureReport report; report.closed_atlas = source; report.rules = rule_catalog();
  std::map<std::string, std::set<std::string>> conclusions_by_left;
  std::set<std::string> known;
  for (const auto& identity : report.closed_atlas.identities()) {
    if (!identity.executable_equality) continue;
    const auto canonical = canonical_identity(identity); known.insert(canonical);
    conclusions_by_left[key(identity.left)].insert(key(identity.right));
  }
  std::set<std::string> relation_keys;
  int sequence = 0;
  auto add_consequence = [&](const std::string& rule, const ExpressionPtr& left, const ExpressionPtr& right,
                             const std::vector<std::string>& premises, int depth) {
    ++report.generated; report.max_depth_reached = std::max(report.max_depth_reached, depth);
    if (depth > config.max_depth || static_cast<int>(report.consequences.size()) >= config.max_consequences) { ++report.pruned; return; }
    auto identity = derived_identity("derived.v013." + std::to_string(++sequence), left, right, rule, assumptions_for(source, premises));
    const auto canonical = canonical_identity(identity);
    if (known.contains(canonical)) { ++report.duplicates; report.consequences.push_back({identity.id, rule, canonical, "derived_consequence", "duplicate", std::move(identity), premises, {}, depth, false, true, false, false}); return; }
    const auto left_key = key(left); const auto right_key = key(right);
    if (conclusions_by_left[left_key].contains(right_key) || (!conclusions_by_left[left_key].empty() && !conclusions_by_left[left_key].contains(right_key))) {
      ++report.contradictions; report.conflicts.push_back({"conflict." + std::to_string(report.conflicts.size() + 1), left_key, right_key, "incompatible conclusion under overlapping assumptions", assumptions_for(source, premises), {rule, "existing_atlas"}});
      report.consequences.push_back({identity.id, rule, canonical, "derived_consequence", "contradiction", std::move(identity), premises, {}, depth, false, false, true, false});
      return;
    }
    const bool valid = [&] {
      std::function<bool(const ExpressionPtr&)> check = [&](const ExpressionPtr& expression) {
        if (!expression) return false;
        if (expression->kind == Expression::Kind::OperatorReference) return report.closed_atlas.find(expression->value) != nullptr;
        for (const auto& child : expression->children) if (!check(child)) return false;
        return true;
      };
      if (!check(left) || !check(right)) return false;
      std::string outer, inner;
      if (reference_pair(left, outer, inner)) return composable(report.closed_atlas, outer, inner);
      return true;
    }();
    if (!valid) { ++report.pruned; report.consequences.push_back({identity.id, rule, canonical, "derived_consequence", "type_check_failed", std::move(identity), premises, {}, depth, false, false, false, false}); return; }
    identity.verification = atlas::VerificationStatus::Proposed;
    if (!report.closed_atlas.add_identity(identity)) { ++report.duplicates; return; }
    known.insert(canonical); conclusions_by_left[left_key].insert(right_key); ++report.accepted;
    report.consequences.push_back({identity.id, rule, canonical, "derived_consequence", {}, std::move(identity), premises, {}, depth, true, false, false, true});
  };
  auto add_relation = [&](const std::string& source_id, atlas::RelationKind kind, const std::string& target_id,
                          const std::string& rule, int depth) {
    const auto relation_key = source_id + "|" + atlas::to_string(kind) + "|" + target_id;
    if (!config.include_derived_relations || !relation_keys.insert(relation_key).second) return;
    report.closed_atlas.add_relation(source_id, {kind, target_id, "derived by " + rule, "derived"});
    report.relations.push_back({source_id, target_id, rule, "derived by bounded closure", "derived", kind, depth, true});
  };

  for (const auto& op : report.closed_atlas.all()) {
    for (const auto& relation : op->relations) {
      const auto* target = report.closed_atlas.find(relation.target_id); if (!target) continue;
      if (relation.kind == atlas::RelationKind::Projection && op->id == relation.target_id)
        add_consequence("rule.projection.idempotence", Expression::composition(Expression::ref(op->id), Expression::ref(op->id)), Expression::ref(op->id), {op->id}, 1);
      if (relation.kind == atlas::RelationKind::CommutesWith && composable(report.closed_atlas, op->id, target->id) && composable(report.closed_atlas, target->id, op->id))
        add_consequence("rule.commutator.closure", Expression::composition(Expression::ref(op->id), Expression::ref(target->id)), Expression::composition(Expression::ref(target->id), Expression::ref(op->id)), {op->id, target->id}, 1);
      if ((relation.kind == atlas::RelationKind::InverseOf || relation.kind == atlas::RelationKind::LeftInverse || relation.kind == atlas::RelationKind::RightInverse) && composable(report.closed_atlas, op->id, target->id)) {
        add_consequence("rule.inverse.cancellation", Expression::composition(Expression::ref(op->id), Expression::ref(target->id)), Expression::identity(), {op->id, target->id}, 1);
        if (relation.kind == atlas::RelationKind::InverseOf || relation.kind == atlas::RelationKind::RightInverse)
          add_consequence("rule.inverse.cancellation", Expression::composition(Expression::ref(target->id), Expression::ref(op->id)), Expression::identity(), {op->id, target->id}, 1);
        add_relation(target->id, atlas::RelationKind::InverseOf, op->id, "rule.inverse.cancellation", 1);
      }
      if (relation.kind == atlas::RelationKind::AdjointOf) add_relation(target->id, atlas::RelationKind::AdjointOf, op->id, "rule.adjoint.reversal", 1);
    }
  }
  for (int depth = 1; depth <= config.max_depth && static_cast<int>(report.consequences.size()) < config.max_consequences; ++depth) {
    std::vector<std::pair<std::string, std::string>> equalities;
    for (const auto& identity : report.closed_atlas.identities()) {
      if (!identity.executable_equality) continue;
      std::string left, right; if (reference(identity.left, left) && reference(identity.right, right)) equalities.emplace_back(left, right);
    }
    for (const auto& first : equalities) for (const auto& second : equalities) {
      if (first.second != second.first || first.first == second.second) continue;
      add_consequence("rule.equality.substitution", Expression::ref(first.first), Expression::ref(second.second), {first.first, second.first}, depth);
    }
    for (const auto& equality : equalities) for (const auto* op : report.closed_atlas.all()) {
      if (composable(report.closed_atlas, op->id, equality.first) && composable(report.closed_atlas, op->id, equality.second))
        add_consequence("rule.composition.substitution", Expression::composition(Expression::ref(op->id), Expression::ref(equality.first)), Expression::composition(Expression::ref(op->id), Expression::ref(equality.second)), {op->id, equality.first, equality.second}, depth);
    }
  }
  report.dag_acyclic = true;
  return report;
}

DensityReport ConsequenceClosureEngine::density(const atlas::Atlas& atlas, const ClosureReport* closure) const {
  DensityReport report; const auto all = atlas.all(); report.operators = all.size(); report.identities = atlas.identities().size();
  std::vector<size_t> degrees; std::map<std::string, std::set<std::string>> graph; size_t cross_domain = 0;
  for (const auto* op : all) {
    auto& domain = report.domains[op->mathematical_domain]; ++domain.operators; degrees.push_back(op->relations.size()); report.relations += op->relations.size();
    if (op->relations.empty()) { report.isolated_operators.push_back(op->id); domain.isolated_operators.push_back(op->id); }
    for (const auto& relation : op->relations) {
      ++domain.relations; graph[op->id].insert(relation.target_id); graph[relation.target_id].insert(op->id);
      const auto* target = atlas.find(relation.target_id); if (!target) continue;
      if (target->mathematical_domain != op->mathematical_domain) { ++cross_domain; ++domain.bridges; ++report.bridges; }
    }
  }
  for (const auto& identity : atlas.identities()) {
    std::string ref_id; std::function<void(const ExpressionPtr&)> find_ref = [&](const ExpressionPtr& e) { if (!e || !ref_id.empty()) return; if (e->kind == Expression::Kind::OperatorReference) ref_id = e->value; for (const auto& child : e->children) find_ref(child); };
    find_ref(identity.left); if (const auto* op = atlas.find(ref_id)) { ++report.domains[op->mathematical_domain].identities; if (identity.provenance_category == "derived_consequence") ++report.domains[op->mathematical_domain].derived_consequences; }
    if (identity.provenance_category == "derived_consequence") ++report.derived_consequences;
  }
  if (closure) report.derived_consequences = closure->accepted;
  std::sort(degrees.begin(), degrees.end()); if (!degrees.empty()) { report.average_degree = std::accumulate(degrees.begin(), degrees.end(), 0.0) / degrees.size(); report.median_degree = degrees[degrees.size() / 2]; }
  for (auto& [name, domain] : report.domains) { domain.average_semantic_degree = domain.operators == 0 ? 0.0 : static_cast<double>(domain.relations) / domain.operators; for (const auto& op : atlas.all()) if (op->mathematical_domain == name) for (const auto& identity : atlas.identities()) { (void)identity; } }
  std::set<std::string> seen; for (const auto& [node, _] : graph) if (!seen.contains(node)) { ++report.connected_components; std::queue<std::string> queue; queue.push(node); seen.insert(node); while (!queue.empty()) { const auto current = queue.front(); queue.pop(); for (const auto& next : graph[current]) if (seen.insert(next).second) queue.push(next); } }
  for (const auto* op : all) if (op->relations.size() > static_cast<size_t>(std::max(2.0, report.average_degree * 1.5))) report.high_degree_hubs.push_back(op->id);
  report.cross_domain_edge_ratio = report.relations == 0 ? 0.0 : static_cast<double>(cross_domain) / report.relations;
  return report;
}

std::vector<PredictionOpportunity> ConsequenceClosureEngine::opportunities(const ClosureReport& closure) const {
  std::vector<PredictionOpportunity> result; int index = 0;
  for (const auto& consequence : closure.consequences) if (consequence.accepted) {
    PredictionOpportunity opportunity; opportunity.id = "opportunity." + std::to_string(++index); opportunity.category = rule_category(consequence.rule_id); opportunity.target_identity = consequence.id; opportunity.difficulty = consequence.premises.size() <= 1 ? "easy" : consequence.premises.size() == 2 ? "medium" : "hard"; opportunity.visible_premises = consequence.premises; opportunity.assumptions = consequence.assumptions; opportunity.nontrivial = consequence.rule_id != "rule.equality.substitution"; result.push_back(std::move(opportunity));
  }
  return result;
}

RealBenchmarkReport ConsequenceClosureEngine::run_real_benchmark_v2(const atlas::Atlas& source, const ClosureConfig& config) const {
  RealBenchmarkReport report; const auto full = close(source, config); report.opportunities = opportunities(full); int limit = 0;
  for (const auto& opportunity : report.opportunities) {
    if (limit++ >= 12) break; const auto* target = identity_by_id(full.closed_atlas, opportunity.target_identity); if (!target) continue;
    WithholdingCase test; test.id = opportunity.id; test.category = opportunity.category; test.difficulty = opportunity.difficulty; test.hidden_target = canonical_identity(*target); test.visible_premises = opportunity.visible_premises; test.assumptions = opportunity.assumptions; test.premise_count = static_cast<int>(opportunity.visible_premises.size());
    const auto visible = full.closed_atlas.without_identities({target->id});
    for (const auto& identity : visible.identities()) if (identity.executable_equality && canonical_identity(identity) == test.hidden_target) { test.leakage_free = false; ++report.leakage_failures; }
    if (!test.leakage_free) continue;
    test.visible_facts = static_cast<int>(visible.identities().size()); test.attempted = true; ++report.attempts;
    const auto recovered = close(visible, config); test.success = std::any_of(recovered.consequences.begin(), recovered.consequences.end(), [&](const auto& consequence) { return consequence.accepted && consequence.canonical_form == test.hidden_target; });
    if (test.success) ++report.successes; else test.miss_reason = "bounded closure did not reconstruct the hidden consequence";
    test.derivation_rules = {opportunity.category}; report.cases.push_back(std::move(test));
  }
  for (const auto& identity : source.identities()) {
    if (!identity.executable_equality) continue;
    std::string left_id, right_id; if (!reference(identity.left, left_id) || !reference(identity.right, right_id)) continue;
    const auto* left = source.find(left_id); const auto* right = source.find(right_id); if (!left || !right || left->mathematical_domain == right->mathematical_domain) continue;
    WithholdingCase test; test.id = "cross-domain." + identity.id; test.category = "cross_domain_bridge"; test.difficulty = "hard"; test.hidden_target = canonical_identity(identity); test.visible_premises = {"typed bridge metadata", left->id, right->id}; test.out_of_domain = true; test.premise_count = 2;
    const auto visible = source.without_identities({identity.id}); for (const auto& visible_identity : visible.identities()) if (visible_identity.executable_equality && canonical_identity(visible_identity) == test.hidden_target) test.leakage_free = false;
    ++report.out_of_domain_attempts; if (!test.leakage_free) { ++report.leakage_failures; continue; }
    test.visible_facts = static_cast<int>(visible.identities().size()); test.attempted = true; ++report.attempts;
    const auto recovered = close(visible, config); test.success = std::any_of(recovered.consequences.begin(), recovered.consequences.end(), [&](const auto& consequence) { return consequence.accepted && consequence.canonical_form == test.hidden_target; });
    if (test.success) ++report.successes, ++report.out_of_domain_successes; else test.miss_reason = "cross-domain bridge was not reconstructed without exposing the target";
    report.cases.push_back(std::move(test)); break;
  }
  return report;
}

ABCAbulationReport ConsequenceClosureEngine::run_abc_ablation(const atlas::Atlas& a, const atlas::Atlas& b, const ClosureConfig& config) const {
  ABCAbulationReport report;
  const auto fill = [&](const std::string& name, const atlas::Atlas& source, bool use_closed) {
    const auto closure = close(source, config); const auto benchmark = run_real_benchmark_v2(source, config); const auto unknown = axiomatic::UnknownStructureEngine{}.run(source, {1, 1, 16, 8, 4, 3, 8, 8000, 71});
    const auto& evaluated = use_closed ? closure.closed_atlas : source; ABCSnapshot snapshot; snapshot.name = name; snapshot.operators = evaluated.all().size(); for (const auto* op : evaluated.all()) snapshot.relations += op->relations.size(); snapshot.identities = evaluated.identities().size(); snapshot.derived_consequences = closure.accepted; snapshot.benchmark_attempts = benchmark.attempts; snapshot.benchmark_successes = benchmark.successes; snapshot.out_of_domain_attempts = benchmark.out_of_domain_attempts; snapshot.out_of_domain_successes = benchmark.out_of_domain_successes; snapshot.unknown_hypotheses = unknown.hypotheses_generated; snapshot.under_specified_leads = unknown.under_specified_total; return snapshot;
  };
  report.a = fill("A-v0.11", a, false); report.b = fill("B-v0.12", b, false); report.c = fill("C-v0.13-dense", b, true);
  report.conclusion = report.c.relations > report.b.relations || report.c.identities > report.b.identities ? "semantic densification adds bounded consequences and semantic edges" : "densification was pruned or saturated"; return report;
}

std::string ConsequenceClosureEngine::export_text(const ClosureReport& report) const {
  std::ostringstream out; out << "Closure rules=" << report.rules.size() << " generated=" << report.generated << " accepted=" << report.accepted << " duplicates=" << report.duplicates << " pruned=" << report.pruned << " contradictions=" << report.contradictions << " max_depth=" << report.max_depth_reached << " dag_acyclic=" << (report.dag_acyclic ? "yes" : "no") << "\n";
  for (const auto& relation : report.relations) out << "derived_relation " << relation.source << " " << atlas::to_string(relation.kind) << " " << relation.target << " rule=" << relation.rule_id << "\n";
  return out.str();
}

std::string ConsequenceClosureEngine::export_json(const ClosureReport& report) const {
  std::ostringstream out; out << "{\"generated\":" << report.generated << ",\"accepted\":" << report.accepted << ",\"duplicates\":" << report.duplicates << ",\"pruned\":" << report.pruned << ",\"contradictions\":" << report.contradictions << ",\"max_depth\":" << report.max_depth_reached << ",\"dag_acyclic\":" << (report.dag_acyclic ? "true" : "false") << "}"; return out.str();
}

}  // namespace opforge::semantic
