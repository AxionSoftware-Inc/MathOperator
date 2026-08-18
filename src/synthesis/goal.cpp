#include "opforge/synthesis/goal.hpp"

#include "opforge/discovery/composition.hpp"

#include <algorithm>
#include <functional>
#include <set>

namespace opforge::synthesis {
namespace {

OperatorCandidate make_goal_candidate(const atlas::OperatorSignature& signature,
                                      atlas::ExpressionPtr expression, const SynthesisGoal& goal,
                                      GrammarRule rule, const std::string& source) {
  OperatorCandidate candidate;
  candidate.expression = std::move(expression);
  candidate.signature = signature;
  candidate.canonical_form = canonical(candidate.expression);
  candidate.id = "G-" + std::to_string(std::hash<std::string>{}(candidate.canonical_form));
  candidate.construction_rule = to_string(rule);
  candidate.category = goal.role;
  candidate.semantic_category = "goal-directed candidate";
  candidate.novelty_status = "unconfirmed";
  candidate.lineage.source_patterns.push_back(source);
  candidate.lineage.source_gaps.push_back(goal.id);
  candidate.lineage.requirements = goal.requirements;
  candidate.lineage.construction_rules.push_back(to_string(rule));
  candidate.assumptions = goal.assumptions;
  candidate.expected_identities = goal.expected_identities;
  candidate.required_structures = goal.requirements;
  candidate.score.structural_fit = 0.5;
  candidate.score.type_completeness = 0.4;
  candidate.score.generalization_power = 0.3;
  candidate.interestingness.reasons.push_back("explicit goal: " + goal.role);
  candidate.interestingness.reasons.push_back("grammar rule justified by structural evidence");
  return candidate;
}

}  // namespace

const char* to_string(GoalRole role) {
  switch (role) {
    case GoalRole::MissingRole: return "missing_role";
    case GoalRole::Correction: return "correction";
    case GoalRole::Unification: return "unification";
    case GoalRole::Factorization: return "factorization";
    case GoalRole::Recovery: return "recovery";
    case GoalRole::Bridge: return "bridge";
  }
  return "unknown";
}

const char* to_string(GrammarRule rule) {
  switch (rule) {
    case GrammarRule::Adjoint: return "adjoint";
    case GrammarRule::Commutator: return "commutator";
    case GrammarRule::AntiCommutator: return "anti_commutator";
    case GrammarRule::DirectSum: return "direct_sum";
    case GrammarRule::ProjectionInclusion: return "projection_inclusion";
    case GrammarRule::WeightedLinearCombination: return "weighted_linear_combination";
    case GrammarRule::CorrectionTerm: return "correction_term";
    case GrammarRule::Conjugation: return "conjugation";
    case GrammarRule::RestrictionExtension: return "restriction_extension";
  }
  return "unknown";
}

std::vector<SynthesisGoal> GoalDirectedSynthesizer::derive_goals(
    const atlas::Atlas& atlas, const patterns::MetaPatternReport& meta,
    const std::vector<research::ResidualCluster>& residuals) const {
  std::vector<SynthesisGoal> goals;
  for (const auto& prediction : meta.predictions) {
    SynthesisGoal goal;
    goal.id = "G-missing-" + prediction.id;
    goal.source_meta_pattern = prediction.source_meta_pattern;
    goal.role = "missing_role";
    goal.purpose = prediction.goal;
    goal.expected_signature.domain = {prediction.expected_signature.input_role, "predicted input"};
    goal.expected_signature.codomain = {prediction.expected_signature.output_role, "predicted output"};
    goal.expected_signature.differential_order = prediction.expected_signature.differential_order;
    goal.expected_signature.linear = prediction.expected_signature.linear;
    goal.expected_signature.local = prediction.expected_signature.local;
    goal.requirements = {"meta-pattern:" + prediction.source_meta_pattern};
    goal.expected_identities = prediction.expected_identities;
    goal.assumptions = prediction.expected_assumptions;
    goal.justification = prediction.confidence_reasons;
    goals.push_back(std::move(goal));
  }
  for (const auto& residual : residuals) {
    SynthesisGoal goal;
    goal.id = "G-correction-" + residual.id;
    goal.source_residual_cluster = residual.id;
    goal.role = "correction";
    goal.purpose = "find C such that A = B + C and C approximates the typed residual";
    goal.expected_signature.domain = {residual.domains.empty() ? "" : residual.domains.front(), "residual domain"};
    goal.expected_signature.codomain = {residual.codomains.empty() ? "" : residual.codomains.front(), "residual codomain"};
    goal.expected_signature.differential_order = -1;
    goal.expected_signature.linear = true;
    goal.expected_signature.local = true;
    goal.requirements = residual.correction_requirements;
    goal.justification = {"residual cluster " + residual.id, "failure-to-generalization loop"};
    goals.push_back(std::move(goal));
  }
  (void)atlas;
  return goals;
}

std::vector<GrammarRule> GoalDirectedSynthesizer::active_rules(const atlas::Atlas& atlas,
                                                               const SynthesisGoal& goal) const {
  std::vector<GrammarRule> rules;
  if (goal.role == "correction") rules.push_back(GrammarRule::CorrectionTerm);
  if (goal.role == "missing_role") rules.push_back(GrammarRule::WeightedLinearCombination);
  bool has_adjoint = false, has_projection = false;
  for (const auto* op : atlas.all()) {
    has_adjoint |= std::find(op->signature.required_structures.begin(), op->signature.required_structures.end(), "inner_product") != op->signature.required_structures.end();
    has_projection |= op->id.find("projection") != std::string::npos;
  }
  if (has_adjoint) rules.push_back(GrammarRule::Adjoint);
  if (has_projection) rules.push_back(GrammarRule::ProjectionInclusion);
  return rules;
}

std::vector<GoalDirectedCandidate> GoalDirectedSynthesizer::synthesize(
    const atlas::Atlas& atlas, const std::vector<SynthesisGoal>& goals, int limit) const {
  std::vector<GoalDirectedCandidate> result;
  std::set<std::string> seen;
  for (const auto& goal : goals) {
    for (const auto rule : active_rules(atlas, goal)) {
      if (static_cast<int>(result.size()) >= limit) return result;
      if (rule == GrammarRule::Adjoint) {
        for (const auto* op : atlas.all()) {
          if (op->signature.domain.id != goal.expected_signature.codomain.id && goal.role == "correction") continue;
          auto signature = op->signature;
          std::swap(signature.domain, signature.codomain);
          std::swap(signature.input_kind, signature.output_kind);
          signature.required_structures.push_back("inner_product");
          auto expression = atlas::Expression::adjoint(atlas::Expression::ref(op->id));
          auto candidate = make_goal_candidate(signature, expression, goal, rule, goal.source_residual_cluster);
          if (seen.insert(candidate.canonical_form).second) result.push_back({std::move(candidate), goal, rule, false, {}});
        }
      } else if (rule == GrammarRule::CorrectionTerm || rule == GrammarRule::WeightedLinearCombination) {
        std::vector<const atlas::OperatorRecord*> matches;
        for (const auto* op : atlas.all()) {
          if (op->signature.domain.id == goal.expected_signature.domain.id &&
              op->signature.codomain.id == goal.expected_signature.codomain.id) matches.push_back(op);
        }
        for (size_t i = 0; i < matches.size(); ++i) {
          for (size_t j = i + 1; j < matches.size(); ++j) {
            auto signature = matches[i]->signature;
            auto expression = atlas::Expression::addition(
                atlas::Expression::ref(matches[i]->id),
                atlas::Expression::scalar_multiplication("-1", atlas::Expression::ref(matches[j]->id)));
            auto candidate = make_goal_candidate(signature, expression, goal, rule,
                                                  goal.source_residual_cluster);
            if (seen.insert(candidate.canonical_form).second) result.push_back({std::move(candidate), goal, rule, true, {}});
            if (static_cast<int>(result.size()) >= limit) return result;
          }
        }
      }
    }
  }
  return result;
}

}  // namespace opforge::synthesis
